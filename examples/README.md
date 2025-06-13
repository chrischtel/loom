# Loom Examples

This directory contains example programs written in the Loom programming language to demonstrate various language features and use cases.

## Available Examples

### Cross-Platform System Programming

- **`advanced_syscalls.loom`** - Comprehensive demonstration of cross-platform syscall functionality
  - Function definitions with syscall integration
  - Error handling patterns
  - Control flow (loops, conditionals) with syscalls
  - Resource management simulation
  - Performance testing
  - Multiple exit strategies

- **`system_interaction.loom`** - Practical system interaction patterns
  - Safe I/O wrapper functions
  - System information display
  - Resource management demonstration
  - Error handling and status reporting
  - Performance testing with syscalls

- **`cross_platform_demo.loom`** - Platform compatibility showcase
  - Identical source code for Windows/Linux/macOS
  - Platform-specific implementation details
  - Cross-platform syscall mapping demonstration
  - Freestanding executable examples

### Key Features Demonstrated

- **Cross-Platform Syscalls**: Same Loom code generates different implementations:
  - Windows: Win32 API calls (WriteFile, ExitProcess)
  - Linux: Direct x86_64 syscall instructions
  - macOS: BSD-style syscall instructions

- **Freestanding Executables**: No libc or runtime dependencies
  - Direct system interface usage
  - Platform-specific entry points
  - Minimal binary size

- **Advanced Language Features**:
  - Function definitions and calls
  - Control flow (if statements, while loops)
  - Local variables and parameters
  - Recursive functions
  - Error handling patterns

## Building Examples

To compile any example:

```bash
# From the root Loom directory
./build/bin/loom.exe examples/advanced_syscalls.loom
./build/bin/loom.exe examples/system_interaction.loom  
./build/bin/loom.exe examples/cross_platform_demo.loom
```

## Platform Support

All examples demonstrate cross-platform functionality:

| Feature | Windows | Linux | macOS |
|---------|---------|-------|--------|
| Compilation | ✅ | ✅ | ✅ |
| Direct syscalls | ✅ | ✅ | ✅ |
| Freestanding execution | ✅ | ✅ | ✅ |
| No runtime dependencies | ✅ | ✅ | ✅ |

## Planned Examples

- `hello_world.loom` - Basic "Hello, World!" program
- `calculator.loom` - Simple calculator with basic arithmetic
- `fibonacci.loom` - Fibonacci sequence generator
- `file_io.loom` - File input/output operations
- `data_structures.loom` - Working with lists, maps, and other collections
- `concurrency.loom` - Concurrent programming examples
- `web_server.loom` - Simple HTTP server (advanced example)

## Current Status

✅ **Advanced syscall examples** - Fully implemented and tested
✅ **Cross-platform support** - Windows, Linux, macOS compatible
✅ **Freestanding execution** - No libc dependencies
🔄 **Additional examples** - Coming soon

## Example Output

When you run the examples, you'll see platform-specific behavior:

### Windows
- Uses WriteFile and ExitProcess APIs
- Links with kernel32.dll only
- Entry point: mainCRTStartup

### Linux  
- Uses direct syscall instructions
- Statically linked executable
- Entry point: _start

### macOS
- Uses BSD-style syscall numbers
- Statically linked executable  
- Entry point: _start

## Building Examples

Once the compiler is complete, examples can be built with:

```bash
bazel build //examples:hello_world
bazel build //examples:calculator
bazel run //examples:fibonacci
```

## Running Examples

```bash
bazel run //examples:hello_world
bazel run //examples:calculator -- [args]
```
