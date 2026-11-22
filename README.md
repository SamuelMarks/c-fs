`std::filesystem` inspired API for C89 (c-fs)
=============================================

[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Doc Coverage](https://img.shields.io/badge/docs-100%25-brightgreen.svg)](#)
[![Test Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen.svg)](#)
[![CI](https://github.com/SamuelMarks/c-fs/actions/workflows/ci.yml/badge.svg)](https://github.com/SamuelMarks/c-fs/actions/workflows/ci.yml)
![C Standard](https://img.shields.io/badge/C-89-blue.svg)

A highly robust, strictly ISO C90 (C89) compliant port of the C++17 `std::filesystem` library. `c-fs` is designed for embedding directly into legacy systems, deep embedded environments, and modern workflows needing high-performance, cross-platform path manipulation and OS stream controls.

Beyond standard filesystem operations, `c-fs` introduces asynchronous scheduling, thread pooling, and execution modality abstractions, enabling sophisticated concurrent I/O on any compiler from MSVC 2005 & MSVC 2026 to modern Clang/GCC.

## Core Features

- **Zero External Dependencies:** Depends solely on the host OS native API (`_wstat64`, `CreateFileW`, `FindFirstFileW`, `stat`, `dirent`, etc.) and the C standard library.
- **Strict C89 Compliance:** Compiles gracefully on legacy compilers without requiring C99/C11 features. Tested rigorously on MSVC 2005.
- **Dynamic Strings & Safe CRT:** Completely safe dynamic memory management strictly wrapping OS path limits. Avoids fixed `MAX_PATH` buffers. On MSVC, automatically leverages Safe CRT functions (e.g., `wcscpy_s`) while elegantly falling back to standard string manipulation on GCC/Clang.
- **Header-Only Capable:** Drop in the `include/cfs/` header directory (`cfs.h`, `log.h`, `no_discard.h`) for header-only integration via `CFS_IMPLEMENTATION`, or build as a static/shared library.
- **C++17 Behavioral Mapping:** Functions meticulously map to the expected behavioral quirks and boundary cases of C++'s `<filesystem>`, returning `CFS_NO_DISCARD`-annotated `cfs_errc` status codes.
- **Wide Character Support:** Native toggleable integration seamlessly utilizing wide character API layers strictly on Windows, bypassing legacy ANSI codepage conversion loss.
- **Flexible Build Options:** Choose between a static library, a shared library (`.dll`/`.so`/`.dylib`), or a header-only `INTERFACE` target to suit your project's architecture.
- **Asynchronous & Multithreaded Modalities:** Built-in deferred execution via configurable runtimes (synchronous inline and multi-threaded worker pools), with foundational API scaffolding for green threads, multiprocessing, and message passing.

## `std::filesystem` Compliance Table

| `std::filesystem` Feature | `c-fs` Equivalent | Status | Notes |
|---------------------------|-------------------|--------|-------|
| **Classes & Types**       |                   |        |       |
| [`path`](https://en.cppreference.com/w/cpp/filesystem/path) | `cfs_path`        | ✅   | Comprehensive path parsing & manipulation |
| [`filesystem_error`](https://en.cppreference.com/w/cpp/filesystem/filesystem_error) | `cfs_error_code`  | ✅   | Raw OS code + full OS-to-POSIX `cfs_errc` translation & `cfs_error_message` |
| [`directory_entry`](https://en.cppreference.com/w/cpp/filesystem/directory_entry) | `cfs_directory_entry` | ✅ | |
| [`directory_iterator`](https://en.cppreference.com/w/cpp/filesystem/directory_iterator) | `cfs_directory_iterator` | ✅ | Includes async init (`cfs_dir_itr_init_async`) |
| [`recursive_directory_iterator`](https://en.cppreference.com/w/cpp/filesystem/recursive_directory_iterator) | `cfs_recursive_directory_iterator` | ✅ | Depth-tracking stack traversal (`depth`, `pop`, `disable_recursion_pending`) |
| [`file_status`](https://en.cppreference.com/w/cpp/filesystem/file_status) | `cfs_file_status` | ✅   | |
| [`space_info`](https://en.cppreference.com/w/cpp/filesystem/space_info) | `cfs_space_info`  | ✅   | |
| [`file_type`](https://en.cppreference.com/w/cpp/filesystem/file_type) | `cfs_file_type`   | ✅   | |
| [`perms`](https://en.cppreference.com/w/cpp/filesystem/perms) | `cfs_perms`       | ✅   | |
| [`perm_options`](https://en.cppreference.com/w/cpp/filesystem/perm_options) | `cfs_perm_options` | ✅   | |
| [`copy_options`](https://en.cppreference.com/w/cpp/filesystem/copy_options) | `cfs_copy_options`| ✅   | |
| [`directory_options`](https://en.cppreference.com/w/cpp/filesystem/directory_options) | `cfs_directory_options` | ✅   | |
| [`file_time_type`](https://en.cppreference.com/w/cpp/filesystem/file_time_type) | `cfs_file_time_type` | ✅| |
| **Functions**             |                   |        |       |
| [`absolute`](https://en.cppreference.com/w/cpp/filesystem/absolute) | `cfs_absolute`    | ✅   | |
| [`canonical`](https://en.cppreference.com/w/cpp/filesystem/canonical) | `cfs_canonical`   | ✅   | |
| [`copy`](https://en.cppreference.com/w/cpp/filesystem/copy) | `cfs_copy`        | ✅   | |
| [`copy_file`](https://en.cppreference.com/w/cpp/filesystem/copy_file) | `cfs_copy_file`   | ✅   | |
| [`copy_symlink`](https://en.cppreference.com/w/cpp/filesystem/copy_symlink) | `cfs_copy_symlink` | ✅   | |
| [`create_directory`](https://en.cppreference.com/w/cpp/filesystem/create_directory) | `cfs_create_directory` | ✅ | |
| [`create_directories`](https://en.cppreference.com/w/cpp/filesystem/create_directory) | `cfs_create_directories` | ✅ | |
| [`create_hard_link`](https://en.cppreference.com/w/cpp/filesystem/create_hard_link) | `cfs_create_hard_link` | ✅ | |
| [`create_symlink`](https://en.cppreference.com/w/cpp/filesystem/create_symlink) | `cfs_create_symlink` | ✅ | |
| [`create_directory_symlink`](https://en.cppreference.com/w/cpp/filesystem/create_directory_symlink) | `cfs_create_directory_symlink` | ✅ | |
| [`current_path`](https://en.cppreference.com/w/cpp/filesystem/current_path) | `cfs_current_path` | ✅  | Set (`cfs_current_path_set`) / Get both available |
| [`equivalent`](https://en.cppreference.com/w/cpp/filesystem/equivalent) | `cfs_equivalent`  | ✅   | |
| [`exists`](https://en.cppreference.com/w/cpp/filesystem/exists) | `cfs_exists` / `cfs_exists_path` | ✅   | Status and path overloads available |
| [`file_size`](https://en.cppreference.com/w/cpp/filesystem/file_size) | `cfs_file_size`   | ✅   | Includes async definition (`cfs_file_size_async`) |
| [`hard_link_count`](https://en.cppreference.com/w/cpp/filesystem/hard_link_count) | `cfs_hard_link_count` | ✅   | |
| [`is_block_file`](https://en.cppreference.com/w/cpp/filesystem/is_block_file) | `cfs_is_block_file` | ✅ | |
| [`is_character_file`](https://en.cppreference.com/w/cpp/filesystem/is_character_file) | `cfs_is_character_file` | ✅ | |
| [`is_directory`](https://en.cppreference.com/w/cpp/filesystem/is_directory) | `cfs_is_directory` | ✅  | |
| [`is_empty`](https://en.cppreference.com/w/cpp/filesystem/is_empty) | `cfs_is_empty_path` | ✅ | |
| [`is_fifo`](https://en.cppreference.com/w/cpp/filesystem/is_fifo) | `cfs_is_fifo`     | ✅   | |
| [`is_other`](https://en.cppreference.com/w/cpp/filesystem/is_other) | `cfs_is_other`    | ✅   | |
| [`is_regular_file`](https://en.cppreference.com/w/cpp/filesystem/is_regular_file) | `cfs_is_regular_file` | ✅ | |
| [`is_socket`](https://en.cppreference.com/w/cpp/filesystem/is_socket) | `cfs_is_socket`   | ✅   | |
| [`is_symlink`](https://en.cppreference.com/w/cpp/filesystem/is_symlink) | `cfs_is_symlink`  | ✅   | |
| [`last_write_time`](https://en.cppreference.com/w/cpp/filesystem/last_write_time) | `cfs_last_write_time` / `cfs_last_write_time_set` | ✅ | Getter and setter both implemented |
| [`permissions`](https://en.cppreference.com/w/cpp/filesystem/permissions) | `cfs_permissions` | ✅   | |
| [`proximate`](https://en.cppreference.com/w/cpp/filesystem/proximate) | `cfs_proximate`   | ✅   | FS-aware via `cfs_weakly_canonical` + `cfs_path_lexically_proximate` |
| [`read_symlink`](https://en.cppreference.com/w/cpp/filesystem/read_symlink) | `cfs_read_symlink` | ✅   | |
| [`relative`](https://en.cppreference.com/w/cpp/filesystem/relative) | `cfs_relative`    | ✅   | FS-aware via `cfs_weakly_canonical` + `cfs_path_lexically_relative` |
| [`remove`](https://en.cppreference.com/w/cpp/filesystem/remove) | `cfs_remove`      | ✅   | Includes async definition (`cfs_remove_async`) |
| [`remove_all`](https://en.cppreference.com/w/cpp/filesystem/remove) | `cfs_remove_all`  | ✅   | |
| [`rename`](https://en.cppreference.com/w/cpp/filesystem/rename) | `cfs_rename`      | ✅   | |
| [`resize_file`](https://en.cppreference.com/w/cpp/filesystem/resize_file) | `cfs_resize_file` | ✅   | |
| [`space`](https://en.cppreference.com/w/cpp/filesystem/space) | `cfs_space`       | ✅   | |
| [`status`](https://en.cppreference.com/w/cpp/filesystem/status) | `cfs_status`      | ✅   | |
| [`status_known`](https://en.cppreference.com/w/cpp/filesystem/status_known) | `cfs_status_known` | ✅   | |
| [`symlink_status`](https://en.cppreference.com/w/cpp/filesystem/status) | `cfs_symlink_status` | ✅| |
| [`temp_directory_path`](https://en.cppreference.com/w/cpp/filesystem/temp_directory_path) | `cfs_temp_directory_path` | ✅ | |
| [`weakly_canonical`](https://en.cppreference.com/w/cpp/filesystem/weakly_canonical) | `cfs_weakly_canonical` | ✅   | |
| [`hash_value`](https://en.cppreference.com/w/cpp/filesystem/path/hash_value) |                   | ➖ | C++ specific STL feature |

## Supported Platforms

The library relies heavily on platform detection macros to switch between backend implementations natively.

*   **Compilers:** GCC, Clang, MSVC (2005 through 2026), MinGW, Open Watcom.
*   **Operating Systems:** DOS, Windows, macOS, Linux, FreeBSD/NetBSD/OpenBSD/DragonFly.
*   **Environments:** Native, Cygwin, Emscripten, Alpine (Musl), Ubuntu/Debian (Glibc).

## Integration

`c-fs` follows the `stb`-style implementation macro pattern when used without building `src/cfs.c`. Ensure `include/` (containing `cfs/cfs.h`, `cfs/log.h`, and `cfs/no_discard.h`) is on your include path, and in exactly *one* C file define `CFS_IMPLEMENTATION` before including `cfs/cfs.h` *(note: if compiling with `DEBUG` defined, also compile `src/cfs.c` or provide `cfs_log_debug`)*:

```c
/* In exactly ONE source file: */
#define CFS_IMPLEMENTATION
#include "cfs/cfs.h"
```

In all other files, just include the header:
```c
#include "cfs/cfs.h"
```

## Quick Examples

### Path Manipulation
```c
#include "cfs/cfs.h"
#include <stdio.h>
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
#include <wchar.h>
#endif

int main(void) {
    cfs_path p;
    const cfs_char_t *str;

    if (cfs_path_init_str(&p, CFS_STR("var/log")) != cfs_errc_success) {
        return 1;
    }
    if (cfs_path_append(&p, CFS_STR("application.log")) != cfs_errc_success ||
        cfs_path_c_str(&p, &str) != cfs_errc_success) {
        cfs_path_destroy(&p);
        return 1;
    }

#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
    wprintf(L"Full Path: %ls\n", str);
#else
    printf("Full Path: %s\n", str);
#endif

    cfs_path_destroy(&p);
    return 0;
}
```

### Asynchronous Operations (Thread Pool)
```c
#include "cfs/cfs.h"
#include <stdio.h>

static void on_size(cfs_request_t *req, void *user_data) {
    cfs_bool *done = (cfs_bool *)user_data;
    if (req->error.errc == cfs_errc_success && req->result_buffer != NULL) {
        printf("Size: " CFS_UNUM_FORMAT "\n",
               *(const cfs_uintmax_t *)req->result_buffer);
    }
    if (done != NULL) {
        *done = cfs_true;
    }
}

int main(void) {
    cfs_runtime_config cfg;
    cfs_error_code ec;
    cfs_runtime_t *rt;
    cfs_path p;
    cfs_bool done = cfs_false;

    cfg.mode = cfs_modality_async;
    cfg.thread_pool_size = 4;
    cfg.ipc_path = NULL;

    if (cfs_runtime_init(&cfg, &rt, &ec) != cfs_errc_success) {
        return 1;
    }
    if (cfs_path_init_str(&p, CFS_STR("file.bin")) != cfs_errc_success) {
        cfs_runtime_destroy(rt);
        return 1;
    }
    if (cfs_file_size_async(rt, &p, on_size, &done) != cfs_errc_success) {
        cfs_path_destroy(&p);
        cfs_runtime_destroy(rt);
        return 1;
    }

    while (!done) {
        if (cfs_runtime_poll(rt) != cfs_errc_success) {
            break;
        }
    }

    cfs_path_destroy(&p);
    cfs_runtime_destroy(rt);
    return 0;
}
```

## CMake Support

Easily integrate via `FetchContent`, `add_subdirectory`, or an installed CMake package (`find_package(cfs CONFIG REQUIRED)`):
```cmake
add_subdirectory(c-fs)
target_link_libraries(my_app PRIVATE cfs::cfs) # or cfs
```

Options:
- `CFS_BUILD_STATIC`: (`ON`) Build `c-fs` as a static library (defaults to `!BUILD_SHARED_LIBS` when `BUILD_SHARED_LIBS` is defined).
- `CFS_BUILD_SHARED`: (`OFF`) Build `c-fs` as a shared library (`.dll` / `.so` / `.dylib`).
- `CFS_HEADER_ONLY`: (`OFF`) Expose `cfs` as a header-only `INTERFACE` target.
- `CFS_INSTALL` *(or `C_FS_INSTALL`)*: (`ON` when top-level, `OFF` as a subproject) Generate install and `cfsConfig.cmake` package export rules.
- `C_FS_CHARSET` *(or `CFS_CHARSET` / `CFS_UNICODE`)*: (`"Unicode"`) Character encoding on Windows (`"Unicode"` defines `CFS_UNICODE` for `wchar_t` APIs; `"ANSI"` / `"MBCS"` selects narrow `char` APIs).
- `C_FS_THREADING` *(or `CFS_THREADING`)*: (`"Multi-threaded"`) Threading configuration (`"Multi-threaded"` defines `CFS_MULTITHREADED`; or `"Single-threaded"`).
- `C_FS_LTO` *(or `CFS_LTO`)*: (`OFF`) Enable Link-Time Optimization (`INTERPROCEDURAL_OPTIMIZATION` and `CFS_ENABLE_LTO`).
- `C_FS_CRT_LINKAGE` *(or `CFS_CRT_LINKAGE`)*: (`"Shared"`) Per-target MSVC CRT linkage (`"Shared"` `/MD` or `"Static"` `/MT`) when `CMAKE_MSVC_RUNTIME_LIBRARY` is unset.
- `C_FS_MSVC_RTC` *(or `CFS_MSVC_RTC`)*: (`""`) Optional MSVC runtime error checks (`RTC1`, `RTCs`, `RTCu`).
- `CFS_BUILD_TESTING` *(or `C_FS_BUILD_TESTING` / `BUILD_TESTING`)*: (`OFF`, top-level only) Build the test suite.
- `C_FS_COVERAGE` *(or `CFS_COVERAGE`)*: (`OFF`, top-level only) Enable `--coverage` instrumentation on GCC/Clang.

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
