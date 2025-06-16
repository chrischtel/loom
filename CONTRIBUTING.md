# Contributing to Loom

Thank you for your interest in contributing to Loom! This document provides guidelines for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Process](#development-process)
- [Pull Request Guidelines](#pull-request-guidelines)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Documentation](#documentation)
- [Release Process](#release-process)

## Code of Conduct

This project follows a simple principle: **Be excellent to each other.** We expect all contributors to:

- Be respectful and constructive in discussions
- Focus on what's best for the community
- Show empathy towards other contributors
- Be open to feedback and different perspectives

## Getting Started

### Prerequisites

- CMake 3.20+
- LLVM 17+ 
- C++20 compatible compiler
- Git
- Basic understanding of compiler design (helpful but not required)

### Setting Up Development Environment

1. **Fork and clone the repository:**
   ```bash
   git clone https://github.com/your-username/loom.git
   cd loom
   ```

2. **Set up build environment:**
   ```bash
   mkdir build && cd build
   cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
   ninja
   ```

3. **Verify the build:**
   ```bash
   ./bin/loom version
   ./bin/loom help
   ```

### Project Structure

```
loom/
├── compiler/           # Compiler implementation
│   ├── cli/           # Command-line interface
│   ├── codegen/       # Code generation (LLVM backend)
│   ├── common/        # Shared utilities
│   ├── parser/        # Parser and AST
│   ├── scanner/       # Lexical analysis
│   ├── sema/          # Semantic analysis
│   └── syscalls/      # System call framework
├── docs/              # Documentation
├── examples/          # Example Loom programs
├── stdlib/            # Standard library (future)
├── testing/           # Test framework
└── .github/           # CI/CD workflows
```

## Development Process

### Branching Model

- `main` - Stable code, ready for release
- `develop` - Integration branch for new features
- `feature/name` - Feature development branches
- `fix/name` - Bug fix branches
- `release/version` - Release preparation branches

### Workflow

1. **Create a feature branch:**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**
3. **Test your changes**
4. **Commit with descriptive messages**
5. **Push and create a pull request**

### Commit Message Format

We use conventional commits for consistency:

```
type(scope): brief description

Optional longer description explaining what and why.

Closes #issue-number
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation updates
- `style`: Formatting, no code change
- `refactor`: Code restructuring
- `test`: Adding tests
- `chore`: Maintenance tasks
- `ci`: CI/CD changes

**Examples:**
```
feat(parser): add support for array literals
fix(codegen): resolve memory leak in function calls
docs: update installation instructions
refactor(sema): simplify type checking logic
```

## Pull Request Guidelines

### Before Creating a PR

- [ ] Ensure your code builds without errors
- [ ] Run existing tests (when available)
- [ ] Update documentation if needed
- [ ] Add tests for new functionality
- [ ] Update CHANGELOG.md for significant changes

### PR Requirements

1. **Clear title and description**
   - Use conventional commit format for title
   - Explain what the PR does and why
   - Reference related issues

2. **Small, focused changes**
   - Keep PRs focused on a single concern
   - Avoid mixing unrelated changes

3. **Code quality**
   - Follow coding standards
   - Include comments for complex logic
   - Ensure no compiler warnings

### PR Process

1. **Automated checks run**
   - Build verification
   - Code formatting check
   - Security scan
   - Diff analysis

2. **Code review**
   - Maintainer review
   - Address feedback
   - Update as needed

3. **Merge**
   - Squash and merge for feature branches
   - Regular merge for release branches

## Coding Standards

### C++ Style Guide

We follow a modified Google C++ style with these key points:

**File naming:**
- Headers: `.hh` extension
- Sources: `.cc` extension
- Snake_case for files: `symbol_table.cc`

**Code formatting:**
```cpp
// Use clang-format for automatic formatting
// 2-space indentation
// 120 character line limit

class ExampleClass {
public:
  ExampleClass(int value);
  
  void doSomething();
  int getValue() const;

private:
  int value_;
  std::string name_;
};
```

**Naming conventions:**
- `PascalCase` for classes and types
- `camelCase` for functions and variables
- `UPPER_CASE` for constants
- `snake_case` for file names
- `trailing_underscore_` for private members

**Documentation:**
```cpp
/**
 * Brief description of the function.
 * 
 * @param parameter Description of parameter
 * @return Description of return value
 */
int exampleFunction(int parameter);
```

### Loom Language Style

For Loom example code:
- Use meaningful variable names
- Add comments explaining non-obvious logic
- Keep examples simple and focused
- Follow the language style guide (in docs/)

## Testing

### Running Tests

```bash
# Build and run all tests
cd build
ninja test

# Run specific test category
ctest -R "parser"
```

### Writing Tests

- Add unit tests for new functionality
- Include integration tests for major features
- Test error conditions and edge cases
- Use descriptive test names

Example test structure:
```cpp
TEST(ParserTest, ParseSimpleFunction) {
  std::string input = "func add(a i32, b i32) i32 { return a + b; }";
  
  Parser parser(input);
  auto ast = parser.parse();
  
  ASSERT_TRUE(ast != nullptr);
  ASSERT_EQ(ast->type(), ASTNodeType::Function);
  // ... additional assertions
}
```

## Documentation

### Types of Documentation

1. **Code documentation** - Inline comments and docstrings
2. **API documentation** - Generated from code comments  
3. **User documentation** - Guides, tutorials, examples
4. **Developer documentation** - Architecture, design decisions

### Writing Guidelines

- Write for your audience (user vs developer)
- Use clear, concise language
- Include practical examples
- Keep documentation up-to-date with code changes
- Use proper markdown formatting

### Building Documentation

```bash
# Generate API docs (future)
cd build
ninja docs

# Preview documentation site
cd docs
python -m http.server 8000
```

## Release Process

### Version Numbering

We use semantic versioning: `MAJOR.MINOR.PATCH-PRERELEASE`

- **MAJOR**: Breaking changes
- **MINOR**: New features, backwards compatible
- **PATCH**: Bug fixes, backwards compatible  
- **PRERELEASE**: alpha, beta, rc versions

### Release Types

1. **Alpha releases** (`v0.1.0-alpha.1`)
   - Early development versions
   - May have breaking changes
   - Released frequently

2. **Beta releases** (`v0.1.0-beta.1`)
   - Feature-complete for the version
   - Testing and bug fixing phase
   - API mostly stable

3. **Release candidates** (`v0.1.0-rc.1`)
   - Final testing before release
   - No new features, only critical fixes

4. **Stable releases** (`v0.1.0`)
   - Production-ready
   - Full testing and documentation

### Creating a Release

1. **Update version numbers**
2. **Update CHANGELOG.md**
3. **Create release branch**
4. **Final testing**
5. **Tag the release**
6. **Automated build and publish**

## Getting Help

- **Discord**: Join our [Discord server](https://discord.gg/loom-lang) (coming soon)
- **Issues**: Search existing issues or create a new one
- **Discussions**: Use GitHub Discussions for questions
- **Email**: Contact maintainers directly for sensitive issues

## Recognition

Contributors are recognized in:
- CHANGELOG.md for significant contributions
- GitHub contributors page
- Annual contributor spotlight (future)

Thank you for contributing to Loom! 🎉
