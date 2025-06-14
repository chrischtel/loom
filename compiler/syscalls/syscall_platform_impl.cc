#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>

#include "syscall_framework.hh"

namespace loom {

// ============================================================================
// WINDOWS SYSCALL IMPLEMENTATION (LIBC-FREE)
// ============================================================================

llvm::Value* SyscallFramework::generateWindowsSyscall(
    SyscallType type, std::vector<llvm::Value*>& args, DataType /*dataType*/) {
  switch (type) {
    case SyscallType::PRINT_STRING: {
      if (args.size() < 2)
        throw std::runtime_error(
            "PRINT_STRING requires string pointer and length");

      llvm::Function* writeFile = module->getFunction("WriteFile");
      llvm::Function* getStdHandle = module->getFunction("GetStdHandle");

      // Get stdout handle (-11 = STD_OUTPUT_HANDLE)
      llvm::Value* stdoutHandle =
          builder->CreateCall(getStdHandle, {builder->getInt32(-11)});

      // Convert length to 32-bit
      llvm::Value* length32 = args[1];
      if (args[1]->getType()->isIntegerTy(64)) {
        length32 = builder->CreateTrunc(args[1], builder->getInt32Ty());
      }

      // Allocate space for bytes written
      llvm::Value* bytesWritten = builder->CreateAlloca(
          builder->getInt32Ty(), nullptr, "bytes_written");

      // Call WriteFile
      llvm::Value* result = builder->CreateCall(
          writeFile, {stdoutHandle,
                      args[0],   // string pointer
                      length32,  // length
                      bytesWritten,
                      llvm::ConstantPointerNull::get(
                          llvm::PointerType::getUnqual(*context))});

      // Add newline
      llvm::Constant* newlineStr = builder->CreateGlobalString("\n", "newline");
      llvm::Value* newlinePtr = builder->CreatePointerCast(
          newlineStr, llvm::PointerType::getUnqual(*context));
      llvm::Value* newlineBytesWritten = builder->CreateAlloca(
          builder->getInt32Ty(), nullptr, "newline_bytes");

      builder->CreateCall(
          writeFile,
          {stdoutHandle, newlinePtr, builder->getInt32(1), newlineBytesWritten,
           llvm::ConstantPointerNull::get(
               llvm::PointerType::getUnqual(*context))});

      return result;
    }

    case SyscallType::EXIT: {
      if (args.empty()) throw std::runtime_error("EXIT requires exit code");

      llvm::Function* exitProcess = module->getFunction("ExitProcess");

      // Convert exit code to 32-bit if needed
      llvm::Value* exitCode32 = args[0];
      if (args[0]->getType()->isIntegerTy(64)) {
        exitCode32 = builder->CreateTrunc(args[0], builder->getInt32Ty());
      }

      builder->CreateCall(exitProcess, {exitCode32});
      return nullptr;
    }

    case SyscallType::MALLOC: {
      if (args.empty()) throw std::runtime_error("MALLOC requires size");

      llvm::Function* virtualAlloc = module->getFunction("VirtualAlloc");

      // VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
      // MEM_COMMIT | MEM_RESERVE = 0x3000, PAGE_READWRITE = 0x04
      return builder->CreateCall(
          virtualAlloc,
          {
              llvm::ConstantPointerNull::get(
                  llvm::PointerType::getUnqual(*context)),
              args[0],                    // size
              builder->getInt32(0x3000),  // MEM_COMMIT | MEM_RESERVE
              builder->getInt32(0x04)     // PAGE_READWRITE
          });
    }

    case SyscallType::FREE: {
      if (args.empty()) throw std::runtime_error("FREE requires pointer");

      llvm::Function* virtualFree = module->getFunction("VirtualFree");

      // VirtualFree(ptr, 0, MEM_RELEASE)
      // MEM_RELEASE = 0x8000
      return builder->CreateCall(
          virtualFree,
          {
              args[0],                   // pointer
              builder->getInt64(0),      // size (must be 0 for MEM_RELEASE)
              builder->getInt32(0x8000)  // MEM_RELEASE
          });
    }

    case SyscallType::WRITE: {
      if (args.size() < 3)
        throw std::runtime_error("WRITE requires fd, buffer, length");

      // For Windows, map file descriptors to handles
      llvm::Function* writeFile = module->getFunction("WriteFile");
      llvm::Function* getStdHandle = module->getFunction("GetStdHandle");

      // Map Unix fd to Windows handle
      llvm::Value* handle;
      if (auto* constFd = llvm::dyn_cast<llvm::ConstantInt>(args[0])) {
        int64_t fd = constFd->getSExtValue();
        if (fd == 1) {  // stdout
          handle = builder->CreateCall(getStdHandle, {builder->getInt32(-11)});
        } else if (fd == 2) {  // stderr
          handle = builder->CreateCall(getStdHandle, {builder->getInt32(-12)});
        } else {
          throw std::runtime_error("Unsupported file descriptor for Windows");
        }
      } else {
        throw std::runtime_error(
            "Dynamic file descriptors not supported on Windows");
      }

      // Convert length to 32-bit
      llvm::Value* length32 = args[2];
      if (args[2]->getType()->isIntegerTy(64)) {
        length32 = builder->CreateTrunc(args[2], builder->getInt32Ty());
      }

      llvm::Value* bytesWritten = builder->CreateAlloca(
          builder->getInt32Ty(), nullptr, "bytes_written");

      return builder->CreateCall(writeFile,
                                 {handle,
                                  args[1],   // buffer
                                  length32,  // length
                                  bytesWritten,
                                  llvm::ConstantPointerNull::get(
                                      llvm::PointerType::getUnqual(*context))});
    }

    default:
      throw std::runtime_error("Unsupported Windows syscall type");
  }
}

// ============================================================================
// LINUX SYSCALL IMPLEMENTATION (LIBC-FREE)
// ============================================================================

llvm::Value* SyscallFramework::generateLinuxSyscall(
    SyscallType type, std::vector<llvm::Value*>& args, DataType /*dataType*/) {
  switch (type) {
    case SyscallType::PRINT_STRING: {
      if (args.size() < 2)
        throw std::runtime_error(
            "PRINT_STRING requires string pointer and length");

      // sys_write = 1
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(1),  // sys_write
          builder->getInt64(1),  // stdout fd
          args[0],               // buffer
          args[1]                // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), builder->getInt64Ty(),
           llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      llvm::Value* result =
          builder->CreateCall(inlineAsm, asmArgs, "write_result");

      // Add newline
      llvm::Constant* newlineStr = builder->CreateGlobalString("\n", "newline");
      llvm::Value* newlinePtr = builder->CreatePointerCast(
          newlineStr, llvm::PointerType::getUnqual(*context));

      std::vector<llvm::Value*> newlineArgs = {
          builder->getInt64(1),  // sys_write
          builder->getInt64(1),  // stdout fd
          newlinePtr,
          builder->getInt64(1)  // length
      };

      builder->CreateCall(inlineAsm, newlineArgs, "newline_result");

      return result;
    }

    case SyscallType::EXIT: {
      if (args.empty()) throw std::runtime_error("EXIT requires exit code");

      // sys_exit = 60
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(60),  // sys_exit
          args[0]                 // exit code
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(), {builder->getInt64Ty(), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "exit_result");
    }

    case SyscallType::MALLOC: {
      if (args.empty()) throw std::runtime_error("MALLOC requires size");

      // sys_mmap = 9
      // mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1,
      // 0)
      std::string asmStr = "syscall";
      std::string constraintStr =
          "={rax},0,{rdi},{rsi},{rdx},{r10},{r8},{r9},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(9),  // sys_mmap
          llvm::ConstantPointerNull::get(
              llvm::PointerType::getUnqual(*context)),  // addr
          args[0],                                      // length
          builder->getInt64(3),   // PROT_READ | PROT_WRITE
          builder->getInt64(34),  // MAP_PRIVATE | MAP_ANONYMOUS
          builder->getInt64(-1),  // fd
          builder->getInt64(0)    // offset
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context),
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt64Ty(), builder->getInt64Ty(), builder->getInt64Ty(),
           builder->getInt64Ty(), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "mmap_result");
    }

    case SyscallType::FREE: {
      if (args.size() < 2)
        throw std::runtime_error("FREE requires pointer and size");

      // sys_munmap = 11
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(11),  // sys_munmap
          args[0],                // addr
          args[1]                 // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "munmap_result");
    }

    case SyscallType::WRITE: {
      if (args.size() < 3)
        throw std::runtime_error("WRITE requires fd, buffer, length");

      // sys_write = 1
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(1),  // sys_write
          args[0],               // fd
          args[1],               // buffer
          args[2]                // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), builder->getInt64Ty(),
           llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "write_result");
    }

    case SyscallType::READ: {
      if (args.size() < 3)
        throw std::runtime_error("READ requires fd, buffer, length");

      // sys_read = 0
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(0),  // sys_read
          args[0],               // fd
          args[1],               // buffer
          args[2]                // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), builder->getInt64Ty(),
           llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "read_result");
    }

    default:
      throw std::runtime_error("Unsupported Linux syscall type");
  }
}

// ============================================================================
// MACOS SYSCALL IMPLEMENTATION (LIBC-FREE)
// ============================================================================

llvm::Value* SyscallFramework::generateMacOSSyscall(
    SyscallType type, std::vector<llvm::Value*>& args, DataType /*dataType*/) {
  switch (type) {
    case SyscallType::PRINT_STRING: {
      if (args.size() < 2)
        throw std::runtime_error(
            "PRINT_STRING requires string pointer and length");

      // BSD sys_write = 0x2000004
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(0x2000004),  // BSD sys_write
          builder->getInt64(1),          // stdout fd
          args[0],                       // buffer
          args[1]                        // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), builder->getInt64Ty(),
           llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      llvm::Value* result =
          builder->CreateCall(inlineAsm, asmArgs, "write_result");

      // Add newline
      llvm::Constant* newlineStr = builder->CreateGlobalString("\n", "newline");
      llvm::Value* newlinePtr = builder->CreatePointerCast(
          newlineStr, llvm::PointerType::getUnqual(*context));

      std::vector<llvm::Value*> newlineArgs = {
          builder->getInt64(0x2000004),  // BSD sys_write
          builder->getInt64(1),          // stdout fd
          newlinePtr,
          builder->getInt64(1)  // length
      };

      builder->CreateCall(inlineAsm, newlineArgs, "newline_result");

      return result;
    }

    case SyscallType::EXIT: {
      if (args.empty()) throw std::runtime_error("EXIT requires exit code");

      // BSD sys_exit = 0x2000001
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(0x2000001),  // BSD sys_exit
          args[0]                        // exit code
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(), {builder->getInt64Ty(), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "exit_result");
    }

    case SyscallType::MALLOC: {
      if (args.empty()) throw std::runtime_error("MALLOC requires size");

      // BSD sys_mmap = 0x20000C5
      std::string asmStr = "syscall";
      std::string constraintStr =
          "={rax},0,{rdi},{rsi},{rdx},{r10},{r8},{r9},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(0x20000C5),  // BSD sys_mmap
          llvm::ConstantPointerNull::get(
              llvm::PointerType::getUnqual(*context)),  // addr
          args[0],                                      // length
          builder->getInt64(3),       // PROT_READ | PROT_WRITE
          builder->getInt64(0x1002),  // MAP_PRIVATE | MAP_ANON
          builder->getInt64(-1),      // fd
          builder->getInt64(0)        // offset
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context),
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt64Ty(), builder->getInt64Ty(), builder->getInt64Ty(),
           builder->getInt64Ty(), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "mmap_result");
    }

    case SyscallType::FREE: {
      if (args.size() < 2)
        throw std::runtime_error("FREE requires pointer and size");

      // BSD sys_munmap = 0x2000049
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(0x2000049),  // BSD sys_munmap
          args[0],                       // addr
          args[1]                        // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
           builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "munmap_result");
    }

    case SyscallType::WRITE: {
      if (args.size() < 3)
        throw std::runtime_error("WRITE requires fd, buffer, length");

      // BSD sys_write = 0x2000004
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(0x2000004),  // BSD sys_write
          args[0],                       // fd
          args[1],                       // buffer
          args[2]                        // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), builder->getInt64Ty(),
           llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "write_result");
    }

    case SyscallType::READ: {
      if (args.size() < 3)
        throw std::runtime_error("READ requires fd, buffer, length");

      // BSD sys_read = 0x2000003
      std::string asmStr = "syscall";
      std::string constraintStr = "={rax},0,{rdi},{rsi},{rdx},~{rcx},~{r11}";

      std::vector<llvm::Value*> asmArgs = {
          builder->getInt64(0x2000003),  // BSD sys_read
          args[0],                       // fd
          args[1],                       // buffer
          args[2]                        // length
      };

      llvm::FunctionType* asmType = llvm::FunctionType::get(
          builder->getInt64Ty(),
          {builder->getInt64Ty(), builder->getInt64Ty(),
           llvm::PointerType::getUnqual(*context), builder->getInt64Ty()},
          false);

      llvm::InlineAsm* inlineAsm =
          llvm::InlineAsm::get(asmType, asmStr, constraintStr, true);
      return builder->CreateCall(inlineAsm, asmArgs, "read_result");
    }

    default:
      throw std::runtime_error("Unsupported macOS syscall type");
  }
}

}  // namespace loom
