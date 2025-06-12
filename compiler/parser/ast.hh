#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../scanner/scanner_internal.hh"

// KORREKTUR 1: Enum an den Anfang der Datei
enum class VarDeclKind { LET, MUT, DEFINE };

// Forward-Deklarationen für den Visitor
class NumberLiteral;
class Identifier;
class StringLiteral;
class AssignmentExpr;
class BinaryExpr;
class UnaryExpr;
class VarDeclNode;
class ExprStmtNode;
class TypeNode;

// KORREKTUR 2: Nur noch EIN Visitor-Interface
class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;
  virtual std::unique_ptr<TypeNode> visit(NumberLiteral& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(StringLiteral& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(Identifier& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(AssignmentExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(BinaryExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(UnaryExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(VarDeclNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(ExprStmtNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(TypeNode& node) = 0;
};

// Basisklasse
class ASTNode {
 public:
  virtual ~ASTNode() = default;
  LoomSourceLocation location;
  virtual std::string toString() const = 0;
  // Es gibt nur noch EINE accept-Methode
  virtual std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) = 0;

 protected:
  ASTNode(const LoomSourceLocation& loc) : location(loc) {}
};

// Zwischen-Basisklassen
class StmtNode : public ASTNode {
 protected:
  using ASTNode::ASTNode;
};

class ExprNode : public ASTNode {
 protected:
  using ASTNode::ASTNode;
};

// Konkrete Knoten
class TypeNode : public ASTNode {
 public:
  std::string name;
  TypeNode(const LoomSourceLocation& loc, const std::string& n)
      : ASTNode(loc), name(n) {}
  std::string toString() const override { return "Type(" + name + ")"; }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

class NumberLiteral : public ExprNode {
 public:
  std::string value;
  bool is_float;
  NumberLiteral(const LoomSourceLocation& loc, const std::string& v, bool f)
      : ExprNode(loc), value(v), is_float(f) {}
  std::string toString() const override {
    return "NumberLiteral(" + value + (is_float ? "f" : "i") + ")";
  }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// ... und so weiter für ALLE deine anderen Knoten.
// Jede `accept`-Methode muss `std::unique_ptr<TypeNode> accept(...) override`
// sein. Hier sind die restlichen als Beispiel:

class Identifier : public ExprNode {
 public:
  std::string name;
  Identifier(const LoomSourceLocation& loc, const std::string& n)
      : ExprNode(loc), name(n) {}
  std::string toString() const override { return "Identifier(" + name + ")"; }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

class StringLiteral : public ExprNode {
 public:
  std::string value;
  StringLiteral(const LoomSourceLocation& loc, const std::string& v)
      : ExprNode(loc), value(v) {}
  std::string toString() const override {
    return "StringLiteral(" + value + ")";
  }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

class AssignmentExpr : public ExprNode {
 public:
  std::string name;
  std::unique_ptr<ExprNode> value;
  AssignmentExpr(const LoomSourceLocation& loc, const std::string& n,
                 std::unique_ptr<ExprNode> v)
      : ExprNode(loc), name(n), value(std::move(v)) {}
  std::string toString() const override {
    return "Assignment(" + name + " = " + (value ? value->toString() : "null") +
           ")";
  }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

class BinaryExpr : public ExprNode {
 public:
  std::unique_ptr<ExprNode> left;
  LoomToken op;
  std::unique_ptr<ExprNode> right;
  BinaryExpr(std::unique_ptr<ExprNode> l, const LoomToken& o,
             std::unique_ptr<ExprNode> r)
      : ExprNode(l->location), left(std::move(l)), op(o), right(std::move(r)) {}
  std::string toString() const override {
    return "Binary(" + (left ? left->toString() : "null") + " " + op.value +
           " " + (right ? right->toString() : "null") + ")";
  }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

class UnaryExpr : public ExprNode {
 public:
  LoomToken op;
  std::unique_ptr<ExprNode> right;
  UnaryExpr(const LoomToken& o, std::unique_ptr<ExprNode> r)
      : ExprNode(o.location), op(o), right(std::move(r)) {}
  std::string toString() const override {
    return "Unary(" + op.value + " " + (right ? right->toString() : "null") +
           ")";
  }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

class VarDeclNode : public StmtNode {
 public:
  std::string name;
  VarDeclKind kind;
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<ExprNode> initializer;
  VarDeclNode(const LoomSourceLocation& loc, const std::string& n,
              VarDeclKind k, std::unique_ptr<TypeNode> t,
              std::unique_ptr<ExprNode> i)
      : StmtNode(loc),
        name(n),
        kind(k),
        type(std::move(t)),
        initializer(std::move(i)) {}
  std::string toString() const override { /* ... */ return "VarDecl(...)"; }
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
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
  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};