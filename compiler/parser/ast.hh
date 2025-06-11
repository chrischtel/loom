// ast.hh
#pragma once

#include <memory>
#include <string>
#include <vector>  // Obwohl du es hier nicht direkt brauchst, ist es oft nützlich

#include "../scanner/scanner_internal.hh"  // Für LoomSourceLocation

// --- Basisklassen ---
class ASTNode {
 public:
  virtual ~ASTNode() = default;
  virtual std::string toString() const = 0;

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
};

class Identifier : public ExprNode {
 public:
  std::string name;

  Identifier(const LoomSourceLocation& loc, const std::string& id_name)
      : ExprNode(loc), name(id_name) {}

  std::string toString() const override { return "Identifier(" + name + ")"; }
};

class StringLiteral : public ExprNode {
 public:
  std::string name;

  StringLiteral(const LoomSourceLocation& loc, const std::string& id_name)
      : ExprNode(loc), name(id_name) {}

  std::string toString() const override {
    return "StringLiteral(" + name + ")";
  }
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
};

class ExprStmtNode : public StmtNode {
 public:
  std::unique_ptr<ExprNode> expression;

  ExprStmtNode(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> expr)
      : StmtNode(loc), expression(std::move(expr)) {}

  std::string toString() const override {
    return "ExprStmt(" + (expression ? expression->toString() : "null") + ")";
  }
};