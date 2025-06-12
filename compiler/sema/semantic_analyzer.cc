// semantic_analyzer.cc
#include "semantic_analyzer.hh"

#include <iostream>
#include <string>

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
  if (node.type && initializer_type) {
    // Fall A: Typ ist deklariert UND es gibt einen Initializer.
    // Check for intelligent literal conversion
    bool types_compatible = false;

    if (node.type->isEqualTo(initializer_type.get())) {
      // Types are exactly equal
      types_compatible = true;
    } else {
      // Check for literal conversion
      if (auto int_literal = dynamic_cast<const IntegerLiteralTypeNode*>(
              initializer_type.get())) {
        if (auto target_int =
                dynamic_cast<const IntegerTypeNode*>(node.type.get())) {
          // Check if integer literal can fit into target integer type
          types_compatible = int_literal->canFitInto(target_int);
        }
      } else if (auto float_literal = dynamic_cast<const FloatLiteralTypeNode*>(
                     initializer_type.get())) {
        if (auto target_float =
                dynamic_cast<const FloatTypeNode*>(node.type.get())) {
          // Check if float literal can fit into target float type
          types_compatible = float_literal->canFitInto(target_float);
        }
      }
    }

    if (!types_compatible) {
      std::string error_msg =
          "Type mismatch: Cannot initialize variable of type '" +
          node.type->getTypeName() + "' with value of type '" +
          initializer_type->getTypeName() + "'";

      // Add helpful info for literal conversions
      if (auto int_literal = dynamic_cast<const IntegerLiteralTypeNode*>(
              initializer_type.get())) {
        error_msg +=
            " (value " + std::to_string(int_literal->value) + " doesn't fit)";
      } else if (auto float_literal = dynamic_cast<const FloatLiteralTypeNode*>(
                     initializer_type.get())) {
        error_msg +=
            " (value " + std::to_string(float_literal->value) + " doesn't fit)";
      }

      error(node.location, error_msg + ".");
    }
  }

  // Schritt 4: Bestimme den finalen Typ der Variable und speichere ihn.
  std::unique_ptr<TypeNode> final_type = nullptr;
  if (node.type) {
    // Wenn ein Typ explizit angegeben wurde, nehmen wir den.
    // Wir müssen eine Kopie erstellen, da der `node.type` unique ist.
    final_type = node.type->accept(*this);
  } else if (initializer_type) {
    // *** HIER KOMMT DIE KORREKTUR ***
    // Ansonsten inferieren (schlussfolgern) wir den Typ vom Initializer
    // UND LÖSEN IHN SOFORT IN EINEN KONKRETEN TYP AUF.

    if (auto* lit =
            dynamic_cast<IntegerLiteralTypeNode*>(initializer_type.get())) {
      // Standard-Inferenz: Ein Integer-Literal ohne Kontext wird zu i32.
      final_type = std::make_unique<IntegerTypeNode>(lit->location, 32, true);
    } else if (auto* lit = dynamic_cast<FloatLiteralTypeNode*>(
                   initializer_type.get())) {
      // Standard-Inferenz: Ein Float-Literal ohne Kontext wird zu f64.
      final_type = std::make_unique<FloatTypeNode>(lit->location, 64);
    } else {
      // Andere Typen (wie string) sind bereits konkret und können übernommen
      // werden.
      final_type = std::move(initializer_type);
    }

  } else {
    // Fall C: Kein Typ und kein Initializer. Das ist ein Fehler in unserer
    // Sprache.
    error(node.location, "Cannot infer type for variable '" + node.name +
                             "' without an explicit type or an initializer.");
    return nullptr;  // Beende die Analyse für diese fehlerhafte Deklaration.
  }
  // Schritt 5: Update the node with the inferred type if it wasn't explicitly
  // set
  if (!node.type && final_type) {
    // Create a copy of the final_type for the node
    if (auto int_literal =
            dynamic_cast<IntegerLiteralTypeNode*>(final_type.get())) {
      node.type = std::make_unique<IntegerLiteralTypeNode>(
          int_literal->location, int_literal->value);
    } else if (auto float_literal =
                   dynamic_cast<FloatLiteralTypeNode*>(final_type.get())) {
      node.type = std::make_unique<FloatLiteralTypeNode>(
          float_literal->location, float_literal->value);
    } else if (auto int_type =
                   dynamic_cast<IntegerTypeNode*>(final_type.get())) {
      node.type = std::make_unique<IntegerTypeNode>(
          int_type->location, int_type->bit_width, int_type->is_signed);
    } else if (auto float_type =
                   dynamic_cast<FloatTypeNode*>(final_type.get())) {
      node.type = std::make_unique<FloatTypeNode>(float_type->location,
                                                  float_type->bit_width);
    } else if (auto string_type =
                   dynamic_cast<StringTypeNode*>(final_type.get())) {
      node.type = std::make_unique<StringTypeNode>(string_type->location);
    }
    // Add other type cases as needed
  }

  // Schritt 6: Definiere die Variable in der Symboltabelle.
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
    // Parse the float value and create a FloatLiteralTypeNode
    double value = std::stod(node.value);
    return std::make_unique<FloatLiteralTypeNode>(node.location, value);
  } else {
    // Parse the integer value and create an IntegerLiteralTypeNode
    long long value = std::stoll(node.value);
    return std::make_unique<IntegerLiteralTypeNode>(node.location, value);
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

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(
    IntegerLiteralTypeNode& node) {
  // Create a copy of the integer literal type
  return std::make_unique<IntegerLiteralTypeNode>(node.location, node.value);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(FloatLiteralTypeNode& node) {
  // Create a copy of the float literal type
  return std::make_unique<FloatLiteralTypeNode>(node.location, node.value);
}