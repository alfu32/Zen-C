# Zen C Language Guide (for Agents)

Zen C is a modern systems language that transpiles to human‑readable C11/GNU C. It keeps C‑level control and ABI compatibility but adds high‑level features like type inference, traits, generics, and pattern matching.

Similar languages: C (syntax/ABI), Rust (traits, enums, pattern matching, RAII ideas), Zig (systems focus, minimal runtime), and a touch of Go (simple tooling/entrypoint style).

## Quick start
- File extension: `.zc`
- Run: `zc run app.zc`
- Build: `zc build app.zc -o app`
- REPL: `zc repl`
- Standard library location: set `ZC_ROOT=/path/to/Zen-C`

## Core syntax
```zc
var x = 42;
const PI = 3.14159;
var explicit: float = 1.0;

fn add(a: int, b: int) -> int { return a + b; }
```

Mutability is enabled by default; optional directive:
```zc
//> immutable-by-default
var mut y = 10;
```

## Types and data
```zc
struct Point { x: int; y: int }
enum Shape { Circle(float), Rect(float, float), Point }
```

Struct init and methods:
```zc
var p = Point { x: 10, y: 20 };
impl Point { fn dist(self) -> float { ... } }
```

Traits and impl:
```zc
trait Drawable { fn draw(self); }
impl Drawable for Circle { fn draw(self) { ... } }
```

## Control flow
```zc
if x > 10 { ... } else { ... }
for i in 0..10 { ... }
match val { 1 => ..., 2 | 3 => ..., _ => ... }
```

## Strings and printing
```zc
println "Value: {x}";
"Hello";     // shorthand println
"Hello"..;   // no newline
```

## Memory management
- Manual allocation via `malloc`/`free`
- `defer` for scoped cleanup
- `autofree var x = malloc(...)`
- Implement `Drop` for RAII‑style cleanup

## Generics and lambdas
```zc
struct Box<T> { item: T }
fn identity<T>(val: T) -> T { return val; }
var double = x -> x * 2;
```

## Interop and build directives
- C headers: `include <stdio.h>` or `import "foo.h"`
- Build directives at file top:
```zc
//> link: -lm
//> include: ./include
//> cflags: -O3
```

Use this guide to write idiomatic Zen C while keeping C‑level performance and interoperability.
