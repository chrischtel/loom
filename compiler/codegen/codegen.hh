// compiler/codegen/codegen.hh
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>  // Hinzufügen

// LLVM-Header
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Triple.h"

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
  // Write IR to file
  void writeIRToFile(const std::string& filename) const;

  // Integrated compilation methods (like Kaleidoscope)
  bool compileToObjectFile(const std::string& filename) const;
  bool compileToExecutable(const std::string& objectFilename,
                           const std::string& executableFilename) const;
  // Initialize LLVM targets (call once at startup)
  bool initializeLLVMTargets();

  // Public access to LLVM module for external compilation
  std::unique_ptr<llvm::Module> module;

 private:
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::IRBuilder<>> builder;

  std::map<std::string, llvm::Value*> named_values;

  // Dispatch-Methoden (unverändert)
  llvm::Value* codegen(ASTNode& node);
  llvm::Value* codegen(NumberLiteral& node);
  llvm::Value* codegen(VarDeclNode& node);

  llvm::Type* typeToLLVMType(TypeNode& type);

  // Generate code for a node with a specific target type (for type casting)
  llvm::Value* codegenWithTargetType(ASTNode& node, llvm::Type* targetType);
};