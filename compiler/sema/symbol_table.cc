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

bool SymbolTable::defineVariable(const std::string& name, VarDeclKind var_kind,
                                 std::shared_ptr<TypeNode> type) {
  VariableInfo var_info{var_kind, type};
  SymbolInfo symbol_info;
  symbol_info.kind = SymbolKind::VARIABLE;
  symbol_info.data = var_info;

  return define(name, symbol_info);
}

bool SymbolTable::defineFunction(
    const std::string& name, std::vector<std::shared_ptr<TypeNode>> param_types,
    std::vector<std::string> param_names,
    std::shared_ptr<TypeNode> return_type) {
  FunctionInfo func_info{param_types, return_type, param_names};
  SymbolInfo info;
  info.kind = SymbolKind::FUNCTION;
  info.data = func_info;

  return define(name, info);
}

const VariableInfo* SymbolTable::lookupVariable(const std::string& name) const {
  const SymbolInfo* symbol = lookup(name);
  if (symbol && symbol->kind == SymbolKind::VARIABLE) {
    return &std::get<VariableInfo>(symbol->data);
  }
  return nullptr;
}

const FunctionInfo* SymbolTable::lookupFunction(const std::string& name) const {
  const SymbolInfo* symbol = lookup(name);

  if (symbol && symbol->kind == SymbolKind::FUNCTION) {
    return &std::get<FunctionInfo>(symbol->data);
  }

  return nullptr;
}

const std::string& SymbolTable::getCurrentFunction() const {
  return current_function_name;
}

bool SymbolTable::isInFunction() const { return current_function_name != ""; }

void SymbolTable::enterFunction(const std::string& function_name) {
  current_function_name = function_name;
  enterScope();
}

void SymbolTable::leaveFunction() {
  current_function_name = "";
  leaveScope();
}

bool SymbolTable::isVariable(const std::string& name) const {
  const SymbolInfo* symbol = lookup(name);
  return symbol && symbol->kind == SymbolKind::VARIABLE;
}

bool SymbolTable::isFunction(const std::string& name) const {
  const SymbolInfo* symbol = lookup(name);
  return symbol && symbol->kind == SymbolKind::FUNCTION;
}
