# Loom Examples

This directory contains example programs written in the Loom programming language to demonstrate various language features and use cases.

## Planned Examples

- `hello_world.loom` - Basic "Hello, World!" program
- `calculator.loom` - Simple calculator with basic arithmetic
- `fibonacci.loom` - Fibonacci sequence generator
- `file_io.loom` - File input/output operations
- `data_structures.loom` - Working with lists, maps, and other collections
- `concurrency.loom` - Concurrent programming examples
- `web_server.loom` - Simple HTTP server (advanced example)

## Current Status

These examples are placeholders and will be implemented once the Loom compiler and standard library are ready.

## Building Examples

Once the compiler is complete, examples can be built with:

```bash
bazel build //examples:hello_world
bazel build //examples:calculator
bazel run //examples:fibonacci
```

## Running Examples

```bash
bazel run //examples:hello_world
bazel run //examples:calculator -- [args]
```
