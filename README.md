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
- Static, Compiling, Type-Safe, Functional
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
