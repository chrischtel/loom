// ast_printer.cc
#include "ast_printer.hh"

void ASTPrinter::indent() {
  for (int i = 0; i < indentation_level; ++i) {
    std::cout << "  ";  // Zwei Leerzeichen pro Einrückungsstufe
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

void ASTPrinter::visit(VarDeclNode& node) {
  indent();
  std::cout << "- VarDecl(" << node.name
            << (node.is_mutable ? ", mutable" : ", immutable") << ")"
            << std::endl;
  if (node.initializer) {
    indentation_level++;
    indent();
    std::cout << "- Initializer: ";   // Kein Zeilenumbruch hier!
    node.initializer->accept(*this);  // Lass das Kind sich selbst drucken
    indentation_level--;
  }
}
void ASTPrinter::visit(ExprStmtNode& node) {
  indent();
  std::cout << "- ExprStmt:" << std::endl;
  indentation_level++;
  if (node.expression) {
    node.expression->accept(*this);
  }
  indentation_level--;
}

void ASTPrinter::visit(AssignmentExpr& node) {
  indent();
  std::cout << "- Assignment(" << node.name << "):" << std::endl;
  if (node.value) {
    indentation_level++;
    indent();
    std::cout << "- Value: ";  // Kein Zeilenumbruch hier!
    node.value->accept(*this);
    indentation_level--;
  }
}

void ASTPrinter::visit(BinaryExpr& node) {
  indent();
  std::cout << "Binary(" << node.op.value << ")" << std::endl;
  indentation_level++;
  if (node.left) {
    indent();
    std::cout << "- Left: ";
    node.left->accept(*this);
  }
  if (node.right) {
    indent();
    std::cout << "- Right: ";
    node.right->accept(*this);
  }
  indentation_level--;
}

// Die "Blätter" des Baumes
void ASTPrinter::visit(NumberLiteral& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
}
void ASTPrinter::visit(Identifier& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
}

void ASTPrinter::visit(StringLiteral& node) {
  indent();
  std::cout << "- " << node.toString() << std::endl;
}