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
  if (node.type && initializer_type) {  // Fall A: Typ ist deklariert UND es
                                        // gibt einen Initializer.
    // Check for memory model type compatibility
    bool types_compatible = false;
    if (node.type->isEqualTo(initializer_type.get())) {
      // Types are exactly equal
      types_compatible = true;
    } else if (node.type->canAcceptFrom(initializer_type.get())) {
      // Use the new canAcceptFrom method for memory model compatibility
      types_compatible = true;
    } else {
      // Special case: Check if we have a struct/union name mismatch due to
      // parsing (parser creates StructTypeNode for all user-defined types)
      if (auto var_struct =
              dynamic_cast<const StructTypeNode*>(node.type.get())) {
        if (auto init_union =
                dynamic_cast<const UnionTypeNode*>(initializer_type.get())) {
          if (var_struct->struct_name == init_union->union_name) {
            types_compatible = true;
          }
        }
      }
      if (auto var_union =
              dynamic_cast<const UnionTypeNode*>(node.type.get())) {
        if (auto init_struct =
                dynamic_cast<const StructTypeNode*>(initializer_type.get())) {
          if (var_union->union_name == init_struct->struct_name) {
            types_compatible = true;
          }
        }
      }
    }

    if (!types_compatible) {
      // Check for literal conversion (legacy compatibility)
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
    } else if (auto struct_type =
                   dynamic_cast<StructTypeNode*>(final_type.get())) {
      node.type = std::make_unique<StructTypeNode>(struct_type->location,
                                                   struct_type->struct_name);
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
  // Handle special null literal
  if (node.name == "null") {
    return std::make_unique<NullTypeNode>(node.location);
  }

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

  // Check if the stored type is a StructTypeNode that should actually be a
  // UnionTypeNode
  if (auto struct_type =
          dynamic_cast<const StructTypeNode*>(var_info.type.get())) {
    if (symbols.isUnionDefined(struct_type->struct_name)) {
      // Return a UnionTypeNode instead
      return std::make_unique<UnionTypeNode>(struct_type->location,
                                             struct_type->struct_name);
    }
  }

  return cloneType(var_info.type.get());
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(AssignmentExpr& node) {
  std::unique_ptr<TypeNode> value_type = node.value->accept(*this);
  if (!value_type) return nullptr;

  // Get the type of the assignment target
  std::unique_ptr<TypeNode> target_type = node.target->accept(*this);
  if (!target_type) return nullptr;

  // For simple variable assignments, do the old mutability check
  if (auto* identifier = dynamic_cast<Identifier*>(node.target.get())) {
    const SymbolInfo* info = symbols.lookup(identifier->name);
    if (!info) {
      error(node.location, "Undeclared identifier '" + identifier->name + "'.");
      return nullptr;
    }

    if (info->kind != SymbolKind::VARIABLE) {
      error(node.location, "'" + identifier->name + "' is not a variable.");
      return nullptr;
    }

    const VariableInfo& var_info = std::get<VariableInfo>(info->data);
    if (var_info.kind != VarDeclKind::MUT) {
      error(node.location,
            "Cannot assign to immutable variable '" + identifier->name + "'.");
      return nullptr;
    }
  }

  bool types_compatible = false;
  if (target_type->isEqualTo(value_type.get())) {
    types_compatible = true;
  } else if (target_type->canAcceptFrom(value_type.get())) {
    types_compatible = true;
  } else {
    // Special case for struct/union name mismatch due to parsing
    if (auto var_struct =
            dynamic_cast<const StructTypeNode*>(target_type.get())) {
      if (auto init_union =
              dynamic_cast<const UnionTypeNode*>(value_type.get())) {
        if (var_struct->struct_name == init_union->union_name) {
          types_compatible = true;
        }
      }
    }
    if (auto var_union =
            dynamic_cast<const UnionTypeNode*>(target_type.get())) {
      if (auto init_struct =
              dynamic_cast<const StructTypeNode*>(value_type.get())) {
        if (var_union->union_name == init_struct->struct_name) {
          types_compatible = true;
        }
      }
    }
  }

  if (!types_compatible) {
    error(node.location, "Type mismatch in assignment: cannot assign " +
                             value_type->toString() + " to " +
                             target_type->toString());
    return nullptr;
  }

  return cloneType(target_type.get());
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
    case TokenType::TOKEN_BITWISE_NOT:
      // Bitwise NOT operator - only for integers
      if (dynamic_cast<const IntegerTypeNode*>(right_type.get())) {
        // Return the same type as the operand
        return right_type->accept(*this);
      } else {
        error(node.op.location, "Operator '~' cannot be applied to type '" +
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

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(NullTypeNode& node) {
  // Create a copy of the null type
  return std::make_unique<NullTypeNode>(node.location);
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

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(ForStmtNode& node) {
  // Analyze the iterable expression (range or array)
  if (node.iterable) {
    std::unique_ptr<TypeNode> iterable_type = node.iterable->accept(*this);

    // For now, we only support ranges and arrays
    // TODO: Add proper type checking for iterables
    if (iterable_type) {
      // Type analysis successful
    }
  }

  // Create a new scope for the loop variable
  symbols.enterScope();

  // Add the loop variable to the scope
  // For ranges, the loop variable is always i32
  auto loop_var_type =
      std::make_unique<IntegerTypeNode>(node.location, 32, true);
  symbols.defineVariable(node.variable_name, VarDeclKind::LET,
                         std::move(loop_var_type));

  // Analyze the loop body
  for (const auto& stmt : node.body) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
  // Exit the loop scope
  symbols.leaveScope();

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
    }  // Return void for print
    return std::make_unique<IntegerTypeNode>(node.location, 32,
                                             true);  // i32 for now
  } else if (node.builtin_name == "print_addr") {
    // $$print_addr takes a pointer/address argument
    if (node.arguments.size() != 1) {
      error(node.location, "$$print_addr expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    // Return void for print_addr
    return std::make_unique<IntegerTypeNode>(node.location, 32,
                                             true);  // i32 for now
  } else if (node.builtin_name == "exit") {
    // $$exit takes an integer exit code
    if (node.arguments.size() != 1) {
      error(node.location, "$$exit expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }  // Return void (never returns)
    return std::make_unique<IntegerTypeNode>(node.location, 32,
                                             true);  // i32 for now
  } else if (node.builtin_name == "syscall") {
    // $$syscall takes syscall number + up to 6 arguments (Windows API mapping)
    if (node.arguments.size() < 1 || node.arguments.size() > 7) {
      error(node.location,
            "$$syscall expects 1-7 arguments (syscall number + up to 6 args), "
            "got " +
                std::to_string(node.arguments.size()));
      return nullptr;
    }  // Return i64 (syscall return value)
    return std::make_unique<IntegerTypeNode>(node.location, 64, true);  // i64
  } else if (node.builtin_name == "socket") {
    // $$socket(domain, type, protocol) -> socket descriptor
    if (node.arguments.size() != 3) {
      error(node.location, "$$socket expects exactly 3 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 64,
                                             true);  // i64 socket
  } else if (node.builtin_name == "bind") {
    // $$bind(socket, sockaddr, addrlen) -> int result
    if (node.arguments.size() != 3) {
      error(node.location, "$$bind expects exactly 3 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "listen") {
    // $$listen(socket, backlog) -> int result
    if (node.arguments.size() != 2) {
      error(node.location, "$$listen expects exactly 2 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "accept") {
    // $$accept(socket, sockaddr, addrlen) -> socket descriptor
    if (node.arguments.size() != 3) {
      error(node.location, "$$accept expects exactly 3 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 64,
                                             true);  // i64 socket
  } else if (node.builtin_name == "connect") {
    // $$connect(socket, sockaddr, addrlen) -> int result
    if (node.arguments.size() != 3) {
      error(node.location, "$$connect expects exactly 3 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "send") {
    // $$send(socket, buffer, length, flags) -> bytes sent
    if (node.arguments.size() != 4) {
      error(node.location, "$$send expects exactly 4 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "recv") {
    // $$recv(socket, buffer, length, flags) -> bytes received
    if (node.arguments.size() != 4) {
      error(node.location, "$$recv expects exactly 4 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "closesocket") {
    // $$closesocket(socket) -> int result
    if (node.arguments.size() != 1) {
      error(node.location, "$$closesocket expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "WSAStartup") {
    // $$WSAStartup(version, wsadata) -> int result
    if (node.arguments.size() != 2) {
      error(node.location, "$$WSAStartup expects exactly 2 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "WSACleanup") {
    // $$WSACleanup() -> int result
    if (node.arguments.size() != 0) {
      error(node.location, "$$WSACleanup expects exactly 0 arguments, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, true);  // i32
  } else if (node.builtin_name == "htons") {
    // $$htons(hostshort) -> network short
    if (node.arguments.size() != 1) {
      error(node.location, "$$htons expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 16, false);  // u16
  } else if (node.builtin_name == "htonl") {
    // $$htonl(hostlong) -> network long
    if (node.arguments.size() != 1) {
      error(node.location, "$$htonl expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, false);  // u32
  } else if (node.builtin_name == "inet_addr") {
    // $$inet_addr(cp) -> network address
    if (node.arguments.size() != 1) {
      error(node.location, "$$inet_addr expects exactly 1 argument, got " +
                               std::to_string(node.arguments.size()));
      return nullptr;
    }
    return std::make_unique<IntegerTypeNode>(node.location, 32, false);  // u32
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

// Memory model visitor implementations

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(ReferenceTypeNode& node) {
  // Analyze the referenced type
  if (node.referenced_type) {
    node.referenced_type->accept(*this);
  }
  // References are always valid at compile time
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(OwnedPointerTypeNode& node) {
  // Analyze the pointed type
  if (node.pointed_type) {
    node.pointed_type->accept(*this);
  }
  // Owned pointers are always valid at compile time
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(RawPointerTypeNode& node) {
  // Analyze the pointed type
  if (node.pointed_type) {
    node.pointed_type->accept(*this);
  }
  // Raw pointers are always valid at compile time
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(NullableTypeNode& node) {
  // Analyze the inner type first
  if (!node.inner_type) {
    error(node.location, "Nullable type node missing inner type");
    return nullptr;
  }

  auto inner_type = node.inner_type->accept(*this);
  if (!inner_type) {
    error(node.location, "Cannot determine inner type for nullable");
    return nullptr;
  }

  // Return a copy of the nullable type with the validated inner type
  auto cloned_inner = cloneType(inner_type.get());
  if (!cloned_inner) {
    error(node.location, "Cannot clone inner type for nullable");
    return nullptr;
  }

  return std::make_unique<NullableTypeNode>(node.location,
                                            std::move(cloned_inner));
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(SliceTypeNode& node) {
  // Analyze the element type
  if (node.element_type) {
    node.element_type->accept(*this);
  }
  // Return a clone of the slice type
  return std::make_unique<SliceTypeNode>(node.location,
                                         cloneType(node.element_type.get()));
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(ReferenceExpr& node) {
  // Taking a reference of an expression
  if (!node.operand) {
    error(node.location, "Reference expression missing operand");
    return nullptr;
  }

  auto operand_type = node.operand->accept(*this);
  if (!operand_type) {
    error(node.location, "Cannot determine type of reference operand");
    return nullptr;
  }

  // Create a reference type from the operand type
  return std::make_unique<ReferenceTypeNode>(node.location,
                                             std::move(operand_type));
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(DereferenceExpr& node) {
  // Dereferencing a pointer or reference
  if (!node.operand) {
    error(node.location, "Dereference expression missing operand");
    return nullptr;
  }

  auto operand_type = node.operand->accept(*this);
  if (!operand_type) {
    error(node.location, "Cannot determine type of dereference operand");
    return nullptr;
  }
  // Check if operand is a reference or owned pointer
  if (auto ref_type = dynamic_cast<ReferenceTypeNode*>(operand_type.get())) {
    // Return a copy of the referenced type - we need to clone it properly
    auto referenced = ref_type->referenced_type.get();
    if (auto int_type = dynamic_cast<IntegerTypeNode*>(referenced)) {
      return std::make_unique<IntegerTypeNode>(
          int_type->location, int_type->bit_width, int_type->is_signed);
    } else if (auto float_type = dynamic_cast<FloatTypeNode*>(referenced)) {
      return std::make_unique<FloatTypeNode>(float_type->location,
                                             float_type->bit_width);
    } else if (auto bool_type = dynamic_cast<BooleanTypeNode*>(referenced)) {
      return std::make_unique<BooleanTypeNode>(bool_type->location);
    } else if (auto string_type = dynamic_cast<StringTypeNode*>(referenced)) {
      return std::make_unique<StringTypeNode>(string_type->location);
    }
    // Add more type cloning as needed
    error(node.location, "Cannot dereference reference to unknown type");
    return nullptr;
  } else if (auto owned_type =
                 dynamic_cast<OwnedPointerTypeNode*>(operand_type.get())) {
    // Return a copy of the pointed type - we need to clone it properly
    auto pointed = owned_type->pointed_type.get();
    if (auto int_type = dynamic_cast<IntegerTypeNode*>(pointed)) {
      return std::make_unique<IntegerTypeNode>(
          int_type->location, int_type->bit_width, int_type->is_signed);
    } else if (auto float_type = dynamic_cast<FloatTypeNode*>(pointed)) {
      return std::make_unique<FloatTypeNode>(float_type->location,
                                             float_type->bit_width);
    } else if (auto bool_type = dynamic_cast<BooleanTypeNode*>(pointed)) {
      return std::make_unique<BooleanTypeNode>(bool_type->location);
    } else if (auto string_type = dynamic_cast<StringTypeNode*>(pointed)) {
      return std::make_unique<StringTypeNode>(string_type->location);
    }  // Add more type cloning as needed
    error(node.location, "Cannot dereference owned pointer to unknown type");
    return nullptr;
  } else if (dynamic_cast<NullableTypeNode*>(operand_type.get())) {
    // Cannot directly dereference nullable - need null check first
    error(node.location,
          "Cannot dereference nullable type '" + operand_type->getTypeName() +
              "' without null check. Use pattern matching or explicit checks.");
    return nullptr;
  } else {
    error(node.location, "Cannot dereference non-pointer type '" +
                             operand_type->getTypeName() +
                             "'. Only references (&T) and owned pointers (^T) "
                             "can be dereferenced.");
    return nullptr;
  }
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(MemberAccessExpr& node) {
  // Member access: obj.field
  if (!node.object) {
    error(node.location, "Member access expression missing object");
    return nullptr;
  }

  auto object_type = node.object->accept(*this);
  if (!object_type) {
    error(node.location, "Cannot determine type of object for member access");
    return nullptr;
  }
  // Check if the object is a struct type
  if (auto struct_type = dynamic_cast<StructTypeNode*>(object_type.get())) {
    // Look up the struct definition in the symbol table
    const StructInfo* struct_info =
        symbols.lookupStruct(struct_type->struct_name);
    if (!struct_info) {
      error(node.location,
            "Unknown struct type '" + struct_type->struct_name + "'");
      return nullptr;
    }

    // Find the field in the struct definition
    for (const auto& field : struct_info->fields) {
      if (field.first == node.member_name) {
        // Found the field, return a clone of its type
        return cloneType(field.second.get());
      }
    }

    error(node.location, "Unknown field '" + node.member_name +
                             "' in struct '" + struct_type->struct_name + "'");
    return nullptr;
  }

  // Check if the object is a union type
  if (auto union_type = dynamic_cast<UnionTypeNode*>(object_type.get())) {
    // Look up the union definition in the symbol table
    const UnionInfo* union_info = symbols.lookupUnion(union_type->union_name);
    if (!union_info) {
      error(node.location,
            "Unknown union type '" + union_type->union_name + "'");
      return nullptr;
    }

    // Find the field in the union definition
    for (const auto& field : union_info->fields) {
      if (field.first == node.member_name) {
        // Found the field, return a clone of its type
        return cloneType(field.second.get());
      }
    }

    error(node.location, "Unknown field '" + node.member_name + "' in union '" +
                             union_type->union_name + "'");
    return nullptr;
  }

  error(node.location,
        "Member access not supported for type: " + object_type->getTypeName());
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(PointerAccessExpr& node) {
  // Pointer access: ptr->field
  if (!node.pointer) {
    error(node.location, "Pointer access expression missing pointer");
    return nullptr;
  }

  auto pointer_type = node.pointer->accept(*this);
  if (!pointer_type) {
    error(node.location, "Cannot determine type of pointer for member access");
    return nullptr;
  }

  // TODO: Implement pointer member access checking
  error(node.location, "Pointer access not yet implemented for type: " +
                           pointer_type->getTypeName());
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(SliceExpr& node) {
  // Slice expression: arr[start..end]
  if (!node.array) {
    error(node.location, "Slice expression missing array");
    return nullptr;
  }

  auto array_type = node.array->accept(*this);
  if (!array_type) {
    error(node.location, "Cannot determine type of array for slicing");
    return nullptr;
  }

  // Analyze start and end indices if present
  if (node.start) {
    auto start_type = node.start->accept(*this);
    // TODO: Check that start is an integer type
  }

  if (node.end) {
    auto end_type = node.end->accept(*this);
    // TODO: Check that end is an integer type
  }

  // TODO: Implement proper slice type derivation from array type
  error(node.location, "Slice expressions not yet fully implemented");
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(DeferStmtNode& node) {
  // Defer statement: defer statement
  if (!node.deferred_statement) {
    error(node.location, "Defer statement missing deferred statement");
    return nullptr;
  }

  // Analyze the deferred statement
  node.deferred_statement->accept(*this);

  // Defer statements don't have a type
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(UnsafeBlockExpr& node) {
  // Unsafe block: unsafe { ... }
  // Analyze all statements in the unsafe block
  for (auto& stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }

  // TODO: Implement proper return type for unsafe blocks
  // For now, unsafe blocks don't return a specific type
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::cloneType(TypeNode* type) {
  if (!type) return nullptr;

  if (auto int_type = dynamic_cast<IntegerTypeNode*>(type)) {
    return std::make_unique<IntegerTypeNode>(
        int_type->location, int_type->bit_width, int_type->is_signed);
  } else if (auto float_type = dynamic_cast<FloatTypeNode*>(type)) {
    return std::make_unique<FloatTypeNode>(float_type->location,
                                           float_type->bit_width);
  } else if (auto bool_type = dynamic_cast<BooleanTypeNode*>(type)) {
    return std::make_unique<BooleanTypeNode>(bool_type->location);
  } else if (auto string_type = dynamic_cast<StringTypeNode*>(type)) {
    return std::make_unique<StringTypeNode>(string_type->location);
  } else if (auto null_type = dynamic_cast<NullTypeNode*>(type)) {
    return std::make_unique<NullTypeNode>(null_type->location);
  } else if (auto nullable_type = dynamic_cast<NullableTypeNode*>(type)) {
    auto cloned_inner = cloneType(nullable_type->inner_type.get());
    if (!cloned_inner) return nullptr;
    return std::make_unique<NullableTypeNode>(nullable_type->location,
                                              std::move(cloned_inner));
  } else if (auto ref_type = dynamic_cast<ReferenceTypeNode*>(type)) {
    auto cloned_ref = cloneType(ref_type->referenced_type.get());
    if (!cloned_ref) return nullptr;
    return std::make_unique<ReferenceTypeNode>(ref_type->location,
                                               std::move(cloned_ref));
  } else if (auto owned_type = dynamic_cast<OwnedPointerTypeNode*>(type)) {
    auto cloned_pointed = cloneType(owned_type->pointed_type.get());
    if (!cloned_pointed) return nullptr;
    return std::make_unique<OwnedPointerTypeNode>(owned_type->location,
                                                  std::move(cloned_pointed));
  } else if (auto raw_type = dynamic_cast<RawPointerTypeNode*>(type)) {
    auto cloned_pointed = cloneType(raw_type->pointed_type.get());
    if (!cloned_pointed) return nullptr;
    return std::make_unique<RawPointerTypeNode>(raw_type->location,
                                                std::move(cloned_pointed));
  } else if (auto struct_type = dynamic_cast<StructTypeNode*>(type)) {
    return std::make_unique<StructTypeNode>(struct_type->location,
                                            struct_type->struct_name);
  } else if (auto union_type = dynamic_cast<UnionTypeNode*>(type)) {
    return std::make_unique<UnionTypeNode>(union_type->location,
                                           union_type->union_name);
  }

  else if (auto slice_type = dynamic_cast<SliceTypeNode*>(type)) {
    auto cloned_element = cloneType(slice_type->element_type.get());
    if (!cloned_element)
      return nullptr;  // Should not happen if types are valid
    return std::make_unique<SliceTypeNode>(slice_type->location,
                                           std::move(cloned_element));
  }

  // Add more type cloning as needed for other type nodes
  return nullptr;
}

bool SemanticAnalyzer::isValidCast(TypeNode* from_type, TypeNode* to_type) {
  if (!from_type || !to_type) return false;

  // Allow casting between integer types
  if (dynamic_cast<IntegerTypeNode*>(from_type) &&
      dynamic_cast<IntegerTypeNode*>(to_type)) {
    return true;
  }

  // Allow casting integer literals to integer types
  if (dynamic_cast<IntegerLiteralTypeNode*>(from_type) &&
      dynamic_cast<IntegerTypeNode*>(to_type)) {
    return true;
  }

  // Allow casting between float types
  if (dynamic_cast<FloatTypeNode*>(from_type) &&
      dynamic_cast<FloatTypeNode*>(to_type)) {
    return true;
  }

  // Allow casting float literals to float types
  if (dynamic_cast<FloatLiteralTypeNode*>(from_type) &&
      dynamic_cast<FloatTypeNode*>(to_type)) {
    return true;
  }

  // Allow casting between array types with compatible element types
  if (auto from_slice = dynamic_cast<SliceTypeNode*>(from_type)) {
    if (auto to_slice = dynamic_cast<SliceTypeNode*>(to_type)) {
      return isValidCast(from_slice->element_type.get(),
                         to_slice->element_type.get());
    }
  }

  // Allow casting integer literals/types to other integer types (for
  // networking)
  if ((dynamic_cast<IntegerLiteralTypeNode*>(from_type) ||
       dynamic_cast<IntegerTypeNode*>(from_type)) &&
      dynamic_cast<IntegerTypeNode*>(to_type)) {
    return true;
  }

  // For now, be permissive with casts to help with networking types
  return true;
}

// --- New Phase 1 Expression Visitors ---

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(ArrayLiteralExpr& node) {
  if (node.elements.empty()) {
    // Empty array - we need to infer the type somehow
    // For now, return a generic array type
    return std::make_unique<SliceTypeNode>(
        node.location,
        std::make_unique<IntegerTypeNode>(node.location, 32, true));
  }

  // Analyze all elements to determine their types
  std::vector<std::unique_ptr<TypeNode>> element_types;
  for (const auto& element : node.elements) {
    auto element_type = element->accept(*this);
    if (!element_type) {
      error(node.location, "Cannot determine type of array element");
      return nullptr;
    }
    element_types.push_back(std::move(element_type));
  }

  // Determine the common type for all elements
  std::unique_ptr<TypeNode> common_type = nullptr;
  // Start with the first element's type as the candidate
  if (dynamic_cast<IntegerLiteralTypeNode*>(element_types[0].get())) {
    // If first element is an integer literal, use i32 as the common type
    common_type = std::make_unique<IntegerTypeNode>(node.location, 32, true);
  } else {
    // Otherwise, use the first element's type
    common_type = cloneType(element_types[0].get());
  }

  // Check that all elements are compatible with the common type
  for (size_t i = 0; i < element_types.size(); ++i) {
    bool compatible = false;

    if (dynamic_cast<IntegerLiteralTypeNode*>(element_types[i].get())) {
      // Integer literals are compatible with integer types
      if (dynamic_cast<IntegerTypeNode*>(common_type.get())) {
        compatible = true;
      }
    } else {
      // Use existing type equality check
      compatible = common_type->isEqualTo(element_types[i].get());
    }

    if (!compatible) {
      error(node.location, "Array elements must have consistent types");
      return nullptr;
    }
  }

  // Return slice type with common element type
  return std::make_unique<SliceTypeNode>(node.location, std::move(common_type));
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(IndexExpr& node) {
  // Analyze array expression
  auto array_type = node.array->accept(*this);
  if (!array_type) {
    error(node.location, "Cannot determine type of indexed expression");
    return nullptr;
  }

  // Analyze index expression
  auto index_type = node.index->accept(*this);
  if (!index_type) {
    error(node.location, "Cannot determine type of index expression");
    return nullptr;
  }

  // Check that index is integer type
  if (!dynamic_cast<IntegerTypeNode*>(index_type.get()) &&
      !dynamic_cast<IntegerLiteralTypeNode*>(index_type.get())) {
    error(node.location, "Array index must be integer type");
    return nullptr;
  }

  // Check that array is slice type
  if (auto slice_type = dynamic_cast<SliceTypeNode*>(array_type.get())) {
    return cloneType(slice_type->element_type.get());
  }

  error(node.location, "Cannot index non-array type");
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(RangeExpr& node) {
  // Analyze start expression
  if (node.start) {
    auto start_type = node.start->accept(*this);
    if (!start_type) {
      error(node.location, "Cannot determine type of range start");
      return nullptr;
    }

    // Check that start is integer type
    if (!dynamic_cast<IntegerTypeNode*>(start_type.get()) &&
        !dynamic_cast<IntegerLiteralTypeNode*>(start_type.get())) {
      error(node.location, "Range start must be integer type");
      return nullptr;
    }
  }

  // Analyze end expression
  if (node.end) {
    auto end_type = node.end->accept(*this);
    if (!end_type) {
      error(node.location, "Cannot determine type of range end");
      return nullptr;
    }

    // Check that end is integer type
    if (!dynamic_cast<IntegerTypeNode*>(end_type.get()) &&
        !dynamic_cast<IntegerLiteralTypeNode*>(end_type.get())) {
      error(node.location, "Range end must be integer type");
      return nullptr;
    }
  }

  // Ranges don't have a concrete type in this implementation
  // They are only used in for loop contexts
  return std::make_unique<IntegerTypeNode>(node.location, 32, true);
}

// --- Struct-related visitor implementations ---

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(StructDeclNode& node) {
  // Collect field information from the struct declaration
  std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>> fields;

  for (const auto& field : node.fields) {
    if (field) {
      // Validate the field type
      auto field_type = field->type->accept(*this);
      if (field_type) {
        // Convert unique_ptr to shared_ptr for storage in symbol table
        fields.emplace_back(field->name,
                            std::shared_ptr<TypeNode>(field_type.release()));
      }
    }
  }

  // Register the struct in the symbol table
  if (!symbols.defineStruct(node.name, std::move(fields))) {
    error(node.location, "Struct '" + node.name + "' is already defined");
  }

  // Struct declarations don't have a return type
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(FieldDeclNode& node) {
  // Validate the field type
  if (node.type) {
    return node.type->accept(*this);
  }

  error(node.location, "Field must have a type");
  return nullptr;
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(StructTypeNode& node) {
  // Check if it's actually a struct type
  if (symbols.isStructDefined(node.struct_name)) {
    return cloneType(&node);
  }

  // Check if it's actually a union type (parser creates StructTypeNode for all
  // user-defined types)
  if (symbols.isUnionDefined(node.struct_name)) {
    // Return a UnionTypeNode instead
    return std::make_unique<UnionTypeNode>(node.location, node.struct_name);
  }

  // Neither struct nor union found
  error(node.location, "Unknown struct type '" + node.struct_name + "'");
  return std::make_unique<StructTypeNode>(node.location, "undefined");
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(StructLiteralExpr& node) {
  // Check if it's actually a struct literal
  if (symbols.isStructDefined(node.struct_name)) {
    // TODO: Validate that the struct exists and has the correct fields
    return std::make_unique<StructTypeNode>(node.location, node.struct_name);
  }

  // Check if it's actually a union literal (parser creates StructLiteralExpr
  // for all aggregate literals)
  if (symbols.isUnionDefined(node.struct_name)) {
    // TODO: Validate that the union exists and has the correct fields
    return std::make_unique<UnionTypeNode>(node.location, node.struct_name);
  }

  // Neither struct nor union found
  error(node.location, "Unknown struct/union type '" + node.struct_name + "'");
  return std::make_unique<StructTypeNode>(node.location, "undefined");
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(CastExpr& node) {
  // Analyze the expression being cast
  auto expr_type = node.expression->accept(*this);
  if (!expr_type) {
    error(node.location, "Cannot determine type of expression in cast");
    return nullptr;
  }

  // Validate the target type by visiting it (this will validate it's
  // well-formed)
  node.target_type->accept(*this);

  // Check if the cast is valid (use the actual type nodes, not the visitor
  // results)
  if (!isValidCast(expr_type.get(), node.target_type.get())) {
    error(node.location, "Invalid cast from '" + expr_type->getTypeName() +
                             "' to '" + node.target_type->getTypeName() + "'");
    return nullptr;
  }

  // Return a clone of the target type
  return cloneType(node.target_type.get());
}

// --- Union and Attribute visitor methods ---

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(UnionDeclNode& node) {
  // Register the union type in the symbol table
  std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>> fields;

  for (const auto& field : node.fields) {
    if (field->type) {
      auto field_type =
          std::shared_ptr<TypeNode>(field->type->accept(*this).release());
      fields.emplace_back(field->name, field_type);
    }
  }
  // Check attributes for packed, etc.
  bool is_packed = false;
  for (const auto& attr : node.attributes) {
    if (attr->name == "packed") {
      is_packed = true;
    }
  }

  // TODO: Use is_packed for code generation
  (void)is_packed;  // Suppress unused warning for now

  symbols.defineUnion(node.name, fields);

  // Return a union type node
  return std::make_unique<UnionTypeNode>(node.location, node.name);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(UnionTypeNode& node) {
  // Verify that the union type exists
  if (!symbols.isUnionDefined(node.union_name)) {
    error(node.location, "Undefined union type: " + node.union_name);
    return std::make_unique<UnionTypeNode>(node.location, "undefined");
  }

  return std::make_unique<UnionTypeNode>(node.location, node.union_name);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(UnionLiteralExpr& node) {
  // Check if the union type exists
  if (!symbols.isUnionDefined(node.union_name)) {
    error(node.location, "Undefined union type: " + node.union_name);
    return std::make_unique<UnionTypeNode>(node.location, "undefined");
  }

  // Type check the value expression
  if (node.value) {
    node.value->accept(*this);
  }

  return std::make_unique<UnionTypeNode>(node.location, node.union_name);
}

std::unique_ptr<TypeNode> SemanticAnalyzer::visit(AttributeNode& node) {
  // Attributes don't have a type, but we need to return something
  // for the visitor pattern. Return void type.
  return std::make_unique<NullTypeNode>(node.location);
}