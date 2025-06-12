// semantic_analyzer.cc
#include <iostream>

#include "semantic_analyze.hh"

SemanticAnalyzer::SemanticAnalyzer() : had_error(false) {
  // Der Konstruktor der SymbolTable wird automatisch aufgerufen
  // und erstellt den globalen Scope.
}

// Die Haupt-Analysefunktion, die den Prozess startet.
void SemanticAnalyzer::analyze(
    const std::vector<std::unique_ptr<StmtNode>>& ast) {
  for (const auto& stmt : ast) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void SemanticAnalyzer::error(const LoomSourceLocation& loc,
                             const std::string& message) {
  had_error = true;
  std::cerr << "Semantic Error at " << loc.toString() << ": " << message
            << std::endl;
}

// --- DIE ERSTE WICHTIGE VISIT-METHODE ---
void SemanticAnalyzer::visit(VarDeclNode& node) {
  if (node.initializer) {
    node.initializer->accept(*this);
  }

  // Erstelle die Informationen für das neue Symbol.
  SymbolInfo info;
  info.kind = node.kind;
  // info.type = ... (später)

  // Versuche, die Variable im aktuellen Scope zu definieren.
  if (!symbols.define(node.name, info)) {
    // Wenn define() false zurückgibt, war es eine Re-Deklaration.
    error(node.location,
          "Variable '" + node.name + "' is already declared in this scope.");
  }
}

// --- DIE ZWEITE WICHTIGE VISIT-METHODE ---
void SemanticAnalyzer::visit(Identifier& node) {
  // Versuche, die Variable in der Symboltabelle zu finden.
  if (symbols.lookup(node.name) == nullptr) {
    error(node.location, "Undeclared identifier '" + node.name + "'.");
  }
  // Später: Annotiere den Knoten mit den gefundenen Informationen.
}

// --- PLATZHALTER FÜR DIE ANDEREN VISIT-METHODEN ---
// Du musst für JEDE visit-Methode aus dem Interface eine Implementierung
// bereitstellen, sonst ist der Sema abstrakt. Für den Anfang können
// sie einfach die Analyse an ihre Kinder weiterleiten.

void SemanticAnalyzer::visit(NumberLiteral& node) { (void)node; }
void SemanticAnalyzer::visit(StringLiteral& node) { (void)node; }
void SemanticAnalyzer::visit(TypeNode& node) { (void)node; }

void SemanticAnalyzer::visit(ExprStmtNode& node) {
  if (node.expression) node.expression->accept(*this);
}

void SemanticAnalyzer::visit(AssignmentExpr& node) {
  // TODO: Prüfen, ob die Variable existiert und mutable ist.
  if (node.value) node.value->accept(*this);
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
  if (node.left) node.left->accept(*this);
  if (node.right) node.right->accept(*this);
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
  if (node.right) node.right->accept(*this);
}