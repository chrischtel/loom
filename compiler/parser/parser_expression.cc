// parser_expression.cc
#include <stdexcept>
#include <string>

#include "parser_internal.hh"

std::unique_ptr<ExprNode> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<ExprNode> Parser::parseAssignment() {
  std::unique_ptr<ExprNode> expr = parseRange();

  if (match(TokenType::TOKEN_EQUAL)) {
    const LoomToken& equals = previous();
    std::unique_ptr<ExprNode> value = parseAssignment();

    // Check if the target is a valid lvalue
    if (dynamic_cast<Identifier*>(expr.get()) ||
        dynamic_cast<MemberAccessExpr*>(expr.get()) ||
        dynamic_cast<IndexExpr*>(expr.get())) {
      return std::make_unique<AssignmentExpr>(equals.location, std::move(expr),
                                              std::move(value));
    }

    error(equals, "Invalid assignment target.");
    return nullptr;
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseRange() {
  std::unique_ptr<ExprNode> expr = parseEquality();

  if (match(TokenType::TOKEN_DOT_DOT) ||
      match(TokenType::TOKEN_DOT_DOT_EQUAL)) {
    const LoomToken& range_op = previous();
    bool inclusive = (range_op.type == TokenType::TOKEN_DOT_DOT_EQUAL);
    std::unique_ptr<ExprNode> end = parseEquality();

    return std::make_unique<RangeExpr>(range_op.location, std::move(expr),
                                       std::move(end), inclusive);
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseEquality() {
  std::unique_ptr<ExprNode> expr = parseBitwiseOr();

  while (match(TokenType::TOKEN_EQUAL_EQUAL)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseBitwiseOr();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseComparison() {
  std::unique_ptr<ExprNode> expr = parseBitwiseXor();
  while (match(TokenType::TOKEN_LESS) || match(TokenType::TOKEN_GREATER) ||
         match(TokenType::TOKEN_LESS_EQUAL) ||
         match(TokenType::TOKEN_GREATER_EQUAL)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseBitwiseXor();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

std::unique_ptr<ExprNode> Parser::parseTerm() {
  std::unique_ptr<ExprNode> expr = parseShift();

  while (match(TokenType::TOKEN_PLUS) || match(TokenType::TOKEN_MINUS)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseShift();  // Parse den rechten Teil
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseFactor() {
  std::unique_ptr<ExprNode> expr = parseUnary();

  while (match(TokenType::TOKEN_STAR) || match(TokenType::TOKEN_SLASH)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseUnary();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseUnary() {
  // Handle memory model unary operators
  if (match(TokenType::TOKEN_AMPERSAND)) {
    // Reference operator: &expr
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseUnary();
    return std::make_unique<ReferenceExpr>(op.location, std::move(right));
  }

  if (match(TokenType::TOKEN_STAR) || match(TokenType::TOKEN_HAT)) {
    // Dereference operators: *expr or ^expr
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseUnary();
    return std::make_unique<DereferenceExpr>(op.location, std::move(right),
                                             op.type);
  }
  // Handle traditional unary operators
  if (match(TokenType::TOKEN_MINUS) || match(TokenType::TOKEN_BANG) ||
      match(TokenType::TOKEN_BITWISE_NOT)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseUnary();
    return std::make_unique<UnaryExpr>(op, std::move(right));
  }
  return parseCall();
}

std::unique_ptr<ExprNode> Parser::parsePrimary() {
  if (match(TokenType::TOKEN_NUMBER_INT)) {
    const LoomToken& token = previous();
    return std::make_unique<NumberLiteral>(token.location, token.value, false);
  }
  if (match(TokenType::TOKEN_NUMBER_FLOAT)) {
    const LoomToken& token = previous();
    return std::make_unique<NumberLiteral>(token.location, token.value, true);
  }
  if (match(TokenType::TOKEN_IDENTIFIER)) {
    const LoomToken& token = previous();

    // Check for struct literal: StructName { ... }
    if (check(TokenType::TOKEN_LEFT_BRACE)) {
      return parseStructLiteral(token.value, token.location);
    }

    return std::make_unique<Identifier>(token.location, token.value);
  }
  if (match(TokenType::TOKEN_BUILTIN)) {
    return parseBuiltinCall();
  }
  if (match(TokenType::TOKEN_STRING)) {
    const LoomToken& token = previous();
    return std::make_unique<StringLiteral>(token.location, token.value);
  }
  if (match(TokenType::TOKEN_KEYWORD_TRUE)) {
    return std::make_unique<BooleanLiteral>(previous().location, true);
  }
  if (match(TokenType::TOKEN_KEYWORD_FALSE)) {
    return std::make_unique<BooleanLiteral>(previous().location, false);
  }
  if (match(TokenType::TOKEN_KEYWORD_NULL)) {
    const LoomToken& token = previous();
    return std::make_unique<Identifier>(
        token.location, "null");  // For now, treat as identifier
  }
  // Array literal: @[1, 2, 3]
  if (match(TokenType::TOKEN_AT)) {
    return parseArrayLiteral();
  }

  // Cast expression: cast(Type, expression)
  if (match(TokenType::TOKEN_KEYWORD_CAST)) {
    return parseCastExpression();
  }

  if (match(TokenType::TOKEN_LEFT_PAREN)) {
    std::unique_ptr<ExprNode> expr = parseExpression();
    consume(TokenType::TOKEN_RIGHT_PAREN, "Exprected ')' after expression.");

    // TODO: GroupingExpr-Node ?
    return expr;
  }

  error(peek(), "Expected expression");
  return nullptr;
}

std::unique_ptr<TypeNode> Parser::parseType() {
  // Handle memory model prefix types: &T, ^T, *T, []T
  if (match(TokenType::TOKEN_AMPERSAND)) {
    // Reference type: &T
    LoomSourceLocation loc = previous().location;
    auto inner_type = parseType();
    return std::make_unique<ReferenceTypeNode>(loc, std::move(inner_type));
  }

  if (match(TokenType::TOKEN_HAT)) {
    // Owned pointer type: ^T
    LoomSourceLocation loc = previous().location;
    auto inner_type = parseType();
    return std::make_unique<OwnedPointerTypeNode>(loc, std::move(inner_type));
  }

  if (match(TokenType::TOKEN_STAR)) {
    // Raw pointer type: *T
    LoomSourceLocation loc = previous().location;
    auto inner_type = parseType();
    return std::make_unique<RawPointerTypeNode>(loc, std::move(inner_type));
  }

  if (match(TokenType::TOKEN_LEFT_BRACKET)) {
    // Slice type: []T
    LoomSourceLocation loc = previous().location;
    consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after '['");
    auto element_type = parseType();
    return std::make_unique<SliceTypeNode>(loc, std::move(element_type));
  }

  // Parse base type
  const LoomToken& type_token = peek();
  consume(TokenType::TOKEN_IDENTIFIER, "Expected type name.");

  std::string type_name = type_token.value;
  std::unique_ptr<TypeNode> base_type;

  // Parse integer types (i8, i16, i32, i64, u8, u16, u32, u64)
  if (type_name.length() >= 2) {
    char first_char = type_name[0];
    if (first_char == 'i' || first_char == 'u') {
      std::string bit_width_str = type_name.substr(1);
      try {
        int bit_width = std::stoi(bit_width_str);
        // Validate common bit widths
        if (bit_width == 8 || bit_width == 16 || bit_width == 32 ||
            bit_width == 64) {
          bool is_signed = (first_char == 'i');
          base_type = std::make_unique<IntegerTypeNode>(type_token.location,
                                                        bit_width, is_signed);
        }
      } catch (const std::exception&) {
        // Fall through to handle as a regular type
      }
    }
    // Parse float types (f16, f32, f64)
    else if (first_char == 'f') {
      std::string bit_width_str = type_name.substr(1);
      try {
        int bit_width = std::stoi(bit_width_str);
        // Validate common float bit widths
        if (bit_width == 16 || bit_width == 32 || bit_width == 64) {
          base_type =
              std::make_unique<FloatTypeNode>(type_token.location, bit_width);
        }
      } catch (const std::exception&) {
        // Fall through to handle as a regular type
      }
    }
  }
  // Handle special types if base_type wasn't set
  if (!base_type) {
    if (type_name == "bool") {
      base_type = std::make_unique<BooleanTypeNode>(type_token.location);
    } else if (type_name == "string") {
      base_type = std::make_unique<StringTypeNode>(type_token.location);
    } else {
      // Check if it's a union or struct type - for now, assume struct
      // TODO: Add proper union/struct type resolution using symbol table
      base_type =
          std::make_unique<StructTypeNode>(type_token.location, type_name);
    }
  }

  // Handle nullable suffix: T?
  if (match(TokenType::TOKEN_QUESTION)) {
    LoomSourceLocation loc = previous().location;
    return std::make_unique<NullableTypeNode>(loc, std::move(base_type));
  }

  return base_type;
}

std::unique_ptr<ExprNode> Parser::parseCall() {
  std::unique_ptr<ExprNode> expr = parsePostfix();

  while (true) {
    if (match(TokenType::TOKEN_LEFT_PAREN)) {
      expr = finishCall(std::move(expr));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parsePostfix() {
  std::unique_ptr<ExprNode> expr = parsePrimary();

  while (true) {
    if (match(TokenType::TOKEN_DOT)) {
      // Member access: expr.field
      const LoomToken& dot = previous();
      consume(TokenType::TOKEN_IDENTIFIER, "Expected field name after '.'");
      const LoomToken& field = previous();
      expr = std::make_unique<MemberAccessExpr>(dot.location, std::move(expr),
                                                field.value);
    } else if (match(TokenType::TOKEN_ARROW)) {
      // Pointer access: expr->field
      const LoomToken& arrow = previous();
      consume(TokenType::TOKEN_IDENTIFIER, "Expected field name after '->'");
      const LoomToken& field = previous();
      expr = std::make_unique<PointerAccessExpr>(arrow.location,
                                                 std::move(expr), field.value);
    } else if (match(TokenType::TOKEN_AT)) {
      // Index access: expr@[index] (original @ syntax for compatibility)
      const LoomToken& at = previous();
      consume(TokenType::TOKEN_LEFT_BRACKET, "Expected '[' after '@'");
      std::unique_ptr<ExprNode> index = parseExpression();
      consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after array index");
      expr = std::make_unique<IndexExpr>(at.location, std::move(expr),
                                         std::move(index),
                                         true);  // use_at_syntax = true
    } else if (match(TokenType::TOKEN_LEFT_BRACKET)) {
      // Traditional array indexing: expr[index]
      const LoomToken& bracket = previous();
      std::unique_ptr<ExprNode> index = parseExpression();
      consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after array index");
      expr = std::make_unique<IndexExpr>(bracket.location, std::move(expr),
                                         std::move(index),
                                         false);  // use_at_syntax = false
    } else {
      break;
    }
  }
  return expr;
}

std::unique_ptr<ExprNode> Parser::finishCall(std::unique_ptr<ExprNode> callee) {
  std::vector<std::unique_ptr<ExprNode>> arguments;

  if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
    do {
      arguments.push_back(parseExpression());
    } while (match(TokenType::TOKEN_COMMA));
  }

  consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");

  // Check if this is a function call (identifier followed by parentheses)
  if (auto* identifier = dynamic_cast<Identifier*>(callee.get())) {
    return std::make_unique<FunctionCallExpr>(
        identifier->location, identifier->name, std::move(arguments));
  }

  error(previous(), "Only identifiers can be called as functions.");
  return nullptr;
}

std::unique_ptr<ExprNode> Parser::parseBuiltinCall() {
  const LoomToken& builtin_token = previous();

  // Extract the builtin name (remove the $$ prefix)
  std::string builtin_name = builtin_token.value.substr(2);  // Remove "$$"

  // Expect opening parenthesis
  consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after builtin name.");

  // Parse arguments (same as regular function calls)
  std::vector<std::unique_ptr<ExprNode>> arguments;
  if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
    do {
      arguments.push_back(parseExpression());
    } while (match(TokenType::TOKEN_COMMA));
  }

  consume(TokenType::TOKEN_RIGHT_PAREN,
          "Expected ')' after builtin arguments.");
  return std::make_unique<BuiltinCallExpr>(builtin_token.location, builtin_name,
                                           std::move(arguments));
}

// Parse array literal: @[elem1, elem2, ...]
std::unique_ptr<ExprNode> Parser::parseArrayLiteral() {
  const LoomToken& at_token = previous();

  consume(TokenType::TOKEN_LEFT_BRACKET, "Expected '[' after '@'");

  std::vector<std::unique_ptr<ExprNode>> elements;

  // Handle empty array
  if (!check(TokenType::TOKEN_RIGHT_BRACKET)) {
    do {
      elements.push_back(parseExpression());
    } while (match(TokenType::TOKEN_COMMA));
  }

  consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after array elements");
  return std::make_unique<ArrayLiteralExpr>(at_token.location,
                                            std::move(elements));
}

// Parse struct literal: StructName { field1: value1, field2: value2 }
std::unique_ptr<ExprNode> Parser::parseStructLiteral(
    const std::string& struct_name, const LoomSourceLocation& loc) {
  consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after struct name");

  std::vector<std::pair<std::string, std::unique_ptr<ExprNode>>> field_values;

  // Handle empty struct
  if (!check(TokenType::TOKEN_RIGHT_BRACE)) {
    do {
      // Skip newlines
      while (match(TokenType::TOKEN_NEWLINE)) {
      }

      if (check(TokenType::TOKEN_RIGHT_BRACE)) break;

      // Parse field: name: value
      consume(TokenType::TOKEN_IDENTIFIER, "Expected field name");
      std::string field_name = previous().value;

      consume(TokenType::TOKEN_COLON, "Expected ':' after field name");

      std::unique_ptr<ExprNode> value = parseExpression();

      field_values.emplace_back(field_name, std::move(value));

      // Optional comma
      if (!match(TokenType::TOKEN_COMMA)) {
        break;
      }
    } while (true);
  }

  consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after struct fields");

  return std::make_unique<StructLiteralExpr>(loc, struct_name,
                                             std::move(field_values));
}

// Parse cast expression: cast(Type, expression)
std::unique_ptr<ExprNode> Parser::parseCastExpression() {
  const LoomToken& cast_token = previous();

  consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'cast'");

  // Parse target type
  std::unique_ptr<TypeNode> target_type = parseType();
  if (!target_type) {
    error(peek(), "Expected type in cast expression");
    return nullptr;
  }

  consume(TokenType::TOKEN_COMMA, "Expected ',' after cast target type");

  // Parse expression to cast
  std::unique_ptr<ExprNode> expression = parseExpression();
  if (!expression) {
    error(peek(), "Expected expression in cast");
    return nullptr;
  }

  consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after cast expression");

  return std::make_unique<CastExpr>(cast_token.location, std::move(target_type),
                                    std::move(expression));
}

std::unique_ptr<ExprNode> Parser::parseBitwiseOr() {
  std::unique_ptr<ExprNode> expr = parseComparison();

  while (match(TokenType::TOKEN_BITWISE_OR)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseComparison();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseBitwiseXor() {
  std::unique_ptr<ExprNode> expr = parseBitwiseAnd();

  while (match(TokenType::TOKEN_HAT)) {  // ^ is context-sensitive, use as
                                         // bitwise XOR in expressions
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseBitwiseAnd();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseBitwiseAnd() {
  std::unique_ptr<ExprNode> expr = parseShift();

  // Note: & is context-sensitive - in expressions it's bitwise AND, as unary
  // it's reference
  while (check(TokenType::TOKEN_AMPERSAND)) {
    // Lookahead to distinguish bitwise AND from reference
    // If next token suggests binary operation, treat as bitwise AND
    if (current + 1 < tokens.size() &&
        (tokens[current + 1].type == TokenType::TOKEN_IDENTIFIER ||
         tokens[current + 1].type == TokenType::TOKEN_NUMBER_INT ||
         tokens[current + 1].type == TokenType::TOKEN_LEFT_PAREN)) {
      advance();  // consume &
      const LoomToken& op = previous();
      std::unique_ptr<ExprNode> right = parseShift();
      expr =
          std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    } else {
      break;  // Leave for unary reference parsing
    }
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseShift() {
  std::unique_ptr<ExprNode> expr = parseFactor();

  while (match(TokenType::TOKEN_LEFT_SHIFT) ||
         match(TokenType::TOKEN_RIGHT_SHIFT)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseFactor();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}