# c-fs Usage Examples

This document outlines how to use the core features of `c-fs`. The library supports a flexible set of build modes including purely header-only integration, linking as a static library, or compiling as a shared DLL/SO library depending on your project needs.

For header-only usage, always ensure exactly one source file in your project has defined `CFS_IMPLEMENTATION` before including `cfs.h` to instantiate the implementation logic.

## 1. Path Construction and Manipulation

`cfs_path` objects are dynamically allocated to prevent `MAX_PATH` truncation issues. They must be initialized and destroyed safely.

```c
#include "cfs/cfs.h"
#include <stdio.h>

int main() {
    cfs_path p;
    
    /* Initialize an empty path or from a string literal */
    cfs_path_init_str(&p, CFS_STR("var/log"));
    
    /* Append dynamically adds separators based on OS */
    cfs_path_append(&p, CFS_STR("application.log"));
    
    printf("Full Path: %s\n", cfs_path_c_str(&p));
    
    /* Path decomposition */
    cfs_path filename = cfs_path_filename(&p);
    cfs_path extension = cfs_path_extension(&p);
    
    printf("Filename: %s\n", cfs_path_c_str(&filename));
    printf("Extension: %s\n", cfs_path_c_str(&extension));
    
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

void check_file(const cfs_char_t* path_str) {
    cfs_path p;
    cfs_error_code ec;
    cfs_path_init_str(&p, path_str);
    
    if (cfs_exists_path(&p, &ec)) {
        cfs_uintmax_t size = cfs_file_size(&p, &ec);
        printf("File size: %llu bytes\n", (unsigned long long)size);
    } else {
        printf("File does not exist. (Error: %d)\n", ec.value);
    }
    
    cfs_path_destroy(&p);
}
```

## 3. Asynchronous, Multithreaded Operations

`c-fs` natively supports advanced asynchronous scheduling, allowing you to defer execution via configurable modalities. This includes built-in thread-pooling to push blocking operations off your main thread seamlessly. 

```c
#include "cfs/cfs.h"
#include <stdio.h>

/* The callback executed on the main thread during cfs_runtime_poll */
void on_file_size_complete(cfs_request_t* req, void* user_data) {
    if (req->error.value == 0 && req->result_buffer) {
        cfs_uintmax_t size = *((cfs_uintmax_t*)req->result_buffer);
        printf("Async Result -> Size: %llu bytes\n", (unsigned long long)size);
    } else {
        printf("Async Operation Failed: %d\n", req->error.value);
    }
}

int main() {
    cfs_runtime_config config;
    cfs_runtime_t* rt;
    cfs_path p;
    cfs_error_code ec;

    /* 1. Setup Runtime */
    config.mode = cfs_modality_async;
    config.thread_pool_size = 4;
    rt = cfs_runtime_init(&config, &ec);
    
    /* 2. Dispatch Task */
    cfs_path_init_str(&p, CFS_STR("huge_file.bin"));
    cfs_file_size_async(rt, &p, on_file_size_complete, NULL);
    
    /* 3. Main Application Loop */
    while (1) {
        /* Poll the runtime to invoke callbacks on the main thread */
        int processed = cfs_runtime_poll(rt);
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
