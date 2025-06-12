#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "parser/ast.hh"

struct SymbolInfo {
  VarDeclKind kind;
};

class SymbolTable {
 private:
  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;

 public:
  SymbolTable();

  void enterScope();
  void leaveScope();

  bool define(const std::string& name, const SymbolInfo& info);

  const SymbolInfo* lookup(const std::string& name) const;
};
