// symbol_table.cc
#include "symbol_table.hh"

SymbolTable::SymbolTable() { enterScope(); }
void SymbolTable::enterScope() { scopes.emplace_back(); }
void SymbolTable::leaveScope() {
  if (scopes.size() > 1) scopes.pop_back();
}

// KORREKTUR: Nimmt info per Wert und movt sie in die Map
bool SymbolTable::define(const std::string& name, SymbolInfo info) {
  if (scopes.back().count(name) > 0) {
    return false;
  }
  scopes.back().emplace(name, std::move(info));
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
  return nullptr;
}