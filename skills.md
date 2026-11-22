# Required Skills for c-fs Contributions

Because `c-fs` bridges legacy constraints with modern API design, contributing effectively to this project requires a cross-section of foundational systems programming skills.

## 1. Strict ISO C90 (C89)
You must be intimately familiar with C89 limitations.
*   **Variable Declarations:** Scoping all variable declarations strictly at the top of lexical blocks (`-Wdeclaration-after-statement`).
*   **Strict Prototypes:** Using `(void)` for zero-argument functions (`-Wstrict-prototypes`, `-Wold-style-definition`, `-Wmissing-prototypes`).
*   **Primitive Types:** Operating without `<stdint.h>` or `<stdbool.h>`, managing `cfs_bool` and 64-bit integer typedefs (`__extension__ long long` / `__int64`) safely across compilers.
*   **Comments:** Utilizing only `/* block comments */`.

## 2. Legacy MSVC (Visual Studio 2005 / C++ 8.0)
The library guarantees compilation on exceptionally old toolchains alongside modern MSVC.
*   **Pre-Vista APIs:** Understanding that Windows XP lacks `CONDITION_VARIABLE`, `SRWLOCK`, and Vista+ symlink APIs, requiring `_MSC_VER < 1500` synchronization fallbacks (`CreateEvent`, `WaitForSingleObject`) and dynamic `GetProcAddress` lookups where needed.
*   **Minimal Windows Headers:** Avoiding global `<windows.h>` bloat in headers and keeping `<sal.h>` outside `extern "C"` blocks for C++ interoperability.
*   **Safe CRT:** Familiarity with Microsoft's Security Enhancements in the CRT (`wcscpy_s`, `strcpy_s`, `_wstat64`, `fopen_s`).

## 3. CMake & Subproject Isolation
*   Structuring cross-platform CMake builds across static (`CFS_BUILD_STATIC`), shared (`CFS_BUILD_SHARED`), and header-only (`CFS_HEADER_ONLY`) distributions.
*   Managing Windows character sets (`C_FS_CHARSET` / `CFS_CHARSET`), threading (`C_FS_THREADING`), LTO (`C_FS_LTO`), and MSVC runtime/check permutations (`C_FS_CRT_LINKAGE`, `C_FS_MSVC_RTC`, `/MT`, `/MD`, `/RTC1`, `/RTCs`, `/RTCu`).
*   Isolating warning and coverage flags (`PRIVATE` / `CFS_IS_TOP_LEVEL`) so consuming parent projects via `FetchContent` never inherit unwanted compile or link options.

## 4. Systems Concurrency & Asynchronous Design
*   **Asynchronous Modalities:** Understanding the inline (`cfs_modality_sync`) and worker thread-pool (`cfs_modality_async`, `cfs_modality_multithread`) dispatch pipelines, as well as extending the currently stubbed greenthread, message-passing, and POSIX multiprocess/shared-memory backends.
*   **POSIX `pthreads` & Win32 Threads:** Wrapping `pthread_mutex_t`, `pthread_cond_t`, and `pthread_t` cleanly alongside Win32 `CRITICAL_SECTION` and thread handles.
*   **Thread-Safety & Lifetimes:** Writing mutex-guarded FIFO queues, clean shutdown wakeup cascades, and atomic reference counting (`InterlockedIncrement`/`InterlockedDecrement` on Win32, `__atomic_fetch_add`/`__atomic_sub_fetch` on GCC/Clang).

## 5. POSIX & Win32 Filesystem Quirks
*   **Win32 Wide Characters:** Deep knowledge of UTF-16 APIs (`_wstat64`, `CreateFileW`, `FindFirstFileW`, `GetFullPathNameW`, `_wremove`) vs narrow ANSI/MBCS equivalents.
*   **Path Resolution:** Handling lexical path edge cases across `/` and `\`, Windows drive letters (`C:`), UNC shares (`\\server\share`), extended-length prefixes (`\\?\`), and stem vs. extension decomposition rules.
*   **Error Mapping:** Bridging raw OS error states (`GetLastError()` and POSIX `errno`) into `cfs_error_code` and the `cfs_errc` enumeration (note: general OS-to-POSIX error translation in `cfs_make_error_code_from_os` is currently stubbed to `cfs_errc_unknown_error`).

## 6. API Safety, Documentation & Test Rigor
*   **`CFS_NO_DISCARD` & Multi-Header Coexistence:** Annotating every fallible public API with `CFS_NO_DISCARD CFS_API cfs_errc` (`include/cfs/no_discard.h`), preserving fallback `NO_DISCARD*` compatibility with sibling libraries, and percolating all errors without `(void)` suppression casts.
*   **100% Documentation & Test Coverage:** Maintaining 100% Doxygen public API documentation coverage and 100% line/branch test coverage (including OOM and syscall failure paths via `CFS_ENABLE_FAULT_INJECTION`).
