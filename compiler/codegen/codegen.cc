// compiler/codegen/codegen.cc
#include "codegen.hh"

#include <iostream>
#include <stdexcept>
#include <typeinfo>

#include "../parser/ast.hh"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"  // Nützlich zum Überprüfen des generierten Codes
#include "llvm/Support/TargetSelect.h"  // For getDefaultTargetTriple
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

void CodeGen::writeIRToFile(const std::string& filename) const {
  std::error_code EC;
  llvm::raw_fd_ostream file(filename, EC);
  if (EC) {
    std::cerr << "Error opening file " << filename << ": " << EC.message()
              << std::endl;
    return;
  }
  module->print(file, nullptr);
}

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

// --- Helper: Generate code with target type for casting ---
llvm::Value* CodeGen::codegenWithTargetType(ASTNode& node,
                                            llvm::Type* targetType) {
  std::cout << "[CodeGen] Generating node with target type casting"
            << std::endl;

  // Generate the base value
  llvm::Value* baseValue = codegen(node);
  if (!baseValue) {
    return nullptr;
  }

  // If types match, return as-is
  if (baseValue->getType() == targetType) {
    std::cout << "[CodeGen] Types already match, no casting needed"
              << std::endl;
    return baseValue;
  }

  // Cast integer types
  if (baseValue->getType()->isIntegerTy() && targetType->isIntegerTy()) {
    std::cout << "[CodeGen] Casting between integer types" << std::endl;

    auto* baseIntType = llvm::cast<llvm::IntegerType>(baseValue->getType());
    auto* targetIntType = llvm::cast<llvm::IntegerType>(targetType);

    if (baseIntType->getBitWidth() > targetIntType->getBitWidth()) {
      // Truncate (e.g., i32 -> i8)
      return builder->CreateTrunc(baseValue, targetType, "trunc");
    } else {
      // Extend (e.g., i8 -> i32)
      return builder->CreateSExt(baseValue, targetType, "sext");
    }
  }

  // Cast float types
  if (baseValue->getType()->isFloatingPointTy() &&
      targetType->isFloatingPointTy()) {
    std::cout << "[CodeGen] Casting between float types" << std::endl;
    return builder->CreateFPCast(baseValue, targetType, "fpcast");
  }

  // Integer to float
  if (baseValue->getType()->isIntegerTy() && targetType->isFloatingPointTy()) {
    std::cout << "[CodeGen] Casting integer to float" << std::endl;
    return builder->CreateSIToFP(baseValue, targetType, "sitofp");
  }

  // Float to integer
  if (baseValue->getType()->isFloatingPointTy() && targetType->isIntegerTy()) {
    std::cout << "[CodeGen] Casting float to integer" << std::endl;
    return builder->CreateFPToSI(baseValue, targetType, "fptosi");
  }

  std::cout << "[CodeGen] ERROR: Unsupported type casting" << std::endl;
  throw std::runtime_error("Unsupported type casting in codegenWithTargetType");
}

// --- Codegen Dispatch ---
llvm::Value* CodeGen::codegen(ASTNode& node) {
  std::cout << "[CodeGen] Dispatching node: " << node.toString() << std::endl;
  // Die Reihenfolge ist wichtig: von spezifisch zu allgemein
  if (auto* n = dynamic_cast<VarDeclNode*>(&node)) {
    std::cout << "[CodeGen] Processing VarDeclNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<IfStmtNode*>(&node)) {
    std::cout << "[CodeGen] Processing IfStmtNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<ExprStmtNode*>(&node)) {
    std::cout << "[CodeGen] Processing ExprStmtNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<FunctionCallExpr*>(&node)) {
    std::cout << "[CodeGen] Processing FunctionCallExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<BinaryExpr*>(&node)) {
    std::cout << "[CodeGen] Processing BinaryExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<Identifier*>(&node)) {
    std::cout << "[CodeGen] Processing Identifier" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<NumberLiteral*>(&node)) {
    std::cout << "[CodeGen] Processing NumberLiteral" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<StringLiteral*>(&node)) {
    std::cout << "[CodeGen] Processing StringLiteral" << std::endl;
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
    std::cout << "[CodeGen] Creating int constant: " << val
              << std::endl;  // TODO: Hier müsste man den Typ genauer bestimmen
                             // (i32, i64 etc.)
    // Fürs Erste nehmen wir immer i32.
    return llvm::ConstantInt::get(
        *context, llvm::APInt(32, static_cast<uint64_t>(val), true));
  }
}

llvm::Value* CodeGen::codegen(StringLiteral& node) {
  std::cout << "[CodeGen] Generating StringLiteral: \"" << node.value << "\""
            << std::endl;

  // Remove quotes from the string value
  std::string str_value = node.value;
  if (str_value.size() >= 2 && str_value.front() == '"' &&
      str_value.back() == '"') {
    str_value = str_value.substr(1, str_value.size() - 2);
  }

  // Create a global string constant
  llvm::Constant* strConstant =
      llvm::ConstantDataArray::getString(*context, str_value, true);

  // Create a global variable to hold the string
  llvm::GlobalVariable* globalStr = new llvm::GlobalVariable(
      *module, strConstant->getType(),
      true,  // isConstant
      llvm::GlobalValue::PrivateLinkage, strConstant, ".str");

  // Return a pointer to the string (i8*)
  std::vector<llvm::Value*> indices = {
      llvm::ConstantInt::get(*context, llvm::APInt(32, 0, false)),
      llvm::ConstantInt::get(*context, llvm::APInt(32, 0, false))};

  llvm::Value* strPtr = builder->CreateInBoundsGEP(
      strConstant->getType(), globalStr, indices, "str.ptr");

  std::cout << "[CodeGen] String constant created successfully" << std::endl;
  return strPtr;
}

// --- Codegen für Statements ---
llvm::Value* CodeGen::codegen(VarDeclNode& node) {
  std::cout << "[CodeGen] Generating VarDeclNode: " << node.name << std::endl;

  // Check if type is null
  if (node.type == nullptr) {
    std::cout << "[CodeGen] ERROR: node.type is nullptr for variable: "
              << node.name << std::endl;
    throw std::runtime_error("Type is null for variable: " + node.name);
  }

  // 2. Bestimme den LLVM-Typ der Variable aus dem AST-Typknoten.
  std::cout << "[CodeGen] Determining LLVM type for variable: " << node.name
            << std::endl;
  llvm::Type* varType = typeToLLVMType(*node.type);
  std::cout << "[CodeGen] LLVM type determined successfully" << std::endl;

  // 1. Generiere den Code für den Initialisierungswert mit dem richtigen Typ.
  std::cout << "[CodeGen] Generating initializer for variable: " << node.name
            << std::endl;
  llvm::Value* initializerVal =
      codegenWithTargetType(*node.initializer, varType);
  std::cout << "[CodeGen] Initializer generated successfully" << std::endl;

  // 3. Erzeuge eine 'alloca'-Instruktion.
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
  variable_types[node.name] =
      varType;  // Store the type for opaque pointer support
  std::cout << "[CodeGen] Variable " << node.name << " added to symbol table"
            << std::endl;

  return nullptr;
}

llvm::Value* CodeGen::codegen(Identifier& node) {
  std::cout << "[CodeGen] Generating Identifier: " << node.name << std::endl;
  // 1. Suche die Variable in unserer Symboltabelle.
  auto it = named_values.find(node.name);
  if (it == named_values.end()) {
    throw std::runtime_error("CodeGen: Unknown variable name '" + node.name +
                             "'.");
  }

  // it->second ist der Zeiger auf den Stack-Speicher (das Ergebnis von alloca).
  llvm::Value* var_ptr = it->second;  // 2. Erzeuge eine 'load'-Instruktion, um
                                      // den Wert aus dem Speicher zu lesen.
  // Look up the variable type from our stored types map
  auto type_it = variable_types.find(node.name);
  if (type_it == variable_types.end()) {
    throw std::runtime_error("CodeGen: Unknown variable type for '" +
                             node.name + "'.");
  }

  llvm::Type* var_type = type_it->second;
  return builder->CreateLoad(var_type, var_ptr, node.name + ".load");
}

llvm::Value* CodeGen::codegen(BinaryExpr& node) {
  std::cout << "[CodeGen] Generating BinaryExpr" << std::endl;
  // 1. Rekursiv den Code für die linke und rechte Seite generieren.
  llvm::Value* L = codegen(*node.left);
  llvm::Value* R = codegen(*node.right);

  if (!L || !R) {
    return nullptr;
  }

  // 2. Type promotion: if one operand is float, promote both to float
  if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
    // Promote both to floating point
    if (L->getType()->isIntegerTy()) {
      L = builder->CreateSIToFP(L, builder->getDoubleTy(), "int2fp");
    }
    if (R->getType()->isIntegerTy()) {
      R = builder->CreateSIToFP(R, builder->getDoubleTy(), "int2fp");
    }  // Generate floating point operations
    switch (node.op.type) {
      case TokenType::TOKEN_PLUS:
        return builder->CreateFAdd(L, R, "fadd.tmp");
      case TokenType::TOKEN_MINUS:
        return builder->CreateFSub(L, R, "fsub.tmp");
      case TokenType::TOKEN_STAR:
        return builder->CreateFMul(L, R, "fmul.tmp");
      case TokenType::TOKEN_SLASH:
        return builder->CreateFDiv(L, R, "fdiv.tmp");
      case TokenType::TOKEN_EQUAL_EQUAL:
        return builder->CreateFCmpOEQ(L, R, "fcmp.tmp");
      default:
        throw std::runtime_error("CodeGen: Unknown binary operator for float.");
    }
  } else {  // Both operands are integers - generate integer operations
    switch (node.op.type) {
      case TokenType::TOKEN_PLUS:
        return builder->CreateAdd(L, R, "add.tmp");
      case TokenType::TOKEN_MINUS:
        return builder->CreateSub(L, R, "sub.tmp");
      case TokenType::TOKEN_STAR:
        return builder->CreateMul(L, R, "mul.tmp");
      case TokenType::TOKEN_SLASH:
        return builder->CreateSDiv(L, R,
                                   "div.tmp");  // SDiv für Signed Integers
      case TokenType::TOKEN_EQUAL_EQUAL:
        return builder->CreateICmpEQ(L, R, "icmp.tmp");
      default:
        throw std::runtime_error(
            "CodeGen: Unknown binary operator for integer.");
    }
  }
}

llvm::Value* CodeGen::codegen(IfStmtNode& node) {
  std::cout << "[CodeGen] Generating IfStmtNode" << std::endl;

  // Generate condition
  llvm::Value* condition_val = codegen(*node.condition);
  if (!condition_val) return nullptr;

  // Get current function
  llvm::Function* current_function = builder->GetInsertBlock()->getParent();

  // Create basic blocks
  llvm::BasicBlock* then_block =
      llvm::BasicBlock::Create(*context, "if.then", current_function);
  llvm::BasicBlock* else_block = nullptr;
  llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(*context, "if.end");

  if (!node.else_body.empty()) {
    else_block = llvm::BasicBlock::Create(*context, "if.else");
  }

  // Branch based on condition
  if (else_block) {
    builder->CreateCondBr(condition_val, then_block, else_block);
  } else {
    builder->CreateCondBr(condition_val, then_block, merge_block);
  }

  // Generate then block
  builder->SetInsertPoint(then_block);
  for (const auto& stmt : node.then_body) {
    codegen(*stmt);
  }
  if (!builder->GetInsertBlock()->getTerminator()) {
    builder->CreateBr(merge_block);
  }
  // Generate else block (if present)
  if (else_block) {
    else_block->insertInto(current_function);
    builder->SetInsertPoint(else_block);
    for (const auto& stmt : node.else_body) {
      codegen(*stmt);
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
      builder->CreateBr(merge_block);
    }
  }

  // Continue with merge block
  merge_block->insertInto(current_function);
  builder->SetInsertPoint(merge_block);
  return nullptr;  // If statements don't return values
}

llvm::Value* CodeGen::codegen(ExprStmtNode& node) {
  std::cout << "[CodeGen] Generating ExprStmtNode" << std::endl;

  // For expression statements, we just evaluate the expression
  // The result value is not used, but the expression may have side effects
  llvm::Value* result = codegen(*node.expression);

  return result;  // Return the value in case it's needed
}

llvm::Value* CodeGen::codegen(FunctionCallExpr& node) {
  std::cout << "[CodeGen] Generating FunctionCallExpr: " << node.function_name
            << std::endl;

  if (node.function_name == "print") {
    // Declare printf if not already declared
    llvm::Function* printf_func = module->getFunction("printf");
    if (!printf_func) {
      // printf has signature: int printf(const char* format, ...)
      llvm::FunctionType* printf_type = llvm::FunctionType::get(
          builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context)},
          true  // vararg
      );
      printf_func = llvm::Function::Create(
          printf_type, llvm::Function::ExternalLinkage, "printf", module.get());
    }

    if (node.arguments.size() != 1) {
      throw std::runtime_error("print() expects exactly one argument");
    }

    // Generate argument
    llvm::Value* arg = codegen(*node.arguments[0]);
    if (!arg) return nullptr;

    // Create format string for the argument type
    llvm::Value* format_str = nullptr;
    if (arg->getType()->isIntegerTy()) {
      // Integer argument - use "%d\n" format
      llvm::Constant* format_const =
          llvm::ConstantDataArray::getString(*context, "%d\n", true);
      llvm::GlobalVariable* format_global = new llvm::GlobalVariable(
          *module, format_const->getType(), true,
          llvm::GlobalValue::PrivateLinkage, format_const, ".str.fmt.int");
      format_str = builder->CreateInBoundsGEP(
          format_const->getType(), format_global,
          {builder->getInt32(0), builder->getInt32(0)}, "fmt.ptr");
    } else if (arg->getType()->isPointerTy()) {
      // String argument - use "%s\n" format
      llvm::Constant* format_const =
          llvm::ConstantDataArray::getString(*context, "%s\n", true);
      llvm::GlobalVariable* format_global = new llvm::GlobalVariable(
          *module, format_const->getType(), true,
          llvm::GlobalValue::PrivateLinkage, format_const, ".str.fmt.str");
      format_str = builder->CreateInBoundsGEP(
          format_const->getType(), format_global,
          {builder->getInt32(0), builder->getInt32(0)}, "fmt.ptr");
    } else {
      throw std::runtime_error("Unsupported argument type for print()");
    }

    // Call printf
    return builder->CreateCall(printf_func, {format_str, arg}, "printf.call");
  }

  throw std::runtime_error("Unknown function: " + node.function_name);
}

// --- Integrated Compilation Methods (like Kaleidoscope) ---

bool CodeGen::initializeLLVMTargets() {
  // Initialize all targets for code generation
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  std::cout << "[CodeGen] LLVM targets initialized successfully" << std::endl;
  return true;
}

bool CodeGen::compileToObjectFile(const std::string& filename) const {
  std::cout << "[CodeGen] Compiling to object file: " << filename << std::endl;

  // Get the target triple for the current system  // Use Windows x64 target
  // triple for now
  std::string targetTripleStr = "x86_64-pc-windows-msvc";
  llvm::Triple targetTriple(targetTripleStr);
  module->setTargetTriple(targetTriple);
  std::string error;
  auto target = llvm::TargetRegistry::lookupTarget(targetTripleStr, error);

  if (!target) {
    std::cerr << "[CodeGen] Error: " << error << std::endl;
    return false;
  }
  auto CPU = "generic";
  auto features = "";

  llvm::TargetOptions opt;
  auto relocationModel = llvm::Reloc::PIC_;
  auto targetMachine = target->createTargetMachine(targetTriple, CPU, features,
                                                   opt, relocationModel);

  module->setDataLayout(targetMachine->createDataLayout());

  std::error_code EC;
  llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);

  if (EC) {
    std::cerr << "[CodeGen] Could not open file: " << EC.message() << std::endl;
    return false;
  }

  llvm::legacy::PassManager pass;
  auto fileType = llvm::CodeGenFileType::ObjectFile;

  if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
    std::cerr << "[CodeGen] TargetMachine can't emit a file of this type"
              << std::endl;
    return false;
  }

  pass.run(*module);
  dest.flush();

  std::cout << "[CodeGen] Successfully wrote object file: " << filename
            << std::endl;
  return true;
}

bool CodeGen::compileToExecutable(const std::string& objectFilename,
                                  const std::string& executableFilename) const {
  std::cout << "[CodeGen] Linking object file to executable..." << std::endl;

  // Use system linker (could be ld, link.exe, etc.)
  std::string linkCmd;

#ifdef _WIN32
  // Windows: Use link.exe or lld-link
  linkCmd =
      "clang \"" + objectFilename + "\" -o \"" + executableFilename + "\"";
#else
  // Unix/Linux: Use ld or clang
  linkCmd =
      "clang \"" + objectFilename + "\" -o \"" + executableFilename + "\"";
#endif

  std::cout << "[CodeGen] Running linker: " << linkCmd << std::endl;

  int result = std::system(linkCmd.c_str());
  if (result == 0) {
    std::cout << "[CodeGen] Successfully linked executable: "
              << executableFilename << std::endl;
    return true;
  } else {
    std::cerr << "[CodeGen] Linking failed with exit code: " << result
              << std::endl;
    return false;
  }
}