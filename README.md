# adonis
adonis-lang is an programming language.


## Dependencies

- cmake 3.4.3+
- llvm 5.0
- Google RE2

## Build 

- mkdir build
- cd build
- cmake ..
- make -j8

## Design Goals
- Zero-cost abstraction
- Static typed, type-safety
- Compile and JIT
- Functional
  - Function object/lambda
  - Pattern matching
  - Inner iterator
  - Immutable heap object?
- Threading/memory model
  - Rust-like, object ownership
  - Erlang-like, actor pattern
  - Clojure-like, pure functional, immutable
  - Go-like, go function/channel/select, message queue model
  - SQL-like, transaction model
- Native support for fail-safe operations on NVRAM
  - Prevent the following errors
    1. inconsistent persistent memory
    1. leak persistent memory

## Persistent memory
- By memory type
  - Persistent static memory
  - Persistent heap
  - Persistent stack?

- By semantics
  - Persistent object(an object whose memory address is in persistent mem,
   will not leak)
    - Built-in type
      - Wrap all operations on it
    - Structure
  - Persistent pointer(a pointer points to a persistent object)
    - A volatile pointer(a pointer points to a volatile object) can
    only live in volatile memory
  - Persistent auto-pointer/gc
    - Reference counting in NVM pointers
    - Root pointers must be in static persistent memory
    - Garbage collection when recovery or out of NVRAM

## Demo
```
// Same as C struct
struct User {
  i0: int32;
  i1: int32;
  i2: int32;
}

// Static persistent memory(similar to C static variable/global variable but persistent)
persistent {
  p0: int32;
  p1: User;
  p2: User;
}

extern {
  fn plus(a: int32, b: int32) int32;
}

// Entrypoint
fn main() {
  // It is trivial that this is atomic on amd64
  p1.i0 = p1.i0 + 1;
  p1.i2 = p1.i2 + 1;

  // Deep copy struct
  // NOTE: "User" struct is at least 12 bytes, > 8 bytes
  // So it may not be atomic without adding tricks
  // Here, the trick is to automatically call nvm_persist to finish writing data
  // on NVM
  // It is a all-success or all-failure operation
  p2 = p1;

  // Adonis makes sure that p2.i0 == p2.i2
  putsInt(p2.i0);
  putsInt(p2.i1);
  putsInt(p2.i2);
}

```
