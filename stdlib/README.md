# Loom Standard Library

This directory will contain the standard library for the Loom programming language, written in Loom itself once the language is self-hosting.

## Planned Structure

- `core/` - Core language functionality (strings, arrays, basic I/O)
- `collections/` - Data structures (lists, maps, sets)
- `io/` - Input/output operations
- `math/` - Mathematical functions
- `system/` - System interaction utilities
- `concurrent/` - Concurrency primitives
- `network/` - Network programming utilities

## Current Status

This directory is currently a placeholder. The standard library will be implemented in Loom once the compiler reaches sufficient maturity.

## Future Build Process

The standard library will be built using custom Bazel rules for Loom:

```bash
bazel build //stdlib:core
bazel build //stdlib:collections
```
