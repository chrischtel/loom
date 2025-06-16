# Loom Programming Language

[![CI](https://github.com/chrischtel/loom/actions/workflows/ci.yml/badge.svg)](https://github.com/chrischtel/loom/actions/workflows/ci.yml)
[![Release](https://github.com/chrischtel/loom/actions/workflows/release.yml/badge.svg)](https://github.com/chrischtel/loom/actions/workflows/release.yml)
[![Nightly](https://github.com/chrischtel/loom/actions/workflows/nightly.yml/badge.svg)](https://github.com/chrischtel/loom/actions/workflows/nightly.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

**Structured Freedom**: A systems programming language that makes the safe, high-performance path the most ergonomic one, while providing clear escape hatches when you need to talk directly to the metal.

## What is Loom?

Loom is a modern systems programming language designed around the philosophy of **Structured Freedom**. It combines the performance and control of systems languages like C and Rust with the expressiveness and safety features of higher-level languages.

### Core Paradigm: Structured Freedom

- **Ergonomic Safety**: Safety features feel natural, not restrictive
- **Pragmatic Performance**: Zero-cost abstractions are the default  
- **Clarity Over Dogma**: Rules serve the developer, not the other way around
- **Expressive Tooling**: Designed for an exceptional developer experience

## Key Features

### 🔒 **Memory Safety with Control**
- Explicit ownership types (`*`, `&`, `[]`) for precise memory management
- Nullable types (`T?`) with pattern matching for null safety
- Phantom types for compile-time resource state tracking

### 🚀 **Performance-First Design**
- Compiles to native executables with no runtime overhead
- Zero-cost abstractions throughout
- Direct system call access when needed

### 🧩 **Expressive Type System**  
- Generics with square bracket syntax: `List[T]`
- Protocols (interfaces) with powerful constraints
- Custom operators for domain-specific languages
- Operator overloading with the `op` keyword

### 🔄 **Modern Concurrency**
- Grant-based permission system for safe concurrent access
- Async/await with structured concurrency
- Context management for implicit parameter passing

### 📦 **Modular Architecture**
- Unique module syntax: `@module` and `from @module take ...`
- Fine-grained visibility control
- Pipeline operators (`|>`) for data transformation

## Quick Example

```loom
struct Point {
    x: f64,
    y: f64,
}

// Custom operators for domain expressiveness  
operator `<->` (left: Point, right: Point) f64 {
    let dx = left.x - right.x;
    let dy = left.y - right.y;
    return sqrt(dx * dx + dy * dy);
}

// Generic function with protocol constraints
func find_closest[T](points: List[T], target: T) T?
requires T: Comparable {
    // Implementation with compile-time guarantees
}

func main() i32 {
    let origin = Point(x: 0.0, y: 0.0);
    let destination = Point(x: 3.0, y: 4.0);
    
    // Use custom operator
    let distance = origin <-> destination;  // 5.0
    
    $$print("Distance calculated");
    return 0;
}
```

## Installation

### Quick Install (Recommended)

**Linux/macOS:**
```bash
curl -sSL https://raw.githubusercontent.com/chrischtel/loom/main/install.sh | bash
```

**Windows (PowerShell):**
```powershell
iwr -useb https://raw.githubusercontent.com/chrischtel/loom/main/install.ps1 | iex
```

### Manual Download

Download pre-built binaries from the [releases page](https://github.com/chrischtel/loom/releases):

- **Stable releases**: Latest stable version
- **Pre-releases**: Alpha/beta versions with newest features  
- **Nightly builds**: Daily builds from main branch (development)

### Package Managers

Coming soon: Homebrew, Chocolatey, and Linux package repositories.

### Build from Source

**Prerequisites:**
- CMake 3.20+
- LLVM 17+
- C++20 compatible compiler (Clang 14+, GCC 11+, MSVC 2022+)
- Ninja (recommended) or Make

**Build steps:**
```bash
git clone https://github.com/chrischtel/loom.git
cd loom
mkdir build && cd build
cmake .. -G Ninja
ninja
```

**Install:**
```bash
ninja install  # or: cmake --install .
```

## Building and Usage

### Compile Loom Programs
```bash
# Compile a program
loom build example.loom

# Run with debugging info
loom build --verbose example.loom

# Check syntax without compiling  
loom check example.loom

# Get help
loom help

# Show version information
loom version
```

## Development Status

Loom is in active development. The language specification is stable, with core features partially implemented. Std library is not yet available, but basic functionality is provided through the Loom compiler itself.



## Philosophy in Action

Loom isn't just another systems language. It's built on the belief that developers shouldn't have to choose between safety and performance, or between expressiveness and control. Every design decision serves the goal of making the right thing the easy thing, while keeping escape hatches available when you need them.

Whether you're building operating systems, game engines, embedded software, or high-performance applications, Loom gives you the tools to express your intent clearly and efficiently.

---

**License**: MIT/Apache-2.0 dual-licensed STD-Library and Loom compiler GPL-3.0 licensed. 
**Status**: Active Development  
**Contributions**: Welcome!

This is a learning project, so feedback and suggestions are welcome!
