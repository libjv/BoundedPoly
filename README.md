# BoundedPoly

`BoundedPoly<Storage, Base, Mover = ...>` is a type abstractor for C++17.
It stores any no-throw movable polymorphic derived class of `Base` that fits into size and alignment of `Storage`.
It provides semantics similar to `std::unique_ptr<Base>` and `std::variant<...>`:

- The memory is owned by the instance (like both, and unlike `std::reference_wrapper<Base>`).
- The type can change over time (like both).
- The actual type of the stored value is not known (like `unique_ptr`).
- The set of accepted types is not closed (line `unique_ptr` and unline `variant`).
- It is allocated on the stack (like `variant`).
- It cannot be empty, even when exceptions are thrown (like `variant`, but greater guarantee).

This allows to use both polymorphic-semantics and stack-allocation (and so, cache-friendlyness).

## More details

- [index.html](https://libjv.github.io/BoundedPoly/index.html) to see examples, benchmarks results and motivations for this library.
- [reference.html](https://libjv.github.io/BoundedPoly/reference.html) to see the documentation reference.

## Build

### Compilation

This project uses **CMake**, which must be installed on your machine.

1. Create a sub-directory named `build` and open a console there.
2. Make `cmake ..` to create the project files.
3. Make `cmake --build .` to compile the project.

There are now 5 executables:
- `ŧests/tests` which executes the tests.
- `examples/shape` which executes the example.
- `examples/benchmark/*` which are the three executables used for the benchmark.

### Documentation

The documentation is compiled with **Asciidoctor**, which must be installed on your machine.
Then open a console in the root directory, and run:

```
asciidoctor -B . -R docs-src -D docs 'docs-src/**/*.adoc'
```
