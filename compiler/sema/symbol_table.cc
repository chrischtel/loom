#include "symbol_table.hh"

SymbolTable::SymbolTable() { enterScope(); }

void SymbolTable::enterScope() { scopes.push_back({}); }

void SymbolTable::leaveScope() {
  if (scopes.size() > 1) {
    scopes.pop_back();
  }
}

bool SymbolTable::define(const std::string& name, const SymbolInfo& info) {
  if (scopes.back().count(name) > 0) {
    return false;  // Re-Deklaration!
  }

  scopes.back()[name] = info;
  return true;
}

const SymbolInfo* SymbolTable::lookup(const std::string& name) const {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    const auto& scope = *it;
    auto symbol_it = scope.find(name);
    if (symbol_it != scope.end()) {
      return &symbol_it->second;
    }
  }

  // In keinem Scope gefunden.
  return nullptr;
}