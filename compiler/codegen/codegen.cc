// compiler/codegen/codegen.cc
#include "codegen.hh"

#include <iostream>
#include <stdexcept>
#include <typeinfo>

#include "../common/logger.hh"
#include "../parser/ast.hh"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

CodeGen::CodeGen() : verbosity_level(loom::VerbosityLevel::NORMAL) {
  context = std::make_unique<llvm::LLVMContext>();
  module = std::make_unique<llvm::Module>("MyLoomModule", *context);
  builder = std::make_unique<llvm::IRBuilder<>>(*context);
  current_function = nullptr;
  symbol_table = nullptr;  // Initialize to null
  symbol_table = nullptr;  // Initialize to null

  // Initialize the syscall framework with no-libc by default
  loom::SyscallConfig config;
  config.use_libc = false;     // Use no-libc for minimal dependencies
  config.auto_newline = true;  // $$print adds newlines
  config.float_precision = 6;  // Default precision
  config.integer_base = 10;    // Decimal by default

  syscallFramework = std::make_unique<loom::SyscallFramework>(
      context.get(), builder.get(), module.get(), config);
}

CodeGen::CodeGen(loom::VerbosityLevel verbosity) : verbosity_level(verbosity) {
  context = std::make_unique<llvm::LLVMContext>();
  module = std::make_unique<llvm::Module>("MyLoomModule", *context);
  builder = std::make_unique<llvm::IRBuilder<>>(*context);
  current_function = nullptr;
  symbol_table = nullptr;  // Initialize to null

  // Initialize the syscall framework with no-libc by default
  loom::SyscallConfig config;
  config.use_libc = false;     // Use no-libc for minimal dependencies
  config.auto_newline = true;  // $$print adds newlines
  config.float_precision = 6;  // Default precision
  config.integer_base = 10;    // Decimal by default

  syscallFramework = std::make_unique<loom::SyscallFramework>(
      context.get(), builder.get(), module.get(), config);
}

void CodeGen::setSymbolTable(SymbolTable* symbols) { symbol_table = symbols; }

int CodeGen::getFieldIndex(const std::string& struct_name,
                           const std::string& field_name) const {
  if (!symbol_table) {
    std::cout << "[CodeGen] ERROR: Symbol table not available for struct/union "
                 "field lookup"
              << std::endl;
    return -1;
  }

  // Try to look up as a struct first
  const StructInfo* struct_info = symbol_table->lookupStruct(struct_name);
  if (struct_info) {
    for (size_t i = 0; i < struct_info->fields.size(); ++i) {
      if (struct_info->fields[i].first == field_name) {
        return static_cast<int>(i);
      }
    }
    std::cout << "[CodeGen] ERROR: Field '" << field_name
              << "' not found in struct '" << struct_name << "'" << std::endl;
    return -1;
  }

  // Try to look up as a union
  const UnionInfo* union_info = symbol_table->lookupUnion(struct_name);
  if (union_info) {
    for (size_t i = 0; i < union_info->fields.size(); ++i) {
      if (union_info->fields[i].first == field_name) {
        return static_cast<int>(i);
      }
    }
    std::cout << "[CodeGen] ERROR: Field '" << field_name
              << "' not found in union '" << struct_name << "'" << std::endl;
    return -1;
  }

  // Neither struct nor union found
  std::cout << "[CodeGen] ERROR: Struct/union not found: " << struct_name
            << std::endl;
  return -1;
}

void CodeGen::generate(const std::vector<std::unique_ptr<StmtNode>>& ast) {
  Logger::debug("Starting code generation");

  // Check for main function
  bool has_main_function = false;
  for (const auto& stmt : ast) {
    if (auto* func_decl = dynamic_cast<FunctionDeclNode*>(stmt.get())) {
      if (func_decl->name == "main") {
        has_main_function = true;
        break;
      }
    }
  }
  if (!has_main_function) {
    Logger::error(
        "No 'main' function found in program. Every Loom program must have a "
        "main function.");
    throw std::runtime_error("Missing main function");
  }
  logMessage(loom::VerbosityLevel::VERBOSE,
             "[CodeGen] Found main function in AST");

  logMessage(
      loom::VerbosityLevel::VERBOSE,
      "[CodeGen] Processing " + std::to_string(ast.size()) + " statements...");

  try {
    for (size_t i = 0; i < ast.size(); ++i) {
      logMessage(loom::VerbosityLevel::DEBUG,
                 "[CodeGen] Processing statement " + std::to_string(i + 1) +
                     "/" + std::to_string(ast.size()));
      codegen(*ast[i]);
      logMessage(loom::VerbosityLevel::DEBUG, "[CodeGen] Statement " +
                                                  std::to_string(i + 1) +
                                                  " completed successfully");
    }
  } catch (const std::exception& e) {
    logMessage(loom::VerbosityLevel::NORMAL,
               "[CodeGen] Error during statement processing: " +
                   std::string(e.what()));
    logMessage(loom::VerbosityLevel::DEBUG, "[CodeGen] Printing IR so far:");
    if (verbosity_level >= loom::VerbosityLevel::DEBUG) {
      print_ir();
    }
    throw;
  }
  logMessage(loom::VerbosityLevel::DEBUG, "[CodeGen] Verifying function...");
  for (auto& function : *module) {
    llvm::verifyFunction(function);
  }
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Function verification completed");
  generateEntryPoint();

  logMessage(loom::VerbosityLevel::VERBOSE,
             "[CodeGen] Code generation completed successfully!");
  if (verbosity_level >= loom::VerbosityLevel::DEBUG) {
    logMessage(loom::VerbosityLevel::DEBUG, "[CodeGen] Generated LLVM IR:");
    print_ir();
  }
}

void CodeGen::print_ir() const { module->print(llvm::outs(), nullptr); }

void CodeGen::write_ir_to_stream(std::ostream& stream) {
  std::string ir_str;
  llvm::raw_string_ostream llvm_stream(ir_str);
  module->print(llvm_stream, nullptr);
  stream << ir_str;
}

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
    logMessage(loom::VerbosityLevel::DEBUG,
               "[CodeGen] Generating Windows entry point...");

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
    logMessage(loom::VerbosityLevel::DEBUG,
               "[CodeGen] Generating Unix-style entry point...");

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

  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Entry point generation completed");
}

// Platform detection based on target triple
TargetPlatform CodeGen::detectTargetPlatform() const {
  llvm::Triple targetTriple(module->getTargetTriple());
  std::string targetTripleStr = targetTriple.str();
  logMessage(
      loom::VerbosityLevel::DEBUG,
      "[CodeGen] Detecting platform from target triple: " + targetTripleStr);

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
  }  // Fallback: detect from preprocessor macros at compile time
#ifdef _WIN32
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Defaulting to Windows platform");
  return TargetPlatform::Windows;
#elif defined(__linux__)
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Defaulting to Linux platform");
  return TargetPlatform::Linux;
#elif defined(__APPLE__)
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Defaulting to macOS platform");
  return TargetPlatform::MacOS;
#else
  logMessage(loom::VerbosityLevel::DEBUG, "[CodeGen] Unknown target platform");
  return TargetPlatform::Unknown;
#endif
}

// Linux syscall implementation using inline assembly
llvm::Value* CodeGen::generateLinuxSyscall(const std::string& name,
                                           std::vector<llvm::Value*>& args) {
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating Linux syscall: " + name);

  if (name == "print" && args.size() >= 1) {
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
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating macOS syscall: " + name);

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
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating Windows syscall: " + name);
  if (name == "print" && args.size() >= 1) {
    // Check if the argument is an integer or a string
    llvm::Type* argType = args[0]->getType();

    if (argType->isIntegerTy()) {
      // Handle integer printing
      std::cout << "[CodeGen] Printing integer value" << std::endl;

      // Use WriteFile API for printing integers
      llvm::Function* writeFile = module->getFunction("WriteFile");
      if (!writeFile) {
        llvm::FunctionType* writeFileType = llvm::FunctionType::get(
            builder->getInt32Ty(),  // BOOL (treated as i32)
            {
                llvm::PointerType::getUnqual(*context),  // HANDLE
                llvm::PointerType::getUnqual(*context),  // LPCVOID (buffer)
                builder->getInt32Ty(),                   // DWORD (size)
                llvm::PointerType::getUnqual(
                    *context),  // LPDWORD (bytes written)
                llvm::PointerType::getUnqual(*context)  // LPOVERLAPPED
            },
            false);
        writeFile = llvm::Function::Create(writeFileType,
                                           llvm::Function::ExternalLinkage,
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
      llvm::Value* stdoutHandle =
          builder->CreateCall(getStdHandle, {builder->getInt32(-11)},
                              "stdout.handle");  // Convert integer to string
      // For simplicity, let's create a formatted string from the integer
      // In a more complete implementation, we'd do the conversion at runtime
      llvm::Value* intValue = args[0];

      // For simplicity, let's create a formatted string from the integer
      // In a more complete implementation, we'd do the conversion at runtime
      if (auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(intValue)) {
        // If it's a compile-time constant, we can convert it directly
        std::string intStr = std::to_string(constInt->getSExtValue());
        llvm::Constant* intStrGlobal =
            builder->CreateGlobalString(intStr, "int_str");
        llvm::Value* intStrPtr = builder->CreatePointerCast(
            intStrGlobal, llvm::PointerType::getUnqual(*context));

        llvm::Value* strSize =
            builder->getInt32(static_cast<uint32_t>(intStr.length()));
        llvm::Value* bytesWritten = builder->CreateAlloca(
            builder->getInt32Ty(), nullptr, "bytes.written");

        // Write the integer string
        llvm::Value* result1 =
            builder->CreateCall(writeFile,
                                {stdoutHandle, intStrPtr, strSize, bytesWritten,
                                 llvm::ConstantPointerNull::get(
                                     llvm::PointerType::getUnqual(*context))},
                                "write.result");

        // Write newline
        llvm::Constant* newlineStr =
            builder->CreateGlobalString("\n", "newline");
        llvm::Value* newlinePtr = builder->CreatePointerCast(
            newlineStr, llvm::PointerType::getUnqual(*context));
        llvm::Value* newlineSize = builder->getInt32(1);
        llvm::Value* bytesWritten2 = builder->CreateAlloca(
            builder->getInt32Ty(), nullptr, "bytes.written.newline");

        builder->CreateCall(
            writeFile,
            {stdoutHandle, newlinePtr, newlineSize, bytesWritten2,
             llvm::ConstantPointerNull::get(
                 llvm::PointerType::getUnqual(*context))},
            "write.newline.result");
        return result1;
      } else {
        // For runtime integers, we convert WITHOUT using libc (no sprintf)
        // Simple approach: print single digit (0-9) for now
        std::cout << "[CodeGen] Converting runtime integer to string (no libc)"
                  << std::endl;

        // For simplicity, we'll handle single digits (0-9) and print <num> for
        // others This keeps us libc-independent while still being functional

        // Get the last digit: value % 10
        llvm::Value* digit =
            builder->CreateURem(intValue, builder->getInt32(10), "digit");

        // Convert digit to ASCII: digit + '0' (48)
        llvm::Value* digit_i8 =
            builder->CreateTrunc(digit, builder->getInt8Ty(), "digit_i8");
        llvm::Value* ascii_digit =
            builder->CreateAdd(digit_i8, builder->getInt8(48), "ascii_digit");

        // Create a single-character buffer
        llvm::Value* digit_alloca =
            builder->CreateAlloca(builder->getInt8Ty(), nullptr, "digit_char");
        builder->CreateStore(ascii_digit, digit_alloca);

        // Write the single digit
        llvm::Value* bytesWritten = builder->CreateAlloca(
            builder->getInt32Ty(), nullptr, "bytes.written");

        llvm::Value* result1 = builder->CreateCall(
            writeFile,
            {stdoutHandle, digit_alloca, builder->getInt32(1), bytesWritten,
             llvm::ConstantPointerNull::get(
                 llvm::PointerType::getUnqual(*context))},
            "write.result");

        // Write newline
        llvm::Constant* newlineStr =
            builder->CreateGlobalString("\n", "newline");
        llvm::Value* newlinePtr = builder->CreatePointerCast(
            newlineStr, llvm::PointerType::getUnqual(*context));
        llvm::Value* newlineSize = builder->getInt32(1);
        llvm::Value* bytesWritten2 = builder->CreateAlloca(
            builder->getInt32Ty(), nullptr, "bytes.written.newline");

        builder->CreateCall(
            writeFile,
            {stdoutHandle, newlinePtr, newlineSize, bytesWritten2,
             llvm::ConstantPointerNull::get(
                 llvm::PointerType::getUnqual(*context))},
            "write.newline.result");

        return result1;
      }
    } else {
      // Handle string printing (existing code)
      std::cout << "[CodeGen] Printing string value" << std::endl;

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
                    *context),  // LPDWORD (bytes written)
                llvm::PointerType::getUnqual(*context)  // LPOVERLAPPED
            },
            false);
        writeFile = llvm::Function::Create(writeFileType,
                                           llvm::Function::ExternalLinkage,
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
        // In a full implementation, we'd track string lengths in the type
        // system
        bufferSize = builder->getInt32(100);
      }

      llvm::Value* bytesWritten = builder->CreateAlloca(
          builder->getInt32Ty(), nullptr, "bytes.written");

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
    }
  } else if (name == "print_addr" && args.size() >= 1) {
    // Print the actual pointer address
    llvm::Function* writeFileAddr = module->getFunction("WriteFile");
    if (!writeFileAddr) {
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
      writeFileAddr =
          llvm::Function::Create(writeFileType, llvm::Function::ExternalLinkage,
                                 "WriteFile", module.get());
    }

    llvm::Function* getStdHandleAddr = module->getFunction("GetStdHandle");
    if (!getStdHandleAddr) {
      llvm::FunctionType* getStdHandleType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context),  // HANDLE
          {builder->getInt32Ty()},                 // DWORD
          false);
      getStdHandleAddr = llvm::Function::Create(getStdHandleType,
                                                llvm::Function::ExternalLinkage,
                                                "GetStdHandle", module.get());
    }

    // Get stdout handle
    llvm::Value* stdoutHandleAddr = builder->CreateCall(
        getStdHandleAddr, {builder->getInt32(-11)}, "stdout.handle.addr");

    // Print the label "ADDRESS: 0x"
    llvm::Constant* addrLabel =
        builder->CreateGlobalString("ADDRESS: 0x", "addr.label");
    llvm::Value* labelPtr = builder->CreatePointerCast(
        addrLabel, llvm::PointerType::getUnqual(*context));

    llvm::Value* bytesWrittenLabel = builder->CreateAlloca(
        builder->getInt32Ty(), nullptr, "bytes.written.label");
    llvm::Value* result = builder->CreateCall(
        writeFileAddr,
        {stdoutHandleAddr, labelPtr, builder->getInt32(12), bytesWrittenLabel,
         llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(
             *context))});  // Get the actual pointer address and format it as
                            // hex manually
    llvm::Value* argValue = args[0];

    // Convert pointer to integer
    llvm::Value* ptrAsInt =
        builder->CreatePtrToInt(argValue, builder->getInt64Ty());

    // Allocate buffer for hex string (16 chars for 64-bit address + null
    // terminator)
    llvm::Value* hexBuffer = builder->CreateAlloca(
        llvm::ArrayType::get(builder->getInt8Ty(), 17), nullptr, "hex.buffer");

    // Create hex digits lookup table
    llvm::Constant* hexDigits =
        builder->CreateGlobalString("0123456789ABCDEF", "hex.digits");
    llvm::Value* hexDigitsPtr = builder->CreatePointerCast(
        hexDigits, llvm::PointerType::getUnqual(*context));

    // Convert address to hex string manually (16 hex digits)
    for (int i = 15; i >= 0; i--) {
      // Extract 4 bits at position i*4
      llvm::Value* shift = builder->getInt64(i * 4);
      llvm::Value* shifted = builder->CreateLShr(ptrAsInt, shift);
      llvm::Value* nibble = builder->CreateAnd(shifted, builder->getInt64(0xF));

      // Get hex character from lookup table
      llvm::Value* hexCharPtr =
          builder->CreateGEP(builder->getInt8Ty(), hexDigitsPtr, nibble);
      llvm::Value* hexChar =
          builder->CreateLoad(builder->getInt8Ty(), hexCharPtr);

      // Store in buffer at position (15-i)
      llvm::Value* bufferPos = builder->CreateGEP(
          builder->getInt8Ty(), hexBuffer, builder->getInt64(15 - i));
      builder->CreateStore(hexChar, bufferPos);
    }

    // Add null terminator
    llvm::Value* nullPos = builder->CreateGEP(builder->getInt8Ty(), hexBuffer,
                                              builder->getInt64(16));
    builder->CreateStore(builder->getInt8(0), nullPos);

    // Print the hex address
    llvm::Value* bytesWrittenHex = builder->CreateAlloca(
        builder->getInt32Ty(), nullptr, "bytes.written.hex");

    builder->CreateCall(
        writeFileAddr,
        {stdoutHandleAddr, hexBuffer, builder->getInt32(16), bytesWrittenHex,
         llvm::ConstantPointerNull::get(
             llvm::PointerType::getUnqual(*context))});

    // Add newline
    llvm::Constant* newlineStrAddr =
        builder->CreateGlobalString("\n", "newline.addr");
    llvm::Value* newlinePtrAddr = builder->CreatePointerCast(
        newlineStrAddr, llvm::PointerType::getUnqual(*context));
    llvm::Value* bytesWrittenNewline = builder->CreateAlloca(
        builder->getInt32Ty(), nullptr, "bytes.written.newline");
    builder->CreateCall(writeFileAddr,
                        {stdoutHandleAddr, newlinePtrAddr, builder->getInt32(1),
                         bytesWrittenNewline,
                         llvm::ConstantPointerNull::get(
                             llvm::PointerType::getUnqual(*context))});
    return result;

  } else if (name == "socket" && args.size() >= 3) {
    // Create a socket using Windows Winsock API
    // Args: domain (AF_INET=2), type (SOCK_STREAM=1), protocol (0)
    llvm::Function* socketFunc = module->getFunction("socket");
    if (!socketFunc) {
      llvm::FunctionType* socketType = llvm::FunctionType::get(
          builder->getInt64Ty(),  // SOCKET (treated as i64)
          {builder->getInt32Ty(), builder->getInt32Ty(), builder->getInt32Ty()},
          false);
      socketFunc = llvm::Function::Create(
          socketType, llvm::Function::ExternalLinkage, "socket", module.get());
    }
    return builder->CreateCall(socketFunc, {args[0], args[1], args[2]});

  } else if (name == "bind" && args.size() >= 3) {
    // Bind socket to address
    // Args: socket, sockaddr*, addrlen
    llvm::Function* bindFunc = module->getFunction("bind");
    if (!bindFunc) {
      llvm::FunctionType* bindType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // int result
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt32Ty()},
          false);
      bindFunc = llvm::Function::Create(
          bindType, llvm::Function::ExternalLinkage, "bind", module.get());
    }
    return builder->CreateCall(bindFunc, {args[0], args[1], args[2]});

  } else if (name == "listen" && args.size() >= 2) {
    // Listen for connections
    // Args: socket, backlog
    llvm::Function* listenFunc = module->getFunction("listen");
    if (!listenFunc) {
      llvm::FunctionType* listenType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // int result
          {builder->getInt64Ty(), builder->getInt32Ty()}, false);
      listenFunc = llvm::Function::Create(
          listenType, llvm::Function::ExternalLinkage, "listen", module.get());
    }
    return builder->CreateCall(listenFunc, {args[0], args[1]});

  } else if (name == "accept" && args.size() >= 3) {
    // Accept incoming connection
    // Args: socket, sockaddr*, addrlen*
    llvm::Function* acceptFunc = module->getFunction("accept");
    if (!acceptFunc) {
      llvm::FunctionType* acceptType = llvm::FunctionType::get(
          builder->getInt64Ty(),  // SOCKET result
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           llvm::PointerType::getUnqual(*context)},
          false);
      acceptFunc = llvm::Function::Create(
          acceptType, llvm::Function::ExternalLinkage, "accept", module.get());
    }
    return builder->CreateCall(acceptFunc, {args[0], args[1], args[2]});

  } else if (name == "connect" && args.size() >= 3) {
    // Connect to remote address
    // Args: socket, sockaddr*, addrlen
    llvm::Function* connectFunc = module->getFunction("connect");
    if (!connectFunc) {
      llvm::FunctionType* connectType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // int result
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt32Ty()},
          false);
      connectFunc =
          llvm::Function::Create(connectType, llvm::Function::ExternalLinkage,
                                 "connect", module.get());
    }
    return builder->CreateCall(connectFunc, {args[0], args[1], args[2]});

  } else if (name == "send" && args.size() >= 4) {
    // Send data on socket
    // Args: socket, buffer, length, flags
    llvm::Function* sendFunc = module->getFunction("send");
    if (!sendFunc) {
      llvm::FunctionType* sendType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // int bytes sent
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt32Ty(), builder->getInt32Ty()},
          false);
      sendFunc = llvm::Function::Create(
          sendType, llvm::Function::ExternalLinkage, "send", module.get());
    }
    return builder->CreateCall(sendFunc, {args[0], args[1], args[2], args[3]});

  } else if (name == "recv" && args.size() >= 4) {
    // Receive data from socket
    // Args: socket, buffer, length, flags
    llvm::Function* recvFunc = module->getFunction("recv");
    if (!recvFunc) {
      llvm::FunctionType* recvType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // int bytes received
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt32Ty(), builder->getInt32Ty()},
          false);
      recvFunc = llvm::Function::Create(
          recvType, llvm::Function::ExternalLinkage, "recv", module.get());
    }
    return builder->CreateCall(recvFunc, {args[0], args[1], args[2], args[3]});

  } else if (name == "closesocket" && args.size() >= 1) {
    // Close socket
    // Args: socket
    llvm::Function* closesocketFunc = module->getFunction("closesocket");
    if (!closesocketFunc) {
      llvm::FunctionType* closesocketType =
          llvm::FunctionType::get(builder->getInt32Ty(),  // int result
                                  {builder->getInt64Ty()}, false);
      closesocketFunc = llvm::Function::Create(closesocketType,
                                               llvm::Function::ExternalLinkage,
                                               "closesocket", module.get());
    }
    return builder->CreateCall(closesocketFunc, {args[0]});

  } else if (name == "WSAStartup" && args.size() >= 2) {
    // Initialize Winsock
    // Args: wVersionRequested, lpWSAData
    llvm::Function* wsaStartupFunc = module->getFunction("WSAStartup");
    if (!wsaStartupFunc) {
      llvm::FunctionType* wsaStartupType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // int result
          {builder->getInt16Ty(), llvm::PointerType::getUnqual(*context)},
          false);
      wsaStartupFunc = llvm::Function::Create(wsaStartupType,
                                              llvm::Function::ExternalLinkage,
                                              "WSAStartup", module.get());
    }
    return builder->CreateCall(wsaStartupFunc, {args[0], args[1]});

  } else if (name == "WSACleanup" && args.size() == 0) {
    // Cleanup Winsock
    llvm::Function* wsaCleanupFunc = module->getFunction("WSACleanup");
    if (!wsaCleanupFunc) {
      llvm::FunctionType* wsaCleanupType =
          llvm::FunctionType::get(builder->getInt32Ty(),  // int result
                                  {}, false);
      wsaCleanupFunc = llvm::Function::Create(wsaCleanupType,
                                              llvm::Function::ExternalLinkage,
                                              "WSACleanup", module.get());
    }
    return builder->CreateCall(wsaCleanupFunc, {});

  } else if (name == "htons" && args.size() >= 1) {
    // Convert host byte order to network byte order (16-bit)
    llvm::Function* htonsFunc = module->getFunction("htons");
    if (!htonsFunc) {
      llvm::FunctionType* htonsType =
          llvm::FunctionType::get(builder->getInt16Ty(),  // u_short result
                                  {builder->getInt16Ty()}, false);
      htonsFunc = llvm::Function::Create(
          htonsType, llvm::Function::ExternalLinkage, "htons", module.get());
    }
    return builder->CreateCall(htonsFunc, {args[0]});

  } else if (name == "htonl" && args.size() >= 1) {
    // Convert host byte order to network byte order (32-bit)
    llvm::Function* htonlFunc = module->getFunction("htonl");
    if (!htonlFunc) {
      llvm::FunctionType* htonlType =
          llvm::FunctionType::get(builder->getInt32Ty(),  // u_long result
                                  {builder->getInt32Ty()}, false);
      htonlFunc = llvm::Function::Create(
          htonlType, llvm::Function::ExternalLinkage, "htonl", module.get());
    }
    return builder->CreateCall(htonlFunc, {args[0]});

  } else if (name == "inet_addr" && args.size() >= 1) {
    // Convert IP address string to binary form
    llvm::Function* inetAddrFunc = module->getFunction("inet_addr");
    if (!inetAddrFunc) {
      llvm::FunctionType* inetAddrType = llvm::FunctionType::get(
          builder->getInt32Ty(),  // unsigned long result
          {llvm::PointerType::getUnqual(*context)}, false);
      inetAddrFunc =
          llvm::Function::Create(inetAddrType, llvm::Function::ExternalLinkage,
                                 "inet_addr", module.get());
    }
    return builder->CreateCall(inetAddrFunc, {args[0]});

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
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Converting TypeNode to LLVM type, typeid: " +
                 std::string(typeid(type).name()));

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
  // Memory model types
  if (dynamic_cast<ReferenceTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found ReferenceTypeNode" << std::endl;
    // References are implemented as pointers in LLVM
    return llvm::PointerType::getUnqual(*context);
  }
  if (dynamic_cast<OwnedPointerTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found OwnedPointerTypeNode" << std::endl;
    // Owned pointers are also implemented as pointers in LLVM
    return llvm::PointerType::getUnqual(*context);
  }
  if (dynamic_cast<RawPointerTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found RawPointerTypeNode" << std::endl;
    // Raw pointers are implemented as opaque pointers in LLVM
    return builder->getPtrTy();
  }
  if (dynamic_cast<NullableTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found NullableTypeNode" << std::endl;
    // Nullable types can be implemented as pointers (null = nullptr)
    // Or as a struct with a flag, but pointer is simpler for now
    return llvm::PointerType::getUnqual(*context);
  }
  if (dynamic_cast<SliceTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found SliceTypeNode" << std::endl;
    // Slices are implemented as a struct { ptr, len }
    std::vector<llvm::Type*> slice_fields = {
        llvm::PointerType::getUnqual(*context),  // data pointer
        builder->getInt64Ty()                    // length
    };
    return llvm::StructType::get(*context, slice_fields);
  }
  if (auto* struct_type = dynamic_cast<StructTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found StructTypeNode: " << struct_type->struct_name
              << std::endl;
    // Look up the struct type by name
    llvm::StructType* llvm_struct =
        llvm::StructType::getTypeByName(*context, struct_type->struct_name);
    if (llvm_struct) {
      return llvm_struct;
    }
    std::cerr << "[CodeGen] Error: Unknown struct type: "
              << struct_type->struct_name << std::endl;
    return nullptr;
  }

  if (auto* union_type = dynamic_cast<UnionTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found UnionTypeNode: " << union_type->union_name
              << std::endl;
    // Unions are also represented as named struct types in LLVM
    llvm::StructType* llvm_union =
        llvm::StructType::getTypeByName(*context, union_type->union_name);
    if (llvm_union) {
      return llvm_union;
    }
    std::cerr << "[CodeGen] Error: Unknown union type: "
              << union_type->union_name << std::endl;
    return nullptr;
  }

  if (dynamic_cast<NullTypeNode*>(&type)) {
    std::cout << "[CodeGen] Found NullTypeNode" << std::endl;
    // Null type is represented as a void pointer
    return llvm::PointerType::getUnqual(*context);
  }
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] ERROR: Unknown TypeNode, type info: " +
                 std::string(typeid(type).name()));
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
  }  // Value to nullable type (represented as pointer)
  if (targetType->isPointerTy()) {
    std::cout << "[CodeGen] Casting value to nullable/pointer type"
              << std::endl;

    // If we already have a pointer, check if we can use it directly
    if (baseValue->getType()->isPointerTy()) {
      // Both are pointers - check if they're compatible
      // For now, let's assume they are compatible (in a real implementation
      // we'd do more thorough type checking)
      std::cout << "[CodeGen] Both types are pointers, returning base value"
                << std::endl;
      return baseValue;
    }

    // For nullable types, we need to allocate space and store the value
    // This is a simplified approach - in a real implementation you'd want
    // to track whether this is actually a nullable or just a pointer

    // Allocate space for the value
    llvm::Value* alloca =
        builder->CreateAlloca(baseValue->getType(), nullptr, "nullable.alloc");

    // Store the value in the allocated space
    builder->CreateStore(baseValue, alloca);

    // Return the pointer to the stored value
    return alloca;
  }
  // Array to slice conversion
  if (targetType->isStructTy()) {
    std::cout << "[CodeGen] Checking if we can convert to slice type"
              << std::endl;

    // Check if target is a slice struct (should have ptr and i64 fields)
    if (targetType->getStructNumElements() == 2) {
      std::cout << "[CodeGen] Converting to slice type" << std::endl;

      // For array literals, baseValue is already a pointer to the first element
      if (baseValue->getType()->isPointerTy()) {
        // We need to determine the array size - for now, let's hardcode it
        // In a real implementation, we'd track this information
        uint64_t arraySize = 3;  // TODO: Get actual size from the array literal

        // Create slice struct
        llvm::Value* slice = llvm::UndefValue::get(targetType);
        slice = builder->CreateInsertValue(slice, baseValue, 0, "slice.ptr");
        slice = builder->CreateInsertValue(slice, builder->getInt64(arraySize),
                                           1, "slice.len");

        return slice;
      }

      // If baseValue is an actual array type
      if (baseValue->getType()->isArrayTy()) {
        auto* arrayType = llvm::cast<llvm::ArrayType>(baseValue->getType());
        uint64_t arraySize = arrayType->getNumElements();

        // Get pointer to first element of array
        llvm::Value* arrayPtr = builder->CreateGEP(
            arrayType, baseValue, {builder->getInt32(0), builder->getInt32(0)},
            "array.ptr");

        // Create slice struct
        llvm::Value* slice = llvm::UndefValue::get(targetType);
        slice = builder->CreateInsertValue(slice, arrayPtr, 0, "slice.ptr");
        slice = builder->CreateInsertValue(slice, builder->getInt64(arraySize),
                                           1, "slice.len");

        return slice;
      }
    }
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
  if (auto* n = dynamic_cast<ForStmtNode*>(&node)) {
    std::cout << "[CodeGen] Processing ForStmtNode" << std::endl;
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
  if (auto* n = dynamic_cast<UnaryExpr*>(&node)) {
    std::cout << "[CodeGen] Processing UnaryExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<CastExpr*>(&node)) {
    std::cout << "[CodeGen] Processing CastExpr" << std::endl;
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
  if (auto* n = dynamic_cast<BooleanLiteral*>(&node)) {
    std::cout << "[CodeGen] Processing BooleanLiteral" << std::endl;
    return codegen(*n);
  }  // Memory model nodes
  if (auto* n = dynamic_cast<ReferenceExpr*>(&node)) {
    std::cout << "[CodeGen] Processing ReferenceExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<DereferenceExpr*>(&node)) {
    std::cout << "[CodeGen] Processing DereferenceExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<MemberAccessExpr*>(&node)) {
    std::cout << "[CodeGen] Processing MemberAccessExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<ArrayLiteralExpr*>(&node)) {
    std::cout << "[CodeGen] Processing ArrayLiteralExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<IndexExpr*>(&node)) {
    std::cout << "[CodeGen] Processing IndexExpr" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<RangeExpr*>(&node)) {
    std::cout << "[CodeGen] Processing RangeExpr" << std::endl;
    return codegen(*n);
  }

  // Struct-related nodes
  if (auto* n = dynamic_cast<StructDeclNode*>(&node)) {
    std::cout << "[CodeGen] Processing StructDeclNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<StructLiteralExpr*>(&node)) {
    std::cout << "[CodeGen] Processing StructLiteralExpr" << std::endl;
    return codegen(*n);
  }

  // Union-related nodes
  if (auto* n = dynamic_cast<UnionDeclNode*>(&node)) {
    std::cout << "[CodeGen] Processing UnionDeclNode" << std::endl;
    return codegen(*n);
  }
  if (auto* n = dynamic_cast<UnionLiteralExpr*>(&node)) {
    std::cout << "[CodeGen] Processing UnionLiteralExpr" << std::endl;
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

llvm::Value* CodeGen::codegen(BooleanLiteral& node) {
  std::cout << "[CodeGen] Generating BooleanLiteral: "
            << (node.value ? "true" : "false") << std::endl;

  // Create a boolean constant (i1 type in LLVM)
  return llvm::ConstantInt::get(builder->getInt1Ty(), node.value ? 1 : 0);
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
  // 3. Erzeuge eine 'alloca'-Instruktion.
  std::cout << "[CodeGen] Creating alloca for variable: " << node.name
            << std::endl;
  llvm::Value* alloca = builder->CreateAlloca(varType, nullptr, node.name);
  std::cout << "[CodeGen] Alloca created successfully" << std::endl;

  // 1. Generate initializer only if provided
  if (node.initializer != nullptr) {
    std::cout << "[CodeGen] Generating initializer for variable: " << node.name
              << std::endl;
    llvm::Value* initializerVal =
        codegenWithTargetType(*node.initializer, varType);
    std::cout << "[CodeGen] Initializer generated successfully" << std::endl;

    // 4. Speichere den Initialisierungswert in dem reservierten Speicher.
    std::cout << "[CodeGen] Storing initializer value in alloca" << std::endl;
    builder->CreateStore(initializerVal, alloca);
    std::cout << "[CodeGen] Store instruction created successfully"
              << std::endl;
  } else {
    std::cout << "[CodeGen] Variable " << node.name
              << " declared without initializer" << std::endl;
  }
  // 5. Store in both the old system (for compatibility) and new unified symbol
  // // table
  named_values[node.name] = alloca;
  variable_types[node.name] = varType;

  // Add variable to unified symbol table in current scope
  if (symbol_table) {
    // Get the variable's type from the node
    std::shared_ptr<TypeNode> var_type_node = nullptr;
    if (node.type) {
      // Clone the type node for the symbol table
      var_type_node = cloneTypeNode(node.type.get());
    }

    if (var_type_node) {
      if (symbol_table->defineVariable(node.name, node.kind, var_type_node)) {
        // Now update with LLVM info
        if (symbol_table->updateVariableLLVM(node.name, alloca, varType)) {
          std::cout << "[CodeGen] Variable " << node.name
                    << " added and updated in unified symbol table"
                    << std::endl;
        }
      } else {
        // Variable already exists, just update LLVM info
        if (symbol_table->updateVariableLLVM(node.name, alloca, varType)) {
          std::cout << "[CodeGen] Variable " << node.name
                    << " LLVM info updated in unified symbol table"

                    << std::endl;
        } else {
          std::cout
              << "[CodeGen] WARNING: Could not update LLVM info for variable: "
              << node.name << std::endl;
        }
      }
    }
  }

  std::cout << "[CodeGen] Variable " << node.name << " added to symbol table"
            << std::endl;

  return nullptr;
}

llvm::Value* CodeGen::codegen(Identifier& node) {
  std::cout << "[CodeGen] Generating Identifier: " << node.name << std::endl;

  // Special case: handle null literal
  if (node.name == "null") {
    std::cout << "[CodeGen] Generating null literal" << std::endl;
    return llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(*context));
  }  // 1. Try the unified symbol table first
  const VariableInfo* var_info = symbol_table->lookupVariable(node.name);
  llvm::Value* var_ptr = nullptr;
  llvm::Type* var_type = nullptr;

  if (var_info && var_info->llvm_value && var_info->llvm_type) {
    // Use unified symbol table
    var_ptr = var_info->llvm_value;
    var_type = var_info->llvm_type;
  } else {
    // Fallback to old system
    auto it = named_values.find(node.name);
    if (it == named_values.end()) {
      throw std::runtime_error("CodeGen: Unknown variable name '" + node.name +
                               "'.");
    }
    var_ptr = it->second;

    auto type_it = variable_types.find(node.name);
    if (type_it == variable_types.end()) {
      throw std::runtime_error("CodeGen: Unknown variable type for '" +
                               node.name + "'.");
    }
    var_type = type_it->second;
  }
  return builder->CreateLoad(var_type, var_ptr, node.name + ".load");
}

llvm::Value* CodeGen::codegen(BinaryExpr& node) {
  std::cout << "[CodeGen] Generating BinaryExpr" << std::endl;
  // 1. Recursively generate code for left and right operands
  llvm::Value* L = codegen(*node.left);
  llvm::Value* R = codegen(*node.right);

  if (!L || !R) {
    return nullptr;
  }

  // 2. Handle type promotion and casting
  llvm::Type* LType = L->getType();
  llvm::Type* RType = R->getType();

  // If one operand is float, promote both to float
  if (LType->isFloatingPointTy() || RType->isFloatingPointTy()) {
    // Promote both to floating point
    if (LType->isIntegerTy()) {
      L = builder->CreateSIToFP(L, builder->getDoubleTy(), "int2fp");
    }
    if (RType->isIntegerTy()) {
      R = builder->CreateSIToFP(R, builder->getDoubleTy(), "int2fp");
    }
    // Generate floating point operations
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
  } else if (LType->isIntegerTy() && RType->isIntegerTy()) {
    // Both operands are integers - handle different bit widths
    unsigned LBits = LType->getIntegerBitWidth();
    unsigned RBits = RType->getIntegerBitWidth();

    // Promote to the larger bit width
    if (LBits != RBits) {
      unsigned targetBits = std::max(LBits, RBits);
      llvm::Type* targetType = builder->getIntNTy(targetBits);

      if (LBits < targetBits) {
        L = builder->CreateSExt(L, targetType, "sext.L");
      }
      if (RBits < targetBits) {
        R = builder->CreateSExt(R, targetType, "sext.R");
      }
    }

    // Generate integer operations
    switch (node.op.type) {
      case TokenType::TOKEN_PLUS:
        return builder->CreateAdd(L, R, "add.tmp");
      case TokenType::TOKEN_MINUS:
        return builder->CreateSub(L, R, "sub.tmp");
      case TokenType::TOKEN_STAR:
        return builder->CreateMul(L, R, "mul.tmp");
      case TokenType::TOKEN_SLASH:
        return builder->CreateSDiv(L, R,
                                   "div.tmp");  // SDiv for signed integers
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

      // Bitwise operations
      case TokenType::TOKEN_AMPERSAND:
        return builder->CreateAnd(L, R, "and.tmp");
      case TokenType::TOKEN_BITWISE_OR:
        return builder->CreateOr(L, R, "or.tmp");
      case TokenType::TOKEN_HAT:  // XOR operation
        return builder->CreateXor(L, R, "xor.tmp");
      case TokenType::TOKEN_LEFT_SHIFT:
        return builder->CreateShl(L, R, "shl.tmp");
      case TokenType::TOKEN_RIGHT_SHIFT:
        return builder->CreateAShr(
            L, R, "ashr.tmp");  // Arithmetic shift for signed integers

      default:
        throw std::runtime_error(
            "CodeGen: Unknown binary operator for integer.");
    }
  } else {
    throw std::runtime_error(
        "CodeGen: Unsupported operand types for binary operation.");
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
  std::cout << "[CodeGen] Generating AssignmentExpr: "
            << node.target->toString() << std::endl;

  // Generate the value to assign
  llvm::Value* value = codegen(*node.value);
  if (!value) return nullptr;

  // Generate the target lvalue
  if (auto* identifier = dynamic_cast<Identifier*>(node.target.get())) {
    // Simple variable assignment
    auto it = named_values.find(identifier->name);
    if (it == named_values.end()) {
      std::cout << "[CodeGen] ERROR: Undefined variable: " << identifier->name
                << std::endl;
      throw std::runtime_error("Undefined variable: " + identifier->name);
    }

    llvm::Value* variable_ptr = it->second;
    builder->CreateStore(value, variable_ptr);

    std::cout << "[CodeGen] Assignment completed for variable: "
              << identifier->name << std::endl;
    return value;
  } else if (dynamic_cast<MemberAccessExpr*>(node.target.get())) {
    // Field assignment - need to implement this
    std::cout << "[CodeGen] TODO: Field assignment not yet implemented"
              << std::endl;
    return nullptr;

  } else if (dynamic_cast<IndexExpr*>(node.target.get())) {
    // Array element assignment - need to implement this
    std::cout << "[CodeGen] TODO: Array element assignment not yet implemented"
              << std::endl;
    return nullptr;
  } else {
    std::cout << "[CodeGen] ERROR: Unsupported assignment target type"
              << std::endl;
    return nullptr;
  }
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

  // Check if function returns void
  if (target_func->getReturnType()->isVoidTy()) {
    // For void functions, don't assign a name to the call result
    builder->CreateCall(target_func, args);
    return nullptr;  // Void functions don't return a value
  } else {
    // For non-void functions, assign a name to the call result
    return builder->CreateCall(target_func, args, node.function_name + ".call");
  }
}

llvm::Value* CodeGen::codegen(BuiltinCallExpr& node) {
  std::cout << "[CodeGen] Generating BuiltinCallExpr: $$" << node.builtin_name
            << std::endl;

  // Generate arguments
  std::vector<llvm::Value*> args;
  for (auto& arg : node.arguments) {
    llvm::Value* argValue = codegen(*arg);
    if (!argValue) return nullptr;
    args.push_back(argValue);
  }

  try {
    // Use the new syscall framework for all builtin calls
    if (node.builtin_name == "print") {
      if (args.size() != 1) {
        throw std::runtime_error("$$print expects exactly 1 argument");
      }  // Determine the data type of the argument
      llvm::Type* argType = args[0]->getType();
      std::cout << "[CodeGen] Print argument type: ";
      argType->print(llvm::outs());
      std::cout << std::endl;

      loom::DataType dataType = loom::DataType::STRING;  // default

      if (argType->isIntegerTy(1)) {
        // Boolean type (i1) - check this first before general integer check
        return syscallFramework->printBoolean(args[0]);
      } else if (argType->isIntegerTy()) {
        if (argType->isIntegerTy(8))
          dataType = loom::DataType::INT8;
        else if (argType->isIntegerTy(16))
          dataType = loom::DataType::INT16;
        else if (argType->isIntegerTy(32))
          dataType = loom::DataType::INT32;
        else if (argType->isIntegerTy(64))
          dataType = loom::DataType::INT64;
        else
          dataType = loom::DataType::INT32;  // fallback

        return syscallFramework->printInteger(args[0], dataType);
      } else if (argType->isFloatTy() || argType->isDoubleTy()) {
        dataType = argType->isFloatTy() ? loom::DataType::FLOAT32
                                        : loom::DataType::FLOAT64;
        return syscallFramework->printFloat(args[0], dataType);
      } else if (argType->isPointerTy()) {
        // Check if it's a string (pointer to i8) or other pointer
        // Note: LLVM opaque pointers - assume string for now
        return syscallFramework->printString(args[0]);

      } else {
        throw std::runtime_error("Unsupported type for $$print");
      }

    } else if (node.builtin_name == "print_addr") {
      if (args.size() != 1) {
        throw std::runtime_error("$$print_addr expects exactly 1 argument");
      }
      return syscallFramework->printPointer(args[0]);

    } else if (node.builtin_name == "exit") {
      if (args.size() != 1) {
        throw std::runtime_error("$$exit expects exactly 1 argument");
      }
      return syscallFramework->generateSyscall(loom::SyscallType::EXIT, args);

    } else if (node.builtin_name == "syscall") {
      if (args.size() < 1) {
        throw std::runtime_error("$$syscall expects at least 1 argument");
      }

      // Generic syscall - first argument is syscall number
      if (auto* constOp = llvm::dyn_cast<llvm::ConstantInt>(args[0])) {
        int64_t syscallNum = constOp->getSExtValue();
        std::vector<llvm::Value*> syscallArgs(args.begin() + 1, args.end());

        // Map common syscall numbers to typed calls
        switch (syscallNum) {
          case 1:  // write
            if (syscallArgs.size() >= 3) {
              return syscallFramework->generateSyscall(loom::SyscallType::WRITE,
                                                       syscallArgs);
            }
            break;
          case 0:  // read
            if (syscallArgs.size() >= 3) {
              return syscallFramework->generateSyscall(loom::SyscallType::READ,
                                                       syscallArgs);
            }
            break;
          case 60:         // exit (Linux)
          case 0x2000001:  // exit (macOS)
            if (!syscallArgs.empty()) {
              return syscallFramework->generateSyscall(loom::SyscallType::EXIT,
                                                       syscallArgs);
            }
            break;
          case 2:  // open
            if (!syscallArgs.empty()) {
              return syscallFramework->generateSyscall(loom::SyscallType::OPEN,
                                                       syscallArgs);
            }
            break;
          case 6:  // close
            if (!syscallArgs.empty()) {
              return syscallFramework->generateSyscall(loom::SyscallType::CLOSE,
                                                       syscallArgs);
            }
            break;
          case 50:  // malloc (custom number)
            if (!syscallArgs.empty()) {
              return syscallFramework->generateSyscall(
                  loom::SyscallType::MALLOC, syscallArgs);
            }
            break;
          case 51:  // free (custom number)
            if (!syscallArgs.empty()) {
              return syscallFramework->generateSyscall(loom::SyscallType::FREE,
                                                       syscallArgs);
            }
            break;
          case 99:  // get_file_size (custom number)
            if (!syscallArgs.empty()) {
              return syscallFramework->generateSyscall(
                  loom::SyscallType::GET_FILE_SIZE, syscallArgs);
            }
            break;
          default:
            return syscallFramework->generateSyscall(loom::SyscallType::GENERIC,
                                                     args);
        }
      }
      return syscallFramework->generateSyscall(loom::SyscallType::GENERIC,
                                               args);

    } else if (node.builtin_name == "socket") {
      if (args.size() != 3) {
        throw std::runtime_error("$$socket expects exactly 3 arguments");
      }
      return syscallFramework->generateNetworkSyscall("socket", args);

    } else if (node.builtin_name == "bind") {
      if (args.size() != 3) {
        throw std::runtime_error("$$bind expects exactly 3 arguments");
      }
      return syscallFramework->generateNetworkSyscall("bind", args);

    } else if (node.builtin_name == "listen") {
      if (args.size() != 2) {
        throw std::runtime_error("$$listen expects exactly 2 arguments");
      }
      return syscallFramework->generateNetworkSyscall("listen", args);

    } else if (node.builtin_name == "accept") {
      if (args.size() != 3) {
        throw std::runtime_error("$$accept expects exactly 3 arguments");
      }
      return syscallFramework->generateNetworkSyscall("accept", args);

    } else if (node.builtin_name == "connect") {
      if (args.size() != 3) {
        throw std::runtime_error("$$connect expects exactly 3 arguments");
      }
      return syscallFramework->generateNetworkSyscall("connect", args);

    } else if (node.builtin_name == "send") {
      if (args.size() != 4) {
        throw std::runtime_error("$$send expects exactly 4 arguments");
      }
      return syscallFramework->generateNetworkSyscall("send", args);

    } else if (node.builtin_name == "recv") {
      if (args.size() != 4) {
        throw std::runtime_error("$$recv expects exactly 4 arguments");
      }
      return syscallFramework->generateNetworkSyscall("recv", args);

    } else if (node.builtin_name == "closesocket") {
      if (args.size() != 1) {
        throw std::runtime_error("$$closesocket expects exactly 1 argument");
      }
      return syscallFramework->generateNetworkSyscall("closesocket", args);

    } else if (node.builtin_name == "WSAStartup") {
      if (args.size() != 2) {
        throw std::runtime_error("$$WSAStartup expects exactly 2 arguments");
      }
      return syscallFramework->generateNetworkSyscall("WSAStartup", args);
    } else if (node.builtin_name == "WSACleanup") {
      if (args.size() != 0) {
        throw std::runtime_error("$$WSACleanup expects no arguments");
      }
      return syscallFramework->generateNetworkSyscall("WSACleanup", args);
    } else if (node.builtin_name == "htons") {
      if (args.size() != 1) {
        throw std::runtime_error("$$htons expects exactly 1 argument");
      }
      return syscallFramework->generateNetworkSyscall("htons", args);

    } else if (node.builtin_name == "strlen") {
      if (args.size() != 1) {
        throw std::runtime_error("$$strlen expects exactly 1 argument");
      }
      // String length calculation for C-style null-terminated strings
      // Implementation: Loop through memory until null terminator
      llvm::Value* strPtr = args[0];

      // Convert to i8* if needed
      if (!strPtr->getType()->isPointerTy()) {
        throw std::runtime_error("$$strlen expects a string pointer argument");
      }

      // Create a basic block for the loop
      llvm::Function* current_function = builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* loop_block =
          llvm::BasicBlock::Create(*context, "strlen.loop", current_function);
      llvm::BasicBlock* end_block =
          llvm::BasicBlock::Create(*context, "strlen.end", current_function);

      // Initialize counter
      llvm::Value* counter =
          builder->CreateAlloca(builder->getInt32Ty(), nullptr, "counter");
      builder->CreateStore(builder->getInt32(0), counter);

      // Initialize current pointer
      llvm::Value* currentPtr =
          builder->CreateAlloca(strPtr->getType(), nullptr, "currentPtr");
      builder->CreateStore(strPtr, currentPtr);

      // Jump to loop
      builder->CreateBr(loop_block);

      // Loop block: check if current character is null
      builder->SetInsertPoint(loop_block);
      llvm::Value* loadedPtr =
          builder->CreateLoad(strPtr->getType(), currentPtr, "loadedPtr");
      llvm::Value* currentChar =
          builder->CreateLoad(builder->getInt8Ty(), loadedPtr, "currentChar");
      llvm::Value* isNull =
          builder->CreateICmpEQ(currentChar, builder->getInt8(0), "isNull");

      // If null, exit loop; otherwise continue
      llvm::BasicBlock* continue_block = llvm::BasicBlock::Create(
          *context, "strlen.continue", current_function);
      builder->CreateCondBr(isNull, end_block, continue_block);

      // Continue block: increment counter and pointer
      builder->SetInsertPoint(continue_block);
      llvm::Value* currentCount =
          builder->CreateLoad(builder->getInt32Ty(), counter, "currentCount");
      llvm::Value* newCount =
          builder->CreateAdd(currentCount, builder->getInt32(1), "newCount");
      builder->CreateStore(newCount, counter);

      llvm::Value* newPtr = builder->CreateGEP(builder->getInt8Ty(), loadedPtr,
                                               builder->getInt32(1), "newPtr");
      builder->CreateStore(newPtr, currentPtr);
      builder->CreateBr(loop_block);

      // End block: return the counter
      builder->SetInsertPoint(end_block);
      return builder->CreateLoad(builder->getInt32Ty(), counter,
                                 "strlen.result");

    } else if (node.builtin_name == "strcat") {
      if (args.size() != 2) {
        throw std::runtime_error("$$strcat expects exactly 2 arguments");
      }
      // String concatenation - for now, we'll implement a simple version
      // that returns a new allocated string (this would need malloc in real
      // implementation) For now, throw an error indicating it needs to be
      // implemented
      throw std::runtime_error(
          "$$strcat not yet implemented - requires memory management");

    } else {
      throw std::runtime_error("Unknown builtin function: $$" +
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
      // Windows: Use minimal linking but include necessary runtime support
      // Include libcmt for floating point and stack checking support
      // Include ws2_32 for networking support
      linkCmd = "clang \"" + objectFilename + "\" -o \"" + executableFilename +
                "\" -lkernel32 -lmsvcrt -lws2_32";
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

  // Enter function scope in symbol table
  if (symbol_table) {
    symbol_table->enterFunction(node.name);
  }

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

  // Leave function scope in symbol table
  if (symbol_table) {
    symbol_table->leaveFunction();
  }

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

// --- Memory Model Code Generation ---

// In codegen.cc, replace the whole function
// In codegen.cc, replace the whole function

llvm::Value* CodeGen::codegen(ReferenceExpr& node) {
  logMessage(loom::VerbosityLevel::DEBUG, "[CodeGen] Generating ReferenceExpr");

  if (!node.operand) {
    throw std::runtime_error("ReferenceExpr missing operand");
  }

  // Handle taking the address of a simple variable
  if (auto* identifier = dynamic_cast<Identifier*>(node.operand.get())) {
    auto it = named_values.find(identifier->name);
    if (it == named_values.end()) {
      throw std::runtime_error("CodeGen: Unknown variable name '" +
                               identifier->name + "'.");
    }
    return it->second;
  }

  // Handle taking the address of an array element: &arr[index]
  if (auto* index_expr = dynamic_cast<IndexExpr*>(node.operand.get())) {
    logMessage(loom::VerbosityLevel::DEBUG,
               "[CodeGen] Taking reference of array element");

    llvm::Value* slice_val = codegen(*index_expr->array);
    llvm::Value* index_val = codegen(*index_expr->index);

    if (!slice_val || !index_val) {
      throw std::runtime_error(
          "Failed to generate slice or index for reference");
    }

    llvm::Value* data_ptr =
        builder->CreateExtractValue(slice_val, 0, "slice.data.ptr");

    // For now, assume i8 elements for raw byte access in networking.
    llvm::Type* element_type = builder->getInt8Ty();

    return builder->CreateInBoundsGEP(element_type, data_ptr, index_val,
                                      "array.element.addr");
  }

  // Handle taking the address of a struct or union field: &obj.field
  if (auto* member_access =
          dynamic_cast<MemberAccessExpr*>(node.operand.get())) {
    logMessage(loom::VerbosityLevel::DEBUG,
               "[CodeGen] Taking reference of struct/union field");

    llvm::Value* object_ptr = nullptr;
    std::string object_name;
    if (auto* identifier =
            dynamic_cast<Identifier*>(member_access->object.get())) {
      object_name = identifier->name;
      auto it = named_values.find(object_name);
      if (it == named_values.end()) {
        throw std::runtime_error("CodeGen: Unknown variable: " + object_name);
      }
      object_ptr = it->second;
    } else {
      throw std::runtime_error(
          "Taking address of fields in complex expressions not yet supported");
    }
    const VariableInfo* var_info = symbol_table->lookupVariable(object_name);
    if (!var_info) {
      // Fallback: try to infer type information from LLVM types
      // This handles the case where variables exist in codegen but not in sema
      // symbol table

      auto type_it = variable_types.find(object_name);
      if (type_it != variable_types.end()) {
        llvm::Type* var_llvm_type = type_it->second;

        // If it's a struct type, try to use getFieldIndex
        if (auto* struct_type =
                llvm::dyn_cast<llvm::StructType>(var_llvm_type)) {
          std::string struct_name = struct_type->getName().str();

          int field_index =
              getFieldIndex(struct_name, member_access->member_name);
          if (field_index != -1) {
            return builder->CreateStructGEP(
                var_llvm_type, object_ptr, field_index,
                member_access->member_name + ".ptr");
          }
        }
        // As a last resort, assume field 0 for struct access
        std::cout << "[CodeGen] Using fallback struct field access for: "
                  << object_name << std::endl;
        if (llvm::dyn_cast<llvm::StructType>(var_llvm_type)) {
          return builder->CreateStructGEP(var_llvm_type, object_ptr, 0,
                                          member_access->member_name + ".ptr");
        }
      }

      throw std::runtime_error(
          "Could not find type info for variable in reference expression: " +
          object_name);
    }

    // Handle Structs
    if (auto* struct_type_node =
            dynamic_cast<StructTypeNode*>(var_info->type.get())) {
      int field_index = getFieldIndex(struct_type_node->struct_name,
                                      member_access->member_name);
      if (field_index != -1) {
        // FIX: Use the variable's name (object_name) as the key, not the type's
        // name.
        llvm::Type* struct_llvm_type = variable_types[object_name];
        return builder->CreateStructGEP(struct_llvm_type, object_ptr,
                                        field_index,
                                        member_access->member_name + ".ptr");
      }
    }

    // Handle Unions
    if (dynamic_cast<UnionTypeNode*>(var_info->type.get())) {
      // The address of any union member is the address of the union itself.
      return object_ptr;
    }
  }

  throw std::runtime_error(
      "Unsupported operand type for address-of operator: " +
      node.operand->toString());
}

llvm::Value* CodeGen::codegen(DereferenceExpr& node) {
  std::cout << "[CodeGen] Generating DereferenceExpr" << std::endl;

  if (!node.operand) {
    std::cout << "[CodeGen] ERROR: DereferenceExpr missing operand"
              << std::endl;
    return nullptr;
  }

  // Generate code for the pointer expression
  llvm::Value* pointer = codegen(*node.operand);
  if (!pointer) {
    std::cout << "[CodeGen] ERROR: Failed to generate pointer for dereference"
              << std::endl;
    return nullptr;
  }

  // Check if it's a pointer type
  if (!pointer->getType()->isPointerTy()) {
    std::cout << "[CodeGen] ERROR: Attempting to dereference non-pointer type"
              << std::endl;
    return nullptr;
  }  // Load the value from the pointer
  std::cout << "[CodeGen] Dereferencing pointer" << std::endl;
  return builder->CreateLoad(builder->getInt32Ty(), pointer, "deref");
}

// In codegen.cc, replace the whole function
// In codegen.cc, replace the whole function
llvm::Value* CodeGen::codegen(MemberAccessExpr& node) {
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating MemberAccessExpr: " + node.member_name);
  // Step 1: Get the pointer to the memory allocation of the object (the alloca)
  // and also try to get the type information.
  llvm::Value* object_ptr = nullptr;
  std::string object_name;
  const VariableInfo* var_info = nullptr;

  if (auto* identifier = dynamic_cast<Identifier*>(node.object.get())) {
    object_name = identifier->name;

    // Try unified symbol table first
    var_info = symbol_table->lookupVariable(object_name);
    if (var_info && var_info->llvm_value) {
      object_ptr = var_info->llvm_value;
    } else {
      // Fallback to old named_values
      auto it = named_values.find(object_name);
      if (it == named_values.end()) {
        throw std::runtime_error("CodeGen: Unknown variable name '" +
                                 object_name + "'.");
      }
      object_ptr = it->second;
    }
  } else {
    throw std::runtime_error("Member access on non-identifier not supported.");
  }

  // Step 2: Look up the variable's semantic type if we don't have it yet.
  if (!var_info) {
    var_info = symbol_table->lookupVariable(object_name);
  }
  if (!var_info || !var_info->type) {
    throw std::runtime_error("Could not find type info for variable '" +
                             object_name + "'.");
  }

  // Handle Union Member Access
  if (auto* union_type_node =
          dynamic_cast<UnionTypeNode*>(var_info->type.get())) {
    const UnionInfo* union_info =
        symbol_table->lookupUnion(union_type_node->union_name);
    if (!union_info) {
      throw std::runtime_error("CodeGen: Union definition not found for '" +
                               union_type_node->union_name + "'.");
    }

    for (const auto& field : union_info->fields) {
      if (field.first == node.member_name) {
        llvm::Type* field_llvm_type = typeToLLVMType(*field.second);
        // Bitcast the pointer and then load the value, providing the explicit
        // type.
        llvm::Value* generic_field_ptr =
            builder->CreateBitCast(object_ptr, builder->getPtrTy());
        return builder->CreateLoad(field_llvm_type, generic_field_ptr,
                                   node.member_name);
      }
    }
  }

  // Handle Struct Member Access - but also check if it's actually a union
  if (auto* struct_type_node =
          dynamic_cast<StructTypeNode*>(var_info->type.get())) {
    // First check if this is actually a union (common parsing issue)
    const UnionInfo* union_info =
        symbol_table->lookupUnion(struct_type_node->struct_name);
    if (union_info) {
      // This is actually a union, handle it as such
      for (const auto& field : union_info->fields) {
        if (field.first == node.member_name) {
          llvm::Type* field_llvm_type = typeToLLVMType(*field.second);
          // Bitcast the pointer and then load the value, providing the explicit
          // type.
          llvm::Value* generic_field_ptr =
              builder->CreateBitCast(object_ptr, builder->getPtrTy());
          return builder->CreateLoad(field_llvm_type, generic_field_ptr,
                                     node.member_name);
        }
      }
      throw std::runtime_error("CodeGen: Union field '" + node.member_name +
                               "' not found in union '" +
                               struct_type_node->struct_name + "'.");
    }

    // Handle as regular struct
    int field_index =
        getFieldIndex(struct_type_node->struct_name, node.member_name);
    if (field_index != -1) {
      llvm::Type* struct_llvm_type = variable_types[object_name];
      llvm::Value* field_ptr = builder->CreateStructGEP(
          struct_llvm_type, object_ptr, field_index, node.member_name + ".ptr");

      const StructInfo* struct_info =
          symbol_table->lookupStruct(struct_type_node->struct_name);
      llvm::Type* field_llvm_type =
          typeToLLVMType(*struct_info->fields[field_index].second);

      return builder->CreateLoad(field_llvm_type, field_ptr, node.member_name);
    }
  }

  logMessage(loom::VerbosityLevel::NORMAL,
             "[CodeGen] ERROR: Unsupported member access for type or field: " +
                 node.member_name);
  return nullptr;
}

// --- New Phase 1 Codegen Methods ---

llvm::Value* CodeGen::codegen(ForStmtNode& node) {
  std::cout << "[CodeGen] Generating ForStmtNode" << std::endl;

  // Get current function
  llvm::Function* current_function = builder->GetInsertBlock()->getParent();

  // Create basic blocks for the for loop
  llvm::BasicBlock* init_block =
      llvm::BasicBlock::Create(*context, "for.init", current_function);
  llvm::BasicBlock* condition_block =
      llvm::BasicBlock::Create(*context, "for.condition");
  llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*context, "for.body");
  llvm::BasicBlock* increment_block =
      llvm::BasicBlock::Create(*context, "for.increment");
  llvm::BasicBlock* exit_block = llvm::BasicBlock::Create(*context, "for.exit");

  // Jump to initialization
  builder->CreateBr(init_block);

  // --- Initialization Block ---
  builder->SetInsertPoint(init_block);

  // Handle range-based for loops
  if (auto* range_expr = dynamic_cast<RangeExpr*>(node.iterable.get())) {
    // Create loop variable and initialize with range start
    llvm::Type* loop_var_type = builder->getInt32Ty();
    llvm::Value* loop_var_alloca =
        builder->CreateAlloca(loop_var_type, nullptr, node.variable_name);

    // Generate start value
    llvm::Value* start_val = codegen(*range_expr->start);
    if (!start_val) return nullptr;
    builder->CreateStore(start_val, loop_var_alloca);

    // Add loop variable to symbol table
    named_values[node.variable_name] = loop_var_alloca;
    variable_types[node.variable_name] = loop_var_type;

    // Generate end value
    llvm::Value* end_val = codegen(*range_expr->end);
    if (!end_val) return nullptr;

    // Jump to condition check
    builder->CreateBr(condition_block);

    // --- Condition Block ---
    condition_block->insertInto(current_function);
    builder->SetInsertPoint(condition_block);

    // Load current loop variable value
    llvm::Value* current_val =
        builder->CreateLoad(loop_var_type, loop_var_alloca, "loop.var");

    // Compare with end value
    llvm::Value* condition;
    if (range_expr->inclusive) {
      condition = builder->CreateICmpSLE(current_val, end_val, "loop.cond");
    } else {
      condition = builder->CreateICmpSLT(current_val, end_val, "loop.cond");
    }

    builder->CreateCondBr(condition, body_block, exit_block);

    // --- Body Block ---
    body_block->insertInto(current_function);
    builder->SetInsertPoint(body_block);

    // Generate body statements
    for (const auto& stmt : node.body) {
      codegen(*stmt);
    }

    // Jump to increment
    if (!builder->GetInsertBlock()->getTerminator()) {
      builder->CreateBr(increment_block);
    }

    // --- Increment Block ---
    increment_block->insertInto(current_function);
    builder->SetInsertPoint(increment_block);

    // Increment loop variable
    llvm::Value* current_val_inc =
        builder->CreateLoad(loop_var_type, loop_var_alloca, "loop.var.inc");
    llvm::Value* next_val =
        builder->CreateAdd(current_val_inc, builder->getInt32(1), "loop.next");
    builder->CreateStore(next_val, loop_var_alloca);

    // Jump back to condition
    builder->CreateBr(condition_block);
  } else {
    // Handle other iterable types (arrays, etc.) in the future
    std::cout << "[CodeGen] ForStmtNode: Non-range iterables not yet supported"
              << std::endl;
    return nullptr;
  }

  // --- Exit Block ---
  exit_block->insertInto(current_function);
  builder->SetInsertPoint(exit_block);

  return nullptr;  // For statements don't return values
}

llvm::Value* CodeGen::codegen(ArrayLiteralExpr& node) {
  std::cout << "[CodeGen] Generating ArrayLiteralExpr with "
            << node.elements.size() << " elements" << std::endl;

  if (node.elements.empty()) {
    // Empty array - create a null pointer for now
    return llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(*context));
  }

  // For simplicity, create an array on the stack
  // In a real implementation, you might want heap allocation for large arrays

  // Determine element type from first element
  llvm::Value* first_element = codegen(*node.elements[0]);
  if (!first_element) return nullptr;

  llvm::Type* element_type = first_element->getType();
  llvm::Type* array_type =
      llvm::ArrayType::get(element_type, node.elements.size());

  // Allocate array on stack
  llvm::Value* array_alloca =
      builder->CreateAlloca(array_type, nullptr, "array.literal");

  // Store each element
  for (size_t i = 0; i < node.elements.size(); ++i) {
    llvm::Value* element_val =
        (i == 0) ? first_element : codegen(*node.elements[i]);
    if (!element_val) return nullptr;

    // Get pointer to array element
    std::vector<llvm::Value*> indices = {
        builder->getInt32(0),  // Array base
        builder->getInt32(i)   // Element index
    };

    llvm::Value* element_ptr = builder->CreateInBoundsGEP(
        array_type, array_alloca, indices, "elem.ptr");
    builder->CreateStore(element_val, element_ptr);
  }
  // Create slice struct { ptr, len }
  // Get pointer to first element
  std::vector<llvm::Value*> indices = {builder->getInt32(0),
                                       builder->getInt32(0)};
  llvm::Value* array_ptr = builder->CreateInBoundsGEP(array_type, array_alloca,
                                                      indices, "array.ptr");

  // Create slice struct type
  std::vector<llvm::Type*> slice_fields = {
      llvm::PointerType::getUnqual(*context),  // data pointer
      builder->getInt64Ty()                    // length
  };
  llvm::Type* slice_type = llvm::StructType::get(*context, slice_fields);

  // Allocate slice struct on stack
  llvm::Value* slice_alloca =
      builder->CreateAlloca(slice_type, nullptr, "slice.tmp");

  // Store pointer field (index 0)
  llvm::Value* ptr_field =
      builder->CreateStructGEP(slice_type, slice_alloca, 0, "slice.ptr.field");
  builder->CreateStore(array_ptr, ptr_field);

  // Store length field (index 1)
  llvm::Value* len_field =
      builder->CreateStructGEP(slice_type, slice_alloca, 1, "slice.len.field");
  llvm::Value* length = builder->getInt64(node.elements.size());
  builder->CreateStore(length, len_field);

  // Load and return the slice struct
  return builder->CreateLoad(slice_type, slice_alloca, "slice.val");
}

llvm::Value* CodeGen::codegen(IndexExpr& node) {
  std::cout << "[CodeGen] Generating IndexExpr" << std::endl;

  // Generate array expression (should be a slice struct)
  llvm::Value* slice_val = codegen(*node.array);
  if (!slice_val) return nullptr;

  // Generate index expression
  llvm::Value* index = codegen(*node.index);
  if (!index) return nullptr;

  // The slice should be a struct { ptr, len }
  // Extract the pointer field (index 0) from the slice struct value
  llvm::Value* array_ptr =
      builder->CreateExtractValue(slice_val, 0, "array.ptr");

  // Calculate element address: array_ptr + index
  llvm::Value* element_ptr = builder->CreateInBoundsGEP(
      builder->getInt32Ty(), array_ptr, index, "elem.ptr");

  // Load the element value
  return builder->CreateLoad(builder->getInt32Ty(), element_ptr, "elem.val");
}

llvm::Value* CodeGen::codegen(RangeExpr& node) {
  std::cout << "[CodeGen] Generating RangeExpr" << std::endl;

  // Ranges themselves don't generate runtime values in this simple
  // implementation They are used as control structures in for loops For now,
  // just return null - ranges are handled specially in for loops
  std::cout << "[CodeGen] RangeExpr used outside of for loop context"
            << std::endl;
  // Avoid unused parameter warning
  (void)node;
  return nullptr;
}

// --- Struct-related code generation ---

llvm::Value* CodeGen::codegen(StructDeclNode& node) {
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating struct declaration: " + node.name);

  // Check for attributes
  bool is_packed = false;
  for (const auto& attr : node.attributes) {
    if (attr && attr->name == "packed") {
      is_packed = true;
      logMessage(loom::VerbosityLevel::DEBUG,
                 "[CodeGen] Struct " + node.name + " is packed");
    }
  }

  // Create LLVM struct type
  std::vector<llvm::Type*> field_types;
  for (const auto& field : node.fields) {
    if (field && field->type) {
      llvm::Type* field_type = typeToLLVMType(*field->type);
      if (field_type) {
        field_types.push_back(field_type);
      } else {
        std::cerr << "[CodeGen] Error: Unknown field type in struct "
                  << node.name << std::endl;
        return nullptr;
      }
    }
  }

  // Create the struct type and register it
  llvm::StructType* struct_type;
  if (is_packed) {
    struct_type = llvm::StructType::create(*context, field_types, node.name,
                                           /*isPacked=*/true);
  } else {
    struct_type = llvm::StructType::create(*context, field_types, node.name);
  }
  (void)struct_type;  // Suppress unused variable warning

  // TODO: Store struct type information for later use in struct literals
  // For now, we'll rely on LLVM's type system

  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Struct declaration complete: " + node.name);
  return nullptr;  // Struct declarations don't return values
}

llvm::Value* CodeGen::codegen(StructLiteralExpr& node) {
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating struct literal: " + node.struct_name);
  if (symbol_table && symbol_table->isUnionDefined(node.struct_name)) {
    logMessage(loom::VerbosityLevel::DEBUG,
               "[CodeGen] Handling as union literal: " + node.struct_name);

    llvm::StructType* union_type =
        llvm::StructType::getTypeByName(*context, node.struct_name);
    if (!union_type) {
      std::cerr << "[CodeGen] Error: Unknown union type: " << node.struct_name
                << std::endl;
      return nullptr;
    }

    llvm::AllocaInst* union_alloca = builder->CreateAlloca(
        union_type, nullptr, node.struct_name + "_literal");

    if (!node.field_values.empty()) {
      const auto& field_pair = node.field_values[0];
      llvm::Value* field_value = codegen(*field_pair.second);

      // FIX: Bitcast to the generic 'ptr' type.
      llvm::Type* generic_ptr_type = builder->getPtrTy();
      llvm::Value* field_ptr =
          builder->CreateBitCast(union_alloca, generic_ptr_type);

      builder->CreateStore(field_value, field_ptr);
    }

    return builder->CreateLoad(union_type, union_alloca);
  }
  // Get the struct type from LLVM context
  llvm::StructType* struct_type =
      llvm::StructType::getTypeByName(*context, node.struct_name);
  if (!struct_type) {
    std::cerr << "[CodeGen] Error: Unknown struct type: " << node.struct_name
              << std::endl;
    return nullptr;
  }

  // Allocate struct on stack
  llvm::AllocaInst* struct_alloca = builder->CreateAlloca(
      struct_type, nullptr, node.struct_name + "_literal");

  // Initialize fields
  for (size_t i = 0; i < node.field_values.size(); ++i) {
    const auto& field_pair = node.field_values[i];
    const std::string& field_name = field_pair.first;
    const auto& field_value_expr = field_pair.second;

    if (field_value_expr) {
      llvm::Value* field_value = codegen(*field_value_expr);
      if (field_value) {
        // Get pointer to field and store value
        llvm::Value* field_ptr = builder->CreateStructGEP(
            struct_type, struct_alloca, i, field_name + "_field");
        builder->CreateStore(field_value, field_ptr);
      }
    }
  }

  // Load and return the struct value
  llvm::Value* struct_value = builder->CreateLoad(struct_type, struct_alloca,
                                                  node.struct_name + "_value");

  logMessage(
      loom::VerbosityLevel::DEBUG,
      "[CodeGen] Struct literal generation complete: " + node.struct_name);
  return struct_value;
}

llvm::Value* CodeGen::codegen(UnaryExpr& node) {
  std::cout << "[CodeGen] Generating UnaryExpr" << std::endl;

  llvm::Value* operand = codegen(*node.right);
  if (!operand) {
    return nullptr;
  }

  switch (node.op.type) {
    case TokenType::TOKEN_MINUS:
      if (operand->getType()->isFloatingPointTy()) {
        return builder->CreateFNeg(operand, "fneg.tmp");
      } else {
        return builder->CreateNeg(operand, "neg.tmp");
      }
    case TokenType::TOKEN_BANG:
      // For boolean negation, assume i1 type
      return builder->CreateNot(operand, "not.tmp");
    case TokenType::TOKEN_BITWISE_NOT:
      // For bitwise negation (~)
      return builder->CreateNot(operand, "bitnot.tmp");
    default:
      throw std::runtime_error("CodeGen: Unknown unary operator.");
  }
}

llvm::Value* CodeGen::codegen(CastExpr& node) {
  std::cout << "[CodeGen] Generating CastExpr" << std::endl;

  // Generate the expression to cast
  llvm::Value* expr_val = codegen(*node.expression);
  if (!expr_val) {
    return nullptr;
  }

  // Get the target LLVM type
  llvm::Type* target_type = typeToLLVMType(*node.target_type);
  if (!target_type) {
    throw std::runtime_error(
        "CodeGen: Cannot convert target type to LLVM type");
  }

  // If types are already the same, no cast needed
  if (expr_val->getType() == target_type) {
    return expr_val;
  }

  // Handle various cast scenarios
  llvm::Type* source_type = expr_val->getType();

  // Integer to integer casts
  if (source_type->isIntegerTy() && target_type->isIntegerTy()) {
    unsigned source_bits = source_type->getIntegerBitWidth();
    unsigned target_bits = target_type->getIntegerBitWidth();

    if (source_bits < target_bits) {
      // Sign extend for larger types
      return builder->CreateSExt(expr_val, target_type, "sext.cast");
    } else if (source_bits > target_bits) {
      // Truncate for smaller types
      return builder->CreateTrunc(expr_val, target_type, "trunc.cast");
    }
  }

  // Integer to float casts
  if (source_type->isIntegerTy() && target_type->isFloatingPointTy()) {
    return builder->CreateSIToFP(expr_val, target_type, "int2fp.cast");
  }

  // Float to integer casts
  if (source_type->isFloatingPointTy() && target_type->isIntegerTy()) {
    return builder->CreateFPToSI(expr_val, target_type, "fp2int.cast");
  }

  // Float to float casts
  if (source_type->isFloatingPointTy() && target_type->isFloatingPointTy()) {
    if (source_type->getTypeID() < target_type->getTypeID()) {
      return builder->CreateFPExt(expr_val, target_type, "fpext.cast");
    } else {
      return builder->CreateFPTrunc(expr_val, target_type, "fptrunc.cast");
    }
  }

  // Pointer/array casts - use bitcast for now
  if (source_type->isPointerTy() && target_type->isPointerTy()) {
    return builder->CreateBitCast(expr_val, target_type, "ptr.cast");
  }

  // Default: try bitcast
  return builder->CreateBitCast(expr_val, target_type, "bitcast.cast");
}

void CodeGen::logMessage(loom::VerbosityLevel required,
                         const std::string& message) const {
  if (verbosity_level >= required) {
    std::cout << message << std::endl;
  }
}

llvm::Value* CodeGen::codegen(UnionDeclNode& node) {
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating union declaration: " + node.name);

  // Check for attributes
  bool is_packed = false;
  for (const auto& attr : node.attributes) {
    if (attr && attr->name == "packed") {
      is_packed = true;
      logMessage(loom::VerbosityLevel::DEBUG,
                 "[CodeGen] Union " + node.name + " is packed");
    }
  }

  // Find the largest field type for union size
  llvm::Type* largest_type = nullptr;
  size_t largest_size = 0;

  for (const auto& field : node.fields) {
    if (field && field->type) {
      llvm::Type* field_type = typeToLLVMType(*field->type);
      if (field_type) {
        size_t field_size =
            module->getDataLayout().getTypeAllocSize(field_type);
        if (field_size > largest_size) {
          largest_size = field_size;
          largest_type = field_type;
        }
      } else {
        std::cerr << "[CodeGen] Error: Unknown field type in union "
                  << node.name << std::endl;
        return nullptr;
      }
    }
  }

  if (!largest_type) {
    std::cerr << "[CodeGen] Error: No valid fields in union " << node.name
              << std::endl;
    return nullptr;
  }
  // Create the union as a struct with a single field of the largest type
  // Union access will be handled through bitcasts
  std::vector<llvm::Type*> union_fields = {largest_type};
  llvm::StructType* union_type;
  if (is_packed) {
    union_type = llvm::StructType::create(*context, union_fields, node.name,
                                          /*isPacked=*/true);
  } else {
    union_type = llvm::StructType::create(*context, union_fields, node.name);
  }
  (void)union_type;  // Suppress unused variable warning

  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Union declaration complete: " + node.name);
  return nullptr;  // Union declarations don't return values
}

llvm::Value* CodeGen::codegen(UnionLiteralExpr& node) {
  logMessage(loom::VerbosityLevel::DEBUG,
             "[CodeGen] Generating union literal: " + node.union_name);

  // Get the union type
  llvm::StructType* union_type =
      llvm::StructType::getTypeByName(*context, node.union_name);

  if (!union_type) {
    std::cerr << "[CodeGen] Error: Unknown union type: " << node.union_name
              << std::endl;
    return nullptr;
  }

  // Generate the value expression
  llvm::Value* value = nullptr;
  if (node.value) {
    value = codegen(*node.value);
    if (!value) {
      return nullptr;
    }
  }

  // Create union storage
  llvm::Value* union_storage =
      builder->CreateAlloca(union_type, nullptr, "union.tmp");
  if (value) {
    // Bitcast the union storage to the value's type and store
    llvm::Value* field_ptr = builder->CreateBitCast(
        union_storage, llvm::PointerType::get(*context, 0), "union.field.ptr");
    builder->CreateStore(value, field_ptr);
  }

  // Load and return the union value
  return builder->CreateLoad(union_type, union_storage, "union.val");
}

// Helper method to clone type nodes for symbol table
std::shared_ptr<TypeNode> CodeGen::cloneTypeNode(TypeNode* type) const {
  if (!type) return nullptr;

  // Handle different type node kinds
  if (auto* int_type = dynamic_cast<IntegerTypeNode*>(type)) {
    return std::make_shared<IntegerTypeNode>(
        int_type->location, int_type->bit_width, int_type->is_signed);
  } else if (auto* float_type = dynamic_cast<FloatTypeNode*>(type)) {
    return std::make_shared<FloatTypeNode>(float_type->location,
                                           float_type->bit_width);
  } else if (auto* bool_type = dynamic_cast<BooleanTypeNode*>(type)) {
    return std::make_shared<BooleanTypeNode>(bool_type->location);
  } else if (auto* string_type = dynamic_cast<StringTypeNode*>(type)) {
    return std::make_shared<StringTypeNode>(string_type->location);
  } else if (auto* struct_type = dynamic_cast<StructTypeNode*>(type)) {
    return std::make_shared<StructTypeNode>(struct_type->location,
                                            struct_type->struct_name);
  } else if (auto* union_type = dynamic_cast<UnionTypeNode*>(type)) {
    return std::make_shared<UnionTypeNode>(union_type->location,
                                           union_type->union_name);
  } else if (auto* slice_type = dynamic_cast<SliceTypeNode*>(type)) {
    auto element_type_shared = cloneTypeNode(slice_type->element_type.get());
    // Convert shared_ptr to unique_ptr for the constructor
    std::unique_ptr<TypeNode> element_type_unique;
    if (element_type_shared) {
      element_type_unique = std::unique_ptr<TypeNode>(
          cloneTypeNodeAsUnique(slice_type->element_type.get()));
    }
    return std::make_shared<SliceTypeNode>(slice_type->location,
                                           std::move(element_type_unique));
  } else if (auto* ptr_type = dynamic_cast<RawPointerTypeNode*>(type)) {
    auto pointed_type_shared = cloneTypeNode(ptr_type->pointed_type.get());
    // Convert shared_ptr to unique_ptr for the constructor
    std::unique_ptr<TypeNode> pointed_type_unique;
    if (pointed_type_shared) {
      pointed_type_unique = std::unique_ptr<TypeNode>(
          cloneTypeNodeAsUnique(ptr_type->pointed_type.get()));
    }
    return std::make_shared<RawPointerTypeNode>(ptr_type->location,
                                                std::move(pointed_type_unique));
  }

  // Add more type cases as needed
  return nullptr;
}

// Helper method to clone type nodes as unique_ptr (for constructors that need
// unique_ptr)
std::unique_ptr<TypeNode> CodeGen::cloneTypeNodeAsUnique(TypeNode* type) const {
  if (!type) return nullptr;

  // Handle different type node kinds
  if (auto* int_type = dynamic_cast<IntegerTypeNode*>(type)) {
    return std::make_unique<IntegerTypeNode>(
        int_type->location, int_type->bit_width, int_type->is_signed);
  } else if (auto* float_type = dynamic_cast<FloatTypeNode*>(type)) {
    return std::make_unique<FloatTypeNode>(float_type->location,
                                           float_type->bit_width);
  } else if (auto* bool_type = dynamic_cast<BooleanTypeNode*>(type)) {
    return std::make_unique<BooleanTypeNode>(bool_type->location);
  } else if (auto* string_type = dynamic_cast<StringTypeNode*>(type)) {
    return std::make_unique<StringTypeNode>(string_type->location);
  } else if (auto* struct_type = dynamic_cast<StructTypeNode*>(type)) {
    return std::make_unique<StructTypeNode>(struct_type->location,
                                            struct_type->struct_name);
  } else if (auto* union_type = dynamic_cast<UnionTypeNode*>(type)) {
    return std::make_unique<UnionTypeNode>(union_type->location,
                                           union_type->union_name);
  } else if (auto* slice_type = dynamic_cast<SliceTypeNode*>(type)) {
    auto element_type = cloneTypeNodeAsUnique(slice_type->element_type.get());
    return std::make_unique<SliceTypeNode>(slice_type->location,
                                           std::move(element_type));
  } else if (auto* ptr_type = dynamic_cast<RawPointerTypeNode*>(type)) {
    auto pointed_type = cloneTypeNodeAsUnique(ptr_type->pointed_type.get());
    return std::make_unique<RawPointerTypeNode>(ptr_type->location,
                                                std::move(pointed_type));
  }

  // Add more type cases as needed
  return nullptr;
}
