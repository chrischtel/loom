// parser_expression.cc
#include "parser_internal.hh"

std::unique_ptr<ExprNode> Parser::parseExpression() {
  // Im Moment leiten wir nur an parsePrimary weiter.
  // Später wird hier die komplexe Logik für Operator-Präzedenz leben.
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