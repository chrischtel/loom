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
  if (node.type && initializer_type) {
    // Fall A: Typ ist deklariert UND es gibt einen Initializer.
    // Wir müssen prüfen, ob die Typen übereinstimmen.
    if (node.type->name != initializer_type->name) {
      error(node.location,
            "Type mismatch: Cannot initialize variable of type '" +
                node.type->name + "' with value of type '" +
                initializer_type->name + "'.");
    }
  }

  // Schritt 4: Bestimme den finalen Typ der Variable und speichere ihn.
  std::unique_ptr<TypeNode> final_type = nullptr;
  if (node.type) {
    // Wenn ein Typ explizit angegeben wurde, nehmen wir den.
    // Wir müssen eine Kopie erstellen, da der `node.type` unique ist.
    final_type =
        std::make_unique<TypeNode>(node.type->location, node.type->name);
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
    return std::make_unique<TypeNode>(node.location, "float");
  } else {
    return std::make_unique<TypeNode>(node.location, "int");
  }
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(StringLiteral& node) {
  return std::make_unique<TypeNode>(node.location, "string");
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(Identifier& node) {
  const SymbolInfo* info = symbols.lookup(node.name);
  if (info == nullptr) {
    error(node.location, "Undeclared identifier '" + node.name + "'.");
    return nullptr;
  }
  // Erstelle eine Kopie des Typs aus der Symboltabelle und gib sie zurück.
  return std::make_unique<TypeNode>(node.location, info->type->name);
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

  if (info->type->name != value_type->name) {
    error(node.location, "Type mismatch: Cannot assign value of type '" +
                             value_type->name + "' to variable '" + node.name +
                             "' of type '" + info->type->name + "'.");
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
      // Für '!' könnte man einen 'bool'-Typ erwarten.
      // Da wir den noch nicht haben, lassen wir vorerst alles zu.
      // Später: if (right_type->name != "bool") { error(...) }
      return std::make_unique<TypeNode>(node.location, "bool");

    case TokenType::TOKEN_MINUS:
      if (right_type->name != "int" && right_type->name != "float") {
        error(node.op.location, "Operator '-' cannot be applied to type '" +
                                    right_type->name + "'.");
        return nullptr;
      }
      return std::make_unique<TypeNode>(node.location, right_type->name);

    default:
      error(node.op.location, "Unknown unary operator.");
      return nullptr;
  }
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(BinaryExpr& node) {
  std::unique_ptr<TypeNode> left_type = node.left->accept(*this);
  std::unique_ptr<TypeNode> right_type = node.right->accept(*this);

  if (!left_type || !right_type) return nullptr;

  // TODO: Implementiere die eigentlichen Typ-Regeln für binäre Operatoren.
  // z.B. int + int -> int, float + float -> float, int + float -> float?
  // Für den Moment nehmen wir einfach an, der Typ ist der des linken Operanden.
  if (left_type->name != right_type->name) {
    error(node.op.location, "Type mismatch for operator '" + node.op.value +
                                "': '" + left_type->name + "' and '" +
                                right_type->name + "'.");
    return nullptr;
  }

  return left_type;
}