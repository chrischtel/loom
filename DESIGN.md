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

### 3.3. Function Declarations
`function_decl = "func" identifier [ generic_params ] "(" [ parameters ] ")" [ type ] [ constraints ] "{" statements "}" ;`
`parameters = parameter { "," parameter } ;`
`parameter = identifier ":" type ;`

### 3.4. Type Declarations
`type_decl = struct_decl | protocol_decl | enum_decl ;`

---

## 4. Core Language Features

### 4.1. Structs
Structs define structured data types with named fields.

**Syntax:**
```loom
struct Point {
    x: f64,
    y: f64,
}

struct Person {
    name: string,
    age: i32,
    active: bool,
}
```

**Grammar:**
`struct_decl = "struct" identifier [ generic_params ] "{" struct_fields "}" ;`
`struct_fields = struct_field { "," struct_field } [ "," ] ;`
`struct_field = identifier ":" type ;`

**Instantiation:**
```loom
let point = Point(x: 1.0, y: 2.0);
let person = Person(name: "Alice", age: 30, active: true);
```

### 4.2. Protocols (Interfaces)
Protocols define behavioral contracts that types can implement.

**Syntax:**
```loom
protocol Drawable:
    draw(self) void
    area(self) f64

protocol Comparable:
    compare(self, other: Self) i32
```

**Grammar:**
`protocol_decl = "protocol" identifier [ generic_params ] ":" protocol_methods ;`
`protocol_methods = protocol_method { protocol_method } ;`
`protocol_method = identifier "(" [ parameters ] ")" [ type ] ;`

### 4.3. Protocol Implementation
Types implement protocols using the `extend` syntax.

**Syntax:**
```loom
extend Point with Drawable:
    draw(self) void {
        $$print("Drawing point at (");
        $$print(self.x);
        $$print(", ");
        $$print(self.y);
        $$print(")");
    }
    
    area(self) f64 {
        return 0.0;  // Points have no area
    }

extend Person with Comparable:
    compare(self, other: Person) i32 {
        if (self.age < other.age) { return -1; }
        if (self.age > other.age) { return 1; }
        return 0;
    }
```

**Grammar:**
`impl_decl = "extend" type "with" identifier [ generic_params ] ":" impl_methods ;`
`impl_methods = impl_method { impl_method } ;`
`impl_method = function_decl ;`

### 4.4. Method Syntax
Loom uses mixed syntax for field access and method calls:

- **Field access:** Use dot notation: `object.field`
- **Method calls:** Use arrow notation: `object->method()`

```loom
let point = Point(x: 5.0, y: 3.0);
let x_coord = point.x;          // Field access
point->draw();                  // Method call
let area = point->area();       // Method call with return value
```

### 4.5. Generics
Loom supports generics using square bracket syntax.

**Generic Structs:**
```loom
struct List[T] {
    data: T[],
    length: i32,
}

struct Pair[T, U] {
    first: T,
    second: U,
}
```

**Generic Functions:**
```loom
func identity[T](value: T) T {
    return value;
}

func find[T](list: List[T], item: T) T? {
    // Implementation here
}
```

**Generic Protocols:**
```loom
protocol Container[T]:
    add(self, item: T) void
    get(self, index: i32) T?
    size(self) i32
```

### 4.6. Generic Constraints
Use `requires` clauses to constrain generic types.

```loom
func sort[T](list: List[T]) List[T] 
requires T: Comparable {
    // Can use T->compare() method
}

func max[T](a: T, b: T) T 
requires T: Comparable {
    if (a->compare(b) > 0) { return a; }
    return b;
}

protocol Numeric[T]:
    add(self, other: T) T
    
func sum[T](numbers: List[T]) T 
requires T: Numeric[T] {
    // Implementation
}
```

### 4.7. Operator Overloading
Define custom behavior for operators using the `op` keyword.

```loom
extend Point with Ops:
    op +(self, other: Point) Point {
        return Point(x: self.x + other.x, y: self.y + other.y);
    }
    
    op ==(self, other: Point) bool {
        return (self.x == other.x && self.y == other.y);
    }
    
    op -(self, other: Point) Point {
        return Point(x: self.x - other.x, y: self.y - other.y);
    }

// Usage
let p1 = Point(x: 1.0, y: 2.0);
let p2 = Point(x: 3.0, y: 4.0);
let p3 = p1 + p2;  // Point(x: 4.0, y: 6.0)
let equal = (p1 == p2);  // false
```

### 4.8. Custom Operators
Define entirely new operators for domain-specific operations.

```loom
// Define a distance operator
operator `<->` (left: Point, right: Point) f64 {
    let dx = left.x - right.x;
    let dy = left.y - right.y;
    return sqrt(dx * dx + dy * dy);
}

// Define a dot product operator for vectors
operator `•` (left: Vector, right: Vector) f64 {
    return (left.x * right.x + left.y * right.y + left.z * right.z);
}

// Usage
let p1 = Point(x: 0.0, y: 0.0);
let p2 = Point(x: 3.0, y: 4.0);
let distance = (p1 <-> p2);  // 5.0

let v1 = Vector(x: 1.0, y: 2.0, z: 3.0);
let v2 = Vector(x: 4.0, y: 5.0, z: 6.0);
let dot = (v1 • v2);  // 32.0
```

**Grammar:**
`custom_op_decl = "operator" "`" operator_symbol "`" "(" parameters ")" [ type ] "{" statements "}" ;`

---

## 5. Advanced Features

### 5.1. Nullable Types
Loom has built-in support for nullable types using the `?` suffix.

```loom
let maybe_number: i32? = nil;
let definitely_number: i32 = 42;

// Safe access with pattern matching
match (maybe_number) {
    nil -> $$print("No value");
    some(value) -> $$print(value);
}
```

### 5.2. Pattern Matching
```loom
match (value) {
    0 -> $$print("Zero");
    1..=10 -> $$print("Small number");
    n if (n > 100) -> $$print("Large number");
    _ -> $$print("Other");
}
```

### 5.3. Memory Management
Loom provides explicit memory management with ownership types.

```loom
let owned: Point* = new Point(x: 1.0, y: 2.0);  // Owned pointer
let reference: Point& = &owned;                  // Reference
let slice: Point[] = [point1, point2, point3];  // Slice
```

---

## 6. Complete Examples

### 6.1. Generic Container with Protocol
```loom
protocol Comparable:
    compare(self, other: Self) i32

struct SortedList[T] 
requires T: Comparable {
    items: T[],
}

extend SortedList[T] with Container[T]:    add(self, item: T) void {
        // Insert in sorted order
        let mut index = 0;
        while (index < self.items.length && self.items[index]->compare(item) < 0) {
            index = index + 1;
        }
        self.items.insert(index, item);
    }
    
    get(self, index: i32) T? {
        if (index >= 0 && index < self.items.length) {
            return some(self.items[index]);
        }
        return nil;
    }
    
    size(self) i32 {
        return self.items.length;
    }
```

### 6.2. Custom Operators for Domain Logic
```loom
struct Money {
    amount: f64,
    currency: string,
}

// Define money-specific operators
operator `$+` (left: Money, right: Money) Money 
requires (left.currency == right.currency) {
    return Money(amount: left.amount + right.amount, currency: left.currency);
}

operator `$>` (left: Money, right: Money) bool 
requires (left.currency == right.currency) {
    return (left.amount > right.amount);
}

// Usage
let price1 = Money(amount: 10.50, currency: "USD");
let price2 = Money(amount: 5.25, currency: "USD");
let total = price1 $+ price2;
let is_expensive = total $> Money(amount: 15.0, currency: "USD");
```

---

## 7. Concurrency Features

### 7.1. Grant-Based Permission System
Loom uses a unique `grant` keyword for managing concurrent access to resources.

```loom
struct SharedCounter {
    value: i32,
}

// Define what permissions can be granted
protocol Counter:
    increment(self) void
    get_value(self) i32

// Grant different levels of access
grant ReadAccess to SharedCounter:
    get_value(self) i32 {
        return self.value;
    }

grant WriteAccess to SharedCounter:
    increment(self) void {
        self.value = self.value + 1;
    }
    
    get_value(self) i32 {
        return self.value;
    }

// Usage in concurrent context
func worker_thread(counter: SharedCounter with WriteAccess) void {
    counter->increment();  // Allowed
}

func monitor_thread(counter: SharedCounter with ReadAccess) void {
    let value = counter->get_value();  // Allowed
    // counter->increment();  // Compile error: no write permission
}
```

### 7.2. Async/Await
```loom
async func fetch_data(url: string) string {
    let response = await http_get(url);
    return response.body;
}

func main() void {
    let future = fetch_data("https://api.example.com");
    let data = await future;
    $$print(data);
}
```

### 7.3. Channels and Message Passing
```loom
struct Channel[T] {
    // Internal implementation
}

func producer(ch: Channel[i32] with WriteAccess) void {
    for (i in 0..10) {
        ch->send(i);
    }
    ch->close();
}

func consumer(ch: Channel[i32] with ReadAccess) void {
    while (let some(value) = ch->receive()) {
        $$print(value);
    }
}
```

---

## 8. Module System

### 8.1. Module Declaration
Loom uses a unique namespace syntax with `@` symbol for module declarations.

```loom
@graphics {
    pub struct Point {
        pub x: f64,
        pub y: f64,
    }
    
    pub protocol Drawable:
        draw(self) void
}

@math {
    pub func sqrt(x: f64) f64 {
        // Implementation
    }
    
    pub func pow(base: f64, exp: f64) f64 {
        // Implementation
    }
}

@collections {
    pub struct List[T] {
        data: T[],
        length: i32,
    }
}
```

### 8.2. Imports
Use the `from` keyword with `@` syntax for imports.

```loom
// Import specific items
from @graphics take Point, Drawable;
from @math take sqrt;
from @std.collections take List;

// Import with aliasing  
from @very.long.module.name take Calculator as Calc;

// Import entire module
from @graphics take *;

// Selective import with braces
from @math take {sqrt, pow, sin, cos};
```

### 8.3. Nested Modules
```loom
@engine {
    @graphics {
        pub struct Renderer { /* ... */ }
        
        @shaders {
            pub struct VertexShader { /* ... */ }
            pub struct FragmentShader { /* ... */ }
        }
    }
    
    @audio {
        pub struct SoundBuffer { /* ... */ }
    }
}

// Import from nested modules
from @engine.graphics take Renderer;
from @engine.graphics.shaders take VertexShader;
```

### 8.4. Module Exports and Visibility
```loom
@mylib {
    // Public - visible outside module
    pub struct PublicStruct { /* ... */ }
    pub func public_function() void { /* ... */ }
    
    // Internal - only visible within this module
    struct InternalStruct { /* ... */ }
    func internal_function() void { /* ... */ }
    
    // Package visibility - visible to other modules in same package
    pkg struct PackageStruct { /* ... */ }
    pkg func package_function() void { /* ... */ }
}
```

---

## 9. Error Handling

### 9.1. Result Types
```loom
enum Result[T, E] {
    Ok(T),
    Err(E),
}

func divide(a: f64, b: f64) Result[f64, string] {
    if (b == 0.0) {
        return Result::Err("Division by zero");
    }
    return Result::Ok(a / b);
}

// Usage with pattern matching
match (divide(10.0, 2.0)) {
    Ok(result) -> $$print(result);
    Err(error) -> $$print("Error: " + error);
}
```

### 9.2. Error Propagation
```loom
func complex_calculation() Result[f64, string] {
    let a = divide(10.0, 2.0)?;  // Auto-propagate error
    let b = divide(a, 3.0)?;
    return Ok(b * 2.0);
}
```

---

## 10. Compile-Time Features

### 10.1. Compile-Time Evaluation
```loom
comptime {
    let build_info = get_build_timestamp();
    $$print("Compiled at: " + build_info);
}

const PI: f64 = comptime 3.14159265359;

func factorial(n: i32) -> i32 comptime {
    if n <= 1 { return 1; }
    return n * factorial(n - 1);
}

const FACT_10: i32 = comptime factorial(10);  // Computed at compile time
```

### 10.2. Macros and Code Generation
```loom
macro generate_getter(field_name: identifier, field_type: type) {
    func get_$field_name(self) -> $field_type {
        return self.$field_name;
    }
}

struct Person {
    name: string,
    age: i32,
}

extend Person with Accessors:
    generate_getter!(name, string);
    generate_getter!(age, i32);
```

---

## 11. Grammar Summary

### 11.1. Complete EBNF Grammar
```ebnf
(* Top Level *)
module = { declaration } ;
declaration = function_decl | struct_decl | protocol_decl | impl_decl | custom_op_decl | mod_decl | use_decl ;

(* Functions *)
function_decl = [ "pub" ] "func" identifier [ generic_params ] "(" [ parameters ] ")" [ type ] [ constraints ] "{" statements "}" ;
generic_params = "[" identifier { "," identifier } "]" ;
parameters = parameter { "," parameter } ;
parameter = identifier ":" type ;
constraints = "requires" constraint { "," constraint } ;
constraint = identifier ":" identifier [ generic_params ] ;

(* Types *)
struct_decl = [ "pub" ] "struct" identifier [ generic_params ] [ constraints ] "{" struct_fields "}" ;
struct_fields = struct_field { "," struct_field } [ "," ] ;
struct_field = [ "pub" ] identifier ":" type ;

protocol_decl = [ "pub" ] "protocol" identifier [ generic_params ] ":" protocol_methods ;
protocol_methods = protocol_method { protocol_method } ;
protocol_method = identifier "(" [ parameters ] ")" [ "->" type ] ;

impl_decl = "extend" type "with" identifier [ generic_params ] ":" impl_methods ;
impl_methods = impl_method { impl_method } ;
impl_method = function_decl | op_overload ;

op_overload = "op" operator "(" parameters ")" [ type ] "{" statements "}" ;
custom_op_decl = "operator" "`" operator_symbol "`" "(" parameters ")" [ type ] [ constraints ] "{" statements "}" ;

(* Types *)
type = basic_type | generic_type | nullable_type | reference_type | slice_type ;
basic_type = "i8" | "i16" | "i32" | "i64" | "f32" | "f64" | "bool" | "string" | identifier ;
generic_type = identifier "[" type { "," type } "]" ;
nullable_type = type "?" ;
reference_type = type "&" | type "*" ;
slice_type = type "[]" ;

(* Expressions *)
expression = assignment | binary_expr | unary_expr | call_expr | field_access | method_call | literal | identifier ;
field_access = expression "." identifier ;
method_call = expression "->" identifier "(" [ arguments ] ")" ;
call_expr = identifier "(" [ arguments ] ")" | identifier "[" type { "," type } "]" "(" [ arguments ] ")" ;

(* Statements *)
statements = statement { statement } ;
statement = var_decl | expr_stmt | if_stmt | while_stmt | for_stmt | return_stmt | defer_stmt ;
```

---

## 12. Language Philosophy and Design Decisions

### 12.1. Why These Syntax Choices?

1. **Mixed Method Syntax (`object.field` vs `object->method()`)**: 
   - Clearly distinguishes between data access and behavior invocation
   - Makes code more readable and intention clearer
   - Prevents confusion about what operations might have side effects

2. **Square Bracket Generics (`List[T]`)**:
   - Avoids parsing ambiguities with comparison operators
   - Visually distinct from function calls
   - More readable in complex nested generics

3. **Protocol Keyword**:
   - More descriptive than "interface" or "trait"
   - Emphasizes the communication aspect of contracts
   - Unique to Loom, reinforcing language identity

4. **Grant-Based Concurrency**:
   - Explicit permission management for shared resources
   - Compile-time verification of access patterns
   - Scales from simple threading to complex distributed systems

5. **Custom Operators**:
   - Domain-specific expressiveness
   - Mathematical and scientific computing support
   - Controlled extension of language syntax

### 12.2. Design Principles Applied

- **Structured Freedom**: Safe defaults with explicit escape hatches
- **Ergonomic Safety**: Safety features feel natural, not restrictive  
- **Pragmatic Performance**: Zero-cost abstractions and compile-time optimizations
- **Clarity Over Dogma**: Syntax serves developer understanding
- **Expressive Tooling**: Language designed for great developer experience

---

## 13. Advanced Keywords and Specialty Features

### 13.1. Phantom Types (`phantom`)
Phantom types enable compile-time resource tracking and state management without runtime overhead.

```loom
// Define phantom type parameters for compile-time tracking
struct File[State] phantom State {
    handle: i32,
}

// Phantom states - exist only at compile time
phantom struct Opened;
phantom struct Closed;

// Type-safe file operations
func open_file(path: string) File[Closed] {
    let handle = system_open(path);
    return File[Closed] { handle: handle };
}

func read_file(file: File[Closed]) (string, File[Closed]) {
    let data = system_read(file.handle);
    return (data, file);  // File remains closed
}

func write_file(file: File[Opened], data: string) File[Opened] {
    system_write(file.handle, data);
    return file;  // File remains open
}

// State transitions
func open(file: File[Closed]) File[Opened] {
    system_activate(file.handle);
    return File[Opened] { handle: file.handle };
}

func close(file: File[Opened]) File[Closed] {
    system_deactivate(file.handle);
    return File[Closed] { handle: file.handle };
}

// Usage - compile-time verified state tracking
func example() void {
    let file = open_file("data.txt");        // File[Closed]
    let (data, file) = read_file(file);      // Still File[Closed]
    let file = open(file);                   // Now File[Opened]
    let file = write_file(file, "hello");    // Still File[Opened]
    let file = close(file);                  // Back to File[Closed]
    
    // write_file(file, "error");            // Compile error: file is closed!
}
```

### 13.2. Context Management (`context`)
Context provides implicit parameter passing and resource management.

```loom
// Define context types
context Logger {
    log_level: LogLevel,
    output: string,
}

context Database {
    connection: DbConnection,
    transaction: Transaction?,
}

// Functions can require context
func log_message(message: string) 
context logger: Logger {
    if logger.log_level >= LogLevel::Info {
        write_to(logger.output, message);
    }
}

func save_user(user: User) 
context db: Database {
    let query = "INSERT INTO users...";
    db.connection->execute(query, user);
}

// Context usage
func main() void {
    with context Logger { log_level: LogLevel::Debug, output: "app.log" } {
        with context Database { connection: connect_db(), transaction: nil } {
            log_message("Starting application");
            let user = User { name: "Alice", age: 30 };
            save_user(user);
            log_message("User saved");
        }
    }
}
```

### 13.3. Deferred Execution (`defer`)
Defer ensures cleanup code runs when leaving scope.

```loom
func process_file(path: string) -> Result[string, string] {
    let file = open_file(path)?;
    defer close_file(file);  // Always runs when leaving scope
    
    let mut data = read_file(file)?;
    defer free_memory(data);  // Cleanup memory
    
    // Complex processing that might fail
    if data.length > MAX_SIZE {
        return Err("File too large");  // defer blocks still run
    }
    
    return Ok(process_data(data));
}  // defer blocks execute in reverse order: free_memory, close_file
```

### 13.4. Stream Processing (`stream`)
Streams provide lazy evaluation and infinite sequence processing.

```loom
// Define stream types
stream[T] = iterator that yields T;

// Stream generators
func fibonacci() stream[i64] {
    let mut a = 0;
    let mut b = 1;
    
    stream {
        yield a;
        let temp = a + b;
        a = b;
        b = temp;
    }
}

func range(start: i32, end: i32) stream[i32] {
    let mut current = start;
    stream {
        while (current < end) {
            yield current;
            current = current + 1;
        }
    }
}

// Stream operations
func main() void {
    fibonacci()
        |> take(10)
        |> filter(|x| x % 2 == 0)
        |> map(|x| x * x)
        |> collect()
        |> print();
}
```

### 13.5. Pipeline Processing (`pipeline`)
Pipelines provide structured data flow with backpressure handling.

```loom
pipeline DataProcessor[T, U] {
    input: Channel[T],
    output: Channel[U],
    
    stages: [
        stage parse: T -> ParsedData,
        stage validate: ParsedData -> ValidData,
        stage transform: ValidData -> U,
    ]
}

func create_processor[T, U]() -> DataProcessor[T, U] {
    pipeline {
        buffer_size: 1000,
        parallelism: 4,
        
        parse: |data| parse_input(data),
        validate: |parsed| {
            if is_valid(parsed) { 
                return Ok(parsed); 
            } else { 
                return Err("Invalid data"); 
            }
        },
        transform: |valid| apply_transform(valid),
    }
}
```

### 13.6. Yield and Generators (`yield`)
Yield creates generator functions that can pause and resume execution.

```loom
func walk_tree[T](node: TreeNode[T]) -> stream[T] {
    yield node.value;
    
    for child in node.children {
        yield from walk_tree(child);  // Yield all values from subtree
    }
}

func fibonacci_generator() -> stream[i64] {
    let mut a = 0;
    let mut b = 1;
    
    loop {
        yield a;
        let next = a + b;
        a = b;
        b = next;
    }
}
```

### 13.7. Resource Revocation (`revoke`)
Revoke provides explicit resource cleanup and permission removal.

```loom
func secure_operation() -> void {
    let permissions = grant_admin_access();
    defer revoke permissions;  // Automatically revoke on scope exit
    
    // Perform privileged operations
    admin_task_1();
    admin_task_2();
    
    if emergency_condition() {
        revoke permissions;  // Explicit early revocation
        return;
    }
    
    admin_task_3();
}  // permissions automatically revoked here if not already done

// Revoke can also work with protocols
func with_database_access[T](operation: func() -> T) -> T {
    let db_access = grant_database_access();
    defer revoke db_access;
    
    return operation();
}
```

### 13.8. Synchronization (`sync`)
Sync provides structured concurrency and synchronization primitives.

```loom
func parallel_work() -> void {
    sync {
        // All async operations in this block are awaited before continuing
        async task1();
        async task2();
        async task3();
    }  // Implicitly awaits all tasks
    
    $$print("All tasks completed");
}

func producer_consumer() -> void {
    let channel = Channel[i32]::new();
    
    sync {
        async producer(channel.clone());
        async consumer(channel.clone());
        async monitor(channel.clone());
    }  // All tasks complete before function returns
}
```

### 13.9. Conditional Compilation (`when`)
When provides compile-time conditional compilation.

```loom
func platform_specific_code() -> void {
    when TARGET_OS == "windows" {
        windows_implementation();
    } elif TARGET_OS == "linux" {
        linux_implementation();
    } else {
        generic_implementation();
    }
}

when DEBUG {
    func debug_print(message: string) -> void {
        $$print("[DEBUG] " + message);
    }
} else {
    func debug_print(message: string) -> void {
        // No-op in release builds
    }
}

struct Config {
    name: string,
    when FEATURE_LOGGING {
        log_level: LogLevel,
    }
    when FEATURE_METRICS {
        metrics_endpoint: string,
    }
}
```

### 13.10. Using Declarations (`using`)
Using provides temporary scope-based imports and context injection.

```loom
func mathematical_calculation() -> f64 {
    using math::{sin, cos, pi, sqrt};  // Temporary imports
    using context precision: f64 = 1e-10;  // Temporary context
    
    let result = sin(pi / 4.0) + cos(pi / 3.0);
    return sqrt(result);
}  // imports and context expire here

// Using can also bring protocol methods into scope
func geometric_operations(shapes: List[Shape]) -> void {
    using Drawable, Measurable;  // Bring protocol methods into scope
    
    for shape in shapes {
        draw();  // Instead of shape->draw()
        let area = area();  // Instead of shape->area()
    }
}
```

---

## 14. Compile-Time Resource Tracking Examples

### 14.1. Memory Safety with Phantom Types
```loom
phantom struct Allocated;
phantom struct Freed;

struct Memory[State] phantom State {
    ptr: *void,
    size: usize,
}

func allocate(size: usize) -> Memory[Allocated] {
    let ptr = system_malloc(size);
    return Memory[Allocated] { ptr: ptr, size: size };
}

func free(memory: Memory[Allocated]) -> Memory[Freed] {
    system_free(memory.ptr);
    return Memory[Freed] { ptr: nil, size: 0 };
}

func use_memory[T](memory: Memory[Allocated], data: T) -> Memory[Allocated] {
    system_write(memory.ptr, data);
    return memory;
}

// Compile-time verified memory management
func safe_memory_usage() -> void {
    let mem = allocate(1024);           // Memory[Allocated]
    let mem = use_memory(mem, "data");  // Still Memory[Allocated]
    let mem = free(mem);                // Now Memory[Freed]
    
    // use_memory(mem, "error");        // Compile error: memory is freed!
}
```

### 14.2. Protocol State Tracking
```loom
phantom struct Connected;
phantom struct Disconnected;
phantom struct Authenticated;

struct NetworkConnection[State] phantom State {
    socket: Socket,
}

protocol Connectable[State] phantom State:    connect(self) NetworkConnection[Connected]

protocol Authenticatable[State] phantom State:
    authenticate(self, credentials: Credentials) NetworkConnection[Authenticated]

extend NetworkConnection[Disconnected] with Connectable[Disconnected]:
    connect(self) -> NetworkConnection[Connected] {
        self.socket->connect();
        return NetworkConnection[Connected] { socket: self.socket };
    }

extend NetworkConnection[Connected] with Authenticatable[Connected]:
    authenticate(self, creds: Credentials) -> NetworkConnection[Authenticated] {
        self.socket->send_auth(creds);
        return NetworkConnection[Authenticated] { socket: self.socket };
    }

// Only authenticated connections can send secure data
func send_secure_data(conn: NetworkConnection[Authenticated], data: SecureData) -> void {
    conn.socket->send_encrypted(data);
}
```

This completes the explanation of all the advanced keywords! Each provides powerful compile-time guarantees and unique capabilities that make Loom both safe and expressive.
