// symbol_table.hh
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "parser/ast.hh"

enum class SymbolKind { VARIABLE, FUNCTION, TYPE, STRUCT, UNION };

struct VariableInfo {
  VarDeclKind kind;
  std::shared_ptr<TypeNode> type;
};

struct FunctionInfo {
  std::vector<std::shared_ptr<TypeNode>> parameter_types;
  std::shared_ptr<TypeNode> return_type;
  std::vector<std::string> parameter_names;
};

struct StructInfo {
  std::string name;
  std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>> fields;
};

struct UnionInfo {
  std::string name;
  std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>> fields;
};

// 3. Dann SymbolInfo (verwendet die obigen)
struct SymbolInfo {
  SymbolKind kind;
  std::variant<VariableInfo, FunctionInfo, StructInfo, UnionInfo> data;
};

class SymbolTable {
 private:
  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
  std::string current_function_name;  // Für Return-Statement validation
 public:
  SymbolTable();
  void enterScope();
  void leaveScope();
  bool define(const std::string& name, SymbolInfo info);

  const SymbolInfo* lookup(const std::string& name) const;
  // Convenience methods
  bool defineVariable(const std::string& name, VarDeclKind var_kind,
                      std::shared_ptr<TypeNode> type);
  bool defineFunction(const std::string& name,
                      std::vector<std::shared_ptr<TypeNode>> param_types,
                      std::vector<std::string> param_names,
                      std::shared_ptr<TypeNode> return_type);
  bool defineStruct(
      const std::string& name,
      std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>> fields);
  bool defineUnion(
      const std::string& name,
      std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>> fields);

  // Lookup methods
  const VariableInfo* lookupVariable(const std::string& name) const;
  const FunctionInfo* lookupFunction(const std::string& name) const;
  const StructInfo* lookupStruct(const std::string& name) const;
  const UnionInfo* lookupUnion(const std::string& name) const;

  // Type checking helpers
  bool isFunction(const std::string& name) const;
  bool isVariable(const std::string& name) const;
  bool isStructDefined(const std::string& name) const;
  bool isUnionDefined(const std::string& name) const;

  void enterFunction(const std::string& function_name);
  void leaveFunction();
  const std::string& getCurrentFunction() const;
  bool isInFunction() const;
};