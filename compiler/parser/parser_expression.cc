// parser_expression.cc
#include <stdexcept>
#include <string>

#include "parser_internal.hh"

std::unique_ptr<ExprNode> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<ExprNode> Parser::parseAssignment() {
  std::unique_ptr<ExprNode> expr = parseEquality();

  if (match(TokenType::TOKEN_EQUAL)) {
    const LoomToken& equals = previous();
    std::unique_ptr<ExprNode> value = parseAssignment();

    if (Identifier* target = dynamic_cast<Identifier*>(expr.get())) {
      return std::make_unique<AssignmentExpr>(target->location, target->name,
                                              std::move(value));
    }

    error(equals, "Invalid assignment target.");
    return nullptr;
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseEquality() {
  std::unique_ptr<ExprNode> expr = parseTerm();

  while (match(TokenType::TOKEN_EQUAL_EQUAL)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseTerm();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<ExprNode> Parser::parseTerm() {
  std::unique_ptr<ExprNode> expr = parseFactor();

  while (match(TokenType::TOKEN_PLUS) || match(TokenType::TOKEN_MINUS)) {
    const LoomToken& op = previous();
    std::unique_ptr<ExprNode> right = parseFactor();  // Parse den rechten Teil
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
  if (match(TokenType::TOKEN_MINUS) || match(TokenType::TOKEN_BANG)) {
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
    return std::make_unique<Identifier>(token.location, token.value);
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
  const LoomToken& type_token = peek();
  consume(TokenType::TOKEN_IDENTIFIER, "Expected type name.");

  std::string type_name = type_token.value;

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
          return std::make_unique<IntegerTypeNode>(type_token.location,
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
          return std::make_unique<FloatTypeNode>(type_token.location,
                                                 bit_width);
        }
      } catch (const std::exception&) {
        // Fall through to handle as a regular type
      }
    }
  }

  // Handle special types
  if (type_name == "bool") {
    return std::make_unique<BooleanTypeNode>(type_token.location);
  } else if (type_name == "string") {
    return std::make_unique<StringTypeNode>(type_token.location);
  }

  // For now, treat unknown types as generic TypeNodes
  // In a real compiler, this would be an error
  throw std::runtime_error("Unknown type: " + type_name);
}

std::unique_ptr<ExprNode> Parser::parseCall() {
  std::unique_ptr<ExprNode> expr = parsePrimary();

  while (true) {
    if (match(TokenType::TOKEN_LEFT_PAREN)) {
      expr = finishCall(std::move(expr));
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