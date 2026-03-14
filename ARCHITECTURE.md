# c-fs Architecture & Internal Design

The `c-fs` project marries the ergonomics of C++17 `<filesystem>` with the rigorous constraints of ISO C90 (C89), while expanding scope to handle advanced concurrent execution modalities. This document outlines the core architectural phases, data structures, and cross-platform strategies.

## Single-Header Layout

The `cfs.h` header is organized into strictly defined logical phases, wrapped in a single monolithic header file via the `CFS_IMPLEMENTATION` macro pattern.

1. **Phase 3: Platform & Compiler Detection:** Maps operating systems (`CFS_OS_WINDOWS`, `CFS_OS_LINUX`), compilers (`CFS_COMPILER_MSVC`, `CFS_COMPILER_GCC`), and build distribution settings (DLL Export/Import, Inline rules, LTO). This handles the flexible library distributions supporting static libraries, shared `.dll`/`.so` builds, and pure header-only inclusion.
2. **Phase 5: Memory & Core Types:** Core abstractions (`cfs_char_t`, `cfs_bool`, custom memory allocator hooks `cfs_malloc`, etc.).
3. **Phase 6: String Handling:** Safe text parsing routines bridging the gap between C standard strings and Safe CRT (`CFS_STRCPY_SAFE`).
4. **Phase 7: Error Handling:** A unified error bridging system. Converts raw OS error codes (`GetLastError()` or `errno`) into POSIX-standardized `cfs_errc` equivalents matching `std::errc`.
5. **Phase 8-14: Path Construction & Lexical operations:** The workhorse of the library. Manages `cfs_path`, ensuring dynamic resizing to accommodate variable path lengths, parsing separators accurately, and decomposing components (stem, filename, extension).
6. **Phase 15-19: Filesystem Operations:** Maps C API functions directly to OS-specific syscalls (`_wstat64`, `stat`, `remove`, `RemoveDirectoryW`). Includes cross-platform iterators for directory trees.
7. **Phase 5.5-5.7: Execution Modalities (Async/Runtime):** Implements thread pools, sync structures, and deferred task dispatching via an Opcode abstraction to enable robust multithreaded, async, multiprocess, and message-passing file system access.

## Core Data Structures

### `cfs_path`
Paths are opaque structures wrapping dynamically sized character buffers.
```c
typedef struct cfs_path {
  cfs_char_t *str;
  cfs_size_t length;
  cfs_size_t capacity;
} cfs_path;
```
All mutations bounds-check against `capacity` and reallocate via `cfs_path_reserve` to avoid buffer overflows.

### `cfs_error_code`
A unified error bridge mapping native OS states to standard states.
```c
typedef struct cfs_error_code {
  int value;     /* The raw OS specific error code (errno or GetLastError()) */
  cfs_errc errc; /* The unified POSIX mapping */
} cfs_error_code;
```

### Execution Runtime (`cfs_runtime_t`)
Defines the behavior of background operations (async).
A `cfs_runtime_t` spins up a configurable Thread Pool and two Thread-Safe FIFO queues: a `work_queue` and a `completion_queue`. 

## Cross-Platform Polyfills

### Windows Threading & Synchronization (Legacy MSVC 2005)
Modern Windows concurrency relies on `CONDITION_VARIABLE` (introduced in Windows Vista). To natively support Windows XP / MSVC 2005 compilation, `c-fs` utilizes a dynamic preprocessor fallback that maps condition variables onto raw Windows `Event` objects (`CreateEvent`, `WaitForSingleObject`) if `_MSC_VER < 1500`.

### Safe CRT Operations
To silence MSVC analyzer warnings and ensure actual memory safety without dropping GCC/Clang support, the architecture routes standard string copies through `CFS_STRCPY_SAFE` macros.
*   **MSVC (Unicode):** Maps to `wcscpy_s`.
*   **MSVC (ANSI):** Maps to `strcpy_s`.
*   **GCC/Clang/POSIX:** Maps to an internally defined standard `cfs_strcpy` implementation.

## The Request Dispatcher

Asynchronous tasks are enqueued as `cfs_request_t` structures containing an `opcode` (e.g., `cfs_opcode_remove`, `cfs_opcode_file_size`), argument data (`target_path`), and a callback function pointer.

Workers pop these requests, map the opcode to the underlying blocking `c-fs` function (`cfs_execute_op_inline`), inject the result or error state into the request context, and push it to the `completion_queue`. The application main-thread triggers callbacks synchronously when calling `cfs_runtime_poll()`.
