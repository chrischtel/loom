// ast_printer.cc
#include "ast_printer.hh"

void ASTPrinter::indent() {
  for (int i = 0; i < indentation_level; ++i) {
    std::cout << "  ";
  }
}

void ASTPrinter::print(const std::vector<std::unique_ptr<StmtNode>>& ast) {
  indent();
  std::cout << "- Program" << std::endl;
  indentation_level++;
  for (const auto& stmt : ast) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
  indentation_level--;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(VarDeclNode& node) {
  indent();
  std::string kind_str;
  switch (node.kind) {
    case VarDeclKind::LET:
      kind_str = "immutable";
      break;
    case VarDeclKind::MUT:
      kind_str = "mutable";
      break;
    case VarDeclKind::DEFINE:
      kind_str = "define";
      break;
  }
  std::cout << "- VarDecl(" << node.name << ", " << kind_str << ")"
            << std::endl;

  if (node.initializer) {
    indentation_level++;
    indent();
    std::cout << "- Initializer:" << std::endl;

    node.initializer->accept(*this);
    indentation_level--;
  }
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(ExprStmtNode& node) {
  indent();
  std::cout << "- ExprStmt:" << std::endl;
  indentation_level++;
  if (node.expression) {
    node.expression->accept(*this);
  }
  indentation_level--;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(AssignmentExpr& node) {
  indent();
  std::cout << "- Assignment(" << node.name << "):" << std::endl;
  if (node.value) {
    indentation_level++;
    indent();
    // Drucke das Label und einen Zeilenumbruch.
    std::cout << "- Value:" << std::endl;
    node.value->accept(*this);
    indentation_level--;
  }
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(BinaryExpr& node) {
  indent();
  std::cout << "- Binary(" << node.op.value << ")" << std::endl;
  indentation_level++;
  if (node.left) {
    indent();
    std::cout << "- Left:" << std::endl;
    node.left->accept(*this);
  }
  if (node.right) {
    indent();
    std::cout << "- Right:" << std::endl;
    node.right->accept(*this);
  }
  indentation_level--;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(UnaryExpr& node) {
  indent();
  std::cout << "- Unary(" << node.op.value << ")" << std::endl;
  indentation_level++;
  if (node.right) {
    indent();
    std::cout << "- Right:" << std::endl;
    node.right->accept(*this);
  }
  indentation_level--;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(NumberLiteral& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}
std::unique_ptr<TypeNode> ASTPrinter::visit(Identifier& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}
std::unique_ptr<TypeNode> ASTPrinter::visit(StringLiteral& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(TypeNode& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(BooleanLiteral& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(IntegerTypeNode& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(FloatTypeNode& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(BooleanTypeNode& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(StringTypeNode& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(IntegerLiteralTypeNode& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}

std::unique_ptr<TypeNode> ASTPrinter::visit(FloatLiteralTypeNode& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
  return nullptr;
}