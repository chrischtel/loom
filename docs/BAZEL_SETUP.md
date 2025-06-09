# Bazel Setup Guide for Loom

This document explains the Bazel build system setup for the Loom programming language project.

## Prerequisites

1. **Install Bazel**: Follow the [official Bazel installation guide](https://bazel.build/install)
2. **C++ Compiler**: Ensure you have a modern C++ compiler (GCC 9+, Clang 10+, or MSVC 2019+)
3. **Git**: For version control

## Project Structure

```
loom/
├── WORKSPACE                 # Bazel workspace definition
├── BUILD.bazel              # Root build configuration
├── .bazelrc                 # Bazel configuration options
├── .bazelversion            # Pinned Bazel version
├── compiler/                # Loom compiler implementation (C++)
│   ├── BUILD.bazel
│   ├── main.cc
│   ├── compiler.h
│   └── README.md
├── stdlib/                  # Standard library (future Loom code)
│   ├── BUILD.bazel
│   └── README.md
├── examples/                # Example Loom programs
│   ├── BUILD.bazel
│   └── README.md
├── testing/                 # Test framework and tests
│   ├── BUILD.bazel
│   └── README.md
└── docs/                   # Documentation
```

## Building the Project

### Build the Compiler
```bash
# Build the main compiler
bazel build //compiler:main

# Build all compiler components
bazel build //compiler:...

# Build the entire project
bazel build //...
```

### Run the Compiler
```bash
# Run the compiler directly
bazel run //compiler:main -- --input=example.loom --output=example.out

# Or use the convenient alias
bazel run //:loom -- --input=example.loom
```

### Build Configurations

The project supports multiple build configurations:

```bash
# Debug build (default for development)
bazel build --config=debug //compiler:main

# Release build (optimized)
bazel build --config=release //compiler:main

# Fast build (for quick iteration)
bazel build --config=fastbuild //compiler:main
```

## Testing

### Run All Tests
```bash
bazel test //testing:all_tests
```

### Run Specific Test Suites
```bash
# Compiler unit tests
bazel test //testing:compiler_tests

# Integration tests
bazel test //testing:compiler_integration_tests
```

### Test with Sanitizers
```bash
# Address sanitizer
bazel test --config=asan //testing:all_tests

# Thread sanitizer  
bazel test --config=tsan //testing:all_tests

# Undefined behavior sanitizer
bazel test --config=ubsan //testing:all_tests
```

### Coverage Reports
```bash
bazel coverage //testing:all_tests
```

## Dependencies

The project uses the following external dependencies managed by Bazel:

- **Abseil C++**: Modern C++ utilities and base libraries
- **Google Test**: C++ testing framework
- **Protocol Buffers**: For potential IR serialization
- **LLVM** (commented out): For code generation backend

## Development Workflow

### 1. Make Changes
Edit source files in `compiler/`, add tests in `testing/`, etc.

### 2. Build and Test
```bash
# Quick build to check compilation
bazel build //compiler:main

# Run tests to ensure nothing broke
bazel test //testing:compiler_tests

# Run the compiler on test files
bazel run //compiler:main -- --input=test.loom
```

### 3. Add New Components

When adding new compiler components:

1. Add source files to `compiler/`
2. Update `compiler/BUILD.bazel` to include new files
3. Add corresponding tests in `testing/compiler/`
4. Update `testing/BUILD.bazel` to include new test files

### 4. Future: Loom Language Support

Once the compiler is mature enough to compile Loom code, you'll need to:

1. Define custom Bazel rules for Loom (similar to `cc_library`, `cc_binary`)
2. Update `stdlib/BUILD.bazel` to use these rules
3. Update `examples/BUILD.bazel` to build Loom programs
4. Add Loom-based tests to `testing/stdlib/`

## Bazel Best Practices

### 1. Visibility
- Use `package(default_visibility = ["//visibility:public"])` sparingly
- Prefer explicit visibility declarations for better encapsulation

### 2. Dependencies
- Keep dependencies minimal and specific
- Use `deps` for direct dependencies, `data` for runtime files

### 3. Testing
- Organize tests by size: `small`, `medium`, `large`
- Use appropriate test data and fixtures

### 4. Performance
- Use `--config=fastbuild` for development
- Use `--config=release` for benchmarking and releases

## Troubleshooting

### Common Issues

1. **Build fails with missing dependencies**
   ```bash
   bazel clean
   bazel build //...
   ```

2. **Tests fail to find test data**
   - Ensure test data is properly declared in `data` attribute
   - Use `bazel test --test_output=all` for detailed output

3. **Slow builds**
   - Use `--config=fastbuild` for development
   - Consider using `--jobs=N` to control parallelism

### Getting Help

- Check the [Bazel documentation](https://bazel.build/docs)
- Look at existing BUILD files for examples
- Use `bazel query` to understand dependency graphs

## Next Steps

1. Implement the core compiler components (lexer, parser, etc.)
2. Add comprehensive tests for each component
3. Define custom Bazel rules for Loom language files
4. Implement the standard library in Loom
5. Create example programs and documentation
