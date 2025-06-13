# Loom

Loom is a compiled programming language for efficient system programming. This is a experimental project that aims to provide a simple, statically typed language with direct access to system resources and cross-platform capabilities.


## Features

- **Static typing** with type inference
- **Function definitions** with typed parameters and return values
- **Variables** - immutable (`let`) and mutable (`mut`) declarations
- **Control flow** - if/else statements, while loops
- **Built-in functions** - `$$print`, `$$syscall`, `$$exit` for system interaction
- **Direct compilation** to machine code without runtime overhead
- **Cross-platform** - same code works on Windows, Linux, and macOS

## Syntax

Basic variable declarations:
```loom
let name: string = "world";
mut counter: i32 = 0;
```

Functions:
```loom
func fibonacci(n: i32) i32 {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}
```

Complete program:
```loom
func main() i32 {
    let result: i32 = fibonacci(10);
    $$print("Result calculated");
    return 0;
}
```

## Building

Requires CMake and a C++ compiler.

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

Compile a Loom file:

```bash
./build/bin/loom examples/cross_platform_demo.loom
```

This produces a standalone executable with no external dependencies.

## Status

Early development. Loom is not yet feature complete but already supports:
- variable declarations using `let`, `mut` and `define`
- function definitions with parameters and return types 
- basic arithmetic operations 
- control flow with `if`, `else`, and `while`
- built-in functions for printing and exiting
- cross-platform syscall support for Windows, Linux, and macOS (untested on macOS and Linux)
- freestanding executables with no libc dependencies