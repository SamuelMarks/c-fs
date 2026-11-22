# c-fs Usage Examples

This document outlines how to use the core features of `c-fs`. The library supports a flexible set of build modes including header-only integration, linking as a static library, or compiling as a shared DLL/SO/DYLIB library depending on your project needs.

For manual header-only usage (without compiling `src/cfs.c`), keep the `include/cfs/` directory (`cfs.h`, `log.h`, and `no_discard.h`) on your include path and define `CFS_IMPLEMENTATION` in *exactly one* C translation unit before including `cfs/cfs.h` *(note: if `DEBUG` is defined, `cfs_log_debug` in `src/cfs.c` is also referenced)*.

## 1. Path Construction and Manipulation

`cfs_path` objects dynamically allocate their character buffers to prevent `MAX_PATH` truncation issues. Every initialized `cfs_path` must be cleaned up with `cfs_path_destroy`, and all `CFS_NO_DISCARD` `cfs_errc` return values should be checked.

```c
#include "cfs/cfs.h"
#include <stdio.h>
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
#include <wchar.h>
#endif

static void print_path(const char *label, const cfs_char_t *value) {
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
    printf("%s: ", label);
    wprintf(L"%ls\n", value);
#else
    printf("%s: %s\n", label, value);
#endif
}

int main(void) {
    cfs_path p;
    cfs_path filename;
    cfs_path extension;
    const cfs_char_t *str;
    int exit_code = 1;

    cfs_path_init(&p);
    cfs_path_init(&filename);
    cfs_path_init(&extension);

    /* Initialize from a string literal and append a child component */
    if (cfs_path_assign(&p, CFS_STR("var/log")) != cfs_errc_success ||
        cfs_path_append(&p, CFS_STR("application.log")) != cfs_errc_success ||
        cfs_path_c_str(&p, &str) != cfs_errc_success) {
        goto cleanup;
    }
    print_path("Full Path", str);

    /* Path decomposition */
    if (cfs_path_filename(&p, &filename) != cfs_errc_success ||
        cfs_path_extension(&p, &extension) != cfs_errc_success) {
        goto cleanup;
    }

    if (cfs_path_c_str(&filename, &str) != cfs_errc_success) {
        goto cleanup;
    }
    print_path("Filename", str);

    if (cfs_path_c_str(&extension, &str) != cfs_errc_success) {
        goto cleanup;
    }
    print_path("Extension", str);

    exit_code = 0;

cleanup:
    /* Memory cleanup is mandatory */
    cfs_path_destroy(&extension);
    cfs_path_destroy(&filename);
    cfs_path_destroy(&p);

    return exit_code;
}
```

## 2. Filesystem Information & Status

Query the file system using `cfs_path` instances to retrieve sizes, types, and statuses. Operations accept an optional `cfs_error_code *` pointer (pass `NULL` if detailed OS error state is not needed) and return a `cfs_errc` status code (`cfs_errc_success == 0`).

```c
#include "cfs/cfs.h"
#include <stdio.h>

void check_file(const cfs_char_t *path_str);

void check_file(const cfs_char_t *path_str) {
    cfs_path p;
    cfs_error_code ec;
    cfs_bool exists = cfs_false;
    cfs_uintmax_t size = 0;
    cfs_errc rc;
    const char *err_msg = "unknown error";

    if (cfs_path_init_str(&p, path_str) != cfs_errc_success) {
        return;
    }

    rc = cfs_exists_path(&p, &exists, &ec);
    if (rc != cfs_errc_success) {
        if (cfs_error_message(rc, &err_msg) == cfs_errc_success) {
            printf("Failed to query path (%d): %s\n", (int)rc, err_msg);
        }
    } else if (!exists) {
        printf("File does not exist.\n");
    } else {
        rc = cfs_file_size(&p, &size, &ec);
        if (rc == cfs_errc_success) {
            printf("File size: " CFS_UNUM_FORMAT " bytes\n", size);
        } else if (cfs_error_message(rc, &err_msg) == cfs_errc_success) {
            printf("Failed to get file size (%d, OS=%d): %s\n",
                   (int)rc, ec.value, err_msg);
        }
    }

    cfs_path_destroy(&p);
}
```

## 3. Asynchronous, Multithreaded Operations

`c-fs` supports deferred execution via `cfs_runtime_t` (`cfs_modality_sync` for inline execution and `cfs_modality_async` / `cfs_modality_multithread` for background worker thread pools). Call `cfs_runtime_poll(rt)` from your main loop to drain completed requests and invoke their callbacks on the calling thread; use `user_data` to track task completion in your application state.

```c
#include "cfs/cfs.h"
#include <stdio.h>

/* Callback executed on the main thread during cfs_runtime_poll */
static void on_file_size_complete(cfs_request_t *req, void *user_data) {
    cfs_bool *done = (cfs_bool *)user_data;

    if (req->error.errc == cfs_errc_success && req->result_buffer != NULL) {
        cfs_uintmax_t size = *((const cfs_uintmax_t *)req->result_buffer);
        printf("Async Result -> Size: " CFS_UNUM_FORMAT " bytes\n", size);
    } else {
        printf("Async Operation Failed (errc=%d, os=%d)\n",
               (int)req->error.errc, req->error.value);
    }

    if (done != NULL) {
        *done = cfs_true;
    }
}

int main(void) {
    cfs_runtime_config config;
    cfs_runtime_t *rt;
    cfs_path p;
    cfs_error_code ec;
    cfs_bool done = cfs_false;

    /* 1. Setup Runtime */
    config.mode = cfs_modality_async;
    config.thread_pool_size = 4;
    config.ipc_path = NULL;
    if (cfs_runtime_init(&config, &rt, &ec) != cfs_errc_success) {
        return 1;
    }

    /* 2. Dispatch Task */
    if (cfs_path_init_str(&p, CFS_STR("huge_file.bin")) != cfs_errc_success) {
        cfs_runtime_destroy(rt);
        return 1;
    }
    if (cfs_file_size_async(rt, &p, on_file_size_complete, &done) !=
        cfs_errc_success) {
        cfs_path_destroy(&p);
        cfs_runtime_destroy(rt);
        return 1;
    }

    /* 3. Main Application Loop */
    while (!done) {
        /* Poll the runtime to invoke completed callbacks on the main thread */
        if (cfs_runtime_poll(rt) != cfs_errc_success) {
            break;
        }
        /* Simulate frame work */
    }

    /* 4. Cleanup */
    cfs_path_destroy(&p);
    cfs_runtime_destroy(rt);
    return 0;
}
```
