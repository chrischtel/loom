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
    std::cout << "- Initializer: ";
    // Wir müssen hier einen Zeilenumbruch unterdrücken, damit es schön aussieht
    // Das ist ein bisschen ein Hack, aber für einen einfachen Printer okay.
    std::cout << node.initializer->toString() << std::endl;
    indentation_level--;
  }
}

void ASTPrinter::visit(ExprStmtNode& node) {
  indent();
  std::cout << "- ExprStmt" << std::endl;
  indentation_level++;
  if (node.expression) {
    node.expression->accept(*this);
  }
  indentation_level--;
}

void ASTPrinter::visit(AssignmentExpr& node) {
  indent();
  std::cout << "- Assignment(" << node.name << ")" << std::endl;
  if (node.value) {
    indentation_level++;
    indent();
    std::cout << "- Value:" << std::endl;
    node.value->accept(*this);  // Delegiere an den Besucher des Kindes
    indentation_level--;
  }
}

void ASTPrinter::visit(BinaryExpr& node) {
  indent();
  std::cout << "- Binary(" << node.op.value << ")" << std::endl;
  if (node.left) {
    indentation_level++;
    indent();
    std::cout << "- Left:" << std::endl;
    node.left->accept(*this);
    indentation_level--;
  }
  if (node.right) {
    indentation_level++;
    indent();
    std::cout << "- Right:" << std::endl;
    node.right->accept(*this);
    indentation_level--;
  }
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