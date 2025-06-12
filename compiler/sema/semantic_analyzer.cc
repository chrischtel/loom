// semantic_analyzer.cc
#include "semantic_analyzer.hh"

#include <iostream>

// --- Konstruktor und Hauptfunktionen ---

SemanticAnalyzer::SemanticAnalyzer() : had_error(false) {
  // Der Konstruktor der SymbolTable wird automatisch aufgerufen
  // und erstellt den globalen Scope für uns.
}

void SemanticAnalyzer::analyze(
    const std::vector<std::unique_ptr<StmtNode>>& ast) {
  for (const auto& stmt : ast) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void SemanticAnalyzer::error(const LoomSourceLocation& loc,
                             const std::string& message) {
  had_error = true;
  std::cerr << "Semantic Error at " << loc.toString() << ": " << message
            << std::endl;
}

// --- visit-Methoden für Statements (geben void zurück) ---

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(VarDeclNode& node) {
  // Schritt 1: Analysiere den Initializer zuerst (falls vorhanden) und hole
  // seinen Typ.
  std::unique_ptr<TypeNode> initializer_type = nullptr;
  if (node.initializer) {
    // Rufe die `accept`-Methode auf, die einen SemaVisitor akzeptiert und einen
    // Typ zurückgibt.
    initializer_type = node.initializer->accept(*this);
  }

  // Schritt 2: Analysiere den deklarierten Typ (falls vorhanden).
  if (node.type) {
    // Das dient im Moment nur der Vollständigkeit. Später könntest du hier
    // prüfen, ob der Typ "i32" überhaupt ein bekannter Typ ist.
    node.type->accept(*this);
  }

  // Schritt 3: Führe die Typ-Prüfung durch.
  if (node.type && initializer_type) {  // Fall A: Typ ist deklariert UND es
                                        // gibt einen Initializer.
    // Wir müssen prüfen, ob die Typen übereinstimmen.
    if (!node.type->isEqualTo(initializer_type.get())) {
      error(node.location,
            "Type mismatch: Cannot initialize variable of type '" +
                node.type->getTypeName() + "' with value of type '" +
                initializer_type->getTypeName() + "'.");
    }
  }

  // Schritt 4: Bestimme den finalen Typ der Variable und speichere ihn.
  std::unique_ptr<TypeNode> final_type = nullptr;
  if (node.type) {
    // Wenn ein Typ explizit angegeben wurde, nehmen wir den.
    // Wir müssen eine Kopie erstellen, da der `node.type` unique ist.
    final_type = node.type->accept(*this);
  } else if (initializer_type) {
    // Ansonsten inferieren (schlussfolgern) wir den Typ vom Initializer.
    final_type = std::move(initializer_type);
  } else {
    // Fall C: Kein Typ und kein Initializer. Das ist ein Fehler in unserer
    // Sprache.
    error(node.location, "Cannot infer type for variable '" + node.name +
                             "' without an explicit type or an initializer.");
    return nullptr;  // Beende die Analyse für diese fehlerhafte Deklaration.
  }
  // Schritt 5: Definiere die Variable in der Symboltabelle.
  SymbolInfo info;
  info.kind = node.kind;
  info.type = std::shared_ptr<TypeNode>(final_type.release());

  if (!symbols.define(node.name, std::move(info))) {
    error(node.location,
          "Variable '" + node.name + "' is already declared in this scope.");
  }
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(ExprStmtNode& node) {
  if (node.expression) {
    // Wir rufen accept auf, aber ignorieren den zurückgegebenen Typ,
    // da das Ergebnis des Ausdrucks nicht verwendet wird.
    node.expression->accept(*this);
  }
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(TypeNode& /* node */) {
  // Vorerst nichts zu tun. Später könnten wir hier prüfen,
  // ob der Typname (z.B. "i32") ein gültiger, bekannter Typ ist.
  return nullptr;
}

// --- visit-Methoden für Expressions (geben einen Typ zurück) ---

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(NumberLiteral& node) {
  if (node.is_float) {
    return std::make_unique<FloatTypeNode>(node.location,
                                           64);  // Default to f64
  } else {
    return std::make_unique<IntegerTypeNode>(node.location,
                                             32);  // Default to i32
  }
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(BooleanLiteral& node) {
  return std::make_unique<BooleanTypeNode>(node.location);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(StringLiteral& node) {
  return std::make_unique<StringTypeNode>(node.location);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(Identifier& node) {
  const SymbolInfo* info = symbols.lookup(node.name);
  if (info == nullptr) {
    error(node.location, "Undeclared identifier '" + node.name + "'.");
    return nullptr;
  }  // Create a copy by using the visitor pattern on the stored type
  return info->type->accept(*this);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(AssignmentExpr& node) {
  std::unique_ptr<TypeNode> value_type = node.value->accept(*this);
  if (!value_type) return nullptr;

  const SymbolInfo* info = symbols.lookup(node.name);

  if (info == nullptr) {
    error(node.location, "Undeclared identifier '" + node.name + "'.");
    return nullptr;
  }
  if (info->kind != VarDeclKind::MUT) {
    error(node.location,
          "Cannot assign to immutable variable '" + node.name + "'.");
    return nullptr;
  }
  if (!info->type->isEqualTo(value_type.get())) {
    error(node.location, "Type mismatch: Cannot assign value of type '" +
                             value_type->getTypeName() + "' to variable '" +
                             node.name + "' of type '" +
                             info->type->getTypeName() + "'.");
    return nullptr;
  }

  // Der Typ des Zuweisungs-Ausdrucks ist der Typ des zugewiesenen Wertes.
  return value_type;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(UnaryExpr& node) {
  std::unique_ptr<TypeNode> right_type = node.right->accept(*this);
  if (!right_type) return nullptr;
  switch (node.op.type) {
    case TokenType::TOKEN_BANG:
      // Logical NOT operator always returns bool
      return std::make_unique<BooleanTypeNode>(node.location);

    case TokenType::TOKEN_MINUS:
      // Check if the type supports unary minus (integers and floats)
      if (dynamic_cast<const IntegerTypeNode*>(right_type.get()) ||
          dynamic_cast<const FloatTypeNode*>(right_type.get())) {
        // Return the same type as the operand
        return right_type->accept(*this);
      } else {
        error(node.op.location, "Operator '-' cannot be applied to type '" +
                                    right_type->getTypeName() + "'.");
        return nullptr;
      }

    default:
      error(node.op.location, "Unknown unary operator.");
      return nullptr;
  }
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(BinaryExpr& node) {
  std::unique_ptr<TypeNode> left_type = node.left->accept(*this);
  std::unique_ptr<TypeNode> right_type = node.right->accept(*this);
  if (!left_type || !right_type) return nullptr;

  // Check if the types are compatible for binary operations
  if (!left_type->isEqualTo(right_type.get())) {
    error(node.op.location, "Type mismatch for operator '" + node.op.value +
                                "': '" + left_type->getTypeName() + "' and '" +
                                right_type->getTypeName() + "'.");
    return nullptr;
  }

  // Return a copy of the left operand's type
  return left_type->accept(*this);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(IntegerTypeNode& node) {
  // Create a copy of the integer type
  return std::make_unique<IntegerTypeNode>(node.location, node.bit_width,
                                           node.is_signed);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(FloatTypeNode& node) {
  // Create a copy of the float type
  return std::make_unique<FloatTypeNode>(node.location, node.bit_width);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(BooleanTypeNode& node) {
  // Create a copy of the boolean type
  return std::make_unique<BooleanTypeNode>(node.location);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(StringTypeNode& node) {
  // Create a copy of the string type
  return std::make_unique<StringTypeNode>(node.location);
}