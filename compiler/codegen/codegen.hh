// compiler/codegen/codegen.hh
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>  // Hinzufügen

// LLVM-Header
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

// Forward-Deklarationen
class StmtNode;  // Wir arbeiten mit der Basisklasse für Statements
class ASTNode;
class NumberLiteral;
class VarDeclNode;
class TypeNode;
class IntegerLiteralTypeNode;
class FloatLiteralTypeNode;

class CodeGen {
 public:
  CodeGen();

  // NEU: Akzeptiert einen Vektor von Statements
  void generate(const std::vector<std::unique_ptr<StmtNode>>& ast);

  void print_ir() const;

 private:
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::IRBuilder<>> builder;

  std::map<std::string, llvm::Value*> named_values;

  // Dispatch-Methoden (unverändert)
  llvm::Value* codegen(ASTNode& node);
  llvm::Value* codegen(NumberLiteral& node);
  llvm::Value* codegen(VarDeclNode& node);

  llvm::Type* typeToLLVMType(TypeNode& type);
};