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

    case SyscallType::OPEN: {
      if (args.empty()) throw std::runtime_error("OPEN requires filename");

      llvm::Function* createFile = module->getFunction("CreateFileA");

      // CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
      // OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) GENERIC_READ = 0x80000000,
      // FILE_SHARE_READ = 0x00000001, OPEN_EXISTING = 3, FILE_ATTRIBUTE_NORMAL
      // = 0x80
      return builder->CreateCall(
          createFile,
          {
              args[0],                        // filename
              builder->getInt32(0x80000000),  // GENERIC_READ
              builder->getInt32(0x00000001),  // FILE_SHARE_READ
              llvm::ConstantPointerNull::get(
                  llvm::PointerType::getUnqual(*context)),  // NULL security
              builder->getInt32(3),                         // OPEN_EXISTING
              builder->getInt32(0x80),  // FILE_ATTRIBUTE_NORMAL
              llvm::ConstantPointerNull::get(
                  llvm::PointerType::getUnqual(*context))  // NULL template
          });
    }

    case SyscallType::CLOSE: {
      if (args.empty()) throw std::runtime_error("CLOSE requires handle");

      llvm::Function* closeHandle = module->getFunction("CloseHandle");
      return builder->CreateCall(closeHandle, {args[0]});
    }

    case SyscallType::READ: {
      if (args.size() < 3)
        throw std::runtime_error("READ requires handle, buffer, length");

      llvm::Function* readFile = module->getFunction("ReadFile");

      // Convert length to 32-bit if needed
      llvm::Value* length32 = args[2];
      if (args[2]->getType()->isIntegerTy(64)) {
        length32 = builder->CreateTrunc(args[2], builder->getInt32Ty());
      }

      llvm::Value* bytesRead =
          builder->CreateAlloca(builder->getInt32Ty(), nullptr, "bytes_read");

      return builder->CreateCall(
          readFile,
          {
              args[0],    // handle
              args[1],    // buffer
              length32,   // length
              bytesRead,  // bytes read
              llvm::ConstantPointerNull::get(
                  llvm::PointerType::getUnqual(*context))  // NULL overlapped
          });
    }

    case SyscallType::GET_FILE_SIZE: {
      if (args.empty())
        throw std::runtime_error("GET_FILE_SIZE requires handle");

      llvm::Function* getFileSize = module->getFunction("GetFileSize");
      return builder->CreateCall(
          getFileSize,
          {
              args[0],  // handle
              llvm::ConstantPointerNull::get(
                  llvm::PointerType::getUnqual(*context))  // NULL for high part
          });
    }

      // ============================================================================
      // NETWORKING SYSCALLS
      // ============================================================================

    case SyscallType::SOCKET: {
      if (args.size() < 3)
        throw std::runtime_error("SOCKET requires family, type, protocol");

      llvm::Function* socketFunc = module->getFunction("socket");
      if (!socketFunc) {
        llvm::FunctionType* socketType = llvm::FunctionType::get(
            // FIX: SOCKET is i64 on 64-bit Windows
            builder->getInt64Ty(),
            {builder->getInt32Ty(), builder->getInt32Ty(),
             builder->getInt32Ty()},
            false);
        socketFunc = llvm::Function::Create(
            socketType, llvm::Function::ExternalLinkage, "socket", module);
      }

      llvm::Value* family = args[0];
      llvm::Value* type = args[1];
      llvm::Value* protocol = args[2];

      if (family->getType()->isIntegerTy(64))
        family = builder->CreateTrunc(family, builder->getInt32Ty());
      if (type->getType()->isIntegerTy(64))
        type = builder->CreateTrunc(type, builder->getInt32Ty());
      if (protocol->getType()->isIntegerTy(64))
        protocol = builder->CreateTrunc(protocol, builder->getInt32Ty());

      return builder->CreateCall(socketFunc, {family, type, protocol});
    }

    case SyscallType::BIND: {
      if (args.size() < 3)
        throw std::runtime_error(
            "BIND requires socket, address, address_length");

      llvm::Function* bindFunc = module->getFunction("bind");
      if (!bindFunc) {
        llvm::FunctionType* bindType = llvm::FunctionType::get(
            builder->getInt32Ty(),
            {// FIX: Socket is i64
             builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
             builder->getInt32Ty()},
            false);
        bindFunc = llvm::Function::Create(
            bindType, llvm::Function::ExternalLinkage, "bind", module);
      }

      // NO LONGER TRUNCATE SOCKET
      return builder->CreateCall(bindFunc, {args[0], args[1], args[2]});
    }

    case SyscallType::LISTEN: {
      if (args.size() < 2)
        throw std::runtime_error("LISTEN requires socket, backlog");

      llvm::Function* listenFunc = module->getFunction("listen");
      if (!listenFunc) {
        llvm::FunctionType* listenType = llvm::FunctionType::get(
            builder->getInt32Ty(),
            {// FIX: Socket is i64
             builder->getInt64Ty(), builder->getInt32Ty()},
            false);
        listenFunc = llvm::Function::Create(
            listenType, llvm::Function::ExternalLinkage, "listen", module);
      }

      // NO LONGER TRUNCATE SOCKET
      return builder->CreateCall(listenFunc, {args[0], args[1]});
    }

    case SyscallType::ACCEPT: {
      if (args.size() < 3)
        throw std::runtime_error(
            "ACCEPT requires socket, address, address_length");

      llvm::Function* acceptFunc = module->getFunction("accept");
      if (!acceptFunc) {
        llvm::FunctionType* acceptType = llvm::FunctionType::get(
            // FIX: Return SOCKET is i64
            builder->getInt64Ty(),
            {// FIX: Socket is i64
             builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
             llvm::PointerType::getUnqual(*context)},
            false);
        acceptFunc = llvm::Function::Create(
            acceptType, llvm::Function::ExternalLinkage, "accept", module);
      }

      // NO LONGER TRUNCATE SOCKET
      return builder->CreateCall(acceptFunc, {args[0], args[1], args[2]});
    }

    case SyscallType::CONNECT: {
      if (args.size() < 3)
        throw std::runtime_error(
            "CONNECT requires socket, address, address_length");

      llvm::Function* connectFunc = module->getFunction("connect");
      if (!connectFunc) {
        llvm::FunctionType* connectType = llvm::FunctionType::get(
            builder->getInt32Ty(),  // int return type
            {
                builder->getInt32Ty(),                   // socket
                llvm::PointerType::getUnqual(*context),  // sockaddr*
                builder->getInt32Ty()                    // address length
            },
            false);
        connectFunc = llvm::Function::Create(
            connectType, llvm::Function::ExternalLinkage, "connect", module);
      }

      llvm::Value* socket = args[0];
      llvm::Value* addrlen = args[2];

      if (socket->getType()->isIntegerTy(64)) {
        socket = builder->CreateTrunc(socket, builder->getInt32Ty());
      }
      if (addrlen->getType()->isIntegerTy(64)) {
        addrlen = builder->CreateTrunc(addrlen, builder->getInt32Ty());
      }

      return builder->CreateCall(connectFunc, {socket, args[1], addrlen},
                                 "connect_result");
    }

    case SyscallType::SEND: {
      if (args.size() < 4)
        throw std::runtime_error("SEND requires socket, buffer, length, flags");

      llvm::Function* sendFunc = module->getFunction("send");
      if (!sendFunc) {
        llvm::FunctionType* sendType = llvm::FunctionType::get(
            builder->getInt32Ty(),  // int return type (bytes sent)
            {
                builder->getInt32Ty(),                   // socket
                llvm::PointerType::getUnqual(*context),  // buffer
                builder->getInt32Ty(),                   // length
                builder->getInt32Ty()                    // flags
            },
            false);
        sendFunc = llvm::Function::Create(
            sendType, llvm::Function::ExternalLinkage, "send", module);
      }

      llvm::Value* socket = args[0];
      llvm::Value* length = args[2];
      llvm::Value* flags = args[3];

      if (socket->getType()->isIntegerTy(64)) {
        socket = builder->CreateTrunc(socket, builder->getInt32Ty());
      }
      if (length->getType()->isIntegerTy(64)) {
        length = builder->CreateTrunc(length, builder->getInt32Ty());
      }
      if (flags->getType()->isIntegerTy(64)) {
        flags = builder->CreateTrunc(flags, builder->getInt32Ty());
      }

      return builder->CreateCall(sendFunc, {socket, args[1], length, flags},
                                 "send_result");
    }

    case SyscallType::RECV: {
      if (args.size() < 4)
        throw std::runtime_error("RECV requires socket, buffer, length, flags");

      llvm::Function* recvFunc = module->getFunction("recv");
      if (!recvFunc) {
        llvm::FunctionType* recvType = llvm::FunctionType::get(
            builder->getInt32Ty(),  // int return type (bytes received)
            {
                builder->getInt32Ty(),                   // socket
                llvm::PointerType::getUnqual(*context),  // buffer
                builder->getInt32Ty(),                   // length
                builder->getInt32Ty()                    // flags
            },
            false);
        recvFunc = llvm::Function::Create(
            recvType, llvm::Function::ExternalLinkage, "recv", module);
      }

      llvm::Value* socket = args[0];
      llvm::Value* length = args[2];
      llvm::Value* flags = args[3];

      if (socket->getType()->isIntegerTy(64)) {
        socket = builder->CreateTrunc(socket, builder->getInt32Ty());
      }
      if (length->getType()->isIntegerTy(64)) {
        length = builder->CreateTrunc(length, builder->getInt32Ty());
      }
      if (flags->getType()->isIntegerTy(64)) {
        flags = builder->CreateTrunc(flags, builder->getInt32Ty());
      }
      return builder->CreateCall(recvFunc, {socket, args[1], length, flags},
                                 "recv_result");
    }

    case SyscallType::HTONS: {
      if (args.empty()) {
        throw std::runtime_error("HTONS requires one 16-bit argument");
      }

      llvm::Value* host_short = args[0];
      if (!host_short->getType()->isIntegerTy(16)) {
        host_short = builder->CreateTrunc(host_short, builder->getInt16Ty(),
                                          "host_short_trunc");
      }

      llvm::Function* bswap_func = llvm::Intrinsic::getOrInsertDeclaration(
          module, llvm::Intrinsic::bswap, {builder->getInt16Ty()});

      return builder->CreateCall(bswap_func, {host_short}, "network_short");
    }

    case SyscallType::GENERIC: {
      // For WSAStartup (args: version, wsadata*)
      if (args.size() == 2) {
        std::string funcName = "WSAStartup";
        llvm::Function* wsaStartupFunc = module->getFunction(funcName);
        if (!wsaStartupFunc) {
          llvm::FunctionType* wsaStartupType = llvm::FunctionType::get(
              builder->getInt32Ty(),
              {builder->getInt16Ty(),  // version (WORD)
                                       // FIX: Use the type of the POINTER
                                       // argument (args[1])
               args[1]->getType()},
              false);
          wsaStartupFunc = llvm::Function::Create(
              wsaStartupType, llvm::Function::ExternalLinkage, funcName,
              module);
        }

        llvm::Value* version = args[0];
        if (version->getType()->isIntegerTy(64)) {
          version = builder->CreateTrunc(version, builder->getInt16Ty());
        } else if (version->getType()->isIntegerTy(32)) {
          version = builder->CreateTrunc(version, builder->getInt16Ty());
        }

        // Call with correct order: version, pointer
        return builder->CreateCall(wsaStartupFunc, {version, args[1]});
      }
      // For WSACleanup (no args)
      else if (args.size() == 0) {
        llvm::Function* wsaCleanupFunc = module->getFunction("WSACleanup");
        if (!wsaCleanupFunc) {
          llvm::FunctionType* wsaCleanupType =
              llvm::FunctionType::get(builder->getInt32Ty(), {}, false);
          wsaCleanupFunc = llvm::Function::Create(
              wsaCleanupType, llvm::Function::ExternalLinkage, "WSACleanup",
              module);
        }
        return builder->CreateCall(wsaCleanupFunc, {});
      }
      // For closesocket (args: socket)
      else if (args.size() == 1) {
        llvm::Function* closesocketFunc = module->getFunction("closesocket");
        if (!closesocketFunc) {
          llvm::FunctionType* closesocketType =
              llvm::FunctionType::get(builder->getInt32Ty(),
                                      {// FIX: Socket is i64
                                       builder->getInt64Ty()},
                                      false);
          closesocketFunc = llvm::Function::Create(
              closesocketType, llvm::Function::ExternalLinkage, "closesocket",
              module);
        }
        // NO LONGER TRUNCATE SOCKET
        return builder->CreateCall(closesocketFunc, {args[0]});
      } else {
        throw std::runtime_error("Unsupported GENERIC syscall argument count");
      }
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
