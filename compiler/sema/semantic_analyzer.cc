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
  if (!symbols.defineVariable(node.name, node.kind, std::move(final_type))) {
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
  }

  // Get the variable info from the symbol data
  if (info->kind != SymbolKind::VARIABLE) {
    error(node.location, "'" + node.name + "' is not a variable.");
    return nullptr;
  }

  const VariableInfo& var_info = std::get<VariableInfo>(info->data);
  return var_info.type->accept(*this);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(AssignmentExpr& node) {
  std::unique_ptr<TypeNode> value_type = node.value->accept(*this);
  if (!value_type) return nullptr;

  const SymbolInfo* info = symbols.lookup(node.name);

  if (info == nullptr) {
    error(node.location, "Undeclared identifier '" + node.name + "'.");
    return nullptr;
  }
  // Get the variable info from the symbol
  if (info->kind != SymbolKind::VARIABLE) {
    error(node.location, "'" + node.name + "' is not a variable.");
    return nullptr;
  }

  const VariableInfo& var_info = std::get<VariableInfo>(info->data);

  if (var_info.kind != VarDeclKind::MUT) {
    error(node.location,
          "Cannot assign to immutable variable '" + node.name + "'.");
    return nullptr;
  }

  // Check type compatibility with literal conversion support
  bool types_compatible = false;

  if (var_info.type->isEqualTo(value_type.get())) {
    types_compatible = true;
  } else {
    // Check for literal conversion
    if (auto int_literal =
            dynamic_cast<const IntegerLiteralTypeNode*>(value_type.get())) {
      if (auto target_int =
              dynamic_cast<const IntegerTypeNode*>(var_info.type.get())) {
        types_compatible = int_literal->canFitInto(target_int);
      }
    } else if (auto float_literal = dynamic_cast<const FloatLiteralTypeNode*>(
                   value_type.get())) {
      if (auto target_float =
              dynamic_cast<const FloatTypeNode*>(var_info.type.get())) {
        types_compatible = float_literal->canFitInto(target_float);
      }
    }
  }

  if (!types_compatible) {
    error(node.location, "Type mismatch: Cannot assign value of type '" +
                             value_type->getTypeName() + "' to variable '" +
                             node.name + "' of type '" +
                             var_info.type->getTypeName() + "'.");
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
  // Special handling for integer literal to integer type compatibility
  bool types_compatible = false;
  std::unique_ptr<TypeNode> result_type = nullptr;

  if (left_type->isEqualTo(right_type.get())) {
    // Types are exactly equal
    types_compatible = true;
    result_type = left_type->accept(*this);
  } else {
    // Check for integer literal compatibility
    auto* left_int_literal =
        dynamic_cast<IntegerLiteralTypeNode*>(left_type.get());
    auto* right_int_literal =
        dynamic_cast<IntegerLiteralTypeNode*>(right_type.get());
    auto* left_int_type = dynamic_cast<IntegerTypeNode*>(left_type.get());
    auto* right_int_type = dynamic_cast<IntegerTypeNode*>(right_type.get());
    if (left_int_literal && right_int_literal) {
      // Both are literals - treat as compatible and return i32 type
      types_compatible = true;
      result_type =
          std::make_unique<IntegerTypeNode>(node.op.location, 32, true);
    } else if (left_int_literal && right_int_type) {
      // Left is literal, right is concrete type - use right type
      types_compatible = true;
      result_type = right_type->accept(*this);
    } else if (left_int_type && right_int_literal) {
      // Left is concrete type, right is literal - use left type
      types_compatible = true;
      result_type = left_type->accept(*this);
    }
  }

  if (!types_compatible) {
    error(node.op.location, "Type mismatch for operator '" + node.op.value +
                                "': '" + left_type->getTypeName() + "' and '" +
                                right_type->getTypeName() + "'.");
    return nullptr;
  }

  // For comparison operators (==, !=, <, >, <=, >=), return boolean type
  if (node.op.value == "==" || node.op.value == "!=" || node.op.value == "<" ||
      node.op.value == ">" || node.op.value == "<=" || node.op.value == ">=") {
    return std::make_unique<BooleanTypeNode>(node.op.location);
  }

  // For other operators, return the result type
  return result_type;
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

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(IfStmtNode& node) {
  // Analyze the condition - it must be boolean
  if (node.condition) {
    std::unique_ptr<TypeNode> condition_type = node.condition->accept(*this);
    if (condition_type &&
        !dynamic_cast<BooleanTypeNode*>(condition_type.get())) {
      error(node.location, "If condition must be boolean type.");
    }
  }

  // Analyze then body
  for (const auto& stmt : node.then_body) {
    if (stmt) {
      stmt->accept(*this);
    }
  }

  // Analyze else body (if present)
  for (const auto& stmt : node.else_body) {
    if (stmt) {
      stmt->accept(*this);
    }
  }

  return nullptr;  // If statements don't return a value
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(WhileStmtNode& node) {
  if (node.condition) {
    std::unique_ptr<TypeNode> condition_type = node.condition->accept(*this);
    if (condition_type &&
        !dynamic_cast<BooleanTypeNode*>(condition_type.get())) {
      error(node.location, "While condition must be boolean type.");
    }
  }

  for (const auto& stmt : node.body) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(
    FunctionCallExpr& node) {  // Check for built-in functions first
  if (node.function_name == "print") {
    // Print function expects exactly one argument
    if (node.arguments.size() != 1) {
      error(node.location, "print() function expects exactly one argument.");
      return nullptr;
    }

    // Analyze the argument type
    if (node.arguments[0]) {
      std::unique_ptr<TypeNode> arg_type = node.arguments[0]->accept(*this);
      // print can accept any type, so we don't need to check it
    }

    // print function returns void (no return value)
    return nullptr;
  }

  // Check for user-defined functions
  const FunctionInfo* func_info = symbols.lookupFunction(node.function_name);
  if (!func_info) {
    error(node.location, "Unknown function: " + node.function_name);
    return nullptr;
  }

  // Check argument count
  if (node.arguments.size() != func_info->parameter_types.size()) {
    error(node.location, "Function '" + node.function_name + "' expects " +
                             std::to_string(func_info->parameter_types.size()) +
                             " arguments, got " +
                             std::to_string(node.arguments.size()));
    return nullptr;
  }

  // Check argument types
  for (size_t i = 0; i < node.arguments.size(); ++i) {
    if (!node.arguments[i]) continue;

    std::unique_ptr<TypeNode> arg_type = node.arguments[i]->accept(*this);
    if (!arg_type) return nullptr;

    // Check if argument type matches parameter type
    if (!arg_type->isEqualTo(func_info->parameter_types[i].get())) {
      // Allow integer literal to integer type compatibility
      auto* arg_literal = dynamic_cast<IntegerLiteralTypeNode*>(arg_type.get());
      auto* param_int =
          dynamic_cast<IntegerTypeNode*>(func_info->parameter_types[i].get());

      if (!(arg_literal && param_int)) {
        error(node.location, "Argument " + std::to_string(i + 1) +
                                 " type mismatch. Expected '" +
                                 func_info->parameter_types[i]->getTypeName() +
                                 "', got '" + arg_type->getTypeName() + "'");
        return nullptr;
      }
    }
  }

  // Return the function's return type
  if (func_info->return_type) {
    return func_info->return_type->accept(*this);
  } else {
    return nullptr;  // void function
  }
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(BuiltinCallExpr& node) {
  std::cout << "[SemanticAnalyzer] Analyzing builtin call: $$"
            << node.builtin_name << std::endl;

  // Validate arguments
  for (auto& arg : node.arguments) {
    if (!arg->accept(*this)) {
      return nullptr;  // Error in argument
    }
  }

  // Validate specific builtin functions
  if (node.builtin_name == "print") {
    // $$print can take string or integer arguments
    if (node.arguments.size() != 1) {
      error(node.location, "$$print expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    // Return void for print
    return std::make_unique<IntegerTypeNode>(node.location, 32,
                                             true);  // i32 for now
  } else if (node.builtin_name == "exit") {
    // $$exit takes an integer exit code
    if (node.arguments.size() != 1) {
      error(node.location, "$$exit expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    // Return void (never returns)
    return std::make_unique<IntegerTypeNode>(node.location, 32,
                                             true);  // i32 for now
  } else if (node.builtin_name == "syscall") {
    // $$syscall takes syscall number + arguments
    if (node.arguments.size() < 1) {
      error(node.location,
            "$$syscall expects at least 1 argument (syscall number)");
      return nullptr;
    }
    // Return i64 (syscall return value)
    return std::make_unique<IntegerTypeNode>(node.location, 64, true);  // i64
  } else {
    error(node.location, "Unknown builtin function: $$" + node.builtin_name);
    return nullptr;
  }
}

// Function-related visitor implementations
std::unique_ptr<TypeNode> SemanticAnalyzer::visit(FunctionDeclNode& node) {
  if (symbols.isFunction(node.name)) {
    error(node.location, "Function '" + node.name + "' already defined.");
    return nullptr;
  }

  std::vector<std::shared_ptr<TypeNode>> param_types;
  std::vector<std::string> param_names;

  for (auto& param : node.parameters) {
    auto param_type = param->type->accept(*this);
    if (!param_type) return nullptr;

    if (std::find(param_names.begin(), param_names.end(), param->name) !=
        param_names.end()) {
      error(param->location, "Duplicate parameter name: " + param->name);
      return nullptr;
    }
    param_types.push_back(std::shared_ptr<TypeNode>(std::move(param_type)));
    param_names.push_back(param->name);
  }

  std::shared_ptr<TypeNode> return_type = nullptr;
  if (node.return_type) {
    auto ret_type = node.return_type->accept(*this);
    if (!ret_type) return nullptr;
    return_type = std::shared_ptr<TypeNode>(ret_type.release());
  }

  if (!symbols.defineFunction(node.name, param_types, param_names,
                              return_type)) {
    error(node.location, "Failed to define function");
    return nullptr;
  }

  symbols.enterFunction(node.name);

  // Parameter in lokalen Scope hinzufügen
  for (size_t i = 0; i < node.parameters.size(); ++i) {
    symbols.defineVariable(param_names[i], VarDeclKind::LET, param_types[i]);
  }

  // Body analysieren
  for (auto& stmt : node.body) {
    if (stmt) stmt->accept(*this);
  }

  // Function Scope verlassen
  symbols.leaveFunction();

  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(ParameterNode& node) {
  // TODO: Implement parameter analysis
  // For now, just return the parameter's type
  if (node.type) {
    return node.type->accept(*this);
  }
  error(node.location, "Parameter without type");
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(ReturnStmtNode& node) {
  // TODO: Implement return statement analysis
  // For now, just analyze the expression if present
  if (node.expression) {
    return node.expression->accept(*this);
  }
  // Return statements don't have types themselves
  return nullptr;
}