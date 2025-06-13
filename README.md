# Loom

# Loom

A small experimental programming language I'm building to learn about compiler design and systems programming.

## What it does

Currently supports basic programs with functions, variables, and simple control flow. Compiles to native executables without external dependencies.

```loom
func fibonacci(n: i32) i32 {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

func main() i32 {
    let result: i32 = fibonacci(10);
    $$print("Result calculated");
    return 0;
}
```

## Building

```bash
mkdir build && cd build
cmake ..
ninja
./bin/loom examples/cross_platform_demo.loom
```

## Current status

Very early development. 

**What's working:**
- Variable declarations (`let`, `mut`)
- Function definitions with parameters and return types
- Basic arithmetic operations (`+`, `-`, `*`, `/`)
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`)
- Control flow (`if`/`else`, `while` loops)
- Built-in functions (`$$print`, `$$exit`, `$$syscall`)
- Cross-platform compilation (Windows, Linux, macOS)
- Generates standalone executables

**What's missing:**
- No standard library yet
- Limited error messages  
- No package system
- Probably has bugs

This is a learning project, so feedback and suggestions are welcome!