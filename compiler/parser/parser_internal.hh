// parser_internal.hh
#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "ast.hh"

class ParseError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class Parser {
 private:
  const std::vector<LoomToken>& tokens;
  size_t current = 0;
  bool had_error = false;

  void advance();
  const LoomToken& peek() const;
  const LoomToken& previous() const;
  bool isAtEnd() const;
  bool check(TokenType type) const;
  bool match(TokenType type);
  void consume(TokenType type, const std::string& message);
  void error(const LoomToken& token, const std::string& message);
  void synchronize();
  std::unique_ptr<StmtNode> parseDeclaration();
  std::unique_ptr<StmtNode> parseVarDeclaration(VarDeclKind kind);
  std::unique_ptr<StmtNode> parseIfStatement();
  std::unique_ptr<StmtNode> parseWhileStatement();
  std::unique_ptr<StmtNode> parseExpressionStatement();
  std::unique_ptr<ExprNode> parseExpression();
  std::unique_ptr<ExprNode> parseAssignment();
  std::unique_ptr<ExprNode> parseEquality();
  std::unique_ptr<ExprNode> parseComparison();
  std::unique_ptr<ExprNode> parseTerm();
  std::unique_ptr<ExprNode> parseFactor();
  std::unique_ptr<ExprNode> parseUnary();
  std::unique_ptr<ExprNode> parseCall();
  std::unique_ptr<ExprNode> parsePrimary();
  std::unique_ptr<ExprNode> finishCall(std::unique_ptr<ExprNode> callee);
  std::unique_ptr<TypeNode> parseType();

 public:
  Parser(const std::vector<LoomToken>& tokens);
  std::vector<std::unique_ptr<StmtNode>> parse();
  bool hasError() const { return had_error; }
};