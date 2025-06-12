// compiler/codegen/codegen.cc
#include "codegen.hh"

#include <iostream>
#include <stdexcept>
#include <typeinfo>

#include "../parser/ast.hh"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"  // Nützlich zum Überprüfen des generierten Codes
#include "llvm/Support/raw_ostream.h"

CodeGen::CodeGen() {
  context = std::make_unique<llvm::LLVMContext>();
  module = std::make_unique<llvm::Module>("MyLoomModule", *context);
  builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

void CodeGen::generate(const std::vector<std::unique_ptr<StmtNode>>& ast) {
  std::cout << "[CodeGen] Starting code generation..." << std::endl;

  // 1. Erstelle die 'main'-Funktion. In C-artigen Sprachen ist der
  //    Einstiegspunkt 'int main()'.
  //    Funktionstyp: int32 ()
  llvm::FunctionType* funcType =
      llvm::FunctionType::get(builder->getInt32Ty(), /*isVarArg=*/false);

  //    Erstelle die Funktion im Modul
  llvm::Function* mainFunc = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, "main", module.get());

  std::cout << "[CodeGen] Created main function" << std::endl;

  // 2. Erstelle den Einstiegs-Block für die Funktion.
  //    Hier wird der Code platziert.
  llvm::BasicBlock* entryBlock =
      llvm::BasicBlock::Create(*context, "entry", mainFunc);
  builder->SetInsertPoint(entryBlock);

  std::cout << "[CodeGen] Created entry block" << std::endl;

  // 3. Generiere den Code für jedes Statement im AST.
  std::cout << "[CodeGen] Processing " << ast.size() << " statements..."
            << std::endl;

  try {
    for (size_t i = 0; i < ast.size(); ++i) {
      std::cout << "[CodeGen] Processing statement " << (i + 1) << "/"
                << ast.size() << std::endl;
      codegen(*ast[i]);
      std::cout << "[CodeGen] Statement " << (i + 1)
                << " completed successfully" << std::endl;
    }
  } catch (const std::exception& e) {
    std::cout << "[CodeGen] Error during statement processing: " << e.what()
              << std::endl;
    std::cout << "[CodeGen] Printing IR so far:" << std::endl;
    print_ir();
    throw;  // Re-throw the exception
  }

  // 4. Füge am Ende der 'main' ein 'return 0;' ein.
  builder->CreateRet(builder->getInt32(0));
  std::cout << "[CodeGen] Added return statement" << std::endl;

  // 5. Überprüfe die Funktion auf Konsistenz (sehr empfohlen!)
  std::cout << "[CodeGen] Verifying function..." << std::endl;
  llvm::verifyFunction(*mainFunc);
  std::cout << "[CodeGen] Function verification completed" << std::endl;

  std::cout << "[CodeGen] Code generation completed successfully!" << std::endl;
  std::cout << "[CodeGen] Generated LLVM IR:" << std::endl;
  print_ir();
}

void CodeGen::print_ir() const { module->print(llvm::outs(), nullptr); }

// --- Helper: AST-Typ zu LLVM-Typ ---
llvm::Type* CodeGen::typeToLLVMType(TypeNode& type) {
  std::cout << "[CodeGen] Converting TypeNode to LLVM type, typeid: "
            << typeid(type).name() << std::endl;

  if (auto* int_type = dynamic_cast<IntegerTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found IntegerTypeNode with bit_width: "
              << int_type->bit_width << std::endl;
    return builder->getIntNTy(int_type->bit_width);
  }
  if (auto* int_literal_type = dynamic_cast<IntegerLiteralTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found IntegerLiteralTypeNode with value: "
              << int_literal_type->value << std::endl;
    // For integer literals, we default to i32
    return builder->getInt32Ty();
  }
  if (auto* float_type = dynamic_cast<FloatTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found FloatTypeNode with bit_width: "
              << float_type->bit_width << std::endl;
    switch (float_type->bit_width) {
      case 16:
        return builder->getHalfTy();
      case 32:
        return builder->getFloatTy();
      case 64:
        return builder->getDoubleTy();
      default:
        throw std::runtime_error("Unsupported float bit width");
    }
  }
  if (auto* float_literal_type = dynamic_cast<FloatLiteralTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found FloatLiteralTypeNode with value: "
              << float_literal_type->value << std::endl;
    // For float literals, we default to double (f64)
    return builder->getDoubleTy();
  }
  if (dynamic_cast<BooleanTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found BooleanTypeNode" << std::endl;
    return builder->getInt1Ty();  // bool wird als 1-bit Integer dargestellt
  }
  if (dynamic_cast<StringTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found StringTypeNode" << std::endl;
    // Strings werden oft als Zeiger auf ein Char-Array (i8*) dargestellt
    // In newer LLVM versions, use getPtrTy() for opaque pointers
    return llvm::PointerType::getUnqual(*context);
  }

  std::cout << "[CodeGen] ERROR: Unknown TypeNode, type info: "
            << typeid(type).name() << std::endl;
  throw std::runtime_error("Unknown TypeNode for CodeGen");
}

// --- Codegen Dispatch ---
llvm::Value* CodeGen::codegen(ASTNode& node) {
  std::cout << "[CodeGen] Dispatching node: " << node.toString() << std::endl;

  // Die Reihenfolge ist wichtig: von spezifisch zu allgemein
  if (auto* n = dynamic_cast<VarDeclNode*>(&node)) {
    std::cout << "[CodeGen] Processing VarDeclNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<NumberLiteral*>(&node)) {
    std::cout << "[CodeGen] Processing NumberLiteral" << std::endl;
    return codegen(*n);
  }
  // ... weitere Knotentypen hier einfügen

  std::cout << "[CodeGen] ERROR: No codegen implementation for node type: "
            << node.toString() << std::endl;
  throw std::runtime_error("CodeGen not implemented for this ASTNode type: " +
                           node.toString());
}

// --- Codegen für Literale ---
llvm::Value* CodeGen::codegen(NumberLiteral& node) {
  std::cout << "[CodeGen] Generating NumberLiteral: " << node.value
            << " (is_float: " << node.is_float << ")" << std::endl;

  if (node.is_float) {
    double val = std::stod(node.value);
    std::cout << "[CodeGen] Creating float constant: " << val << std::endl;
    // TODO: Hier müsste man den Typ genauer bestimmen (f32, f64 etc.)
    // Fürs Erste nehmen wir immer f64 (double).
    return llvm::ConstantFP::get(*context, llvm::APFloat(val));
  } else {
    long long val = std::stoll(node.value);
    std::cout << "[CodeGen] Creating int constant: " << val << std::endl;
    // TODO: Hier müsste man den Typ genauer bestimmen (i32, i64 etc.)
    // Fürs Erste nehmen wir immer i32.
    return llvm::ConstantInt::get(
        *context, llvm::APInt(32, static_cast<uint64_t>(val), true));
  }
}

// --- Codegen für Statements ---
llvm::Value* CodeGen::codegen(VarDeclNode& node) {
  std::cout << "[CodeGen] Generating VarDeclNode: " << node.name << std::endl;

  // 1. Generiere den Code für den Initialisierungswert.
  //    z.B. bei 'let x = 42;', wird hier der Wert '42' generiert.
  std::cout << "[CodeGen] Generating initializer for variable: " << node.name
            << std::endl;
  llvm::Value* initializerVal = codegen(*node.initializer);
  std::cout << "[CodeGen] Initializer generated successfully" << std::endl;

  // 2. Bestimme den LLVM-Typ der Variable aus dem AST-Typknoten.
  //    Dein Semantic Analyzer hat sichergestellt, dass node.type existiert.
  std::cout << "[CodeGen] Determining LLVM type for variable: " << node.name
            << std::endl;

  // Check if type is null
  if (node.type == nullptr) {
    std::cout << "[CodeGen] ERROR: node.type is nullptr for variable: "
              << node.name << std::endl;
    throw std::runtime_error("Type is null for variable: " + node.name);
  }

  llvm::Type* varType = typeToLLVMType(*node.type);
  std::cout << "[CodeGen] LLVM type determined successfully" << std::endl;

  // 3. Erzeuge eine 'alloca'-Instruktion. Das reserviert Speicher
  //    auf dem Stack für die Variable am Anfang der Funktion.
  std::cout << "[CodeGen] Creating alloca for variable: " << node.name
            << std::endl;
  llvm::Value* alloca = builder->CreateAlloca(varType, nullptr, node.name);
  std::cout << "[CodeGen] Alloca created successfully" << std::endl;

  // 4. Speichere den Initialisierungswert in dem reservierten Speicher.
  std::cout << "[CodeGen] Storing initializer value in alloca" << std::endl;
  builder->CreateStore(initializerVal, alloca);
  std::cout << "[CodeGen] Store instruction created successfully" << std::endl;

  // 5. Merke dir den Speicherort der Variable in unserer "Symboltabelle".
  named_values[node.name] = alloca;
  std::cout << "[CodeGen] Variable " << node.name << " added to symbol table"
            << std::endl;

  // Eine Deklaration erzeugt keinen "Wert", daher geben wir nullptr zurück.
  return nullptr;
}