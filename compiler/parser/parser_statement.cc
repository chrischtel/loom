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
    if (match(TokenType::TOKEN_KEYWORD_FUNC)) return parseFunctionDeclaration();
    if (match(TokenType::TOKEN_KEYWORD_IF)) return parseIfStatement();
    if (match(TokenType::TOKEN_KEYWORD_WHILE)) return parseWhileStatement();
    if (match(TokenType::TOKEN_KEYWORD_RETURN)) return parseReturnStatement();
    if (match(TokenType::TOKEN_KEYWORD_DEFER)) return parseDeferStatement();
    if (match(TokenType::TOKEN_KEYWORD_UNSAFE)) return parseUnsafeBlock();

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

// Parse function declaration: func name(param1: type1, param2: type2) ->
// return_type { body }
std::unique_ptr<StmtNode> Parser::parseFunctionDeclaration() {
  LoomSourceLocation func_loc = previous().location;

  // Function name
  consume(TokenType::TOKEN_IDENTIFIER, "Expected function name after 'func'.");
  std::string func_name = previous().value;

  // Parameters
  consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after function name.");
  std::vector<std::unique_ptr<ParameterNode>> parameters;

  if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
    do {
      parameters.push_back(parseParameter());
    } while (match(TokenType::TOKEN_COMMA));
  }

  consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");

  // Return type (optional, default to void)
  std::unique_ptr<TypeNode> return_type = nullptr;
  if (!check(TokenType::TOKEN_LEFT_BRACE)) {
    return_type = parseType();
  }

  // Function body
  consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' before function body.");
  std::vector<std::unique_ptr<StmtNode>> body;

  while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    if (auto stmt = parseDeclaration()) {
      body.push_back(std::move(stmt));
    }
  }

  consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after function body.");

  return std::make_unique<FunctionDeclNode>(
      func_loc, func_name, std::move(parameters), std::move(return_type),
      std::move(body));
}

// Parse return statement: return expression;
std::unique_ptr<StmtNode> Parser::parseReturnStatement() {
  LoomSourceLocation return_loc = previous().location;

  std::unique_ptr<ExprNode> expression = nullptr;
  if (!check(TokenType::TOKEN_SEMICOLON)) {
    expression = parseExpression();
  }

  consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after return statement.");

  return std::make_unique<ReturnStmtNode>(return_loc, std::move(expression));
}

// Parse parameter: name: type
std::unique_ptr<ParameterNode> Parser::parseParameter() {
  LoomSourceLocation param_loc = peek().location;

  consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name.");
  std::string param_name = previous().value;

  consume(TokenType::TOKEN_COLON, "Expected ':' after parameter name.");
  std::unique_ptr<TypeNode> param_type = parseType();

  return std::make_unique<ParameterNode>(param_loc, param_name,
                                         std::move(param_type));
}

// Parse defer statement: defer statement
std::unique_ptr<StmtNode> Parser::parseDeferStatement() {
  LoomSourceLocation defer_loc = previous().location;

  // Parse the statement to be deferred
  std::unique_ptr<StmtNode> deferred_stmt = parseExpressionStatement();

  return std::make_unique<DeferStmtNode>(defer_loc, std::move(deferred_stmt));
}

// Parse unsafe block: unsafe { statements }
std::unique_ptr<StmtNode> Parser::parseUnsafeBlock() {
  LoomSourceLocation unsafe_loc = previous().location;

  consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after 'unsafe'");

  std::vector<std::unique_ptr<StmtNode>> statements;

  // Parse statements until we hit the closing brace
  while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
    auto stmt = parseDeclaration();
    if (stmt) {
      statements.push_back(std::move(stmt));
    }
  }

  consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after unsafe block");

  // For now, wrap it as an expression statement containing an unsafe block
  // expression
  auto unsafe_expr =
      std::make_unique<UnsafeBlockExpr>(unsafe_loc, std::move(statements));
  return std::make_unique<ExprStmtNode>(unsafe_loc, std::move(unsafe_expr));
}