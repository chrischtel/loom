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

class SyscallFramework {
 public:
  SyscallFramework(llvm::LLVMContext* context, llvm::IRBuilder<>* builder,
                   llvm::Module* module);

  // Main syscall generation interface
  llvm::Value* generateSyscall(SyscallType type,
                               std::vector<llvm::Value*>& args,
                               DataType dataType = DataType::INT32);

  // Type-specific print functions (no hardcoded lengths)
  llvm::Value* printString(llvm::Value* stringPtr,
                           llvm::Value* length = nullptr);
  llvm::Value* printInteger(llvm::Value* intValue, DataType type,
                            int base = 10);
  llvm::Value* printFloat(llvm::Value* floatValue, DataType type,
                          int precision = 6);
  llvm::Value* printBoolean(llvm::Value* boolValue);
  llvm::Value* printPointer(llvm::Value* ptrValue);

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

  // Memory management (libc-free)
  llvm::Value* allocateBuffer(llvm::Value* size);
  llvm::Value* freeBuffer(llvm::Value* ptr);

 private:
  llvm::LLVMContext* context;
  llvm::IRBuilder<>* builder;
  llvm::Module* module;

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
