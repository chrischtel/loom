#pragma once

#include "parser/ast.hh"
#include "symbol_table.hh"

class SemanticAnalyzer : public ASTVisitor {
 private:
  SymbolTable symbols;
  bool had_error = false;

  void error(const LoomSourceLocation& loc, const std::string& message);

 public:
  SemanticAnalyzer();

  void analyze(const std::vector<std::unique_ptr<StmtNode>>& ast);
  bool hasError() const { return had_error; }

  void visit(NumberLiteral& node) override;
  void visit(Identifier& node) override;
  void visit(StringLiteral& node) override;
  void visit(AssignmentExpr& node) override;
  void visit(BinaryExpr& node) override;
  void visit(UnaryExpr& node) override;
  void visit(VarDeclNode& node) override;
  void visit(ExprStmtNode& node) override;
  void visit(TypeNode& node) override;
};
