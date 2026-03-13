`std::filesystem` inspired API for C89 (c-fs)
=============================================

[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![CI](https://github.com/SamuelMarks/c-orm/actions/workflows/ci.yml/badge.svg)](https://github.com/SamuelMarks/c-orm/actions/workflows/ci.yml)
[![Doc Coverage](https://img.shields.io/badge/docs-100%25-brightgreen.svg)](#)
[![Test Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen.svg)](#)
![C Standard](https://img.shields.io/badge/C-89-blue.svg)

A highly robust, strictly ISO C90 (C89) compliant port of the C++17 `std::filesystem` library. `c-fs` is designed for embedding directly into legacy systems, deep embedded environments, and modern workflows needing high-performance, cross-platform path manipulation and OS stream controls. 

Beyond standard filesystem operations, `c-fs` introduces asynchronous scheduling, thread pooling, and execution modality abstractions, enabling sophisticated concurrent I/O on any compiler from MSVC 2005 & MSVC 2026 to modern Clang/GCC.

## Core Features

- **Zero External Dependencies:** Depends solely on the host OS native API (`GetFileAttributesW`, `stat`, `dirent`, etc.) and the C standard library.
- **Strict C89 Compliance:** Compiles gracefully on legacy compilers without requiring C99/C11 features. Tested rigorously on MSVC 2005.
- **Dynamic Strings & Safe CRT:** Completely safe dynamic memory management strictly wrapping OS path limits. Avoids fixed `MAX_PATH` buffers. On MSVC, automatically leverages Safe CRT functions (e.g., `wcscpy_s`) while elegantly falling back to standard string manipulation on GCC/Clang.
- **Single-Header Native:** Distribute strictly as `include/cfs/cfs.h` for easy, drop-in integration.
- **C++17 Behavioral Mapping:** Functions meticulously map to the expected behavioral quirks and boundary cases of C++'s `<filesystem>`.
- **Wide Character Support:** Native toggleable integration seamlessly utilizing wide character API layers strictly on Windows, bypassing legacy ANSI codepage conversion loss.
- **Asynchronous Modalities:** First-class support for deferred execution via configurable runtimes (Thread pools, Green threads, Multiprocessing, Message Passing).

## Supported Platforms

The library relies heavily on platform detection macros to switch between backend implementations natively.

*   **Compilers:** GCC, Clang, MSVC (2005 through 2026), MinGW.
*   **Operating Systems:** DOS, Windows, macOS, Linux, FreeBSD/OpenBSD.
*   **Environments:** Native, Cygwin, Alpine (Musl), Debian (Glibc).

## Integration

`c-fs` follows the single-header `stb` library philosophy. Include `cfs.h` in your project. In exactly *one* C file, define `CFS_IMPLEMENTATION` before including it to construct the compilation object.

```c
/* In exactly ONE source file: */
#define CFS_IMPLEMENTATION
#include "cfs/cfs.h"
```

In all other files, just include the header:
```c
#include "cfs/cfs.h"
```

## CMake Support

Easily integrate via FetchContent or `add_subdirectory`:
```cmake
add_subdirectory(c-fs)
target_link_libraries(my_app PRIVATE cfs)
```

Options:
- `CFS_BUILD_SHARED`: (OFF) Enable `.dll` / `.so` generation.
- `CFS_UNICODE`: (ON) Enable native `wchar_t` bridging on Windows targets.
- `CFS_HEADER_ONLY`: (OFF) Expose strictly as an interface wrapper for parent projects.

## Project Documentation

*   **[ARCHITECTURE.md](./ARCHITECTURE.md):** Deep dive into the structural layout, macro systems, execution modalities, and legacy compatibility choices.
*   **[USAGE.md](./USAGE.md):** Comprehensive API examples ranging from basic path manipulation to asynchronous runtime thread-pooling.
*   **[skills.md](./skills.md):** A guide to the required domains of knowledge to contribute effectively to this repository.
*   **[llm.txt](./llm.txt):** Core prompt directives for LLM agents contributing to this codebase.

---

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or <https://www.apache.org/licenses/LICENSE-2.0>)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or <https://opensource.org/licenses/MIT>)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
