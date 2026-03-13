# Required Skills for c-fs Contributions

Because `c-fs` bridges legacy constraints with modern API design, contributing effectively to this project requires a cross-section of foundational systems programming skills.

## 1. Strict ISO C90 (C89)
You must be intimately familiar with C89 limitations.
*   **Variable Declarations:** Knowing how to scope variable declarations strictly at the top of code blocks.
*   **Primitive Types:** Operating without `<stdint.h>` or `<stdbool.h>`, managing custom `cfs_bool` and integer typedefs manually based on compiler definitions.
*   **Comments:** Utilizing only `/* block comments */`.

## 2. Legacy MSVC (Visual Studio 2005 / C++ 8.0)
The library guarantees compilation on exceptionally old toolchains.
*   **Pre-Vista APIs:** Understanding that Windows XP does not have `CONDITION_VARIABLE`, `SRWLOCK`, or `GetTickCount64`. Designing robust synchronization fallbacks using raw `HANDLE` events (`CreateEvent`, `WaitForSingleObject`).
*   **Safe CRT:** Familiarity with Microsoft's Security Enhancements in the CRT (`_s` functions like `strcpy_s` and `_wstat64`).

## 3. CMake & Cross-Compilation
*   Understanding how to structure cross-platform CMake builds.
*   Knowing how to toggle `INTERFACE` libraries, `FetchContent`, and MSVC runtime permutations (`/MT`, `/MD`, `/RTCs`).

## 4. Systems Concurrency
*   **POSIX pthreads:** Designing abstractions that cleanly wrap `pthread_mutex_t`, `pthread_cond_t`, and `pthread_t`.
*   **Thread-Safety:** Writing lock-free or mutex-guarded thread-safe FIFO queues, handling shutdown cascade signals to wake waiting worker threads safely.
*   **Memory Management:** Thread-safe reference counting methodologies (e.g., using `InterlockedIncrement` on Win32, and `__atomic_fetch_add` on GCC).

## 5. POSIX & Win32 Filesystem Quirks
*   **Win32 Wide Characters:** Deep knowledge of UTF-16 APIs (`GetFileAttributesW`, `_wremove`, `_wstat64`) vs narrow APIs.
*   **Path Resolution:** Understanding lexicographical path edge cases (e.g., treating `\` and `/` appropriately based on the host OS), handling root names (`C:`), and stem vs. extension parsing logic.
*   **Error Mapping:** Knowing how to map standard `errno` and `GetLastError()` results back to a unified `std::errc` enum safely.