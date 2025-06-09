# Loom Testing Framework

This directory contains the comprehensive testing infrastructure for the Loom project, including unit tests for the compiler (written in C++) and future unit tests for the standard library (written in Loom).

## Structure

### Test Framework
- `test_runner.h/cc` - Core test runner infrastructure
- `test_utils.h/cc` - Testing utilities and helpers
- `main_test_runner.cc` - Main test runner executable

### Compiler Tests (C++)
- `compiler/` - Unit tests for compiler components
  - `lexer_test.cc` - Lexer unit tests
  - `parser_test.cc` - Parser unit tests
  - `ast_test.cc` - AST unit tests
  - `semantic_analyzer_test.cc` - Semantic analysis tests
  - `code_generator_test.cc` - Code generation tests

### Integration Tests
- `integration/` - End-to-end compiler tests
  - `compile_and_run_test.cc` - Compilation and execution tests
  - `error_handling_test.cc` - Error reporting tests

### Standard Library Tests (Future)
- `stdlib/` - Unit tests for stdlib (written in Loom)
  - `core_test.loom` - Core library tests
  - `collections_test.loom` - Collections library tests
  - `io_test.loom` - I/O library tests

### Test Data
- `data/` - Test input files and expected outputs

## Running Tests

Run all tests:
```bash
bazel test //testing:all_tests
```

Run specific test suites:
```bash
bazel test //testing:compiler_tests
bazel test //testing:compiler_integration_tests
```

Run with coverage:
```bash
bazel coverage //testing:all_tests
```

## Test Configuration

Tests are configured in `.bazelrc` with appropriate flags for detailed output and error reporting.
