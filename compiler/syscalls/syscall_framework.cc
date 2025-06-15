#include "syscall_framework.hh"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InlineAsm.h>

#include <iostream>

namespace loom {

SyscallFramework::SyscallFramework(llvm::LLVMContext* context,
                                   llvm::IRBuilder<>* builder,
                                   llvm::Module* module)
    : context(context), builder(builder), module(module) {
  // Declare platform-specific APIs based on target
  Platform platform = detectPlatform();
  switch (platform) {
    case Platform::WINDOWS:
      declareWindowsAPIs();
      break;
    case Platform::LINUX:
      declareLinuxSyscalls();
      break;
    case Platform::MACOS:
      declareMacOSSyscalls();
      break;
    default:
      // If platform detection fails, assume the current platform
      std::cout << "[SyscallFramework] Platform detection failed, using "
                   "compile-time platform"
                << std::endl;
#ifdef _WIN32
      declareWindowsAPIs();
#elif defined(__linux__)
      declareLinuxSyscalls();
#elif defined(__APPLE__)
      declareMacOSSyscalls();
#else
      throw std::runtime_error("Unsupported platform for syscall framework");
#endif
  }
}

SyscallFramework::Platform SyscallFramework::detectPlatform() {
  std::string targetTriple = module->getTargetTriple().str();

  // If target triple is empty, use compile-time detection
  if (targetTriple.empty()) {
#ifdef _WIN32
    return Platform::WINDOWS;
#elif defined(__linux__)
    return Platform::LINUX;
#elif defined(__APPLE__)
    return Platform::MACOS;
#else
    return Platform::UNKNOWN;
#endif
  }

  // Use target triple if available
  if (targetTriple.find("windows") != std::string::npos ||
      targetTriple.find("win32") != std::string::npos ||
      targetTriple.find("mingw") != std::string::npos) {
    return Platform::WINDOWS;
  } else if (targetTriple.find("linux") != std::string::npos) {
    return Platform::LINUX;
  } else if (targetTriple.find("darwin") != std::string::npos ||
             targetTriple.find("macos") != std::string::npos) {
    return Platform::MACOS;
  }
  return Platform::UNKNOWN;
}

llvm::Value* SyscallFramework::generateSyscall(SyscallType type,
                                               std::vector<llvm::Value*>& args,
                                               DataType dataType) {
  Platform platform = detectPlatform();

  switch (platform) {
    case Platform::WINDOWS:
      return generateWindowsSyscall(type, args, dataType);
    case Platform::LINUX:
      return generateLinuxSyscall(type, args, dataType);
    case Platform::MACOS:
      return generateMacOSSyscall(type, args, dataType);
    default:
      throw std::runtime_error("Unsupported platform for syscall generation");
  }
}

// ============================================================================
// TYPE-SPECIFIC PRINT FUNCTIONS (LIBC-FREE)
// ============================================================================

llvm::Value* SyscallFramework::printString(llvm::Value* stringPtr,
                                           llvm::Value* length) {
  if (!length) {
    length = calculateStringLength(stringPtr);
  }

  std::vector<llvm::Value*> args = {stringPtr, length};
  return generateSyscall(SyscallType::PRINT_STRING, args);
}

llvm::Value* SyscallFramework::printInteger(llvm::Value* intValue,
                                            DataType type, int base) {
  // Convert integer to string without libc
  llvm::Value* stringBuffer = integerToString(intValue, type, base);

  // Calculate length of the converted string
  llvm::Value* length = calculateStringLength(stringBuffer);

  std::vector<llvm::Value*> args = {stringBuffer, length};
  return generateSyscall(SyscallType::PRINT_STRING, args);
}

llvm::Value* SyscallFramework::printFloat(llvm::Value* floatValue,
                                          DataType type, int precision) {
  // Convert float to string without libc
  llvm::Value* stringBuffer = floatToString(floatValue, type, precision);

  // Calculate length of the converted string
  llvm::Value* length = calculateStringLength(stringBuffer);

  std::vector<llvm::Value*> args = {stringBuffer, length};
  return generateSyscall(SyscallType::PRINT_STRING, args);
}

llvm::Value* SyscallFramework::printBoolean(llvm::Value* boolValue) {
  // Create "true" and "false" strings
  llvm::Constant* trueStr = builder->CreateGlobalString("true", "bool_true");
  llvm::Constant* falseStr = builder->CreateGlobalString("false", "bool_false");

  // Select string based on boolean value
  llvm::Value* selectedStr =
      builder->CreateSelect(boolValue, trueStr, falseStr);
  llvm::Value* length = builder->CreateSelect(boolValue,
                                              builder->getInt64(4),   // "true"
                                              builder->getInt64(5));  // "false"

  std::vector<llvm::Value*> args = {selectedStr, length};
  return generateSyscall(SyscallType::PRINT_STRING, args);
}

llvm::Value* SyscallFramework::printPointer(llvm::Value* ptrValue) {
  // Convert pointer to hex string (0x...)
  llvm::Value* ptrAsInt =
      builder->CreatePtrToInt(ptrValue, builder->getInt64Ty());

  // Allocate buffer for "0x" + 16 hex digits + null terminator
  llvm::Value* hexBuffer = createStaticBuffer(19, "hex_buffer");

  // Add "0x" prefix
  llvm::Value* prefixPtr =
      builder->CreateGEP(builder->getInt8Ty(), hexBuffer, builder->getInt64(0));
  builder->CreateStore(builder->getInt8('0'), prefixPtr);
  llvm::Value* xPtr =
      builder->CreateGEP(builder->getInt8Ty(), hexBuffer, builder->getInt64(1));
  builder->CreateStore(builder->getInt8('x'), xPtr);

  // Convert to hex digits (libc-free)
  llvm::Constant* hexDigits =
      builder->CreateGlobalString("0123456789abcdef", "hex_digits");
  llvm::Value* hexDigitsPtr = builder->CreatePointerCast(
      hexDigits, llvm::PointerType::getUnqual(*context));

  for (int i = 15; i >= 0; i--) {
    llvm::Value* shift = builder->getInt64(i * 4);
    llvm::Value* shifted = builder->CreateLShr(ptrAsInt, shift);
    llvm::Value* nibble = builder->CreateAnd(shifted, builder->getInt64(0xF));

    llvm::Value* hexCharPtr =
        builder->CreateGEP(builder->getInt8Ty(), hexDigitsPtr, nibble);
    llvm::Value* hexChar =
        builder->CreateLoad(builder->getInt8Ty(), hexCharPtr);

    llvm::Value* bufferPos = builder->CreateGEP(
        builder->getInt8Ty(), hexBuffer, builder->getInt64(2 + (15 - i)));
    builder->CreateStore(hexChar, bufferPos);
  }

  // Add null terminator
  llvm::Value* nullPos = builder->CreateGEP(builder->getInt8Ty(), hexBuffer,
                                            builder->getInt64(18));
  builder->CreateStore(builder->getInt8(0), nullPos);

  std::vector<llvm::Value*> args = {hexBuffer, builder->getInt64(18)};
  return generateSyscall(SyscallType::PRINT_STRING, args);
}

// ============================================================================
// UTILITY FUNCTIONS (LIBC-FREE)
// ============================================================================

llvm::Value* SyscallFramework::calculateStringLength(llvm::Value* stringPtr) {
  // Calculate string length without strlen (libc-free)
  llvm::BasicBlock* entryBB = builder->GetInsertBlock();
  llvm::Function* currentFunc = entryBB->getParent();

  llvm::BasicBlock* loopBB =
      llvm::BasicBlock::Create(*context, "strlen_loop", currentFunc);
  llvm::BasicBlock* exitBB =
      llvm::BasicBlock::Create(*context, "strlen_exit", currentFunc);

  // Initialize counter
  llvm::Value* counter =
      builder->CreateAlloca(builder->getInt64Ty(), nullptr, "strlen_counter");
  builder->CreateStore(builder->getInt64(0), counter);

  builder->CreateBr(loopBB);

  // Loop to count characters
  builder->SetInsertPoint(loopBB);
  llvm::Value* currentCount =
      builder->CreateLoad(builder->getInt64Ty(), counter);
  llvm::Value* charPtr =
      builder->CreateGEP(builder->getInt8Ty(), stringPtr, currentCount);
  llvm::Value* currentChar = builder->CreateLoad(builder->getInt8Ty(), charPtr);

  // Check if null terminator
  llvm::Value* isNull = builder->CreateICmpEQ(currentChar, builder->getInt8(0));

  // Increment counter
  llvm::Value* nextCount =
      builder->CreateAdd(currentCount, builder->getInt64(1));
  builder->CreateStore(nextCount, counter);

  builder->CreateCondBr(isNull, exitBB, loopBB);

  // Return length
  builder->SetInsertPoint(exitBB);
  return builder->CreateLoad(builder->getInt64Ty(), counter);
}

llvm::Value* SyscallFramework::integerToString(llvm::Value* intValue,
                                               DataType type, int base) {
  // Convert integer to string without libc (supports bases 2, 8, 10, 16)
  if (base < 2 || base > 16) {
    throw std::runtime_error("Unsupported base for integer conversion");
  }

  // Allocate buffer (64 chars should be enough for any integer in any base)
  llvm::Value* buffer = createStaticBuffer(65, "int_str_buffer");

  // Handle different integer sizes
  llvm::Type* intType = intValue->getType();
  if (!intType->isIntegerTy()) {
    throw std::runtime_error("Value is not an integer type");
  }

  // Convert to 64-bit for uniform processing
  llvm::Value* int64Val;
  if (type == DataType::INT8 || type == DataType::INT16 ||
      type == DataType::INT32) {
    int64Val = builder->CreateSExt(intValue, builder->getInt64Ty());
  } else if (type == DataType::UINT8 || type == DataType::UINT16 ||
             type == DataType::UINT32) {
    int64Val = builder->CreateZExt(intValue, builder->getInt64Ty());
  } else {
    int64Val = intValue;
  }

  // Simple conversion: start at the beginning of buffer
  llvm::Value* writePos =
      builder->CreateAlloca(builder->getInt64Ty(), nullptr, "write_pos");
  builder->CreateStore(builder->getInt64(0), writePos);

  // Handle negative numbers
  llvm::Value* isNegative =
      builder->CreateICmpSLT(int64Val, builder->getInt64(0));
  llvm::Value* absVal =
      builder->CreateSelect(isNegative, builder->CreateNeg(int64Val), int64Val);

  // Write minus sign if negative
  llvm::BasicBlock* entryBB = builder->GetInsertBlock();
  llvm::Function* currentFunc = entryBB->getParent();
  llvm::BasicBlock* signBB =
      llvm::BasicBlock::Create(*context, "sign_check", currentFunc);
  llvm::BasicBlock* digitsBB =
      llvm::BasicBlock::Create(*context, "digits", currentFunc);

  builder->CreateCondBr(isNegative, signBB, digitsBB);

  // Add minus sign
  builder->SetInsertPoint(signBB);
  llvm::Value* minusPtr =
      builder->CreateGEP(builder->getInt8Ty(), buffer, builder->getInt64(0));
  builder->CreateStore(builder->getInt8('-'), minusPtr);
  builder->CreateStore(builder->getInt64(1), writePos);
  builder->CreateBr(digitsBB);

  // Convert digits
  builder->SetInsertPoint(digitsBB);
  intToStringDigits(absVal, buffer, writePos, type, base);

  return buffer;
}

llvm::Value* SyscallFramework::intToStringDigits(
    llvm::Value* intValue, llvm::Value* buffer, llvm::Value* writePos,
    DataType /*type*/, int base) {  // Simplified digit conversion - write
                                    // digits in reverse order then reverse
  llvm::Constant* digitChars =
      builder->CreateGlobalString("0123456789abcdef", "digit_chars");
  llvm::Value* digitPtr = builder->CreatePointerCast(
      digitChars, llvm::PointerType::getUnqual(*context));

  llvm::BasicBlock* entryBB = builder->GetInsertBlock();
  llvm::Function* currentFunc = entryBB->getParent();

  llvm::BasicBlock* loopBB =
      llvm::BasicBlock::Create(*context, "digit_loop", currentFunc);
  llvm::BasicBlock* exitBB =
      llvm::BasicBlock::Create(*context, "digit_exit", currentFunc);

  // Initialize working value and current position
  llvm::Value* workingVal =
      builder->CreateAlloca(builder->getInt64Ty(), nullptr, "working_val");
  llvm::Value* currentPos =
      builder->CreateAlloca(builder->getInt64Ty(), nullptr, "current_pos");

  builder->CreateStore(intValue, workingVal);
  llvm::Value* startPos = builder->CreateLoad(builder->getInt64Ty(), writePos);
  builder->CreateStore(startPos, currentPos);

  // Handle zero case specially
  llvm::BasicBlock* zeroBB =
      llvm::BasicBlock::Create(*context, "zero_case", currentFunc);
  llvm::Value* isZero = builder->CreateICmpEQ(intValue, builder->getInt64(0));
  builder->CreateCondBr(isZero, zeroBB, loopBB);

  // Zero case: just store '0'
  builder->SetInsertPoint(zeroBB);
  llvm::Value* zeroPtr =
      builder->CreateGEP(builder->getInt8Ty(), buffer, startPos);
  builder->CreateStore(builder->getInt8('0'), zeroPtr);
  llvm::Value* nextPos1 = builder->CreateAdd(startPos, builder->getInt64(1));
  builder->CreateStore(nextPos1, currentPos);
  builder->CreateBr(exitBB);

  // Digit extraction loop (write digits in reverse order)
  builder->SetInsertPoint(loopBB);
  llvm::Value* currentVal =
      builder->CreateLoad(builder->getInt64Ty(), workingVal);
  llvm::Value* pos = builder->CreateLoad(builder->getInt64Ty(), currentPos);

  // Extract digit: currentVal % base
  llvm::Value* digit = builder->CreateURem(currentVal, builder->getInt64(base));

  // Get character for digit
  llvm::Value* digitCharPtr =
      builder->CreateGEP(builder->getInt8Ty(), digitPtr, digit);
  llvm::Value* digitChar =
      builder->CreateLoad(builder->getInt8Ty(), digitCharPtr);

  // Store digit in buffer
  llvm::Value* bufferPos =
      builder->CreateGEP(builder->getInt8Ty(), buffer, pos);
  builder->CreateStore(digitChar, bufferPos);

  // Update working value: currentVal / base
  llvm::Value* nextVal =
      builder->CreateUDiv(currentVal, builder->getInt64(base));
  builder->CreateStore(nextVal, workingVal);

  // Update position
  llvm::Value* nextPos = builder->CreateAdd(pos, builder->getInt64(1));
  builder->CreateStore(nextPos, currentPos);

  // Continue if more digits
  llvm::Value* hasMoreDigits =
      builder->CreateICmpNE(nextVal, builder->getInt64(0));
  builder->CreateCondBr(hasMoreDigits, loopBB, exitBB);

  builder->SetInsertPoint(exitBB);

  // Add null terminator
  llvm::Value* finalPos =
      builder->CreateLoad(builder->getInt64Ty(), currentPos);
  llvm::Value* nullPtr =
      builder->CreateGEP(builder->getInt8Ty(), buffer, finalPos);
  builder->CreateStore(builder->getInt8(0), nullPtr);

  // Reverse the digit portion only (from startPos to finalPos-1)
  reverseStringDigits(buffer, startPos, finalPos);

  return buffer;
}

llvm::Value* SyscallFramework::handleNegativeInteger(llvm::Value* intValue,
                                                     llvm::Value* buffer,
                                                     DataType /*type*/) {
  // Handle negative sign for signed integers
  llvm::Value* isNegative =
      builder->CreateICmpSLT(intValue, builder->getInt64(0));

  // Add negative sign if needed
  llvm::Value* signPtr =
      builder->CreateGEP(builder->getInt8Ty(), buffer, builder->getInt64(0));
  llvm::Value* minusChar = builder->getInt8('-');
  llvm::Value* spaceChar = builder->getInt8(' ');

  llvm::Value* signChar =
      builder->CreateSelect(isNegative, minusChar, spaceChar);
  builder->CreateStore(signChar, signPtr);

  // Return buffer pointer (with or without sign)
  return builder->CreateSelect(
      isNegative, buffer,
      builder->CreateGEP(builder->getInt8Ty(), buffer, builder->getInt64(1)));
}

llvm::Value* SyscallFramework::floatToString(llvm::Value* /*floatValue*/,
                                             DataType /*type*/,
                                             int /*precision*/) {
  // Float to string conversion without libc
  // This is a simplified implementation - for full IEEE 754 compliance, more
  // work needed

  // For now, just return a placeholder string
  // TODO: Implement proper float to string conversion
  llvm::Constant* placeholder =
      builder->CreateGlobalString("(float)", "float_placeholder");
  return builder->CreatePointerCast(placeholder,
                                    llvm::PointerType::getUnqual(*context));
}

// ============================================================================
// MEMORY MANAGEMENT (LIBC-FREE)
// ============================================================================

llvm::Value* SyscallFramework::createStaticBuffer(size_t size,
                                                  const std::string& name) {
  // Create a static buffer without malloc
  llvm::ArrayType* bufferType =
      llvm::ArrayType::get(builder->getInt8Ty(), size);
  llvm::Value* buffer = builder->CreateAlloca(bufferType, nullptr, name);

  // Zero initialize the buffer
  llvm::Value* zeroValue = llvm::ConstantAggregateZero::get(bufferType);
  builder->CreateStore(zeroValue, buffer);

  // Return pointer to first element
  return builder->CreateGEP(bufferType, buffer,
                            {builder->getInt64(0), builder->getInt64(0)});
}

llvm::Value* SyscallFramework::allocateBuffer(llvm::Value* size) {
  // Use platform-specific memory allocation (no malloc)
  std::vector<llvm::Value*> args = {size};
  return generateSyscall(SyscallType::MALLOC, args);
}

llvm::Value* SyscallFramework::freeBuffer(llvm::Value* ptr) {
  // Use platform-specific memory deallocation (no free)
  std::vector<llvm::Value*> args = {ptr};
  return generateSyscall(SyscallType::FREE, args);
}

void SyscallFramework::reverseStringDigits(llvm::Value* buffer,
                                           llvm::Value* startPos,
                                           llvm::Value* endPos) {
  // Simple string reversal implementation
  llvm::BasicBlock* entryBB = builder->GetInsertBlock();
  llvm::Function* currentFunc = entryBB->getParent();

  llvm::BasicBlock* loopBB =
      llvm::BasicBlock::Create(*context, "reverse_loop", currentFunc);
  llvm::BasicBlock* exitBB =
      llvm::BasicBlock::Create(*context, "reverse_exit", currentFunc);

  // Initialize left and right pointers
  llvm::Value* left =
      builder->CreateAlloca(builder->getInt64Ty(), nullptr, "left");
  llvm::Value* right =
      builder->CreateAlloca(builder->getInt64Ty(), nullptr, "right");

  builder->CreateStore(startPos, left);
  llvm::Value* rightStart = builder->CreateSub(endPos, builder->getInt64(1));
  builder->CreateStore(rightStart, right);

  builder->CreateBr(loopBB);

  // Reversal loop
  builder->SetInsertPoint(loopBB);
  llvm::Value* leftVal = builder->CreateLoad(builder->getInt64Ty(), left);
  llvm::Value* rightVal = builder->CreateLoad(builder->getInt64Ty(), right);

  // Check if left < right
  llvm::Value* shouldContinue = builder->CreateICmpSLT(leftVal, rightVal);
  llvm::BasicBlock* swapBB =
      llvm::BasicBlock::Create(*context, "swap", currentFunc);
  builder->CreateCondBr(shouldContinue, swapBB, exitBB);

  // Swap characters
  builder->SetInsertPoint(swapBB);
  llvm::Value* leftPtr =
      builder->CreateGEP(builder->getInt8Ty(), buffer, leftVal);
  llvm::Value* rightPtr =
      builder->CreateGEP(builder->getInt8Ty(), buffer, rightVal);

  llvm::Value* leftChar = builder->CreateLoad(builder->getInt8Ty(), leftPtr);
  llvm::Value* rightChar = builder->CreateLoad(builder->getInt8Ty(), rightPtr);

  builder->CreateStore(rightChar, leftPtr);
  builder->CreateStore(leftChar, rightPtr);

  // Update pointers
  llvm::Value* newLeft = builder->CreateAdd(leftVal, builder->getInt64(1));
  llvm::Value* newRight = builder->CreateSub(rightVal, builder->getInt64(1));
  builder->CreateStore(newLeft, left);
  builder->CreateStore(newRight, right);

  builder->CreateBr(loopBB);

  builder->SetInsertPoint(exitBB);
}

// ============================================================================
// PLATFORM-SPECIFIC API DECLARATIONS
// ============================================================================

void SyscallFramework::declareWindowsAPIs() {
  // Declare Windows API functions

  // WriteFile
  if (!module->getFunction("WriteFile")) {
    llvm::FunctionType* writeFileType = llvm::FunctionType::get(
        builder->getInt32Ty(),
        {
            llvm::PointerType::getUnqual(*context),  // HANDLE
            llvm::PointerType::getUnqual(*context),  // LPCVOID
            builder->getInt32Ty(),                   // DWORD
            llvm::PointerType::getUnqual(*context),  // LPDWORD
            llvm::PointerType::getUnqual(*context)   // LPOVERLAPPED
        },
        false);
    llvm::Function::Create(writeFileType, llvm::Function::ExternalLinkage,
                           "WriteFile", module);
  }

  // GetStdHandle
  if (!module->getFunction("GetStdHandle")) {
    llvm::FunctionType* getStdHandleType = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(*context), {builder->getInt32Ty()}, false);
    llvm::Function::Create(getStdHandleType, llvm::Function::ExternalLinkage,
                           "GetStdHandle", module);
  }

  // ExitProcess
  if (!module->getFunction("ExitProcess")) {
    llvm::FunctionType* exitProcessType = llvm::FunctionType::get(
        builder->getVoidTy(), {builder->getInt32Ty()}, false);
    llvm::Function::Create(exitProcessType, llvm::Function::ExternalLinkage,
                           "ExitProcess", module);
  }

  // VirtualAlloc
  if (!module->getFunction("VirtualAlloc")) {
    llvm::FunctionType* virtualAllocType = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(*context),
        {
            llvm::PointerType::getUnqual(*context),  // LPVOID
            builder->getInt64Ty(),                   // SIZE_T
            builder->getInt32Ty(),                   // DWORD
            builder->getInt32Ty()                    // DWORD
        },
        false);
    llvm::Function::Create(virtualAllocType, llvm::Function::ExternalLinkage,
                           "VirtualAlloc", module);
  }

  // VirtualFree
  if (!module->getFunction("VirtualFree")) {
    llvm::FunctionType* virtualFreeType = llvm::FunctionType::get(
        builder->getInt32Ty(),
        {
            llvm::PointerType::getUnqual(*context),  // LPVOID
            builder->getInt64Ty(),                   // SIZE_T
            builder->getInt32Ty()                    // DWORD
        },
        false);
    llvm::Function::Create(virtualFreeType, llvm::Function::ExternalLinkage,
                           "VirtualFree", module);
  }

  // CreateFile
  if (!module->getFunction("CreateFileA")) {
    llvm::FunctionType* createFileType = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(*context),
        {
            llvm::PointerType::getUnqual(*context),  // LPCSTR
            builder->getInt32Ty(),                   // DWORD (access)
            builder->getInt32Ty(),                   // DWORD (share)
            llvm::PointerType::getUnqual(*context),  // LPSECURITY_ATTRIBUTES
            builder->getInt32Ty(),                   // DWORD (creation)
            builder->getInt32Ty(),                   // DWORD (flags)
            llvm::PointerType::getUnqual(*context)   // HANDLE (template)
        },
        false);
    llvm::Function::Create(createFileType, llvm::Function::ExternalLinkage,
                           "CreateFileA", module);
  }

  // ReadFile
  if (!module->getFunction("ReadFile")) {
    llvm::FunctionType* readFileType = llvm::FunctionType::get(
        builder->getInt32Ty(),
        {
            llvm::PointerType::getUnqual(*context),  // HANDLE
            llvm::PointerType::getUnqual(*context),  // LPVOID
            builder->getInt32Ty(),                   // DWORD
            llvm::PointerType::getUnqual(*context),  // LPDWORD
            llvm::PointerType::getUnqual(*context)   // LPOVERLAPPED
        },
        false);
    llvm::Function::Create(readFileType, llvm::Function::ExternalLinkage,
                           "ReadFile", module);
  }

  // CloseHandle
  if (!module->getFunction("CloseHandle")) {
    llvm::FunctionType* closeHandleType = llvm::FunctionType::get(
        builder->getInt32Ty(),
        {llvm::PointerType::getUnqual(*context)},  // HANDLE
        false);
    llvm::Function::Create(closeHandleType, llvm::Function::ExternalLinkage,
                           "CloseHandle", module);
  }

  // GetFileSize
  if (!module->getFunction("GetFileSize")) {
    llvm::FunctionType* getFileSizeType = llvm::FunctionType::get(
        builder->getInt32Ty(),
        {
            llvm::PointerType::getUnqual(*context),  // HANDLE
            llvm::PointerType::getUnqual(*context)   // LPDWORD (high part)
        },
        false);
    llvm::Function::Create(getFileSizeType, llvm::Function::ExternalLinkage,
                           "GetFileSize", module);
  }
}

void SyscallFramework::declareLinuxSyscalls() {
  // Linux syscalls are handled via inline assembly
  // No function declarations needed
}

void SyscallFramework::declareMacOSSyscalls() {
  // macOS syscalls are handled via inline assembly
  // No function declarations needed
}

}  // namespace loom
