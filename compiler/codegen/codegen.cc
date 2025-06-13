// compiler/codegen/codegen.cc
#include "codegen.hh"

#include <iostream>
#include <stdexcept>
#include <typeinfo>

#include "../parser/ast.hh"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"  // Nützlich zum Überprüfen des generierten Codes
#include "llvm/Support/TargetSelect.h"  // For getDefaultTargetTriple
#include "llvm/Support/raw_ostream.h"

CodeGen::CodeGen() {
  context = std::make_unique<llvm::LLVMContext>();
  module = std::make_unique<llvm::Module>("MyLoomModule", *context);
  builder = std::make_unique<llvm::IRBuilder<>>(*context);
  current_function = nullptr;  // Initialize current function context
}

void CodeGen::generate(const std::vector<std::unique_ptr<StmtNode>>& ast) {
  std::cout << "[CodeGen] Starting code generation..." << std::endl;

  // 1. Check if there's a main function in the AST
  bool has_main_function = false;
  for (const auto& stmt : ast) {
    if (auto* func_decl = dynamic_cast<FunctionDeclNode*>(stmt.get())) {
      if (func_decl->name == "main") {
        has_main_function = true;
        break;
      }
    }
  }

  // 2. Error if no main function found
  if (!has_main_function) {
    throw std::runtime_error(
        "Error: No 'main' function found in program. Every Loom program must "
        "have a main function.");
  }

  std::cout << "[CodeGen] Found main function in AST" << std::endl;

  // 3. Generate code for all statements (including main function)
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
  // 4. Verify all functions in the module
  std::cout << "[CodeGen] Verifying function..." << std::endl;
  for (auto& function : *module) {
    llvm::verifyFunction(function);
  }
  std::cout << "[CodeGen] Function verification completed" << std::endl;

  // 5. Generate Windows entry point for freestanding executable
  generateEntryPoint();

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

void CodeGen::generateEntryPoint() {
  TargetPlatform platform = detectTargetPlatform();

  if (platform == TargetPlatform::Windows) {
    std::cout << "[CodeGen] Generating Windows entry point..." << std::endl;

    // Create mainCRTStartup function that calls our main function
    llvm::FunctionType* entryType = llvm::FunctionType::get(
        builder->getVoidTy(),  // void return (Windows entry points return void)
        {},                    // no parameters
        false                  // not variadic
    );

    llvm::Function* entryFunc = llvm::Function::Create(
        entryType, llvm::Function::ExternalLinkage,
        "mainCRTStartup",  // Windows expects this entry point
        module.get());

    // Create entry block
    llvm::BasicBlock* entryBlock =
        llvm::BasicBlock::Create(*context, "entry", entryFunc);
    builder->SetInsertPoint(entryBlock);

    // Get the main function
    llvm::Function* mainFunc = module->getFunction("main");
    if (!mainFunc) {
      throw std::runtime_error(
          "Cannot find main function for entry point generation");
    }

    // Call main function
    llvm::Value* mainResult = builder->CreateCall(mainFunc, {}, "main.result");

    // Exit with the result from main using ExitProcess
    llvm::Function* exitProcess = module->getFunction("ExitProcess");
    if (!exitProcess) {
      llvm::FunctionType* exitProcessType =
          llvm::FunctionType::get(builder->getVoidTy(),     // void return
                                  {builder->getInt32Ty()},  // UINT uExitCode
                                  false);
      exitProcess = llvm::Function::Create(exitProcessType,
                                           llvm::Function::ExternalLinkage,
                                           "ExitProcess", module.get());
    }
    builder->CreateCall(exitProcess, {mainResult});
    builder->CreateUnreachable();

  } else if (platform == TargetPlatform::Linux ||
             platform == TargetPlatform::MacOS) {
    std::cout << "[CodeGen] Generating Unix-style entry point..." << std::endl;

    // For Linux/macOS, create _start function that calls main and then exit
    // syscall
    llvm::FunctionType* entryType =
        llvm::FunctionType::get(builder->getVoidTy(),  // void return
                                {},                    // no parameters
                                false                  // not variadic
        );

    llvm::Function* entryFunc =
        llvm::Function::Create(entryType, llvm::Function::ExternalLinkage,
                               "_start",  // Unix expects this entry point
                               module.get());

    // Create entry block
    llvm::BasicBlock* entryBlock =
        llvm::BasicBlock::Create(*context, "entry", entryFunc);
    builder->SetInsertPoint(entryBlock);

    // Get the main function
    llvm::Function* mainFunc = module->getFunction("main");
    if (!mainFunc) {
      throw std::runtime_error(
          "Cannot find main function for entry point generation");
    }

    // Call main function
    llvm::Value* mainResult = builder->CreateCall(mainFunc, {}, "main.result");

    // Exit with the result from main using syscall
    std::vector<llvm::Value*> exitArgs = {mainResult};
    if (platform == TargetPlatform::Linux) {
      generateLinuxSyscall("exit", exitArgs);
    } else {
      generateMacOSSyscall("exit", exitArgs);
    }
    builder->CreateUnreachable();
  }

  std::cout << "[CodeGen] Entry point generation completed" << std::endl;
}

// Platform detection based on target triple
TargetPlatform CodeGen::detectTargetPlatform() const {
  llvm::Triple targetTriple(module->getTargetTriple());
  std::string targetTripleStr = targetTriple.str();
  std::cout << "[CodeGen] Detecting platform from target triple: "
            << targetTripleStr << std::endl;

  if (targetTripleStr.find("windows") != std::string::npos ||
      targetTripleStr.find("win32") != std::string::npos ||
      targetTripleStr.find("msvc") != std::string::npos) {
    return TargetPlatform::Windows;
  } else if (targetTripleStr.find("linux") != std::string::npos) {
    return TargetPlatform::Linux;
  } else if (targetTripleStr.find("apple") != std::string::npos ||
             targetTripleStr.find("darwin") != std::string::npos ||
             targetTripleStr.find("macos") != std::string::npos) {
    return TargetPlatform::MacOS;
  }
  // Fallback: detect from preprocessor macros at compile time
#ifdef _WIN32
  std::cout << "[CodeGen] Defaulting to Windows platform" << std::endl;
  return TargetPlatform::Windows;
#elif defined(__linux__)
  std::cout << "[CodeGen] Defaulting to Linux platform" << std::endl;
  return TargetPlatform::Linux;
#elif defined(__APPLE__)
  std::cout << "[CodeGen] Defaulting to macOS platform" << std::endl;
  return TargetPlatform::MacOS;
#else
  std::cout << "[CodeGen] Unknown target platform" << std::endl;
  return TargetPlatform::Unknown;
#endif
}

// Linux syscall implementation using inline assembly
llvm::Value* CodeGen::generateLinuxSyscall(const std::string& name,
                                           std::vector<llvm::Value*>& args) {
  std::cout << "[CodeGen] Generating Linux syscall: " << name << std::endl;

  if (name == "print" && args.size() >= 1) {
    // Linux write syscall: sys_write = 1
    // int64_t write(int fd, const void *buf, size_t count)

    // Create the inline assembly for Linux x86_64 syscall
    std::string asmStr = "syscall";
    std::string constraintStr =
        "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";  // Prepare syscall
                                                     // arguments
    std::vector<llvm::Value*> asmArgs;
    asmArgs.push_back(
        llvm::ConstantInt::get(builder->getInt64Ty(), 1));  // sys_write
    asmArgs.push_back(
        llvm::ConstantInt::get(builder->getInt64Ty(), 1));  // stdout fd
    asmArgs.push_back(args[0]);                             // buffer

    // Calculate string length - for string literals we can get the length at
    // compile time
    llvm::Value* length;
    if (llvm::GlobalVariable* globalVar =
            llvm::dyn_cast<llvm::GlobalVariable>(args[0])) {
      // This is a string literal - get compile-time length
      if (llvm::ConstantDataArray* stringData =
              llvm::dyn_cast<llvm::ConstantDataArray>(
                  globalVar->getInitializer())) {
        // Get length excluding null terminator
        uint64_t str_length = stringData->getNumElements() - 1;
        length = llvm::ConstantInt::get(builder->getInt64Ty(), str_length);
      } else {
        // Fallback for unknown string format
        length = llvm::ConstantInt::get(builder->getInt64Ty(), 50);
      }
    } else {
      // For non-literal strings, use a conservative default
      length = llvm::ConstantInt::get(builder->getInt64Ty(), 100);
    }
    asmArgs.push_back(length);

    // Create inline assembly call
    llvm::FunctionType* asmType = llvm::FunctionType::get(
        builder->getInt64Ty(),
        {builder->getInt64Ty(), builder->getInt64Ty(),
         llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
        false);

    llvm::InlineAsm* inlineAsm =
        llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
    return builder->CreateCall(inlineAsm, asmArgs, "syscall.result");

  } else if (name == "exit" && args.size() >= 1) {
    // Linux exit syscall: sys_exit = 60

    std::string asmStr = "syscall";
    std::string constraintStr = "={rax},0,{rdi},~{rcx},~{r11}";

    std::vector<llvm::Value*> asmArgs;
    asmArgs.push_back(
        llvm::ConstantInt::get(builder->getInt64Ty(), 60));  // sys_exit
    asmArgs.push_back(args[0]);                              // exit code

    llvm::FunctionType* asmType = llvm::FunctionType::get(
        builder->getInt64Ty(), {builder->getInt64Ty(), builder->getInt64Ty()},
        false);

    llvm::InlineAsm* inlineAsm =
        llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
    return builder->CreateCall(inlineAsm, asmArgs, "syscall.result");

  } else if (name == "syscall" && args.size() >= 1) {
    // Generic Linux syscall
    std::string asmStr = "syscall";
    std::string constraintStr = "={rax},0";

    std::vector<llvm::Type*> argTypes = {builder->getInt64Ty()};
    std::vector<llvm::Value*> asmArgs = {args[0]};  // syscall number

    // Add up to 6 syscall arguments (Linux x86_64 calling convention)
    const char* regs[] = {"{rdi}", "{rsi}", "{rdx}", "{r10}", "{r8}", "{r9}"};
    for (size_t i = 1; i < args.size() && i <= 6; ++i) {
      constraintStr += "," + std::string(regs[i - 1]);
      argTypes.push_back(builder->getInt64Ty());
      asmArgs.push_back(args[i]);
    }

    constraintStr += ",~{rcx},~{r11}";

    llvm::FunctionType* asmType =
        llvm::FunctionType::get(builder->getInt64Ty(), argTypes, false);
    llvm::InlineAsm* inlineAsm =
        llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
    return builder->CreateCall(inlineAsm, asmArgs, "syscall.result");
  }

  throw std::runtime_error("Unsupported Linux syscall: " + name);
}

// macOS syscall implementation using inline assembly
llvm::Value* CodeGen::generateMacOSSyscall(const std::string& name,
                                           std::vector<llvm::Value*>& args) {
  std::cout << "[CodeGen] Generating macOS syscall: " << name << std::endl;

  if (name == "print" && args.size() >= 1) {
    // macOS write syscall: 0x2000004 (BSD syscall numbers are offset by
    // 0x2000000)

    std::string asmStr = "syscall";
    std::string constraintStr = "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";
    std::vector<llvm::Value*> asmArgs;
    asmArgs.push_back(
        llvm::ConstantInt::get(builder->getInt64Ty(), 0x2000004));  // sys_write
    asmArgs.push_back(
        llvm::ConstantInt::get(builder->getInt64Ty(), 1));  // stdout fd
    asmArgs.push_back(args[0]);                             // buffer

    // Calculate string length - for string literals we can get the length at
    // compile time
    llvm::Value* length;
    if (llvm::GlobalVariable* globalVar =
            llvm::dyn_cast<llvm::GlobalVariable>(args[0])) {
      // This is a string literal - get compile-time length
      if (llvm::ConstantDataArray* stringData =
              llvm::dyn_cast<llvm::ConstantDataArray>(
                  globalVar->getInitializer())) {
        // Get length excluding null terminator
        uint64_t str_length = stringData->getNumElements() - 1;
        length = llvm::ConstantInt::get(builder->getInt64Ty(), str_length);
      } else {
        // Fallback for unknown string format
        length = llvm::ConstantInt::get(builder->getInt64Ty(), 50);
      }
    } else {
      // For non-literal strings, use a conservative default
      length = llvm::ConstantInt::get(builder->getInt64Ty(), 100);
    }
    asmArgs.push_back(length);

    llvm::FunctionType* asmType = llvm::FunctionType::get(
        builder->getInt64Ty(),
        {builder->getInt64Ty(), builder->getInt64Ty(),
         llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
        false);

    llvm::InlineAsm* inlineAsm =
        llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
    return builder->CreateCall(inlineAsm, asmArgs, "syscall.result");

  } else if (name == "exit" && args.size() >= 1) {
    // macOS exit syscall: 0x2000001

    std::string asmStr = "syscall";
    std::string constraintStr = "={rax},0,{rdi},~{rcx},~{r11}";

    std::vector<llvm::Value*> asmArgs;
    asmArgs.push_back(
        llvm::ConstantInt::get(builder->getInt64Ty(), 0x2000001));  // sys_exit
    asmArgs.push_back(args[0]);                                     // exit code

    llvm::FunctionType* asmType = llvm::FunctionType::get(
        builder->getInt64Ty(), {builder->getInt64Ty(), builder->getInt64Ty()},
        false);

    llvm::InlineAsm* inlineAsm =
        llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
    return builder->CreateCall(inlineAsm, asmArgs, "syscall.result");

  } else if (name == "syscall" && args.size() >= 1) {
    // Generic macOS syscall
    std::string asmStr = "syscall";
    std::string constraintStr = "={rax},0";

    std::vector<llvm::Type*> argTypes = {builder->getInt64Ty()};
    std::vector<llvm::Value*> asmArgs = {args[0]};  // syscall number

    // Add up to 6 syscall arguments (macOS x86_64 calling convention)
    const char* regs[] = {"{rdi}", "{rsi}", "{rdx}", "{r10}", "{r8}", "{r9}"};
    for (size_t i = 1; i < args.size() && i <= 6; ++i) {
      constraintStr += "," + std::string(regs[i - 1]);
      argTypes.push_back(builder->getInt64Ty());
      asmArgs.push_back(args[i]);
    }

    constraintStr += ",~{rcx},~{r11}";

    llvm::FunctionType* asmType =
        llvm::FunctionType::get(builder->getInt64Ty(), argTypes, false);
    llvm::InlineAsm* inlineAsm =
        llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
    return builder->CreateCall(inlineAsm, asmArgs, "syscall.result");
  }

  throw std::runtime_error("Unsupported macOS syscall: " + name);
}

// Windows syscall implementation using Windows API calls
llvm::Value* CodeGen::generateWindowsSyscall(const std::string& name,
                                             std::vector<llvm::Value*>& args) {
  std::cout << "[CodeGen] Generating Windows syscall: " << name << std::endl;
  if (name == "print" && args.size() >= 1) {
    // Use WriteFile API for printing with automatic newline
    llvm::Function* writeFile = module->getFunction("WriteFile");
    if (!writeFile) {
      llvm::FunctionType* writeFileType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // BOOL (treated as i32)
          {
              llvm::PointerType::getUnqual(*context),  // HANDLE
              llvm::PointerType::getUnqual(*context),  // LPCVOID (buffer)
              builder->getInt32Ty(),                   // DWORD (size)
              llvm::PointerType::getUnqual(
                  *context),                          // LPDWORD (bytes written)
              llvm::PointerType::getUnqual(*context)  // LPOVERLAPPED
          },
          false);
      writeFile =
          llvm::Function::Create(writeFileType, llvm::Function::ExternalLinkage,
                                 "WriteFile", module.get());
    }

    llvm::Function* getStdHandle = module->getFunction("GetStdHandle");
    if (!getStdHandle) {
      llvm::FunctionType* getStdHandleType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context),  // HANDLE
          {builder->getInt32Ty()},                 // DWORD
          false);
      getStdHandle = llvm::Function::Create(getStdHandleType,
                                            llvm::Function::ExternalLinkage,
                                            "GetStdHandle", module.get());
    }

    // Get stdout handle (STD_OUTPUT_HANDLE = -11)
    llvm::Value* stdoutHandle = builder->CreateCall(
        getStdHandle, {builder->getInt32(-11)}, "stdout.handle");

    // Calculate string length - for string literals we can get the length at
    // compile time
    llvm::Value* bufferSize;
    if (llvm::GlobalVariable* globalVar =
            llvm::dyn_cast<llvm::GlobalVariable>(args[0])) {
      // This is a string literal - get compile-time length
      if (llvm::ConstantDataArray* stringData =
              llvm::dyn_cast<llvm::ConstantDataArray>(
                  globalVar->getInitializer())) {
        // Get length excluding null terminator
        uint64_t length = stringData->getNumElements() - 1;
        bufferSize = builder->getInt32(static_cast<uint32_t>(length));
      } else {
        // Fallback for unknown string format
        bufferSize = builder->getInt32(50);  // Conservative estimate
      }
    } else {
      // For non-literal strings, use a conservative default
      // In a full implementation, we'd track string lengths in the type system
      bufferSize = builder->getInt32(100);
    }

    llvm::Value* bytesWritten =
        builder->CreateAlloca(builder->getInt32Ty(), nullptr, "bytes.written");

    // First write the string content
    llvm::Value* result1 = builder->CreateCall(
        writeFile,
        {stdoutHandle, args[0], bufferSize, bytesWritten,
         llvm::ConstantPointerNull::get(
             llvm::PointerType::getUnqual(*context))},
        "write.result");  // Then write a newline for $$print (but not for
                          // direct $$syscall)
    llvm::Constant* newlineStr = builder->CreateGlobalString("\n", "newline");
    llvm::Value* newlinePtr = builder->CreatePointerCast(
        newlineStr, llvm::PointerType::getUnqual(*context));
    llvm::Value* newlineSize = builder->getInt32(1);
    llvm::Value* bytesWritten2 = builder->CreateAlloca(
        builder->getInt32Ty(), nullptr, "bytes.written.newline");

    // Write the newline (don't need to store result)
    builder->CreateCall(writeFile,
                        {stdoutHandle, newlinePtr, newlineSize, bytesWritten2,
                         llvm::ConstantPointerNull::get(
                             llvm::PointerType::getUnqual(*context))},
                        "write.newline.result");

    return result1;  // Return the result of the main write operation

  } else if (name == "exit" && args.size() >= 1) {
    // Use ExitProcess API as before
    llvm::Function* exitProcess = module->getFunction("ExitProcess");
    if (!exitProcess) {
      llvm::FunctionType* exitProcessType = llvm::FunctionType::get(
          builder->getVoidTy(), {builder->getInt32Ty()}, false);
      exitProcess = llvm::Function::Create(exitProcessType,
                                           llvm::Function::ExternalLinkage,
                                           "ExitProcess", module.get());
    }

    builder->CreateCall(exitProcess, {args[0]});
    return nullptr;

  } else if (name == "syscall" && args.size() >= 1) {
    // For Windows, we map common syscall numbers to Windows API calls
    if (auto* const_op = llvm::dyn_cast<llvm::ConstantInt>(args[0])) {
      int64_t op_value = const_op->getSExtValue();

      if (op_value == 1 && args.size() >= 4) {
        // Write operation - redirect to our print implementation
        std::vector<llvm::Value*> printArgs = {args[2]};  // buffer
        return generateWindowsSyscall("print", printArgs);
      } else if (op_value == 60 && args.size() >= 2) {
        // Exit operation - redirect to our exit implementation
        std::vector<llvm::Value*> exitArgs = {args[1]};  // exit code
        return generateWindowsSyscall("exit", exitArgs);
      }
    }

    // Fallback: return error for unsupported syscalls
    return llvm::ConstantInt::get(builder->getInt64Ty(), -1);
  }

  throw std::runtime_error("Unsupported Windows syscall: " + name);
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
  if (auto* n = dynamic_cast<WhileStmtNode*>(&node)) {
    std::cout << "[CodeGen] Processing WhileStmtNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<ExprStmtNode*>(&node)) {
    std::cout << "[CodeGen] Processing ExprStmtNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<AssignmentExpr*>(&node)) {
    std::cout << "[CodeGen] Processing AssignmentExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<FunctionCallExpr*>(&node)) {
    std::cout << "[CodeGen] Processing FunctionCallExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<BuiltinCallExpr*>(&node)) {
    std::cout << "[CodeGen] Processing BuiltinCallExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<FunctionDeclNode*>(&node)) {
    std::cout << "[CodeGen] Processing FunctionDeclNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<ReturnStmtNode*>(&node)) {
    std::cout << "[CodeGen] Processing ReturnStmtNode" << std::endl;
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
      case TokenType::TOKEN_LESS:
        return builder->CreateFCmpOLT(L, R, "fcmp.tmp");
      case TokenType::TOKEN_LESS_EQUAL:
        return builder->CreateFCmpOLE(L, R, "fcmp.tmp");
      case TokenType::TOKEN_GREATER:
        return builder->CreateFCmpOGT(L, R, "fcmp.tmp");
      case TokenType::TOKEN_GREATER_EQUAL:
        return builder->CreateFCmpOGE(L, R, "fcmp.tmp");
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
      case TokenType::TOKEN_LESS:
        return builder->CreateICmpSLT(L, R, "icmp.tmp");
      case TokenType::TOKEN_LESS_EQUAL:
        return builder->CreateICmpSLE(L, R, "icmp.tmp");
      case TokenType::TOKEN_GREATER:
        return builder->CreateICmpSGT(L, R, "icmp.tmp");
      case TokenType::TOKEN_GREATER_EQUAL:
        return builder->CreateICmpSGE(L, R, "icmp.tmp");
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

llvm::Value* CodeGen::codegen(WhileStmtNode& node) {
  std::cout << "[CodeGen] Generating WhileStmtNode" << std::endl;

  // Get current function
  llvm::Function* current_function = builder->GetInsertBlock()->getParent();

  // Create basic blocks
  llvm::BasicBlock* header_block =
      llvm::BasicBlock::Create(*context, "loop.header", current_function);

  // Body block (ohne function parameter, wird später eingefügt):
  llvm::BasicBlock* body_block =
      llvm::BasicBlock::Create(*context, "loop.body");

  // Exit block (ohne function parameter, wird später eingefügt):
  llvm::BasicBlock* exit_block =
      llvm::BasicBlock::Create(*context, "loop.exit");

  // 1. Springe vom aktuellen Block zum Header
  builder->CreateBr(header_block);

  // 2. Header - Bedingung evaluieren
  builder->SetInsertPoint(header_block);
  llvm::Value* condition_val = codegen(*node.condition);
  if (!condition_val) return nullptr;

  // Conditional Branch: wenn true → body, wenn false → exit
  builder->CreateCondBr(condition_val, body_block, exit_block);

  // 3. Body - Statements ausführen und zurück zum Header
  body_block->insertInto(current_function);
  builder->SetInsertPoint(body_block);

  // Führe alle Statements im Body aus
  for (const auto& stmt : node.body) {
    codegen(*stmt);
  }

  // Springe zurück zum Header (das ist der Schlüssel!)
  if (!builder->GetInsertBlock()->getTerminator()) {
    builder->CreateBr(header_block);  // ← Zurück zum Header!
  }

  // 4. Exit - Nach der Schleife weitermachen
  exit_block->insertInto(current_function);
  builder->SetInsertPoint(exit_block);

  return nullptr;  // While statements don't return values
}

llvm::Value* CodeGen::codegen(ExprStmtNode& node) {
  std::cout << "[CodeGen] Generating ExprStmtNode" << std::endl;

  // For expression statements, we just evaluate the expression
  // The result value is not used, but the expression may have side effects
  llvm::Value* result = codegen(*node.expression);
  return result;  // Return the value in case it's needed
}

llvm::Value* CodeGen::codegen(AssignmentExpr& node) {
  std::cout << "[CodeGen] Generating AssignmentExpr: " << node.name
            << std::endl;

  // Generate the value to assign
  llvm::Value* value = codegen(*node.value);
  if (!value) return nullptr;

  // Find the variable in the symbol table
  auto it = named_values.find(node.name);
  if (it == named_values.end()) {
    std::cout << "[CodeGen] ERROR: Undefined variable: " << node.name
              << std::endl;
    throw std::runtime_error("Undefined variable: " + node.name);
  }

  llvm::Value* variable_ptr = it->second;

  // Store the new value
  builder->CreateStore(value, variable_ptr);

  std::cout << "[CodeGen] Assignment completed for variable: " << node.name
            << std::endl;
  return value;  // Return the assigned value
}

llvm::Value* CodeGen::codegen(FunctionCallExpr& node) {
  std::cout << "[CodeGen] Generating FunctionCallExpr: " << node.function_name
            << std::endl;

  // Handle built-in functions
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

  // Handle user-defined functions
  llvm::Function* target_func = module->getFunction(node.function_name);
  if (!target_func) {
    std::cout << "[CodeGen] ERROR: Function '" << node.function_name
              << "' not found in module" << std::endl;
    throw std::runtime_error("Function not found: " + node.function_name);
  }

  // Generate arguments
  std::vector<llvm::Value*> args;
  for (auto& arg_node : node.arguments) {
    llvm::Value* arg_value = codegen(*arg_node);
    if (!arg_value) {
      std::cout
          << "[CodeGen] ERROR: Failed to generate argument for function call"
          << std::endl;
      return nullptr;
    }
    args.push_back(arg_value);
  }

  // Verify argument count matches function signature
  if (args.size() != target_func->arg_size()) {
    std::cout << "[CodeGen] ERROR: Argument count mismatch. Expected "
              << target_func->arg_size() << ", got " << args.size()
              << std::endl;
    throw std::runtime_error("Argument count mismatch for function: " +
                             node.function_name);
  }

  // Create function call
  std::cout << "[CodeGen] Creating call to function: " << node.function_name
            << " with " << args.size() << " arguments" << std::endl;
  return builder->CreateCall(target_func, args, node.function_name + ".call");
}

llvm::Value* CodeGen::codegen(BuiltinCallExpr& node) {
  std::cout << "[CodeGen] Generating BuiltinCallExpr: $$" << node.builtin_name
            << std::endl;

  // Detect target platform for cross-platform support
  TargetPlatform platform = detectTargetPlatform();

  // Generate arguments
  std::vector<llvm::Value*> args;
  for (auto& arg : node.arguments) {
    llvm::Value* argValue = codegen(*arg);
    if (!argValue) return nullptr;
    args.push_back(argValue);
  }

  // Validate argument count for specific builtins
  if (node.builtin_name == "print" && args.size() != 1) {
    throw std::runtime_error("$$print expects exactly 1 argument");
  }
  if (node.builtin_name == "exit" && args.size() != 1) {
    throw std::runtime_error("$$exit expects exactly 1 argument");
  }
  if (node.builtin_name == "syscall" && args.size() < 1) {
    throw std::runtime_error("$$syscall expects at least 1 argument");
  }

  // Generate platform-specific code
  try {
    switch (platform) {
      case TargetPlatform::Linux:
        return generateLinuxSyscall(node.builtin_name, args);
      case TargetPlatform::MacOS:
        return generateMacOSSyscall(node.builtin_name, args);
      case TargetPlatform::Windows:
        return generateWindowsSyscall(node.builtin_name, args);
      default:
        throw std::runtime_error("Unsupported target platform for builtin: " +
                                 node.builtin_name);
    }
  } catch (const std::exception& e) {
    std::cout << "[CodeGen] Error generating builtin $$" << node.builtin_name
              << ": " << e.what() << std::endl;
    throw;
  }
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

  // Get target triple based on platform
  std::string targetTripleStr;
  TargetPlatform platform = detectTargetPlatform();

  switch (platform) {
    case TargetPlatform::Windows:
      targetTripleStr = "x86_64-pc-windows-msvc";
      break;
    case TargetPlatform::Linux:
      targetTripleStr = "x86_64-pc-linux-gnu";
      break;
    case TargetPlatform::MacOS:
      targetTripleStr = "x86_64-apple-darwin";
      break;
    default:
      // Fallback to platform-specific defaults
#ifdef _WIN32
      targetTripleStr = "x86_64-pc-windows-msvc";
#elif defined(__linux__)
      targetTripleStr = "x86_64-pc-linux-gnu";
#elif defined(__APPLE__)
      targetTripleStr = "x86_64-apple-darwin";
#else
      targetTripleStr = "x86_64-unknown-unknown";
#endif
      break;
  }

  std::cout << "[CodeGen] Using target triple: " << targetTripleStr
            << std::endl;

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

  // Detect platform for cross-platform linking
  TargetPlatform platform = detectTargetPlatform();
  std::string linkCmd;
  switch (platform) {
    case TargetPlatform::Windows:
      // Windows: Use clang with minimal runtime support
      // Include compiler-rt for __chkstk and other compiler builtins
      linkCmd = "clang \"" + objectFilename + "\" -o \"" + executableFilename +
                "\" -nostdlib -lkernel32 -lmsvcrt";
      break;

    case TargetPlatform::Linux:
      // Linux: Use clang with no libc, link for Linux syscalls
      linkCmd = "clang \"" + objectFilename + "\" -o \"" + executableFilename +
                "\" -nostdlib -static";
      break;

    case TargetPlatform::MacOS:
      // macOS: Use clang with no libc, link for macOS syscalls
      linkCmd = "clang \"" + objectFilename + "\" -o \"" + executableFilename +
                "\" -nostdlib -static";
      break;

    default:
      // Fallback: Use standard linking
      linkCmd =
          "clang \"" + objectFilename + "\" -o \"" + executableFilename + "\"";
      break;
  }

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

// --- Function Declaration Codegen ---
llvm::Value* CodeGen::codegen(FunctionDeclNode& node) {
  std::cout << "[CodeGen] Generating function: " << node.name << std::endl;

  // 1. Convert parameter types to LLVM types
  std::vector<llvm::Type*> param_types;
  for (auto& param : node.parameters) {
    llvm::Type* llvm_type = typeToLLVMType(*param->type);
    if (!llvm_type) {
      std::cout << "[CodeGen] ERROR: Failed to convert parameter type"
                << std::endl;
      return nullptr;
    }
    param_types.push_back(llvm_type);
  }

  // 2. Convert return type to LLVM type (default to void if null)
  llvm::Type* return_type = builder->getVoidTy();  // Default: void
  if (node.return_type) {
    return_type = typeToLLVMType(*node.return_type);
    if (!return_type) {
      std::cout << "[CodeGen] ERROR: Failed to convert return type"
                << std::endl;
      return nullptr;
    }
  }

  // 3. Create function type
  llvm::FunctionType* func_type =
      llvm::FunctionType::get(return_type, param_types, false);

  // 4. Create function
  llvm::Function* llvm_func = llvm::Function::Create(
      func_type, llvm::Function::ExternalLinkage, node.name, module.get());

  if (!llvm_func) {
    std::cout << "[CodeGen] ERROR: Failed to create function" << std::endl;
    return nullptr;
  }

  // 5. Set parameter names
  auto arg_it = llvm_func->arg_begin();
  for (size_t i = 0; i < node.parameters.size(); ++i, ++arg_it) {
    arg_it->setName(node.parameters[i]->name);
  }

  // 6. Create entry block
  llvm::BasicBlock* entry_block =
      llvm::BasicBlock::Create(*context, "entry", llvm_func);

  // Save current insertion point and function context
  llvm::BasicBlock* prev_block = builder->GetInsertBlock();
  llvm::Function* prev_function = current_function;

  // Switch to function context
  builder->SetInsertPoint(entry_block);
  current_function = llvm_func;

  // Save previous named values (for nested scopes)
  auto prev_named_values = named_values;
  auto prev_variable_types = variable_types;

  // 7. Add parameters to symbol table
  arg_it = llvm_func->arg_begin();
  for (size_t i = 0; i < node.parameters.size(); ++i, ++arg_it) {
    // Create alloca for parameter (for mutable parameters)
    llvm::Type* param_type = typeToLLVMType(*node.parameters[i]->type);
    llvm::AllocaInst* alloca =
        builder->CreateAlloca(param_type, nullptr, node.parameters[i]->name);

    // Store parameter value in alloca
    builder->CreateStore(&*arg_it, alloca);

    // Add to symbol table
    named_values[node.parameters[i]->name] = alloca;
    variable_types[node.parameters[i]->name] = param_type;
  }

  // 8. Generate function body
  for (auto& stmt : node.body) {
    if (stmt) {
      codegen(*stmt);
    }
  }

  // 9. Add default return if needed
  if (return_type->isVoidTy()) {
    // Void function - add return void if no return statement at end
    if (builder->GetInsertBlock()->getTerminator() == nullptr) {
      builder->CreateRetVoid();
    }
  } else {
    // Non-void function - should have return statement
    if (builder->GetInsertBlock()->getTerminator() == nullptr) {
      std::cout
          << "[CodeGen] WARNING: Non-void function without return statement"
          << std::endl;
      // Add default return (could be improved)
      if (return_type->isIntegerTy()) {
        builder->CreateRet(llvm::ConstantInt::get(return_type, 0));
      } else if (return_type->isFloatingPointTy()) {
        builder->CreateRet(llvm::ConstantFP::get(return_type, 0.0));
      }
    }
  }

  // 10. Restore previous context
  current_function = prev_function;
  named_values = prev_named_values;
  variable_types = prev_variable_types;

  if (prev_block) {
    builder->SetInsertPoint(prev_block);
  }

  std::cout << "[CodeGen] Function generation complete: " << node.name
            << std::endl;
  return llvm_func;
}

// --- Return Statement Codegen ---
llvm::Value* CodeGen::codegen(ReturnStmtNode& node) {
  std::cout << "[CodeGen] Generating return statement" << std::endl;

  if (!current_function) {
    std::cout << "[CodeGen] ERROR: Return statement outside function"
              << std::endl;
    return nullptr;
  }

  if (node.expression) {
    // Return with value
    llvm::Value* return_value = codegen(*node.expression);
    if (!return_value) {
      std::cout << "[CodeGen] ERROR: Failed to generate return expression"
                << std::endl;
      return nullptr;
    }

    return builder->CreateRet(return_value);
  } else {
    // Return void
    return builder->CreateRetVoid();
  }
}