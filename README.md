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

## Learn by Examples
```
// Declare a struct
struct User {
  i0: int32
  i1: int32
  i2: int32
}

// Declare a persistent block
// All variables declared here are like C static/global variables,
// but they live in NVM
persistent {
  // built-in type
  p0: int32
  // struct type
  p1: User
  p2: User
  // pointer type
  pp4: *int32
  pp5: *User
}

// External functions and variables
extern {
  fn putsInt(val: int32);
  fn plus(i1: int32, i2: int32) int32;
}


// Function definition
fn AL__main() {

  // assignment operator, member access operator, plus operator
  p1.i0 = p1.i0 + 1;
  p1.i2 = p1.i2 + 1;

  // copy assignment operator
  p2 = p1;

  // function call
  putsInt(p2.i0);
  putsInt(p2.i1);
  putsInt(p2.i2);
  putsInt(plus(p2.i0, p2.i2));

  // address-of operator
  pp4 = &p1.i0;
  // dereference operator
  putsInt(*pp4);

  pp5 = &p2;
  putsInt((*pp5).i0);
}
```

## Concurrency
```
pm: pmutx;

// using persistent channel + move semantics
fn producer0(ch: pchannel<User>) {
  loop {
    ch <- new_persistent_user();
  }
}

fn producer1(ch: pchannel<User>) {
  loop {
    ch <- new_persistent_user();
  }
}

fn consumer0(ch0: pchannel<User>, ch1: pchannel<User>) {
  loop {
    select {
      u := <- ch0: {
        puts(u.id);
      }
      u := <- ch1: {
        puts(u.id);
      }
    }
  }
}

fn main() {
  persistent {
    tx0, tx1: pchannel<User>::tx;
    rx0, rx1: pchannel<User>::rx;
  }
  tx0, rx0 := pchannel<User>("ch0");
  tx1, rx1 := pchannel<User>("ch1");

  thread::spawn(producer0, tx0);
  thread::spawn(producer1, tx1);
  thread::spawn(consumer0, rx0, rx1);
}
```

## Nested Transaction
```
fn main() {
  x = 1;
  y = 1;
  transaction(x, y) "tx0" {
    x += 1;
    transaction(y) "tx1" {
      y += 1;
    }
    y += 1;
  }
  puts(x);
  puts(y);
}
```

## TODOs

- Annotation
