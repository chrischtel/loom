// parser_statement.cc
#include "parser_internal.hh"

std::unique_ptr<StmtNode> Parser::parseDeclaration() {
  try {
    // Skip newlines at the beginning of declarations
    while (match(TokenType::TOKEN_NEWLINE)) {
      // Just consume newlines
    }

    // If we hit end of file or closing brace after skipping newlines, return
    // null
    if (isAtEnd() || check(TokenType::TOKEN_RIGHT_BRACE)) {
      return nullptr;
    }
    if (match(TokenType::TOKEN_KEYWORD_LET))
      return parseVarDeclaration(VarDeclKind::LET);
    if (match(TokenType::TOKEN_KEYWORD_MUT))
      return parseVarDeclaration(VarDeclKind::MUT);
    if (match(TokenType::TOKEN_KEYWORD_DEFINE))
      return parseVarDeclaration(VarDeclKind::DEFINE);
    if (match(TokenType::TOKEN_KEYWORD_IF)) return parseIfStatement();
    if (match(TokenType::TOKEN_KEYWORD_WHILE)) return parseWhileStatement();

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

std::unique_ptr<StmtNode> Parser::parseIfStatement() {
  auto if_loc = previous().location;  // 'if' token location

  consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'if'.");
  std::unique_ptr<ExprNode> condition = parseExpression();
  consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after if condition.");

  consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' before if body.");
  std::vector<std::unique_ptr<StmtNode>> then_body;

  // Parse statements until we hit '}'
  while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    if (auto stmt = parseDeclaration()) {
      then_body.push_back(std::move(stmt));
    }
  }
  consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after if body.");

  std::vector<std::unique_ptr<StmtNode>> else_body;
  if (match(TokenType::TOKEN_KEYWORD_ELSE)) {
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' before else body.");

    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
      if (auto stmt = parseDeclaration()) {
        else_body.push_back(std::move(stmt));
      }
    }
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after else body.");
  }

  return std::make_unique<IfStmtNode>(
      if_loc, std::move(condition), std::move(then_body), std::move(else_body));
}

std::unique_ptr<StmtNode> Parser::parseWhileStatement() {
  auto while_loc = previous().location;  // location of the 'while' token

  consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'while'.");
  std::unique_ptr<ExprNode> condition = parseExpression();
  consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after while condition.");

  consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' before while body.");
  std::vector<std::unique_ptr<StmtNode>> body;

  while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    if (auto stmt = parseDeclaration()) {
      body.push_back(std::move(stmt));
    }
  }

  consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after while body.");

  return std::make_unique<WhileStmtNode>(while_loc, std::move(condition),
                                         std::move(body));
}