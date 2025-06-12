// ast_printer.hh
#pragma once

#include <iostream>

#include "parser/ast.hh"

class ASTPrinter : public ASTVisitor {
 public:
  void print(const std::vector<std::unique_ptr<StmtNode>>& ast);
  std::unique_ptr<TypeNode> visit(NumberLiteral& node) override;
  std::unique_ptr<TypeNode> visit(Identifier& node) override;
  std::unique_ptr<TypeNode> visit(StringLiteral& node) override;
  std::unique_ptr<TypeNode> visit(AssignmentExpr& node) override;
  std::unique_ptr<TypeNode> visit(BinaryExpr& node) override;
  std::unique_ptr<TypeNode> visit(VarDeclNode& node) override;
  std::unique_ptr<TypeNode> visit(ExprStmtNode& node) override;
  std::unique_ptr<TypeNode> visit(UnaryExpr& node) override;
  std::unique_ptr<TypeNode> visit(TypeNode& node) override;

 private:
  void indent();
  int indentation_level = 0;
};