// ast.hh
#pragma once

#include <memory>
#include <string>
#include <vector>  // Obwohl du es hier nicht direkt brauchst, ist es oft nützlich

#include "../scanner/scanner_internal.hh"  // Für LoomSourceLocation

class NumberLiteral;
class Identifier;
class StringLiteral;
class AssignmentExpr;
class BinaryExpr;
class VarDeclNode;
class ExprStmtNode;

class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;
  virtual void visit(NumberLiteral& node) = 0;
  virtual void visit(Identifier& node) = 0;
  virtual void visit(StringLiteral& node) = 0;
  virtual void visit(AssignmentExpr& node) = 0;
  virtual void visit(BinaryExpr& node) = 0;
  virtual void visit(VarDeclNode& node) = 0;
  virtual void visit(ExprStmtNode& node) = 0;
};

// --- Basisklassen ---
class ASTNode {
 public:
  virtual ~ASTNode() = default;
  virtual std::string toString() const = 0;
  virtual void accept(ASTVisitor& visitor) = 0;

  LoomSourceLocation location;

 protected:
  ASTNode(const LoomSourceLocation& loc) : location(loc) {}
};

class StmtNode : public ASTNode {
 protected:
  using ASTNode::ASTNode;
};

class ExprNode : public ASTNode {
 protected:
  using ASTNode::ASTNode;
};

// --- Konkrete Expression-Knoten ---
class NumberLiteral : public ExprNode {
 public:
  std::string value;
  bool is_float;

  NumberLiteral(const LoomSourceLocation& loc, const std::string& val,
                bool float_type)
      : ExprNode(loc), value(val), is_float(float_type) {}

  std::string toString() const override {
    return "NumberLiteral(" + value + (is_float ? "f" : "i") + ")";
  }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class Identifier : public ExprNode {
 public:
  std::string name;

  Identifier(const LoomSourceLocation& loc, const std::string& id_name)
      : ExprNode(loc), name(id_name) {}

  std::string toString() const override { return "Identifier(" + name + ")"; }
  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class AssignmentExpr : public ExprNode {
 public:
  std::string name;
  std::unique_ptr<ExprNode> value;
  AssignmentExpr(const LoomSourceLocation& loc, const std::string& name,
                 std::unique_ptr<ExprNode> value)
      : ExprNode(loc), name(name), value(std::move(value)) {}

  std::string toString() const override {
    return "Assignment(" + name + " = " +
           (value ? value->toString() : "nulll") + ")";
  }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class BinaryExpr : public ExprNode {
 public:
  std::unique_ptr<ExprNode> left;
  LoomToken op;
  std::unique_ptr<ExprNode> right;

  BinaryExpr(std::unique_ptr<ExprNode> left, const LoomToken& op,
             std::unique_ptr<ExprNode> right)
      : ExprNode(left->location),
        left(std::move(left)),
        op(op),
        right(std::move(right)) {}

  std::string toString() const override {
    return "Binary(" + (left ? left->toString() : "null") + " " + op.value +
           " " + (right ? right->toString() : "null") + ")";
  }

  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class StringLiteral : public ExprNode {
 public:
  std::string name;

  StringLiteral(const LoomSourceLocation& loc, const std::string& id_name)
      : ExprNode(loc), name(id_name) {}

  std::string toString() const override {
    return "StringLiteral(" + name + ")";
  }
  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

// --- Konkrete Statement-Knoten ---
class VarDeclNode : public StmtNode {
 public:
  std::string name;
  bool is_mutable;
  std::unique_ptr<ExprNode> initializer;

  VarDeclNode(const LoomSourceLocation& loc, const std::string& name,
              bool is_mutable, std::unique_ptr<ExprNode> initializer)
      : StmtNode(loc),
        name(name),
        is_mutable(is_mutable),
        initializer(std::move(initializer)) {}

  std::string toString() const override {
    return "VarDecl(" + name + (is_mutable ? ", mut" : "") + ")";
  }
  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class ExprStmtNode : public StmtNode {
 public:
  std::unique_ptr<ExprNode> expression;

  ExprStmtNode(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> expr)
      : StmtNode(loc), expression(std::move(expr)) {}

  std::string toString() const override {
    return "ExprStmt(" + (expression ? expression->toString() : "null") + ")";
  }
  void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};