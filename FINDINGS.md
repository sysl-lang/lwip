# What vendoring lwIP found in sysl

Found against the prerelease of sysl's `vendored-c` branch (3ffffa28), meant for alpha.14. Nothing below
is worked around in the vendored C, which is upstream 2.2.1 byte for byte.

## The vendored-C manifest block (feedback on the prerelease)

1. **`build-lib` ignores the `c` block (bug).** It passes no `-I` for the shim (`'lwip/opt.h' file not
   found`) and compiles none of the library's sources. `build-c` and an executable build use the block;
   `build-lib` collects `c_sources` the old way (std_artifact.sysl's `build_members`).
2. **`include` and `export` repeat each other.** For lwIP the two lists are identical. Wanted: `export =
   true` to export the include roots.
3. **Paths climb out of `root`.** The package's own config directory is `"../../../config"`, because
   include paths are relative to `root`. Wanted: a form relative to the package root.
4. **A C library's headers cannot come from a consumer.** cstd's headers reach only packages that depend
   on cstd. Without target-only dependencies (branch `target-deps`) lwIP cannot depend on it, and
   depending on it unconditionally links cstd's `memcpy` into a hosted binary.
5. **`c_include` replaces a header whole.** A consumer's `lwipopts.h` shadows the package's entirely. The
   package's default uses `#ifndef` defaults and `#include_next`, so a consumer's file may set only what
   it changes. Worth documenting as the pattern.
6. **One failed probe cascades.** After a `c const` probe fails, every declaration in the file reads as
   undefined, hiding the first error.
7. **A consumer's build parses its dependency's `@tests` files.**
8. **Cosmetic:** a diagnostic reads `c headers: … takes  from 0 package(s)`.

## The language

9. **A method of a type declared in a `@tests` file may not call that file's helpers (bug).** It is
   refused with "only another such file, or a '@test' function, may name it", which contradicts
   attributes.md. The test driver holds a `*Wire` and a struct for the stream pattern instead.
   Reproduced on alpha.12 and the prerelease.
10. **A test at the project root that reads a module `var` silently vanishes (bug).** The run says "1
    passed" with exit 0; the same test under a module directory runs and fails as it should. The
    override test keeps its clock in a local meanwhile. Reproduced on alpha.12 and the prerelease: the
    trigger is a `var` declared in the root `@tests` file itself, and the run reports "1 passed" of two
    declared tests.
11. **A `const` constructor cannot store `null` (gap).** `const holder(n) -> Holder = Holder(null, n)` is
    refused ("an address exists only when the program runs"), so `Interface` has no constant-data
    constructor and a board cannot keep one in module storage. The board example keeps its state in a
    loop frame. Possibly covered by sysl's `addr-fold` branch.

**Status: all accepted for alpha.14.**
- vendored-c round 2 covers 1, 2, 3, 5 (documented), 6 and 8. **Built and adopted** against edb75ec4:
  1 (`build-lib` host passes), 2 (`export = true`), 3 (`"/config"`), 5 (the pattern is in packages.md).
  Still rough: 6 (a refused probe now quotes clang's one error, but the sysl errors that follow, "an
  array length must be a constant" and "unknown type 'c.Netif'", still print, 25 in all, and the span
  points at the block's first entry `ERR_OK`, not the broken line); 8 not seen. `build-lib --target
  thumbv7em-freestanding` stops on `'inttypes.h' file not found`, the expected libc gap (4).
- `tests-files` covers 7, 9 and 10.
- `addr-fold` covers 11.
- `lib-override` covers nested path dependencies.
- `target-deps` (`if = "!libc"`) covers 4.

The test reshapes around 9 and 10 come out when alpha.14 ships.
