# Loom Programming Language

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

## Building and Usage

### Build from Source
```bash
mkdir build && cd build
cmake ..
ninja
```

### Compile Loom Programs
```bash
# Compile a program
./bin/loom build example.loom

# Run with debugging info
./bin/loom build --verbose example.loom

# Check syntax without compiling  
./bin/loom check example.loom
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
