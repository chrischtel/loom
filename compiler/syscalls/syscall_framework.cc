#include "syscall_framework.hh"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InlineAsm.h>

#include <iostream>

namespace loom {

SyscallFramework::SyscallFramework(llvm::LLVMContext* context,
                                   llvm::IRBuilder<>* builder,
                                   llvm::Module* module,
                                   const SyscallConfig& config)
    : context(context), builder(builder), module(module), config(config) {
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

llvm::Value* SyscallFramework::generateNetworkSyscall(
    const std::string& syscallName, std::vector<llvm::Value*>& args) {
  Platform platform = detectPlatform();

  // Currently only Windows is supported for networking syscalls
  if (platform == Platform::WINDOWS) {
    // Map network syscall names to appropriate Windows API calls
    if (syscallName == "socket" && args.size() >= 3) {
      return generateWindowsSyscall(SyscallType::SOCKET, args, DataType::INT32);
    } else if (syscallName == "bind" && args.size() >= 3) {
      return generateWindowsSyscall(SyscallType::BIND, args, DataType::INT32);
    } else if (syscallName == "listen" && args.size() >= 2) {
      return generateWindowsSyscall(SyscallType::LISTEN, args, DataType::INT32);
    } else if (syscallName == "accept" && args.size() >= 3) {
      return generateWindowsSyscall(SyscallType::ACCEPT, args, DataType::INT64);
    } else if (syscallName == "connect" && args.size() >= 3) {
      return generateWindowsSyscall(SyscallType::CONNECT, args,
                                    DataType::INT32);
    } else if (syscallName == "send" && args.size() >= 4) {
      return generateWindowsSyscall(SyscallType::SEND, args, DataType::INT32);
    } else if (syscallName == "recv" && args.size() >= 4) {
      return generateWindowsSyscall(SyscallType::RECV, args, DataType::INT32);
    } else if (syscallName == "closesocket" && args.size() >= 1) {
      return generateWindowsSyscall(SyscallType::GENERIC, args,
                                    DataType::INT32);
    } else if (syscallName == "WSAStartup" && args.size() >= 2) {
      return generateWindowsSyscall(SyscallType::GENERIC, args,
                                    DataType::INT32);
    } else if (syscallName == "WSACleanup" && args.size() == 0) {
      return generateWindowsSyscall(SyscallType::GENERIC, args,
                                    DataType::INT32);
    } else if (syscallName == "htons" && args.size() == 1) {
      return generateWindowsSyscall(
          SyscallType::HTONS, args,
          DataType::
              INT16);  // ===================================================================
    } else {
      throw std::runtime_error("Unsupported network syscall: " + syscallName);
    }
  } else {
    throw std::runtime_error(
        "Network syscalls not yet implemented for this platform");
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
  // Use provided base or config default
  if (base == -1) {
    base = config.integer_base;
  }

  // Choose implementation based on config
  llvm::Value* stringBuffer;
  if (config.use_libc) {
    stringBuffer = integerToStringLibc(intValue, type, base);
  } else {
    stringBuffer = integerToStringNoLibc(intValue, type, base);
  }

  // Calculate length of the converted string
  llvm::Value* length = calculateStringLength(stringBuffer);

  std::vector<llvm::Value*> args = {stringBuffer, length};
  llvm::Value* result = generateSyscall(SyscallType::PRINT_STRING, args);

  // Add newline if configured to do so
  if (config.auto_newline) {
    llvm::Constant* newlineStr = builder->CreateGlobalString("\n", "newline");
    llvm::Value* newlinePtr = builder->CreatePointerCast(
        newlineStr, llvm::PointerType::getUnqual(*context));
    std::vector<llvm::Value*> newlineArgs = {newlinePtr, builder->getInt64(1)};
    generateSyscall(SyscallType::PRINT_STRING, newlineArgs);
  }

  return result;
}

llvm::Value* SyscallFramework::printFloat(llvm::Value* floatValue,
                                          DataType type, int precision) {
  // Use provided precision or config default
  if (precision == -1) {
    precision = config.float_precision;
  }

  // Choose implementation based on config
  llvm::Value* stringBuffer;
  if (config.use_libc) {
    stringBuffer = floatToStringLibc(floatValue, type, precision);
  } else {
    stringBuffer = floatToStringNoLibc(floatValue, type, precision);
  }

  // Calculate length of the converted string
  llvm::Value* length = calculateStringLength(stringBuffer);

  std::vector<llvm::Value*> args = {stringBuffer, length};
  llvm::Value* result = generateSyscall(SyscallType::PRINT_STRING, args);

  // Add newline if configured to do so
  if (config.auto_newline) {
    llvm::Constant* newlineStr = builder->CreateGlobalString("\n", "newline");
    llvm::Value* newlinePtr = builder->CreatePointerCast(
        newlineStr, llvm::PointerType::getUnqual(*context));
    std::vector<llvm::Value*> newlineArgs = {newlinePtr, builder->getInt64(1)};
    generateSyscall(SyscallType::PRINT_STRING, newlineArgs);
  }

  return result;
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

llvm::Value* SyscallFramework::floatToString(llvm::Value* floatValue,
                                             DataType type, int precision) {
  // IMPROVED PRECISE FLOAT-TO-STRING CONVERSION WITHOUT LIBC
  // This handles proper precision and multiple decimal places

  // Convert f32 to f64 for unified handling
  llvm::Value* doubleValue = floatValue;
  if (type == DataType::FLOAT32) {
    doubleValue =
        builder->CreateFPExt(floatValue, builder->getDoubleTy(), "to_double");
  }

  // Allocate buffer for result string (32 bytes should be enough)
  llvm::Type* i8Type = builder->getInt8Ty();
  llvm::Value* buffer =
      builder->CreateAlloca(i8Type, builder->getInt32(32), "float_buffer");
  llvm::Value* currentPos = builder->getInt32(0);

  // Check if negative
  llvm::Value* zero = llvm::ConstantFP::get(builder->getDoubleTy(), 0.0);
  llvm::Value* isNegative =
      builder->CreateFCmpOLT(doubleValue, zero, "is_negative");

  // Handle negative sign
  llvm::Function* currentFunc = builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* negativeBB =
      llvm::BasicBlock::Create(*context, "negative", currentFunc);
  llvm::BasicBlock* positiveBB =
      llvm::BasicBlock::Create(*context, "positive", currentFunc);
  llvm::BasicBlock* convertBB =
      llvm::BasicBlock::Create(*context, "convert", currentFunc);

  builder->CreateCondBr(isNegative, negativeBB, positiveBB);

  // Handle negative
  builder->SetInsertPoint(negativeBB);
  llvm::Value* minusPtr = builder->CreateGEP(i8Type, buffer, currentPos);
  builder->CreateStore(builder->getInt8(45), minusPtr);  // '-'
  llvm::Value* absValue = builder->CreateFNeg(doubleValue, "abs_value");
  llvm::Value* negPos = builder->CreateAdd(currentPos, builder->getInt32(1));
  builder->CreateBr(convertBB);

  // Handle positive
  builder->SetInsertPoint(positiveBB);
  builder->CreateBr(convertBB);

  // Convert the absolute value
  builder->SetInsertPoint(convertBB);
  llvm::PHINode* valueToConvert =
      builder->CreatePHI(builder->getDoubleTy(), 2, "value_to_convert");
  valueToConvert->addIncoming(absValue, negativeBB);
  valueToConvert->addIncoming(doubleValue, positiveBB);

  llvm::PHINode* writePos =
      builder->CreatePHI(builder->getInt32Ty(), 2, "write_pos");
  writePos->addIncoming(negPos, negativeBB);
  writePos->addIncoming(currentPos, positiveBB);

  // Extract integer part
  llvm::Value* intPart =
      builder->CreateFPToUI(valueToConvert, builder->getInt64Ty(), "int_part");

  // Convert integer part to string
  llvm::Value* posAfterInt =
      convertLargeIntegerToString(buffer, writePos, intPart);

  // Add decimal point
  llvm::Value* dotPtr = builder->CreateGEP(i8Type, buffer, posAfterInt);
  builder->CreateStore(builder->getInt8(46), dotPtr);  // '.'
  llvm::Value* fracStartPos =
      builder->CreateAdd(posAfterInt, builder->getInt32(1));

  // Extract fractional part
  llvm::Value* intPartFloat =
      builder->CreateUIToFP(intPart, builder->getDoubleTy(), "int_as_float");
  llvm::Value* fracPart =
      builder->CreateFSub(valueToConvert, intPartFloat, "frac_part");

  // Convert fractional digits with proper precision
  llvm::Value* fracPos = fracStartPos;
  for (int i = 0; i < precision; i++) {
    // Multiply by 10 to get next digit
    fracPart = builder->CreateFMul(
        fracPart, llvm::ConstantFP::get(builder->getDoubleTy(), 10.0),
        "frac_times_10");
    llvm::Value* digit =
        builder->CreateFPToUI(fracPart, builder->getInt32Ty(), "frac_digit");

    // Convert to ASCII
    llvm::Value* digitAscii =
        builder->CreateAdd(builder->CreateTrunc(digit, builder->getInt8Ty()),
                           builder->getInt8(48), "digit_ascii");

    // Store digit
    llvm::Value* digitPtr = builder->CreateGEP(i8Type, buffer, fracPos);
    builder->CreateStore(digitAscii, digitPtr);
    fracPos = builder->CreateAdd(fracPos, builder->getInt32(1));

    // Update fractional part (subtract the digit we just extracted)
    llvm::Value* digitFloat =
        builder->CreateUIToFP(digit, builder->getDoubleTy(), "digit_float");
    fracPart = builder->CreateFSub(fracPart, digitFloat, "remaining_frac");
  }

  // Null terminate
  llvm::Value* nullPtr = builder->CreateGEP(i8Type, buffer, fracPos);
  builder->CreateStore(builder->getInt8(0), nullPtr);

  return buffer;
}

// ============================================================================
// COMPLEX MATHEMATICAL OPERATIONS FOR PRECISE FLOAT CONVERSION
// ============================================================================

llvm::Value* SyscallFramework::calculateLog10(llvm::Value* value) {
  // Calculate log10 using change of base: log10(x) = ln(x) / ln(10)
  // Declare intrinsic functions for logarithm
  llvm::Function* logFunc = llvm::Intrinsic::getOrInsertDeclaration(
      module, llvm::Intrinsic::log, builder->getDoubleTy());

  // Calculate ln(value)
  llvm::Value* lnValue = builder->CreateCall(logFunc, {value}, "ln_value");

  // ln(10) ≈ 2.302585092994046
  llvm::Value* ln10 =
      llvm::ConstantFP::get(builder->getDoubleTy(), 2.302585092994046);

  // log10(value) = ln(value) / ln(10)
  llvm::Value* log10Value = builder->CreateFDiv(lnValue, ln10, "log10_value");

  return log10Value;
}

llvm::Value* SyscallFramework::calculateMantissa(llvm::Value* value,
                                                 llvm::Value* exponent) {
  // Calculate mantissa: value / (10^exponent)

  // Convert exponent to float
  llvm::Value* expFloat =
      builder->CreateSIToFP(exponent, builder->getDoubleTy(), "exp_float");
  // Calculate 10^exponent using pow intrinsic
  llvm::Function* powFunc = llvm::Intrinsic::getOrInsertDeclaration(
      module, llvm::Intrinsic::pow, builder->getDoubleTy());
  llvm::Value* ten = llvm::ConstantFP::get(builder->getDoubleTy(), 10.0);
  llvm::Value* power =
      builder->CreateCall(powFunc, {ten, expFloat}, "power_of_ten");

  // mantissa = value / (10^exponent)
  llvm::Value* mantissa = builder->CreateFDiv(value, power, "mantissa");

  return mantissa;
}

llvm::Value* SyscallFramework::convertMantissaToString(llvm::Value* buffer,
                                                       llvm::Value* startPos,
                                                       llvm::Value* mantissa,
                                                       int precision) {
  // Convert mantissa to string with specified precision
  llvm::Type* i8Type = builder->getInt8Ty();
  llvm::Value* currentPos = startPos;

  // Extract integer part (should be 1-9 for normalized mantissa)
  llvm::Value* intPart =
      builder->CreateFPToUI(mantissa, builder->getInt32Ty(), "mantissa_int");
  llvm::Value* intDigit =
      builder->CreateAdd(builder->CreateTrunc(intPart, builder->getInt8Ty()),
                         builder->getInt8(48), "int_ascii");

  // Store integer digit
  llvm::Value* intPtr = builder->CreateGEP(i8Type, buffer, currentPos);
  builder->CreateStore(intDigit, intPtr);
  currentPos = builder->CreateAdd(currentPos, builder->getInt32(1));

  // Add decimal point if precision > 0
  if (precision > 0) {
    llvm::Value* dotPtr = builder->CreateGEP(i8Type, buffer, currentPos);
    builder->CreateStore(builder->getInt8(46), dotPtr);  // '.'
    currentPos = builder->CreateAdd(currentPos, builder->getInt32(1));

    // Extract fractional part
    llvm::Value* intPartFloat =
        builder->CreateUIToFP(intPart, builder->getDoubleTy(), "int_as_float");
    llvm::Value* fracPart =
        builder->CreateFSub(mantissa, intPartFloat, "frac_part");

    // Convert fractional digits
    for (int i = 0; i < precision; i++) {
      // Multiply by 10 to get next digit
      fracPart = builder->CreateFMul(
          fracPart, llvm::ConstantFP::get(builder->getDoubleTy(), 10.0),
          "frac_times_10");
      llvm::Value* digit =
          builder->CreateFPToUI(fracPart, builder->getInt32Ty(), "frac_digit");

      // Convert to ASCII
      llvm::Value* digitAscii =
          builder->CreateAdd(builder->CreateTrunc(digit, builder->getInt8Ty()),
                             builder->getInt8(48), "digit_ascii");

      // Store digit
      llvm::Value* digitPtr = builder->CreateGEP(i8Type, buffer, currentPos);
      builder->CreateStore(digitAscii, digitPtr);
      currentPos = builder->CreateAdd(currentPos, builder->getInt32(1));

      // Update fractional part
      llvm::Value* digitFloat =
          builder->CreateUIToFP(digit, builder->getDoubleTy(), "digit_float");
      fracPart = builder->CreateFSub(fracPart, digitFloat, "remaining_frac");
    }
  }

  return currentPos;
}

llvm::Value* SyscallFramework::convertRegularFloatToString(
    llvm::Value* buffer, llvm::Value* startPos, llvm::Value* value,
    int precision) {
  // Convert regular floating point number to string
  llvm::Type* i8Type = builder->getInt8Ty();
  llvm::Value* currentPos = startPos;

  // Extract integer part
  llvm::Value* intPart =
      builder->CreateFPToUI(value, builder->getInt64Ty(), "int_part");

  // Convert integer part to string (handle up to 64-bit integers)
  currentPos = convertLargeIntegerToString(buffer, currentPos, intPart);

  // Add decimal point if precision > 0
  if (precision > 0) {
    llvm::Value* dotPtr = builder->CreateGEP(i8Type, buffer, currentPos);
    builder->CreateStore(builder->getInt8(46), dotPtr);  // '.'
    currentPos = builder->CreateAdd(currentPos, builder->getInt32(1));

    // Extract fractional part
    llvm::Value* intPartFloat =
        builder->CreateUIToFP(intPart, builder->getDoubleTy(), "int_as_float");
    llvm::Value* fracPart =
        builder->CreateFSub(value, intPartFloat, "frac_part");

    // Convert fractional digits with proper precision
    for (int i = 0; i < precision; i++) {
      fracPart = builder->CreateFMul(
          fracPart, llvm::ConstantFP::get(builder->getDoubleTy(), 10.0),
          "frac_times_10");
      llvm::Value* digit =
          builder->CreateFPToUI(fracPart, builder->getInt32Ty(), "frac_digit");

      llvm::Value* digitAscii =
          builder->CreateAdd(builder->CreateTrunc(digit, builder->getInt8Ty()),
                             builder->getInt8(48), "digit_ascii");

      llvm::Value* digitPtr = builder->CreateGEP(i8Type, buffer, currentPos);
      builder->CreateStore(digitAscii, digitPtr);
      currentPos = builder->CreateAdd(currentPos, builder->getInt32(1));

      llvm::Value* digitFloat =
          builder->CreateUIToFP(digit, builder->getDoubleTy(), "digit_float");
      fracPart = builder->CreateFSub(fracPart, digitFloat, "remaining_frac");
    }
  }

  return currentPos;
}

llvm::Value* SyscallFramework::convertLargeIntegerToString(
    llvm::Value* buffer, llvm::Value* startPos, llvm::Value* intValue) {
  // Convert large integer to string using division method
  llvm::Type* i8Type = builder->getInt8Ty();

  // Handle zero specially
  llvm::Function* currentFunc = builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* zeroCheckBB =
      llvm::BasicBlock::Create(*context, "zero_check", currentFunc);
  llvm::BasicBlock* zeroHandleBB =
      llvm::BasicBlock::Create(*context, "zero_handle", currentFunc);
  llvm::BasicBlock* nonZeroBB =
      llvm::BasicBlock::Create(*context, "non_zero", currentFunc);
  llvm::BasicBlock* continueBB =
      llvm::BasicBlock::Create(*context, "continue_int", currentFunc);

  builder->CreateBr(zeroCheckBB);

  builder->SetInsertPoint(zeroCheckBB);
  llvm::Value* isZero =
      builder->CreateICmpEQ(intValue, builder->getInt64(0), "is_zero");
  builder->CreateCondBr(isZero, zeroHandleBB, nonZeroBB);

  // Handle zero
  builder->SetInsertPoint(zeroHandleBB);
  llvm::Value* zeroPtr = builder->CreateGEP(i8Type, buffer, startPos);
  builder->CreateStore(builder->getInt8(48), zeroPtr);  // '0'
  llvm::Value* zeroEndPos = builder->CreateAdd(startPos, builder->getInt32(1));
  builder->CreateBr(continueBB);

  // Handle non-zero integers
  builder->SetInsertPoint(nonZeroBB);

  // Create temporary buffer for digits (reversed)
  llvm::Value* tempBuffer =
      builder->CreateAlloca(i8Type, builder->getInt32(32), "temp_digits");
  llvm::Value* digitCount =
      builder->CreateAlloca(builder->getInt32Ty(), nullptr, "digit_count");
  builder->CreateStore(builder->getInt32(0), digitCount);

  // Extract digits in reverse order
  llvm::Value* remainingValue = intValue;
  llvm::BasicBlock* digitLoopBB =
      llvm::BasicBlock::Create(*context, "digit_loop", currentFunc);
  llvm::BasicBlock* digitLoopBodyBB =
      llvm::BasicBlock::Create(*context, "digit_body", currentFunc);
  llvm::BasicBlock* digitLoopEndBB =
      llvm::BasicBlock::Create(*context, "digit_end", currentFunc);

  builder->CreateBr(digitLoopBB);

  // Digit extraction loop
  builder->SetInsertPoint(digitLoopBB);
  llvm::PHINode* currentValue =
      builder->CreatePHI(builder->getInt64Ty(), 2, "current_value");
  currentValue->addIncoming(remainingValue, nonZeroBB);

  llvm::Value* loopCondition = builder->CreateICmpNE(
      currentValue, builder->getInt64(0), "loop_condition");
  builder->CreateCondBr(loopCondition, digitLoopBodyBB, digitLoopEndBB);

  // Extract digit
  builder->SetInsertPoint(digitLoopBodyBB);
  llvm::Value* digit =
      builder->CreateURem(currentValue, builder->getInt64(10), "digit");
  llvm::Value* digitAscii =
      builder->CreateAdd(builder->CreateTrunc(digit, builder->getInt8Ty()),
                         builder->getInt8(48), "digit_ascii");

  // Store digit in temp buffer
  llvm::Value* currentCount =
      builder->CreateLoad(builder->getInt32Ty(), digitCount, "current_count");
  llvm::Value* tempPos = builder->CreateGEP(i8Type, tempBuffer, currentCount);
  builder->CreateStore(digitAscii, tempPos);

  // Increment count and update value
  llvm::Value* newCount =
      builder->CreateAdd(currentCount, builder->getInt32(1));
  builder->CreateStore(newCount, digitCount);
  llvm::Value* newValue =
      builder->CreateUDiv(currentValue, builder->getInt64(10), "new_value");
  currentValue->addIncoming(newValue, digitLoopBodyBB);

  builder->CreateBr(digitLoopBB);

  // Copy digits from temp buffer to main buffer in reverse order
  builder->SetInsertPoint(digitLoopEndBB);
  llvm::Value* finalCount =
      builder->CreateLoad(builder->getInt32Ty(), digitCount, "final_count");

  // Reverse copy loop
  llvm::BasicBlock* copyLoopBB =
      llvm::BasicBlock::Create(*context, "copy_loop", currentFunc);
  llvm::BasicBlock* copyBodyBB =
      llvm::BasicBlock::Create(*context, "copy_body", currentFunc);
  llvm::BasicBlock* copyEndBB =
      llvm::BasicBlock::Create(*context, "copy_end", currentFunc);

  llvm::Value* copyIndex =
      builder->CreateAlloca(builder->getInt32Ty(), nullptr, "copy_index");
  builder->CreateStore(builder->getInt32(0), copyIndex);
  builder->CreateBr(copyLoopBB);

  builder->SetInsertPoint(copyLoopBB);
  llvm::Value* currentIndex =
      builder->CreateLoad(builder->getInt32Ty(), copyIndex, "current_index");
  llvm::Value* copyCondition =
      builder->CreateICmpULT(currentIndex, finalCount, "copy_condition");
  builder->CreateCondBr(copyCondition, copyBodyBB, copyEndBB);

  builder->SetInsertPoint(copyBodyBB);
  // Source index: finalCount - 1 - currentIndex (reverse order)
  llvm::Value* sourceIndex =
      builder->CreateSub(builder->CreateSub(finalCount, builder->getInt32(1)),
                         currentIndex, "source_index");
  llvm::Value* sourcePtr = builder->CreateGEP(i8Type, tempBuffer, sourceIndex);
  llvm::Value* digitValue =
      builder->CreateLoad(i8Type, sourcePtr, "digit_value");

  // Destination index: startPos + currentIndex
  llvm::Value* destIndex = builder->CreateAdd(startPos, currentIndex);
  llvm::Value* destPtr = builder->CreateGEP(i8Type, buffer, destIndex);
  builder->CreateStore(digitValue, destPtr);

  // Increment index
  llvm::Value* nextIndex =
      builder->CreateAdd(currentIndex, builder->getInt32(1));
  builder->CreateStore(nextIndex, copyIndex);
  builder->CreateBr(copyLoopBB);

  builder->SetInsertPoint(copyEndBB);
  llvm::Value* nonZeroEndPos = builder->CreateAdd(startPos, finalCount);
  builder->CreateBr(continueBB);

  // Continue
  builder->SetInsertPoint(continueBB);
  llvm::PHINode* finalPos =
      builder->CreatePHI(builder->getInt32Ty(), 2, "final_pos");
  finalPos->addIncoming(zeroEndPos, zeroHandleBB);
  finalPos->addIncoming(nonZeroEndPos, copyEndBB);

  return finalPos;
}

llvm::Value* SyscallFramework::convertIntegerToString(llvm::Value* buffer,
                                                      llvm::Value* startPos,
                                                      llvm::Value* intValue,
                                                      int /*base*/) {
  // Convert 32-bit integer to string with specified base
  llvm::Value* int64Value =
      builder->CreateSExt(intValue, builder->getInt64Ty(), "int64_value");

  // Handle negative numbers
  llvm::Function* currentFunc = builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* negCheckBB =
      llvm::BasicBlock::Create(*context, "neg_check", currentFunc);
  llvm::BasicBlock* negHandleBB =
      llvm::BasicBlock::Create(*context, "neg_handle", currentFunc);
  llvm::BasicBlock* posHandleBB =
      llvm::BasicBlock::Create(*context, "pos_handle", currentFunc);
  llvm::BasicBlock* convertIntBB =
      llvm::BasicBlock::Create(*context, "convert_int", currentFunc);

  builder->CreateBr(negCheckBB);

  builder->SetInsertPoint(negCheckBB);
  llvm::Value* isNegative =
      builder->CreateICmpSLT(int64Value, builder->getInt64(0), "is_negative");
  builder->CreateCondBr(isNegative, negHandleBB, posHandleBB);

  // Handle negative
  builder->SetInsertPoint(negHandleBB);
  llvm::Type* i8Type = builder->getInt8Ty();
  llvm::Value* minusPtr = builder->CreateGEP(i8Type, buffer, startPos);
  builder->CreateStore(builder->getInt8(45), minusPtr);  // '-'
  llvm::Value* absValue = builder->CreateNeg(int64Value, "abs_value");
  llvm::Value* negStartPos = builder->CreateAdd(startPos, builder->getInt32(1));
  builder->CreateBr(convertIntBB);

  // Handle positive
  builder->SetInsertPoint(posHandleBB);
  builder->CreateBr(convertIntBB);

  // Convert
  builder->SetInsertPoint(convertIntBB);
  llvm::PHINode* valueToConvert =
      builder->CreatePHI(builder->getInt64Ty(), 2, "value_to_convert");
  valueToConvert->addIncoming(absValue, negHandleBB);
  valueToConvert->addIncoming(int64Value, posHandleBB);

  llvm::PHINode* currentPos =
      builder->CreatePHI(builder->getInt32Ty(), 2, "current_pos");
  currentPos->addIncoming(negStartPos, negHandleBB);
  currentPos->addIncoming(startPos, posHandleBB);

  return convertLargeIntegerToString(buffer, currentPos, valueToConvert);
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

// ============================================================================
// LIBC-BASED IMPLEMENTATIONS (ROBUST AND FAST)
// ============================================================================

llvm::Value* SyscallFramework::floatToStringLibc(llvm::Value* floatValue,
                                                 DataType type, int precision) {
  // Use sprintf for robust float formatting

  // Declare sprintf if not already declared
  llvm::Function* sprintfFunc = module->getFunction("sprintf");
  if (!sprintfFunc) {
    llvm::FunctionType* sprintfType = llvm::FunctionType::get(
        builder->getInt32Ty(),  // Returns number of chars written
        {
            llvm::PointerType::getUnqual(*context),  // char* buffer
            llvm::PointerType::getUnqual(*context),  // const char* format
        },
        true);  // variadic
    sprintfFunc = llvm::Function::Create(
        sprintfType, llvm::Function::ExternalLinkage, "sprintf", module);
  }

  // Allocate buffer for result (64 bytes should be enough for any float)
  llvm::Type* i8Type = builder->getInt8Ty();
  llvm::Value* buffer =
      builder->CreateAlloca(i8Type, builder->getInt32(64), "float_buffer");

  // Create format string based on precision
  std::string formatStr = "%." + std::to_string(precision) + "f";
  llvm::Constant* formatGlobal =
      builder->CreateGlobalString(formatStr, "float_format");
  llvm::Value* formatPtr = builder->CreatePointerCast(
      formatGlobal, llvm::PointerType::getUnqual(*context));

  // Convert f32 to f64 if needed (sprintf expects double for %f)
  llvm::Value* doubleValue = floatValue;
  if (type == DataType::FLOAT32) {
    doubleValue =
        builder->CreateFPExt(floatValue, builder->getDoubleTy(), "to_double");
  }

  // Call sprintf
  builder->CreateCall(sprintfFunc, {buffer, formatPtr, doubleValue},
                      "sprintf_result");

  return buffer;
}

llvm::Value* SyscallFramework::integerToStringLibc(llvm::Value* intValue,
                                                   DataType type, int base) {
  // Use sprintf for robust integer formatting

  // Declare sprintf if not already declared
  llvm::Function* sprintfFunc = module->getFunction("sprintf");
  if (!sprintfFunc) {
    llvm::FunctionType* sprintfType = llvm::FunctionType::get(
        builder->getInt32Ty(),  // Returns number of chars written
        {
            llvm::PointerType::getUnqual(*context),  // char* buffer
            llvm::PointerType::getUnqual(*context),  // const char* format
        },
        true);  // variadic
    sprintfFunc = llvm::Function::Create(
        sprintfType, llvm::Function::ExternalLinkage, "sprintf", module);
  }

  // Allocate buffer for result (32 bytes should be enough for any integer)
  llvm::Type* i8Type = builder->getInt8Ty();
  llvm::Value* buffer =
      builder->CreateAlloca(i8Type, builder->getInt32(32), "int_buffer");

  // Create format string based on base and type
  std::string formatStr;
  switch (base) {
    case 8:
      formatStr = "%o";  // octal
      break;
    case 16:
      formatStr = "%x";  // hexadecimal
      break;
    default:
      // Determine if signed or unsigned
      switch (type) {
        case DataType::UINT8:
        case DataType::UINT16:
        case DataType::UINT32:
        case DataType::UINT64:
          formatStr = "%u";  // unsigned decimal
          break;
        default:
          formatStr = "%d";  // signed decimal
          break;
      }
      break;
  }

  llvm::Constant* formatGlobal =
      builder->CreateGlobalString(formatStr, "int_format");
  llvm::Value* formatPtr = builder->CreatePointerCast(
      formatGlobal, llvm::PointerType::getUnqual(*context));

  // Extend/truncate integer to appropriate size for sprintf
  llvm::Value* printValue = intValue;
  llvm::Type* targetType = builder->getInt32Ty();

  // For 64-bit integers, use %lld format
  if (type == DataType::INT64 || type == DataType::UINT64) {
    targetType = builder->getInt64Ty();
    if (formatStr == "%d")
      formatStr = "%lld";
    else if (formatStr == "%u")
      formatStr = "%llu";
    else if (formatStr == "%x")
      formatStr = "%llx";
    else if (formatStr == "%o")
      formatStr = "%llo";

    // Update format string
    formatGlobal = builder->CreateGlobalString(formatStr, "int64_format");
    formatPtr = builder->CreatePointerCast(
        formatGlobal, llvm::PointerType::getUnqual(*context));
  }

  // Cast to target type
  if (intValue->getType() != targetType) {
    if (intValue->getType()->getIntegerBitWidth() <
        targetType->getIntegerBitWidth()) {
      printValue = builder->CreateSExt(intValue, targetType, "sext");
    } else if (intValue->getType()->getIntegerBitWidth() >
               targetType->getIntegerBitWidth()) {
      printValue = builder->CreateTrunc(intValue, targetType, "trunc");
    }
  }

  // Call sprintf
  builder->CreateCall(sprintfFunc, {buffer, formatPtr, printValue},
                      "sprintf_result");

  return buffer;
}

// ============================================================================
// NO-LIBC IMPLEMENTATIONS (MINIMAL DEPENDENCIES)
// ============================================================================

llvm::Value* SyscallFramework::floatToStringNoLibc(llvm::Value* floatValue,
                                                   DataType type,
                                                   int precision) {
  // Use the existing no-libc implementation but improve it
  return floatToString(floatValue, type, precision);
}

llvm::Value* SyscallFramework::integerToStringNoLibc(llvm::Value* intValue,
                                                     DataType type,
                                                     int /*base*/) {
  // ROBUST INTEGER-TO-STRING CONVERSION WITHOUT LIBC
  // Handles all integer sizes and bases properly

  // Allocate buffer for result string (32 bytes should be enough)
  llvm::Type* i8Type = builder->getInt8Ty();
  llvm::Value* buffer =
      builder->CreateAlloca(i8Type, builder->getInt32(32), "int_buffer");

  // Extend to 64-bit for uniform handling
  llvm::Value* int64Value;
  switch (type) {
    case DataType::INT8:
    case DataType::INT16:
    case DataType::INT32:
      int64Value =
          builder->CreateSExt(intValue, builder->getInt64Ty(), "sext_to_64");
      break;
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::UINT32:
      int64Value =
          builder->CreateZExt(intValue, builder->getInt64Ty(), "zext_to_64");
      break;
    case DataType::INT64:
    case DataType::UINT64:
      int64Value = intValue;
      break;
    default:
      int64Value =
          builder->CreateSExt(intValue, builder->getInt64Ty(), "default_sext");
      break;
  }

  // Use the robust conversion method
  llvm::Value* finalPos =
      convertLargeIntegerToString(buffer, builder->getInt32(0), int64Value);

  // Null terminate
  llvm::Value* nullPtr = builder->CreateGEP(i8Type, buffer, finalPos);
  builder->CreateStore(builder->getInt8(0), nullPtr);

  return buffer;
}

}  // namespace loom
