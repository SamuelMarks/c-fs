# c-fs Architecture & Internal Design

The `c-fs` project marries the ergonomics of C++17 `<filesystem>` with the rigorous constraints of ISO C90 (C89), while expanding scope to handle concurrent execution modalities. This document outlines the core architectural phases, data structures, and cross-platform strategies.

## Header & Compilation Layout

Public declarations and header-only implementation blocks live under `include/cfs/`:

*   `include/cfs/cfs.h`: Primary public API and `CFS_IMPLEMENTATION`-guarded implementation body.
*   `include/cfs/no_discard.h`: Namespaced `CFS_NO_DISCARD`, `CFS_NO_DISCARD_TYPE`, and `CFS_NO_DISCARD_TYPEDEF` attributes (`[[nodiscard]]`, `__attribute__((warn_unused_result))`, MSVC SAL `_Check_return_`, `<sal.h>` included outside `extern "C"`), plus `#ifndef NO_DISCARD*` fallbacks so `c-fs` coexists cleanly alongside sibling C libraries regardless of include order.
*   `include/cfs/log.h`: Internal `LOG_DEBUG` / `cfs_log_debug` diagnostics interface.
*   `src/cfs.c`: Compiled library translation unit defining `CFS_IMPLEMENTATION`, `cfs_log_debug`, and optional `CFS_ENABLE_FAULT_INJECTION` test hooks.

Within `cfs.h`, declarations and `#ifdef CFS_IMPLEMENTATION` definitions are organized into logical phases:

1. **Phase 3: Platform & Compiler Detection:** Maps operating systems (`CFS_OS_WINDOWS`, `CFS_OS_LINUX`, `CFS_OS_MACOS`, `CFS_OS_BSD`, `CFS_OS_EMSCRIPTEN`, `CFS_OS_DOS`), environments (`CFS_ENV_MINGW`, `CFS_ENV_CYGWIN`), and compilers (`CFS_COMPILER_MSVC`, `CFS_COMPILER_CLANG`, `CFS_COMPILER_GCC`).
2. **Phase 4: Distribution Modes Scaffolding:** Configures symbol visibility (`CFS_API`), C89-safe inline specifiers (`CFS_INLINE`), thread-local qualifiers (`CFS_THREAD_LOCAL`), and LTO visibility (`CFS_EXTERNALLY_VISIBLE`) across static, shared (`.dll`/`.so`/`.dylib`), and header-only (`CFS_HEADER_ONLY_MODE`) builds.
3. **Phase 5: Memory & Core Types:** Core type abstractions (`cfs_char_t`, `cfs_bool`, `cfs_size_t`, `CFS_NO_DISCARD_TYPE`-annotated `cfs_errc`) and checked allocator wrappers (`cfs_malloc`, `cfs_realloc`, `cfs_calloc`, `cfs_free`, `cfs_set_oom_handler`).
4. **Phase 6: String & Charset Handling:** Safe string copy macros (`CFS_STRCPY_SAFE`, `CFS_STRNCPY_SAFE`), checked string helpers (`cfs_strlen`, `cfs_strcpy`, `cfs_strcmp`), and UTF-8/UTF-16 (`cfs_utf8_to_utf16`, `cfs_utf16_to_utf8`) plus multibyte/wide (`cfs_mb_to_wide`, `cfs_wide_to_mb`) conversion routines.
5. **Phase 7: Error Handling:** Unified error structures (`cfs_error_code`) and helpers (`cfs_set_error`, `cfs_clear_error`, `cfs_error_message`, `cfs_make_error_code_from_os`, `cfs_get_last_error`).
6. **Phases 8–14: Path Construction, Decomposition & Lexical Operations:** Manages `cfs_path`, dynamically resizing buffers, normalizing separators, parsing Windows drive/UNC/extended-length roots, and computing lexical relative/normal forms (`cfs_path_lexically_normal`, `cfs_path_lexically_relative`, `cfs_path_lexically_proximate`).
7. **Phases 15–19: Filesystem Operations & Directory Iteration:** Maps C API functions to host syscalls (`_wstat64`, `stat`, `CreateFileW`, `FindFirstFileW`, `opendir`/`readdir`, `statvfs`, etc.). Note that `cfs_recursive_directory_iterator` currently wraps single-level iteration rather than descending into nested subdirectories.
8. **Phases 5.5–5.7 & Async Phases 3–4: Execution Modalities, IPC & Greenthreads:** Implements `cfs_runtime_t`, worker thread pools, mutex/condition-variable queues, atomic request reference counting (`cfs_request_retain`/`cfs_request_release`), binary request serialization, and foundational scaffolding/stubs for message pipes, multiprocessing, shared memory, named semaphores, and cooperative greenthreads.

## Core Data Structures

### `cfs_path`
`cfs_path` is a publicly defined value-type struct (allowing stack allocation in C89 without an extra heap indirection for the handle itself) wrapping a dynamically allocated `cfs_char_t` buffer:
```c
typedef struct cfs_path {
  cfs_char_t *str;
  cfs_size_t length;   /* Length in cfs_char_t elements (excluding NUL) */
  cfs_size_t capacity; /* Allocated capacity in cfs_char_t elements */
} cfs_path;
```
All mutations bounds-check against `capacity` and grow the buffer via `cfs_path_reserve` before copying. By contrast, stateful handles such as `cfs_runtime_t` and `cfs_directory_iterator` are genuinely opaque pointers in the public header.

### `cfs_error_code`
Pairs raw OS error integers with the library's `cfs_errc` enumeration:
```c
typedef struct cfs_error_code {
  int value;     /* Raw OS error code (e.g., GetLastError(); POSIX errno wiring is currently stubbed) */
  cfs_errc errc; /* Standardized cfs_errc code */
} cfs_error_code;
```
Argument-validation and explicit checks populate specific `cfs_errc_*` enumerators directly, whereas `cfs_make_error_code_from_os` / `cfs_record_last_error` currently normalize non-zero OS errors to `cfs_errc_unknown_error`.

### Execution Runtime (`cfs_runtime_t`)
Defines how deferred `cfs_request_t` operations execute:
*   `cfs_modality_sync`: Executes `cfs_execute_op_inline` and invokes the user callback immediately inside `cfs_dispatch_request`.
*   `cfs_modality_async` / `cfs_modality_multithread`: Allocates a `work_queue`, a `completion_queue`, and a worker `cfs_thread_pool_t` (defaulting to 4 threads when `thread_pool_size == 0`).
*   Other `cfs_modality` values (`singlethread`, `multiprocess`, `greenthread`, `message_passing`) are reserved enum modalities whose runtime queue backends are not yet wired into `cfs_runtime_init`.

## Cross-Platform Polyfills

### Windows Threading & Synchronization (Legacy MSVC 2005)
Modern Windows concurrency relies on `CONDITION_VARIABLE` (introduced in Windows Vista). To natively support Windows XP / MSVC 2005 compilation, `c-fs` utilizes a preprocessor fallback that maps condition variables onto raw Windows `Event` objects (`CreateEvent`, `WaitForSingleObject`) when `_MSC_VER < 1500`.

### Safe CRT Operations
To satisfy MSVC security warnings while preserving strict C89 portability across GCC, Clang, and MinGW, bounded internal copies pre-allocate exact capacity via `cfs_path_reserve` and route string writes through `CFS_STRCPY_SAFE` (and `CFS_STRNCPY_SAFE`):
*   **MSVC (Unicode):** Maps to `wcscpy_s` / `wcsncpy_s`.
*   **MSVC (ANSI):** Maps to `strcpy_s` / `strncpy_s`.
*   **Non-MSVC Windows (Unicode, e.g. MinGW):** Maps to `((void)(dst_sz), wcscpy((dst), (src)))` / `wcsncpy`.
*   **GCC/Clang/POSIX & ANSI:** Maps to `((void)(dst_sz), strcpy((dst), (src)))` / `strncpy`.

## The Request Dispatcher

Asynchronous tasks are enqueued as reference-counted `cfs_request_t` structures containing an `opcode` (e.g., `cfs_opcode_remove`, `cfs_opcode_file_size`, `cfs_opcode_dir_itr_init`), cloned path arguments, result storage, and a `cfs_callback_t` function pointer.

In thread-pool modalities, worker threads pop requests from `work_queue`, run the underlying blocking `c-fs` operation via `cfs_execute_op_inline` (unless cancelled), record results or `cfs_error_code` state on the request, and push completed items onto `completion_queue`. Calling `cfs_runtime_poll()` on the application thread drains `completion_queue`, invokes each request's callback synchronously, and releases the runtime's reference via `cfs_request_release()`.
