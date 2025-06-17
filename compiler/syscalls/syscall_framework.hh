#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include <string>
#include <vector>

namespace loom {

enum class SyscallType {
  PRINT_STRING,
  PRINT_INTEGER,
  PRINT_POINTER,
  PRINT_FLOAT,
  PRINT_BOOLEAN,
  EXIT,
  READ,
  WRITE,
  OPEN,
  CLOSE,
  SOCKET,
  HTONS,
  BIND,
  LISTEN,
  ACCEPT,
  CONNECT,
  SEND,
  RECV,
  MALLOC,
  FREE,
  MMAP,
  MUNMAP,
  GET_FILE_SIZE,
  GENERIC
};

enum class DataType {
  INT8,
  INT16,
  INT32,
  INT64,
  UINT8,
  UINT16,
  UINT32,
  UINT64,
  FLOAT32,
  FLOAT64,
  STRING,
  POINTER,
  BOOLEAN
};

// Configuration for syscall framework
struct SyscallConfig {
  bool use_libc = true;  // Whether to use libc functions (sprintf, etc.)
  bool auto_newline =
      true;                 // Whether $$print should add newlines automatically
  int float_precision = 6;  // Default precision for float printing
  int integer_base = 10;    // Default base for integer printing
};

class SyscallFramework {
 public:
  SyscallFramework(llvm::LLVMContext* context, llvm::IRBuilder<>* builder,
                   llvm::Module* module, const SyscallConfig& config = {});
  // Configuration management
  void setConfig(const SyscallConfig& new_config) { this->config = new_config; }
  const SyscallConfig& getConfig() const { return config; }

  // Main syscall generation interface
  llvm::Value* generateSyscall(SyscallType type,
                               std::vector<llvm::Value*>& args,
                               DataType dataType = DataType::INT32);
  // Type-specific print functions (improved)
  llvm::Value* printString(llvm::Value* stringPtr,
                           llvm::Value* length = nullptr);
  llvm::Value* printInteger(llvm::Value* intValue, DataType type,
                            int base = -1);  // -1 means use config default
  llvm::Value* printFloat(llvm::Value* floatValue, DataType type,
                          int precision = -1);  // -1 means use config default
  llvm::Value* printBoolean(llvm::Value* boolValue);
  llvm::Value* printPointer(llvm::Value* ptrValue);
  // High-level print function (like $$print)
  llvm::Value* print(llvm::Value* value, DataType type = DataType::INT32);

  // Network syscall interface
  llvm::Value* generateNetworkSyscall(const std::string& syscallName,
                                      std::vector<llvm::Value*>& args);

  // Platform-specific implementations
  llvm::Value* generateWindowsSyscall(SyscallType type,
                                      std::vector<llvm::Value*>& args,
                                      DataType dataType);
  llvm::Value* generateLinuxSyscall(SyscallType type,
                                    std::vector<llvm::Value*>& args,
                                    DataType dataType);
  llvm::Value* generateMacOSSyscall(SyscallType type,
                                    std::vector<llvm::Value*>& args,
                                    DataType dataType);
  // Utility functions
  llvm::Value* calculateStringLength(llvm::Value* stringPtr);
  llvm::Value* integerToString(llvm::Value* intValue, DataType type,
                               int base = 10);
  llvm::Value* floatToString(llvm::Value* floatValue, DataType type,
                             int precision = 6);
  llvm::Value* reverseString(llvm::Value* stringPtr, llvm::Value* length);

  // Complex mathematical operations for precise conversion
  llvm::Value* calculateLog10(llvm::Value* value);
  llvm::Value* calculateMantissa(llvm::Value* value, llvm::Value* exponent);
  llvm::Value* convertMantissaToString(llvm::Value* buffer,
                                       llvm::Value* startPos,
                                       llvm::Value* mantissa, int precision);
  llvm::Value* convertRegularFloatToString(llvm::Value* buffer,
                                           llvm::Value* startPos,
                                           llvm::Value* value, int precision);
  llvm::Value* convertLargeIntegerToString(llvm::Value* buffer,
                                           llvm::Value* startPos,
                                           llvm::Value* intValue);
  llvm::Value* convertIntegerToString(llvm::Value* buffer,
                                      llvm::Value* startPos,
                                      llvm::Value* intValue, int base);

  // Libc-based implementations (when config.use_libc = true)
  llvm::Value* floatToStringLibc(llvm::Value* floatValue, DataType type,
                                 int precision);
  llvm::Value* integerToStringLibc(llvm::Value* intValue, DataType type,
                                   int base);

  // No-libc implementations (when config.use_libc = false)
  llvm::Value* floatToStringNoLibc(llvm::Value* floatValue, DataType type,
                                   int precision);
  llvm::Value* integerToStringNoLibc(llvm::Value* intValue, DataType type,
                                     int base);

  // Memory management (libc-free)
  llvm::Value* allocateBuffer(llvm::Value* size);
  llvm::Value* freeBuffer(llvm::Value* ptr);

 private:
  llvm::LLVMContext* context;
  llvm::IRBuilder<>* builder;
  llvm::Module* module;
  SyscallConfig config;  // Configuration for syscall behavior

  // Platform detection
  enum class Platform { WINDOWS, LINUX, MACOS, UNKNOWN };
  Platform detectPlatform();  // Helper functions for integer conversion
  llvm::Value* intToStringDigits(llvm::Value* intValue, llvm::Value* buffer,
                                 llvm::Value* writePos, DataType type,
                                 int base);
  llvm::Value* handleNegativeInteger(llvm::Value* intValue, llvm::Value* buffer,
                                     DataType type);
  void reverseStringDigits(llvm::Value* buffer, llvm::Value* startPos,
                           llvm::Value* endPos);

  // OS-specific API declarations
  void declareWindowsAPIs();
  void declareLinuxSyscalls();
  void declareMacOSSyscalls();

  // Buffer management
  llvm::Value* createStaticBuffer(size_t size, const std::string& name);
  llvm::Value* createDynamicBuffer(llvm::Value* size);
};

}  // namespace loom
