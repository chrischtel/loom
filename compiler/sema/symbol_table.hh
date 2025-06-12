// symbol_table.hh
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "parser/ast.hh"

struct SymbolInfo {
  VarDeclKind kind;
  std::shared_ptr<TypeNode> type;
};

class SymbolTable {
 private:
  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;

 public:
  SymbolTable();
  void enterScope();
  void leaveScope();
  // KORREKTUR: Nimmt info per Wert, ist nicht const
  bool define(const std::string& name, SymbolInfo info);
  // KORREKTUR: Ist korrekt const
  const SymbolInfo* lookup(const std::string& name) const;
};