# c-fs Usage Examples

This document outlines how to use the core features of `c-fs`. The library supports a flexible set of build modes including purely header-only integration, linking as a static library, or compiling as a shared DLL/SO library depending on your project needs.

For header-only usage, always ensure exactly one source file in your project has defined `CFS_IMPLEMENTATION` before including `cfs.h` to instantiate the implementation logic.

## 1. Path Construction and Manipulation

`cfs_path` objects are dynamically allocated to prevent `MAX_PATH` truncation issues. They must be initialized and destroyed safely.

```c
#include "cfs/cfs.h"
#include <stdio.h>
#include <assert.h>

int main() {
    cfs_path p;
    cfs_path filename;
    cfs_path extension;
    const cfs_char_t *str;

    /* Initialize an empty path or from a string literal */
    assert(cfs_path_init_str(&p, CFS_STR("var/log")) == 0);

    /* Append dynamically adds separators based on OS */
    assert(cfs_path_append(&p, CFS_STR("application.log")) == 0);

    assert(cfs_path_c_str(&p, &str) == 0);
    printf("Full Path: %s\n", str);

    /* Path decomposition */
    cfs_path_init(&filename);
    cfs_path_init(&extension);
    assert(cfs_path_filename(&p, &filename) == 0);
    assert(cfs_path_extension(&p, &extension) == 0);

    assert(cfs_path_c_str(&filename, &str) == 0);
    printf("Filename: %s\n", str);
    assert(cfs_path_c_str(&extension, &str) == 0);
    printf("Extension: %s\n", str);

    /* Memory cleanup is mandatory */
    cfs_path_destroy(&filename);
    cfs_path_destroy(&extension);
    cfs_path_destroy(&p);

    return 0;
}
```

## 2. Filesystem Information & Status

Query the file system using path instances to retrieve sizes, types, and statuses. Operations support passing an optional `cfs_error_code` pointer; if passed as `NULL`, errors are ignored silently (or trigger standard error handling bounds depending on configuration).

```c
#include "cfs/cfs.h"
#include <stdio.h>
#include <assert.h>

void check_file(const cfs_char_t* path_str) {
    cfs_path p;
    cfs_error_code ec;
    cfs_bool exists;
    cfs_uintmax_t size;

    assert(cfs_path_init_str(&p, path_str) == 0);

    if (cfs_exists_path(&p, &exists, &ec) == 0 && exists) {
        if (cfs_file_size(&p, &size, &ec) == 0) {
            printf("File size: " CFS_UNUM_FORMAT " bytes\n", (cfs_uintmax_t)size);
        } else {
            printf("Failed to get file size. (Error: %d)\n", ec.value);
        }
    } else {
        printf("File does not exist or error occurred. (Error: %d)\n", ec.value);
    }

    cfs_path_destroy(&p);
}
```

## 3. Asynchronous, Multithreaded Operations

`c-fs` natively supports advanced asynchronous scheduling, allowing you to defer execution via configurable modalities. This includes built-in thread-pooling to push blocking operations off your main thread seamlessly.

```c
#include "cfs/cfs.h"
#include <stdio.h>
#include <assert.h>

/* The callback executed on the main thread during cfs_runtime_poll */
void on_file_size_complete(cfs_request_t* req, void* user_data) {
    if (req->error.value == 0 && req->result_buffer) {
        cfs_uintmax_t size = *((cfs_uintmax_t*)req->result_buffer);
        printf("Async Result -> Size: " CFS_UNUM_FORMAT " bytes\n", (cfs_uintmax_t)size);
    } else {
        printf("Async Operation Failed: %d\n", req->error.value);
    }
}

int main() {
    cfs_runtime_config config;
    cfs_runtime_t* rt;
    cfs_path p;
    cfs_error_code ec;
    int processed;

    /* 1. Setup Runtime */
    config.mode = cfs_modality_async;
    config.thread_pool_size = 4;
    config.ipc_path = NULL;
    assert(cfs_runtime_init(&config, &rt, &ec) == 0);

    /* 2. Dispatch Task */
    assert(cfs_path_init_str(&p, CFS_STR("huge_file.bin")) == 0);
    assert(cfs_file_size_async(rt, &p, on_file_size_complete, NULL) == 0);

    /* 3. Main Application Loop */
    while (1) {
        /* Poll the runtime to invoke callbacks on the main thread */
        processed = cfs_runtime_poll(rt);
        if (processed > 0) {
            break; /* For example purposes, break after processing our task */
        }
        /* Simulate frame work */
    }

    /* 4. Cleanup */
    cfs_path_destroy(&p);
    cfs_runtime_destroy(rt);
    return 0;
}
```
