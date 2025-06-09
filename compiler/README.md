# Loom Compiler

This directory contains the implementation of the Loom programming language compiler.

## Structure

- `main.cc` - Entry point for the compiler
- `lexer.h/cc` - Lexical analyzer (tokenization)
- `parser.h/cc` - Syntax analyzer (parsing)
- `ast.h/cc` - Abstract Syntax Tree representation
- `semantic_analyzer.h/cc` - Semantic analysis and type checking
- `code_generator.h/cc` - Code generation backend
- `error_reporter.h/cc` - Error reporting utilities
- `compiler.h` - Main compiler interface

## Building

To build the compiler:

```bash
bazel build //compiler:main
```

To run the compiler:

```bash
bazel run //compiler:main -- [options] <source_file>
```

## Testing

Run compiler tests:

```bash
bazel test //testing:compiler_tests
```
