// parser_expression.cc
#include "parser_internal.hh"

std::unique_ptr<ExprNode> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<ExprNode> Parser::parseAssignment() {
  std::unique_ptr<ExprNode> expr = parseTerm();

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
  // Im Moment leiten wir nur an parsePrimary weiter.
  // Später würde hier die Logik für '*' und '/' stehen,
  // die genau wie in parseTerm funktioniert.
  return parsePrimary();
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

  error(peek(), "Expected expression");
  return nullptr;
}