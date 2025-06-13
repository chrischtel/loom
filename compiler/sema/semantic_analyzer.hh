// semantic_analyzer.hh
#pragma once

#include "parser/ast.hh"
#include "symbol_table.hh"

class SemanticAnalyzer : public ASTVisitor {
 private:
  SymbolTable symbols;
  bool had_error = false;
  void error(const LoomSourceLocation& loc, const std::string& message);
  std::unique_ptr<TypeNode> cloneType(TypeNode* type);

 public:
  SemanticAnalyzer();
  void analyze(const std::vector<std::unique_ptr<StmtNode>>& ast);
  bool hasError() const { return had_error; }

  std::unique_ptr<TypeNode> visit(NumberLiteral& node) override;
  std::unique_ptr<TypeNode> visit(StringLiteral& node) override;
  std::unique_ptr<TypeNode> visit(BooleanLiteral& node) override;
  std::unique_ptr<TypeNode> visit(Identifier& node) override;
  std::unique_ptr<TypeNode> visit(BinaryExpr& node) override;
  std::unique_ptr<TypeNode> visit(UnaryExpr& node) override;
  std::unique_ptr<TypeNode> visit(AssignmentExpr& node) override;
  std::unique_ptr<TypeNode> visit(VarDeclNode& node) override;
  std::unique_ptr<TypeNode> visit(ExprStmtNode& node) override;
  std::unique_ptr<TypeNode> visit(IfStmtNode& node) override;
  std::unique_ptr<TypeNode> visit(WhileStmtNode& node) override;
  std::unique_ptr<TypeNode> visit(FunctionCallExpr& node) override;
  std::unique_ptr<TypeNode> visit(TypeNode& node) override;
  std::unique_ptr<TypeNode> visit(IntegerTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(FloatTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(BooleanTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(StringTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(NullTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(IntegerLiteralTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(
      FloatLiteralTypeNode& node) override;  // Function-related visitors
  std::unique_ptr<TypeNode> visit(FunctionDeclNode& node) override;
  std::unique_ptr<TypeNode> visit(ParameterNode& node) override;
  std::unique_ptr<TypeNode> visit(ReturnStmtNode& node) override;
  std::unique_ptr<TypeNode> visit(BuiltinCallExpr& node) override;

  // Memory model visitors
  std::unique_ptr<TypeNode> visit(ReferenceTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(OwnedPointerTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(NullableTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(SliceTypeNode& node) override;
  std::unique_ptr<TypeNode> visit(ReferenceExpr& node) override;
  std::unique_ptr<TypeNode> visit(DereferenceExpr& node) override;
  std::unique_ptr<TypeNode> visit(MemberAccessExpr& node) override;
  std::unique_ptr<TypeNode> visit(PointerAccessExpr& node) override;
  std::unique_ptr<TypeNode> visit(SliceExpr& node) override;
  std::unique_ptr<TypeNode> visit(DeferStmtNode& node) override;
  std::unique_ptr<TypeNode> visit(UnsafeBlockExpr& node) override;
};