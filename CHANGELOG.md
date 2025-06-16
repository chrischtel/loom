# Loom Language Changelog

## [v0.1.0-alpha.3] - UNRELEASED - Phase 1 MVP: String Functions & Network Programming
### Added
- **String builtin functions**:
  - `$$strlen(str)` - Calculate string length automatically
  - Full LLVM implementation with null-terminator detection
  - Seamless integration with string literals and pointers
- **Enhanced type system**:
  - Automatic integer width promotion in binary expressions
  - Improved casting between integer types (i8, i16, i32, i64)
  - Better string literal to pointer conversion
- **Network programming support**:
  - Windows socket syscalls: `$$socket`, `$$bind`, `$$listen`, `$$accept`, `$$send`, `$$closesocket`
  - Winsock initialization: `$$WSAStartup`, `$$WSACleanup`
  - Complete HTTP server capabilities
- **Unified symbol table**:
  - Symbol table consistency between semantic analysis and codegen
  - Proper variable scoping in function contexts
  - Fixed union member access in codegen
- **Array literals** with unique `@` syntax: `@[1, 2, 3]`
- **Array indexing** with bracket syntax: `arr[index]`
- **Array type system**:
  - Slice types: `[]i32` for dynamic arrays
  - Array literal to slice conversion
  - Type checking for array elements

### Enhanced
- **Codegen improvements**:
  - Function scope management in symbol table
  - Type node cloning for proper memory management
  - Union field access validation
  - Integer width mismatch handling
- **String handling**:
  - Clean string literal usage in network code
  - Automatic length calculation eliminates manual byte counting
  - Proper HTTP protocol implementation
- Scanner recognizes array tokens and string functions
- Parser builds AST nodes for arrays and builtin calls
- Type casting system supports comprehensive type conversions

### Examples
```loom
// String length calculation with $$strlen
func test_strings() {
    let message: *i8 = cast(*i8, "Hello, World!");
    let length: i32 = $$strlen(message);  // Returns 13
    $$print("Message length: ");
    $$print(length);
}

// HTTP Server using $$strlen for proper response handling
func http_server() {
    // Network setup...
    let status: *i8 = cast(*i8, "HTTP/1.1 200 OK\r\n");
    let html: *i8 = cast(*i8, "<!DOCTYPE html><html>...</html>");
    
    // Automatic length calculation
    let status_len: i32 = $$strlen(status);
    let html_len: i32 = $$strlen(html);
    
    // Send response with correct lengths
    $$send(client_socket, status, status_len, 0);
    $$send(client_socket, html, html_len, 0);
}

// Array literals and indexing
func test_arrays() {
    let arr: []i32 = @[10, 20, 30];
    let val: i32 = arr[0];  // Gets 10
}
```

### Known Bugs
- **For loops and ranges**: Not yet implemented (planned for future release)
- **Symbol table edge cases**: Complex nested scopes may occasionally fail variable lookup
- **HTTP Content-Length calculation**: Currently requires manual calculation for dynamic content

### Technical Limitations
- String functions work perfectly for null-terminated strings
- Network programming requires Windows platform (Winsock dependency)
- Array functionality is basic but stable
- HTTP server handles one request per execution (design choice for simplicity)
- $$strlen operates on runtime strings - proper C-style null termination required

---

## [v0.1.0-alpha.2] - [13/6/2025] - Function Support & Cross-Platform Syscalls
### Added
- Function declarations with parameters and return types
- Function calls with argument passing
- Return statements
- Recursive functions (factorial, fibonacci work)
- Comparison operators: `<=` and `>=`
- Local variables in functions
- Enhanced print() function for integers and strings
- **Cross-platform syscall support** for Windows, Linux, and macOS
- Platform-specific code generation:
  - Windows: Direct Windows API calls (WriteFile, ExitProcess)
  - Linux: Inline assembly syscall instructions
  - macOS: Inline assembly with BSD-style syscall numbers
- Automatic platform detection from LLVM target triple
- Unified builtin interface (`$$print`, `$$exit`, `$$syscall`) across all platforms
- Platform-specific entry points:
  - Windows: `mainCRTStartup`
  - Linux/macOS: `_start`
- **Freestanding executable support** - no libc dependency on any platform
- Cross-platform linking with platform-appropriate flags
- Generic `$$syscall(number, args...)` interface with platform mapping

### Enhanced
- Improved codegen architecture with platform-specific syscall methods
- Target triple configuration for cross-compilation
- Entry point generation based on target platform

### Fixed
- Type checking for integer literal operations
- Missing comparison operators in scanner and parser
- Symbol table now supports both variables and functions

### Technical
- New AST nodes: FunctionDeclNode, ParameterNode, ReturnStmtNode
- Enhanced semantic analyzer for function validation
- LLVM code generation for functions
- Improved error messages for missing main function
- New platform detection enum: `TargetPlatform` (Windows/Linux/MacOS/Unknown)
- Platform-specific syscall generation methods:
  - `generateWindowsSyscall()`: Maps to Windows API
  - `generateLinuxSyscall()`: x86_64 inline assembly
  - `generateMacOSSyscall()`: BSD syscall numbers with inline assembly
- Updated linking strategy for freestanding executables
- Cross-platform object file generation with correct target triples

### Examples
```loom
// Function example
func add(x: i32, y: i32) i32 {
    return x + y;
}

// Cross-platform syscall example
func main() i32 {
    print(add(3, 4));
    $$print("Hello from cross-platform Loom!");
    
    // Generic syscall interface works on all platforms
    $$syscall(1, 1, "Direct syscall!", 15);
    $$syscall(60, 0);  // Cross-platform exit
    
    return 0;
}
```

### Platform Support Matrix
| Feature | Windows | Linux | macOS |
|---------|---------|-------|--------|
| Direct API calls | ✅ | ✅ | ✅ |
| Freestanding executables | ✅ | ✅ | ✅ |
| Syscall mapping | ✅ | ✅ | ✅ |
| No libc dependency | ✅ | ✅ | ✅ |

---

## TODO - in the future
### NOTE: This is a work in progress and does NOT reflect the PLANNED changes for the next release.
- Add support for multi-line strings with triple backticks
- for loops
- String variables (currently only string literals in print work)
- Arrays and collections
- More built-in functions
- Better error messages and debugging support

## [v0.1.0-alpha.1] - Previous Release
### Added
- Support for String literals with escape sequences
- Added support for multi-line comments (""" comment\n\n comment """)

### Fixed
- Fixed broken escape sequence handling in string literals