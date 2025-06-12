// parser_statement.cc
#include "parser_internal.hh"

std::unique_ptr<StmtNode> Parser::parseDeclaration() {
  try {
    if (match(TokenType::TOKEN_KEYWORD_LET))
      return parseVarDeclaration(VarDeclKind::LET);
    if (match(TokenType::TOKEN_KEYWORD_MUT))
      return parseVarDeclaration(VarDeclKind::MUT);
    if (match(TokenType::TOKEN_KEYWORD_DEFINE))
      return parseVarDeclaration(VarDeclKind::DEFINE);

    return parseExpressionStatement();
  } catch (const ParseError&) {
    synchronize();
    return nullptr;
  }
}

std::unique_ptr<StmtNode> Parser::parseVarDeclaration(VarDeclKind kind) {
  const LoomToken& name_token = peek();
  consume(TokenType::TOKEN_IDENTIFIER,
          "Expected variable name after 'let'/'mut'.");
  std::string name = previous().value;

  std::unique_ptr<TypeNode> type = nullptr;
  if (match(TokenType::TOKEN_COLON)) {
    type = parseType();
  }

  std::unique_ptr<ExprNode> initializer = nullptr;
  if (match(TokenType::TOKEN_EQUAL)) {
    initializer = parseExpression();
  }

  consume(TokenType::TOKEN_SEMICOLON,
          "Expected ';' after variable declaration.");

  return std::make_unique<VarDeclNode>(name_token.location, name, kind,
                                       std::move(type), std::move(initializer));
}

std::unique_ptr<StmtNode> Parser::parseExpressionStatement() {
  auto expr_loc = peek().location;
  std::unique_ptr<ExprNode> expr = parseExpression();
  consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after expression.");
  return std::make_unique<ExprStmtNode>(expr_loc, std::move(expr));
}