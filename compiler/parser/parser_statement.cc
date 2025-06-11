// parser_statement.cc
#include "parser_internal.hh"

std::unique_ptr<StmtNode> Parser::parseDeclaration() {
  try {
    if (match(TokenType::TOKEN_KEYWORD_LET)) return parseLetDeclaration(false);
    if (match(TokenType::TOKEN_KEYWORD_MUT)) return parseLetDeclaration(true);
    if (match(TokenType::TOKEN_KEYWORD_DEFINE))
      return parseLetDeclaration(false);

    return parseExpressionStatement();
  } catch (const ParseError&) {
    synchronize();
    return nullptr;
  }
}

std::unique_ptr<StmtNode> Parser::parseLetDeclaration(bool is_mutable) {
  const LoomToken& name_token = peek();
  consume(TokenType::TOKEN_IDENTIFIER,
          "Expected variable name after 'let'/'mut'.");
  std::string name = previous().value;

  std::unique_ptr<ExprNode> initializer = nullptr;
  if (match(TokenType::TOKEN_EQUAL)) {
    initializer = parseExpression();
  }

  consume(TokenType::TOKEN_SEMICOLON,
          "Expected ';' after variable declaration.");
  return std::make_unique<VarDeclNode>(name_token.location, name, is_mutable,
                                       std::move(initializer));
}

std::unique_ptr<StmtNode> Parser::parseExpressionStatement() {
  auto expr_loc = peek().location;
  std::unique_ptr<ExprNode> expr = parseExpression();
  consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after expression.");
  return std::make_unique<ExprStmtNode>(expr_loc, std::move(expr));
}