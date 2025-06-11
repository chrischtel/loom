// ast_printer.hh
#pragma once

#include <iostream>

#include "parser/ast.hh"

class ASTPrinter : public ASTVisitor {
 public:
  void print(const std::vector<std::unique_ptr<StmtNode>>& ast);

  void visit(NumberLiteral& node) override;
  void visit(Identifier& node) override;
  void visit(StringLiteral& node) override;
  void visit(AssignmentExpr& node) override;
  void visit(BinaryExpr& node) override;
  void visit(VarDeclNode& node) override;
  void visit(ExprStmtNode& node) override;

 private:
  void indent();
  int indentation_level = 0;
};