# Loom Language Changelog

## [v0.1.0-alpha.3] - [14/6/2025] - Phase 1 MVP: Arrays, For Loops & Ranges
### Added
- **Array literals** with unique `@` syntax: `@[1, 2, 3]`
- **Array indexing** with unique `@` syntax: `arr@[index]`
- **For loops** with parentheses syntax: `for(i in range)`
- **Range expressions**:
  - Exclusive ranges: `0..10` (0 to 9)
  - Inclusive ranges: `0..=10` (0 to 10)
- **Array type system**:
  - Slice types: `[]i32` for dynamic arrays
  - Array literal to slice conversion
  - Type checking for array elements
- **Enhanced semantic analyzer**:
  - Fixed array literal type consistency checking
  - Integer literal type unification
  - Loop variable scoping
- **LLVM code generation**:
  - Array literals generate proper stack-allocated arrays
  - Array-to-slice conversion with `{ ptr, length }` struct
  - For loop control flow with proper basic blocks
  - Range evaluation (both exclusive and inclusive)

### Enhanced
- Scanner recognizes new tokens: `@`, `..`, `..=`, `for`, `in`
- Parser builds AST nodes: `ArrayLiteralExpr`, `IndexExpr`, `ForStmtNode`, `RangeExpr`
- Type casting system supports array-to-slice conversion
- Integer printing support in Windows syscalls (placeholder implementation)

### Examples
```loom
// Array literals and indexing
func test_arrays() {
    let arr: []i32 = @[10, 20, 30];
    let val: i32 = arr@[0];  // Gets 10
}

// For loops with ranges
func test_loops() {
    // Exclusive range (0, 1, 2, 3, 4)
    for(i in 0..5) {
        $$print(i);
    }
    
    // Inclusive range (1, 2, 3)
    for(j in 1..=3) {
        $$print(j);
    }
}
```

### Known Bugs
- **Array indexing semantic analysis failure**: Complex programs with array indexing may fail during semantic analysis due to variable scoping issues
- **Integer printing shows placeholders**: Runtime integer variables print `<integer>` instead of actual values
- **Hardcoded array sizes**: Array-to-slice conversion uses hardcoded size (3) instead of dynamic size detection
- **Array indexing codegen incomplete**: IndexExpr codegen may not handle slice types correctly
- **Function call semantic analysis**: Programs with multiple functions may fail semantic analysis after the first builtin call
- **Symbol table scoping**: Variable lookup may fail in complex nested scopes with arrays

### Technical Limitations
- Array indexing works in simple cases but fails in complex multi-function programs
- Integer printing is compile-time only (constants work, runtime variables show placeholders)
- Array sizes must be small and are hardcoded in type conversion
- Phase 1 focuses on syntax and basic functionality rather than robust runtime behavior

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