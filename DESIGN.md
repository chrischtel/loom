# The Loom Programming Language Specification

**Version:** 0.4.0 (In Development)

**Status:** DRAFT

**Authors:** Christian Brendlin and The Loom Programming Language Contributors

This document is the official specification for the Loom programming language. It is the authoritative source for Loom's syntax and semantics.

## Table of Contents
<!-- You can add a ToC here as the document grows -->
N/A

## 1. Introduction

### 1.1. Philosophy and Goals
The core philosophy is **Structured Freedom**: make the safe, high-performance path the most ergonomic one, while providing clear, explicit escape hatches for when you need to talk directly to the metal.

### 1.2. Guiding Principles
*   **Ergonomic Safety:** Safety features should feel natural, not restrictive.
*   **Pragmatic Performance:** Zero-cost abstractions are the default.
*   **Clarity Over Dogma:** Rules should serve the developer, not the other way around.
*   **Expressive Tooling:** The language is designed to enable a great developer experience.

### 1.3. Notation
The syntax of Loom is defined using Extended Backus-Naur Form (EBNF).
*   `"terminal"`: A literal terminal symbol.
*   `non_terminal`: A production rule.
*   `[ ... ]`: Optional item (0 or 1 times).
*   `{ ... }`: Repetition (0 or more times).
*   `( ... )`: Grouping.
*   `|`: Alternation.
*   `(* ... *)`: A comment describing the rule.

---

## 2. Lexical Structure

### 2.1. Character Set
Loom source files are encoded in **UTF-8**.

### 2.2. Comments
*   Single-line comments start with `//` and continue to the end of the line.
*   Multi-line comments start with `/*` and end with `*/`. They can be nested.
*   Documentation comments start with `///` or `//!`.

### 2.3. Keywords
*   **Declarations:** `func`, `const`, `let`, `mut`, `mod`, `use`, `pub`, `type`, `struct`, `enum`, `interface`, `impl`
*   **Control Flow:** `if`, `elif`, `else`, `while`, `for`, `in`, `match`, `when`, `break`, `continue`, `return`, `yield`
*   **Specialty** `defer`, `comptime`, `stream`, `phantom`, `context`, `using`, `with`, `grant`, `revoke`, `pipeline`, `async`, `await`, `sync`
*   **Literals & Built-ins:** `true`, `false`, `nil`, `void`

### 2.4. Operators and Punctuation
*(List all operators and symbols)*
*   **Operators:** `+`, `-`, `*`, `/`, `%`, `**`, `==`, `!=`, `|>` ...
*   **Punctuation:** `(`, `)`, `{`, `}`, `[`, `]`, `,`, `:`, `.` ...

### 2.5. Identifiers
An identifier starts with a letter or underscore, followed by any number of letters, numbers, or underscores.
`identifier = (letter | "_") { letter | digit | "_" } ;`

### 2.6. Literals
*   **Integers:** Decimal (`123`), Hex (`0xFF`), Binary (`0b1011`), Octal (`0o755`). Underscores (`1_000_000`) are allowed as separators.
*   **Floating-Point:** `3.14`, `1.0e-5`.
*   **Strings:** Double-quoted (`"hello"`). Escape sequences: `\n`, `\r`, `\t`, `\\`, `\"`, `\u{...}`.
*   **Booleans:** `true`, `false`.
*   **Special:** `nil`, `void`.

### 2.7. Statement Separation
Semicolons (`;`) are used to separate statements.
---

## 3. Syntactic Structure (Grammar)

### 3.1. Top-Level Declarations
`module = { declaration } ;`
`declaration = function_decl | type_decl | const_decl | ... ;`

### 3.2. Variable Declarations
`variable_decl = ( "let" | "mut" ) identifier [ ":" type ] [ "=" expression ] ;`

