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
class BooleanLiteral;
class AssignmentExpr;
class BinaryExpr;
class UnaryExpr;
class VarDeclNode;
class ExprStmtNode;
class IfStmtNode;
class WhileStmtNode;
class FunctionCallExpr;
class BuiltinCallExpr;
class TypeNode;
class IntegerTypeNode;
class FloatTypeNode;
class BooleanTypeNode;
class StringTypeNode;
class NullTypeNode;
class IntegerLiteralTypeNode;
class FloatLiteralTypeNode;
class FunctionDeclNode;
class ParameterNode;
class ReturnStmtNode;

// Memory model nodes
class ReferenceTypeNode;
class OwnedPointerTypeNode;
class NullableTypeNode;
class SliceTypeNode;
class ReferenceExpr;
class DereferenceExpr;
class MemberAccessExpr;
class PointerAccessExpr;
class SliceExpr;
class DeferStmtNode;
class UnsafeBlockExpr;
class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;
  virtual std::unique_ptr<TypeNode> visit(NumberLiteral& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(StringLiteral& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(BooleanLiteral& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(Identifier& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(AssignmentExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(BinaryExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(UnaryExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(VarDeclNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(FunctionDeclNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(ParameterNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(ReturnStmtNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(ExprStmtNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(IfStmtNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(WhileStmtNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(FunctionCallExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(BuiltinCallExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(TypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(IntegerTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(FloatTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(BooleanTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(StringTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(NullTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(IntegerLiteralTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(FloatLiteralTypeNode& node) = 0;

  // Memory model visitors
  virtual std::unique_ptr<TypeNode> visit(ReferenceTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(OwnedPointerTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(NullableTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(SliceTypeNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(ReferenceExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(DereferenceExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(MemberAccessExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(PointerAccessExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(SliceExpr& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(DeferStmtNode& node) = 0;
  virtual std::unique_ptr<TypeNode> visit(UnsafeBlockExpr& node) = 0;
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

// Base type node - now abstract
class TypeNode : public ASTNode {
 public:
  virtual bool isEqualTo(const TypeNode* other) const = 0;
  virtual std::string getTypeName() const = 0;
  // New method to check if this type can accept a value from another type
  virtual bool canAcceptFrom(const TypeNode* other) const = 0;

 protected:
  using ASTNode::ASTNode;
};

// Integer types (i8, i16, i32, i64)
class IntegerTypeNode : public TypeNode {
 public:
  int bit_width;
  bool is_signed;

  IntegerTypeNode(const LoomSourceLocation& loc, int width,
                  bool signed_type = true)
      : TypeNode(loc), bit_width(width), is_signed(signed_type) {}

  std::string toString() const override {
    return (is_signed ? "i" : "u") + std::to_string(bit_width);
  }

  std::string getTypeName() const override {
    return (is_signed ? "i" : "u") + std::to_string(bit_width);
  }
  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_int = dynamic_cast<const IntegerTypeNode*>(other)) {
      return this->bit_width == other_int->bit_width &&
             this->is_signed == other_int->is_signed;
    }
    return false;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    // Can always accept from exactly the same type
    if (isEqualTo(other)) {
      return true;
    }
    // For now, we'll handle literal compatibility in the semantic analyzer
    // This method can be extended for more complex type conversions
    return false;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Float types (f16, f32, f64)
class FloatTypeNode : public TypeNode {
 public:
  int bit_width;

  FloatTypeNode(const LoomSourceLocation& loc, int width)
      : TypeNode(loc), bit_width(width) {}

  std::string toString() const override {
    return "f" + std::to_string(bit_width);
  }

  std::string getTypeName() const override {
    return "f" + std::to_string(bit_width);
  }
  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_float = dynamic_cast<const FloatTypeNode*>(other)) {
      return this->bit_width == other_float->bit_width;
    }
    return false;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    // Can always accept from exactly the same type
    if (isEqualTo(other)) {
      return true;
    }
    // For now, we'll handle literal compatibility in the semantic analyzer
    return false;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Boolean type
class BooleanTypeNode : public TypeNode {
 public:
  BooleanTypeNode(const LoomSourceLocation& loc) : TypeNode(loc) {}

  std::string toString() const override { return "bool"; }

  std::string getTypeName() const override { return "bool"; }
  bool isEqualTo(const TypeNode* other) const override {
    return dynamic_cast<const BooleanTypeNode*>(other) != nullptr;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// String type
class StringTypeNode : public TypeNode {
 public:
  StringTypeNode(const LoomSourceLocation& loc) : TypeNode(loc) {}

  std::string toString() const override { return "string"; }

  std::string getTypeName() const override { return "string"; }
  bool isEqualTo(const TypeNode* other) const override {
    return dynamic_cast<const StringTypeNode*>(other) != nullptr;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Null type - represents the type of null literals
class NullTypeNode : public TypeNode {
 public:
  NullTypeNode(const LoomSourceLocation& loc) : TypeNode(loc) {}

  std::string toString() const override { return "null"; }

  std::string getTypeName() const override { return "null"; }

  bool isEqualTo(const TypeNode* other) const override {
    return dynamic_cast<const NullTypeNode*>(other) != nullptr;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Special type for integer literals that can be converted to appropriate types
class IntegerLiteralTypeNode : public TypeNode {
 public:
  long long value;  // Store the actual literal value

  IntegerLiteralTypeNode(const LoomSourceLocation& loc, long long val)
      : TypeNode(loc), value(val) {}

  std::string toString() const override {
    return "IntegerLiteral(" + std::to_string(value) + ")";
  }

  std::string getTypeName() const override { return "literal_int"; }

  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_lit = dynamic_cast<const IntegerLiteralTypeNode*>(other)) {
      return this->value == other_lit->value;
    }
    return false;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }

  // Check if this literal can fit into a specific integer type
  bool canFitInto(const IntegerTypeNode* target) const {
    if (target->is_signed) {
      switch (target->bit_width) {
        case 8:
          return value >= -128 && value <= 127;
        case 16:
          return value >= -32768 && value <= 32767;
        case 32:
          return value >= -2147483648LL && value <= 2147483647LL;
        case 64:
          return true;  // long long can always fit in i64
        default:
          return false;
      }
    } else {
      // Unsigned integers
      if (value < 0) return false;  // Can't fit negative values in unsigned
      switch (target->bit_width) {
        case 8:
          return value <= 255;
        case 16:
          return value <= 65535;
        case 32:
          return static_cast<unsigned long long>(value) <= 4294967295ULL;
        case 64:
          return value >= 0;  // Any non-negative long long fits in u64
        default:
          return false;
      }
    }
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Special type for float literals that can be converted to appropriate types
class FloatLiteralTypeNode : public TypeNode {
 public:
  double value;  // Store the actual literal value

  FloatLiteralTypeNode(const LoomSourceLocation& loc, double val)
      : TypeNode(loc), value(val) {}

  std::string toString() const override {
    return "FloatLiteral(" + std::to_string(value) + ")";
  }

  std::string getTypeName() const override { return "literal_float"; }

  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_lit = dynamic_cast<const FloatLiteralTypeNode*>(other)) {
      return this->value == other_lit->value;
    }
    return false;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }
  // Check if this literal can fit into a specific float type
  bool canFitInto(const FloatTypeNode* target) const {
    // For simplicity, assume all float literals can fit into f32 and f64
    // f16 might have range issues, but we'll be permissive for now
    return target->bit_width >= 16;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Memory model type nodes

// Reference type: &T
class ReferenceTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> referenced_type;

  ReferenceTypeNode(const LoomSourceLocation& loc,
                    std::unique_ptr<TypeNode> type)
      : TypeNode(loc), referenced_type(std::move(type)) {}

  std::string toString() const override {
    return "&" + referenced_type->toString();
  }

  std::string getTypeName() const override {
    return "ref_" + referenced_type->getTypeName();
  }

  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_ref = dynamic_cast<const ReferenceTypeNode*>(other)) {
      return referenced_type->isEqualTo(other_ref->referenced_type.get());
    }
    return false;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Owned pointer type: ^T
class OwnedPointerTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> pointed_type;

  OwnedPointerTypeNode(const LoomSourceLocation& loc,
                       std::unique_ptr<TypeNode> type)
      : TypeNode(loc), pointed_type(std::move(type)) {}

  std::string toString() const override {
    return "^" + pointed_type->toString();
  }

  std::string getTypeName() const override {
    return "owned_" + pointed_type->getTypeName();
  }

  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_owned = dynamic_cast<const OwnedPointerTypeNode*>(other)) {
      return pointed_type->isEqualTo(other_owned->pointed_type.get());
    }
    return false;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Nullable type: T?
class NullableTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> inner_type;

  NullableTypeNode(const LoomSourceLocation& loc,
                   std::unique_ptr<TypeNode> type)
      : TypeNode(loc), inner_type(std::move(type)) {}

  std::string toString() const override { return inner_type->toString() + "?"; }

  std::string getTypeName() const override {
    return "nullable_" + inner_type->getTypeName();
  }

  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_nullable = dynamic_cast<const NullableTypeNode*>(other)) {
      return inner_type->isEqualTo(other_nullable->inner_type.get());
    }
    return false;
  }
  bool canAcceptFrom(const TypeNode* other) const override {
    // Nullable types can accept their inner type, other nullable of same type,
    // or null
    return isEqualTo(other) || inner_type->isEqualTo(other) ||
           dynamic_cast<const NullTypeNode*>(other) != nullptr;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Slice type: []T
class SliceTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> element_type;

  SliceTypeNode(const LoomSourceLocation& loc, std::unique_ptr<TypeNode> type)
      : TypeNode(loc), element_type(std::move(type)) {}

  std::string toString() const override {
    return "[]" + element_type->toString();
  }

  std::string getTypeName() const override {
    return "slice_" + element_type->getTypeName();
  }

  bool isEqualTo(const TypeNode* other) const override {
    if (auto other_slice = dynamic_cast<const SliceTypeNode*>(other)) {
      return element_type->isEqualTo(other_slice->element_type.get());
    }
    return false;
  }

  bool canAcceptFrom(const TypeNode* other) const override {
    return isEqualTo(other);
  }

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

class BooleanLiteral : public ExprNode {
 public:
  bool value;

  BooleanLiteral(const LoomSourceLocation& loc, bool val)
      : ExprNode(loc), value(val) {}

  std::string toString() const override {
    return "BooleanLiteral(" + std::string(value ? "true" : "false") + ")";
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

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

// Parameter node for function declarations
class ParameterNode : public ASTNode {
 public:
  std::string name;
  std::unique_ptr<TypeNode> type;

  ParameterNode(const LoomSourceLocation& loc, const std::string& param_name,
                std::unique_ptr<TypeNode> param_type)
      : ASTNode(loc), name(param_name), type(std::move(param_type)) {}

  std::string toString() const override {
    return name + ": " + (type ? type->toString() : "unknown");
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Function declaration node
class FunctionDeclNode : public StmtNode {
 public:
  std::string name;
  std::vector<std::unique_ptr<ParameterNode>> parameters;
  std::unique_ptr<TypeNode> return_type;
  std::vector<std::unique_ptr<StmtNode>> body;

  FunctionDeclNode(const LoomSourceLocation& loc, const std::string& func_name,
                   std::vector<std::unique_ptr<ParameterNode>> params,
                   std::unique_ptr<TypeNode> ret_type,
                   std::vector<std::unique_ptr<StmtNode>> func_body)
      : StmtNode(loc),
        name(func_name),
        parameters(std::move(params)),
        return_type(std::move(ret_type)),
        body(std::move(func_body)) {}

  std::string toString() const override {
    std::string result = "FunctionDecl(" + name + "(";
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (i > 0) result += ", ";
      result += parameters[i] ? parameters[i]->toString() : "null";
    }
    result += ") -> ";
    result += return_type ? return_type->toString() : "void";
    result += " {";
    for (size_t i = 0; i < body.size(); ++i) {
      if (i > 0) result += ", ";
      result += body[i] ? body[i]->toString() : "null";
    }
    result += "})";
    return result;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Return statement node
class ReturnStmtNode : public StmtNode {
 public:
  std::unique_ptr<ExprNode> expression;  // null for void returns

  ReturnStmtNode(const LoomSourceLocation& loc,
                 std::unique_ptr<ExprNode> expr = nullptr)
      : StmtNode(loc), expression(std::move(expr)) {}

  std::string toString() const override {
    std::string result = "ReturnStmt(";
    result += expression ? expression->toString() : "void";
    result += ")";
    return result;
  }

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

// IfStmtNode for if-else statements
class IfStmtNode : public StmtNode {
 public:
  std::unique_ptr<ExprNode> condition;
  std::vector<std::unique_ptr<StmtNode>> then_body;
  std::vector<std::unique_ptr<StmtNode>> else_body;  // optional

  IfStmtNode(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> cond,
             std::vector<std::unique_ptr<StmtNode>> then_stmts,
             std::vector<std::unique_ptr<StmtNode>> else_stmts = {})
      : StmtNode(loc),
        condition(std::move(cond)),
        then_body(std::move(then_stmts)),
        else_body(std::move(else_stmts)) {}

  std::string toString() const override {
    std::string result =
        "IfStmt(cond: " + (condition ? condition->toString() : "null") +
        ", then: [";
    for (const auto& stmt : then_body) {
      result += stmt->toString() + ", ";
    }
    result += "], else: [";
    for (const auto& stmt : else_body) {
      result += stmt->toString() + ", ";
    }
    result += "])";
    return result;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

class WhileStmtNode : public StmtNode {
 public:
  std::unique_ptr<ExprNode> condition;
  std::vector<std::unique_ptr<StmtNode>> body;

  WhileStmtNode(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> cond,
                std::vector<std::unique_ptr<StmtNode>> stmts)
      : StmtNode(loc), condition(std::move(cond)), body(std::move(stmts)) {}

  std::string toString() const override {
    std::string result = "WhileStmt(cond: ";
    result += condition ? condition->toString() : "null";
    result += ", body: [";
    for (size_t i = 0; i < body.size(); ++i) {
      if (i > 0) result += ", ";
      result += body[i] ? body[i]->toString() : "null";
    }
    result += "])";
    return result;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Defer statement: defer statement
class DeferStmtNode : public StmtNode {
 public:
  std::unique_ptr<StmtNode> deferred_statement;

  DeferStmtNode(const LoomSourceLocation& loc, std::unique_ptr<StmtNode> stmt)
      : StmtNode(loc), deferred_statement(std::move(stmt)) {}

  std::string toString() const override {
    return "defer " +
           (deferred_statement ? deferred_statement->toString() : "null");
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// FunctionCallExpr for print() calls
class FunctionCallExpr : public ExprNode {
 public:
  std::string function_name;
  std::vector<std::unique_ptr<ExprNode>> arguments;

  FunctionCallExpr(const LoomSourceLocation& loc, const std::string& name,
                   std::vector<std::unique_ptr<ExprNode>> args)
      : ExprNode(loc), function_name(name), arguments(std::move(args)) {}

  std::string toString() const override {
    std::string result = "FunctionCall(" + function_name + "(";
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (i > 0) result += ", ";
      result += arguments[i] ? arguments[i]->toString() : "null";
    }
    result += "))";
    return result;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// BuiltinCallExpr for $$builtin() calls
class BuiltinCallExpr : public ExprNode {
 public:
  std::string builtin_name;  // e.g., "print", "exit", "syscall"
  std::vector<std::unique_ptr<ExprNode>> arguments;

  BuiltinCallExpr(const LoomSourceLocation& loc, const std::string& name,
                  std::vector<std::unique_ptr<ExprNode>> args)
      : ExprNode(loc), builtin_name(name), arguments(std::move(args)) {}

  std::string toString() const override {
    std::string result = "BuiltinCall($$" + builtin_name + "(";
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (i > 0) result += ", ";
      result += arguments[i] ? arguments[i]->toString() : "null";
    }
    result += "))";
    return result;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Memory model expression nodes

// Reference expression: &expr
class ReferenceExpr : public ExprNode {
 public:
  std::unique_ptr<ExprNode> operand;

  ReferenceExpr(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> expr)
      : ExprNode(loc), operand(std::move(expr)) {}

  std::string toString() const override {
    return "&(" + (operand ? operand->toString() : "null") + ")";
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Dereference expression: *expr or ^expr
class DereferenceExpr : public ExprNode {
 public:
  std::unique_ptr<ExprNode> operand;
  TokenType deref_type;  // TOKEN_STAR for *, TOKEN_HAT for ^

  DereferenceExpr(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> expr,
                  TokenType type)
      : ExprNode(loc), operand(std::move(expr)), deref_type(type) {}

  std::string toString() const override {
    std::string op = (deref_type == TokenType::TOKEN_STAR) ? "*" : "^";
    return op + "(" + (operand ? operand->toString() : "null") + ")";
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Member access expression: expr.field
class MemberAccessExpr : public ExprNode {
 public:
  std::unique_ptr<ExprNode> object;
  std::string member_name;

  MemberAccessExpr(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> obj,
                   const std::string& member)
      : ExprNode(loc), object(std::move(obj)), member_name(member) {}

  std::string toString() const override {
    return (object ? object->toString() : "null") + "." + member_name;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Pointer access expression: expr->field
class PointerAccessExpr : public ExprNode {
 public:
  std::unique_ptr<ExprNode> pointer;
  std::string member_name;

  PointerAccessExpr(const LoomSourceLocation& loc,
                    std::unique_ptr<ExprNode> ptr, const std::string& member)
      : ExprNode(loc), pointer(std::move(ptr)), member_name(member) {}

  std::string toString() const override {
    return (pointer ? pointer->toString() : "null") + "->" + member_name;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Slice expression: expr[start..end]
class SliceExpr : public ExprNode {
 public:
  std::unique_ptr<ExprNode> array;
  std::unique_ptr<ExprNode> start;
  std::unique_ptr<ExprNode> end;

  SliceExpr(const LoomSourceLocation& loc, std::unique_ptr<ExprNode> arr,
            std::unique_ptr<ExprNode> s, std::unique_ptr<ExprNode> e)
      : ExprNode(loc),
        array(std::move(arr)),
        start(std::move(s)),
        end(std::move(e)) {}

  std::string toString() const override {
    return (array ? array->toString() : "null") + "[" +
           (start ? start->toString() : "") + ".." +
           (end ? end->toString() : "") + "]";
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};

// Unsafe block expression: unsafe { ... }
class UnsafeBlockExpr : public ExprNode {
 public:
  std::vector<std::unique_ptr<StmtNode>> statements;

  UnsafeBlockExpr(const LoomSourceLocation& loc,
                  std::vector<std::unique_ptr<StmtNode>> stmts)
      : ExprNode(loc), statements(std::move(stmts)) {}

  std::string toString() const override {
    std::string result = "unsafe { ";
    for (const auto& stmt : statements) {
      result += stmt ? stmt->toString() : "null";
      result += "; ";
    }
    result += "}";
    return result;
  }

  std::unique_ptr<TypeNode> accept(ASTVisitor& visitor) override {
    return visitor.visit(*this);
  }
};