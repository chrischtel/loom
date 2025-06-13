# Loom Language Changelog

## [v0.1.0-alpha.2] - [UNRELEASED] - Function Support
### Added
- Function declarations with parameters and return types
- Function calls with argument passing
- Return statements
- Recursive functions (factorial, fibonacci work)
- Comparison operators: `<=` and `>=`
- Local variables in functions
- Enhanced print() function for integers and strings

### Fixed
- Type checking for integer literal operations
- Missing comparison operators in scanner and parser
- Symbol table now supports both variables and functions

### Technical
- New AST nodes: FunctionDeclNode, ParameterNode, ReturnStmtNode
- Enhanced semantic analyzer for function validation
- LLVM code generation for functions
- Improved error messages for missing main function

### Examples
```loom
func add(x: i32, y: i32) i32 {
    return x + y;
}

func main() i32 {
    print(add(3, 4));
    return 0;
}
```

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