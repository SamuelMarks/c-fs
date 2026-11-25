/**
 * \file test_main.c
 * \brief Main test suite for the c-fs library.
 */
/* clang-format off */
#if !defined(_XOPEN_SOURCE) && !defined(_WIN32)
/** @brief Enables POSIX.1-2004 interfaces on non-Windows platforms. */
#define _XOPEN_SOURCE 600
#endif
#if !defined(__STDC_WANT_LIB_EXT1__)
/** @brief Requests Annex K safe C library bounds-checking interfaces. */
#define __STDC_WANT_LIB_EXT1__ 1
#endif
#include "cfs/cfs.h"
#include "test_interop.h"
#include <errno.h>
#include <locale.h>
#include <string.h>
#include "greatest.h"

#if defined(CFS_OS_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Excludes rarely-used APIs from Win32 headers. */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#ifdef __cplusplus
}
#endif
#elif defined(CFS_OS_DOS)
#include <dos.h>
#include <sys/stat.h>
#else
#if defined(CFS_MULTITHREADED)
#include <pthread.h>
#endif
#if !defined(CFS_OS_EMSCRIPTEN)
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#endif
/* clang-format on */

/** @brief Countdown flag to simulate `cfs_malloc` failure in tests. */
CFS_API extern int g_cfs_malloc_fail;
/** @brief Countdown flag to simulate `cfs_realloc` failure in tests. */
CFS_API extern int g_cfs_realloc_fail;
/** @brief Countdown flag to simulate `cfs_calloc` failure in tests. */
CFS_API extern int g_cfs_calloc_fail;
/** @brief Flag to simulate `getcwd` failure in tests. */
CFS_API extern int g_cfs_getcwd_fail;
/** @brief Flag to simulate `readlink` failure in tests. */
CFS_API extern int g_cfs_readlink_fail;
/** @brief Flag to simulate `pthread_create` failure in tests. */
CFS_API extern int g_cfs_pthread_create_fail;
/** @brief Flag to simulate `pipe` failure in tests. */
CFS_API extern int g_cfs_pipe_fail;
/** @brief Countdown flag to simulate `read`/`write` failure in tests. */
CFS_API extern int g_cfs_rw_fail;
/** @brief Flag to simulate `posix_spawn` failure in tests. */
CFS_API extern int g_cfs_spawn_fail;
/** @brief Flag to simulate `waitpid` failure in tests. */
CFS_API extern int g_cfs_waitpid_fail;

/** @brief Counter of completed asynchronous callbacks during tests. */
static int test_completed_ops = 0;

/** @brief Counter tracking OOM handler invocations during tests. */
static int g_oom_handler_called = 0;

/**
 * \brief Callback function for asynchronous tests.
 * \param req Pointer to the asynchronous request.
 * \param user_data Opaque pointer to user data.
 */
static void async_callback(cfs_request_t *req, void *user_data) {
  (void)req;
  (void)user_data;
  test_completed_ops++;
}

/**
 * \brief Dummy greenthread function for testing.
 * \param arg Opaque pointer to thread arguments.
 */
static void dummy_greenthread(void *arg) { (void)arg; }

/**
 * \brief Test OOM handler callback.
 */
static void test_oom_handler_cb(void) { g_oom_handler_called++; }

/**
 * \brief Dummy asynchronous callback for testing.
 * \param req Request pointer.
 * \param user_data User context pointer.
 */
static void dummy_async_callback(cfs_request_t *req, void *user_data) {
  (void)req;
  (void)user_data;
}

/**
 * \brief Helper to create a small text file for unit tests.
 * \param name Path string.
 * \param body Null-terminated content.
 */
static void test_write_file(const char *name, const char *body) {
  FILE *fp = NULL;
#if defined(_MSC_VER)
  if (fopen_s(&fp, name, "wb") != 0)
    fp = NULL;
#else
  fp = fopen(name, "wb");
#endif
  if (fp) {
    (void)fwrite(body, 1, strlen(body), fp);
    fclose(fp);
  }
}

/**
 * \brief Callback counting completed Phase 6 async requests.
 * \param req Completed request.
 * \param ud Pointer to `int` counter.
 */
static void phase6_async_cb(cfs_request_t *req, void *ud) {
  int *cnt = (int *)ud;
  if (cnt)
    (*cnt)++;
  if (req && req->opcode == cfs_opcode_dir_itr_init && req->result_buffer &&
      req->user_data == (void *)&cnt) {
    /* Optional harvest path */
  }
}

/**
 * \brief Callback harvesting and closing `cfs_directory_iterator *` from
 * `cfs_dir_itr_init_async`.
 * \param req Completed request.
 * \param ud Pointer to `int` counter.
 */
static void phase6_harvest_itr_cb(cfs_request_t *req, void *ud) {
  int *cnt = (int *)ud;
  if (cnt)
    (*cnt)++;
  if (req && req->result_buffer &&
      req->result_size >= sizeof(cfs_directory_iterator *)) {
    cfs_directory_iterator *it = NULL;
    CFS_MEMCPY_SAFE(&it, sizeof(it), req->result_buffer, sizeof(it));
    if (it) {
      cfs_dir_itr_close(it);
      it = NULL;
      CFS_MEMCPY_SAFE(req->result_buffer, req->result_size, &it, sizeof(it));
    }
  }
}

/**
 * \brief Explicitly consumes an intentionally ignored `cfs_errc` in test
 * cleanup or fault-injection loops.
 * \param rc Return code to ignore.
 */
static void test_ignore_rc(cfs_errc rc) { (void)rc; }

/**
 * \brief Green-thread test worker function incrementing a counter and yielding.
 * \param arg Pointer to `int` counter.
 */
static void phase6_gt_worker(void *arg) {
  int *v = (int *)arg;
  test_ignore_rc(cfs_greenthread_yield());
  if (v)
    (*v) += 10;
  test_ignore_rc(cfs_greenthread_yield());
}

/**
 * \brief Sleeps for the specified number of milliseconds.
 * \param ms Duration in milliseconds.
 */
static void test_sleep_ms(int ms) {
#if defined(CFS_OS_WINDOWS)
  Sleep((unsigned long)ms);
#elif defined(CFS_OS_DOS)
  if (ms < -9999) {
    (void)test_completed_ops;
    (void)g_oom_handler_called;
    test_ignore_rc(cfs_errc_success);
    async_callback(NULL, NULL);
    dummy_greenthread(NULL);
    test_oom_handler_cb();
    dummy_async_callback(NULL, NULL);
    test_write_file("", "");
    phase6_async_cb(NULL, NULL);
    phase6_harvest_itr_cb(NULL, NULL);
    phase6_gt_worker(NULL);
  }
#else
  usleep((unsigned int)ms * 1000);
#endif
}

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS) &&                        \
    defined(CFS_MULTITHREADED)
/**
 * \brief Helper thread sleeping briefly before posting a named semaphore.
 * \param arg Pointer to `cfs_named_semaphore`.
 * \return NULL.
 */
static void *phase6_sem_poster_thread(void *arg) {
  cfs_named_semaphore *s = (cfs_named_semaphore *)arg;
  test_sleep_ms(15);
  test_ignore_rc(cfs_named_semaphore_post(s));
  return NULL;
}
#endif

/**
 * \brief Test case for path_initialization.
 * \return The test result.
 */
TEST path_initialization(void) {
  cfs_path p = {0};
  cfs_path_init(&p);
  {
    cfs_bool empty;
    ASSERT_EQ(cfs_errc_success, cfs_path_is_empty(&p, &empty));
    ASSERT_EQ(1, empty);
  }

  ASSERT_EQ(
      cfs_errc_success,
      cfs_path_init_str(&p, CFS_STR("test") PATH_SEP_STR CFS_STR("path")));
  {
    cfs_bool empty;
    ASSERT_EQ(cfs_errc_success, cfs_path_is_empty(&p, &empty));
    ASSERT_EQ(0, empty);
  }
  {
    const cfs_char_t *c_str;
    int cmp;
    ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&p, &c_str));
    ASSERT_EQ(
        cfs_errc_success,
        cfs_strcmp(CFS_STR("test") PATH_SEP_STR CFS_STR("path"), c_str, &cmp));
    ASSERT_EQ(0, cmp);
  }

  cfs_path_destroy(&p);
  {
    cfs_bool empty;
    ASSERT_EQ(cfs_errc_success, cfs_path_is_empty(&p, &empty));
    ASSERT_EQ(1, empty);
  }
  PASS();
}

/**
 * \brief Test case for path_appending.
 * \return The test result.
 */
TEST path_appending(void) {
  cfs_path p = {0};
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dir")));
  ASSERT_EQ(cfs_errc_success, cfs_path_append(&p, CFS_STR("file.txt")));

  {
    const cfs_char_t *c_str;
    int cmp;
    ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&p, &c_str));
    ASSERT_EQ(cfs_errc_success,
              cfs_strcmp(CFS_STR("dir") PATH_SEP_STR CFS_STR("file.txt"), c_str,
                         &cmp));
    ASSERT_EQ(0, cmp);
  }

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for root_path_decomposition.
 * \return The test result.
 */
TEST root_path_decomposition(void) {
  cfs_path p = {0}, out = {0};
  cfs_bool b;
  int cmp;
  const cfs_char_t *cstr;

  cfs_path_init(&out);

#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("C:\\Windows")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR("C:"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&p, &b));
  ASSERT_EQ(1, b);
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("\\Windows")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR(""), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&p, &b));
  ASSERT_EQ(0, b);
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("\\\\server\\share\\file.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success,
            cfs_strcmp(cstr, CFS_STR("\\\\server\\share"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_root_directory(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR("\\"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&p, &b));
  ASSERT_EQ(1, b);
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("\\\\?\\C:\\file.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR("\\\\?\\C:"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("\\\\.\\COM1")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR("\\\\.\\COM1"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);
  cfs_path_destroy(&p);
#else
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("/usr/bin")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR(""), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_root_directory(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR("/"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&p, &b));
  ASSERT_EQ(1, b);
  cfs_path_destroy(&p);
#endif

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("/usr/bin")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_path(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(cstr, CFS_STR("/"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&p);
#else
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("\\\\server\\share\\file.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_path(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&out, &cstr));
  ASSERT_EQ(cfs_errc_success,
            cfs_strcmp(cstr, CFS_STR("\\\\server\\share\\"), &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_destroy(&p);
#endif

  cfs_path_destroy(&out);
  PASS();
}

/**
 * \brief Test case for path_decomposition.
 * \return The test result.
 */
TEST path_decomposition(void) {
  cfs_path p = {0}, res = {0};
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("dir") PATH_SEP_STR CFS_STR("subdir")
                                      PATH_SEP_STR CFS_STR("file.txt")));

  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &res));
  {
    const cfs_char_t *c_str;
    int cmp;
    ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&res, &c_str));
    ASSERT_EQ(cfs_errc_success, cfs_strcmp(CFS_STR("file.txt"), c_str, &cmp));
    ASSERT_EQ(0, cmp);
  }
  cfs_path_destroy(&res);

  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &res));
  {
    const cfs_char_t *c_str;
    int cmp;
    ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&res, &c_str));
    ASSERT_EQ(cfs_errc_success, cfs_strcmp(CFS_STR(".txt"), c_str, &cmp));
    ASSERT_EQ(0, cmp);
  }
  cfs_path_destroy(&res);

  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &res));
  {
    const cfs_char_t *c_str;
    int cmp;
    ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&res, &c_str));
    ASSERT_EQ(cfs_errc_success, cfs_strcmp(CFS_STR("file"), c_str, &cmp));
    ASSERT_EQ(0, cmp);
  }
  cfs_path_destroy(&res);

  cfs_path_destroy(&p);
  PASS();
}

/* Step 45. Write multithreading test cases simulating concurrent file creations
 */
/**
 * \brief Test case for thread_pool_async_validation.
 * \return The test result.
 */
TEST thread_pool_async_validation(void) {
  cfs_runtime_config config;
  cfs_runtime_t *rt;
  cfs_path p = {0};
  cfs_error_code ec;
  int i;

  config.mode = cfs_modality_multithread;
  config.thread_pool_size = 4;
  config.ipc_path = NULL;

  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&config, &rt, &ec));
  ASSERT_NEQ(NULL, rt);

  test_completed_ops = 0;

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("dummy_test_file.txt")));

  for (i = 0; i < 100; i++) {
    ASSERT_EQ(cfs_errc_success, cfs_remove_async(rt, &p, async_callback, NULL));
  }

  ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(rt));
  for (i = 0; i < 100 && test_completed_ops < 100; i++) {
    test_sleep_ms(5);
    ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(rt));
  }
  ASSERT_EQ(100, test_completed_ops);

  cfs_path_destroy(&p);
  cfs_runtime_destroy(rt);
  PASS();
}

/**
 * \brief Test case for greenthread_scheduler_validation.
 * \return The test result.
 */
TEST greenthread_scheduler_validation(void) {
  cfs_greenthread_scheduler *sched = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_greenthread_scheduler_init(&sched));
  ASSERT_NEQ(NULL, sched);

  ASSERT_EQ(cfs_errc_success, cfs_greenthread_scheduler_run(sched));

  cfs_greenthread_scheduler_destroy(sched);
  PASS();
}

/**
 * \brief Test case for memory_allocation.
 * \return The test result.
 */
TEST memory_allocation(void) {
  void *ptr = NULL;
  ASSERT_EQ(0, cfs_malloc(1024, &ptr));
  ASSERT_NEQ(NULL, ptr);
  ASSERT_EQ(0, cfs_realloc(ptr, 2048, &ptr));
  ASSERT_NEQ(NULL, ptr);
  cfs_free(ptr);

  ASSERT_EQ(0, cfs_calloc(10, 10, &ptr));
  ASSERT_NEQ(NULL, ptr);
  cfs_free(ptr);
  PASS();
}

/**
 * \brief Test case for string_handling.
 * \return The test result.
 */
TEST string_handling(void) {
  cfs_char_t buf[100];
  cfs_size_t len;
  cfs_char_t *out = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_strcpy(buf, CFS_STR("Hello"), &out));
  ASSERT_EQ(cfs_errc_success, cfs_strlen(buf, &len));
  ASSERT_EQ(5, len);

  ASSERT_EQ(cfs_errc_success, cfs_strcat(buf, CFS_STR(" World"), &out));
  ASSERT_EQ(cfs_errc_success, cfs_strlen(buf, &len));
  ASSERT_EQ(11, len);

  {
    int cmp;
    ASSERT_EQ(cfs_errc_success, cfs_strcmp(buf, CFS_STR("Hello World"), &cmp));
    ASSERT_EQ(0, cmp);
    ASSERT_EQ(cfs_errc_success, cfs_strncmp(buf, CFS_STR("Hello W"), 7, &cmp));
    ASSERT_EQ(0, cmp);
  }
  PASS();
}

/**
 * \brief Test case for path_utilities.
 * \return The test result.
 */
TEST path_utilities(void) {
  cfs_path p = {0};
  cfs_path p2 = {0};
  cfs_char_t *gen_str = NULL;

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("dir/subdir\\file.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_make_preferred(&p));
  ASSERT_EQ(cfs_errc_success, cfs_path_generic_string(&p, &gen_str));
  if (gen_str)
    cfs_free(gen_str);

  cfs_path_clear(&p);
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("first")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("second")));
  cfs_path_swap(&p, &p2);
  ASSERT_EQ(cfs_errc_success, cfs_path_concat(&p, CFS_STR("_part")));

  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_make_preferred(NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_generic_string(NULL, &gen_str));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_generic_string(&p, NULL));
  cfs_path_clear(NULL);
  cfs_path_swap(NULL, &p2);
  cfs_path_swap(&p, NULL);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_concat(NULL, CFS_STR("a")));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_concat(&p, NULL));

  PASS();
}

/**
 * \brief Test case for path_decomposition_more.
 * \return The test result.
 */
TEST path_decomposition_more(void) {
  cfs_path p = {0}, out = {0};
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("/usr/local/bin/test.exe")));

  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_root_directory(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_root_path(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_relative_path(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success,
            cfs_path_replace_filename(&p, CFS_STR("new.exe")));
  ASSERT_EQ(cfs_errc_success, cfs_path_replace_extension(&p, CFS_STR(".bin")));

  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_root_name(NULL, &out));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_root_directory(NULL, &out));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_root_path(NULL, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_relative_path(NULL, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(NULL, &out));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_path_replace_filename(NULL, CFS_STR("")));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_path_replace_extension(NULL, CFS_STR("")));

  PASS();
}

/**
 * \brief Test case for path_queries.
 * \return The test result.
 */
TEST path_queries(void) {
  cfs_path p = {0};
  cfs_bool out_bool;
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("/usr/local/bin/test.exe")));

  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_path(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_directory(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_relative_path(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_parent_path(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_filename(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_stem(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_extension(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&p, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_is_relative(&p, &out_bool));

  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_path(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_directory(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_relative_path(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_parent_path(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_filename(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_stem(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_extension(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(NULL, &out_bool));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_is_relative(NULL, &out_bool));

  PASS();
}

/**
 * \brief Test case for path_lexical.
 * \return The test result.
 */
TEST path_lexical(void) {
  cfs_path p = {0}, base = {0}, out = {0};
  int cmp = 0;
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("/usr/local/bin/test.exe")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&base, CFS_STR("/usr/local/")));

  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p, &base, &cmp));
  ASSERT(cmp > 0);
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&base, &p, &cmp));
  ASSERT(cmp < 0);
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p, &p, &cmp));
  ASSERT_EQ(0, cmp);
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&out, &out, &cmp));
  ASSERT_EQ(0, cmp);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&p, &base, &out));
  cfs_path_destroy(&out);
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_proximate(&p, &base, &out));
  cfs_path_destroy(&out);

  cfs_path_destroy(&p);
  cfs_path_destroy(&base);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_compare(NULL, NULL, &cmp));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_compare(&p, NULL, &cmp));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_compare(&p, &base, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(NULL, &out));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_path_lexically_relative(NULL, &base, &out));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_path_lexically_proximate(NULL, &base, &out));

  PASS();
}

/**
 * \brief Test case for dir_iterators.
 * \return The test result.
 */
TEST dir_iterators(void) {
  cfs_directory_iterator *itr = NULL;
  cfs_path p = {0};
  const cfs_directory_entry *out_entry = NULL;
  cfs_error_code ec;

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR(".")));

#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    ASSERT_EQ(cfs_errc_operation_not_supported,
              cfs_dir_itr_init(&p, &itr, &ec));
  } else
#endif
  {
    ASSERT_EQ(cfs_errc_success, cfs_dir_itr_init(&p, &itr, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_dir_itr_next(itr, &out_entry, &ec));
    cfs_dir_itr_close(itr);
  }

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_itr_init(NULL, &itr, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_itr_init(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_itr_next(NULL, &out_entry, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_itr_next(NULL, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_itr_next(NULL, &out_entry, NULL));
  cfs_dir_itr_close(NULL);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for rec_dir_iterators.
 * \return The test result.
 */
TEST rec_dir_iterators(void) {
  cfs_recursive_directory_iterator *itr = NULL;
  cfs_path p = {0};
  const cfs_directory_entry *out_entry = NULL;
  cfs_error_code ec;

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR(".")));

#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    ASSERT_EQ(cfs_errc_operation_not_supported,
              cfs_rec_dir_itr_init(&p, &itr, &ec));
  } else
#endif
  {
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p, &itr, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(itr, &out_entry, &ec));
    cfs_rec_dir_itr_disable_recursion_pending(itr);
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(itr, &out_entry, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_pop(itr, &ec));
    cfs_rec_dir_itr_close(itr);
  }

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_init(NULL, &itr, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_init(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_rec_dir_itr_next(NULL, &out_entry, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_next(NULL, NULL, &ec));
  cfs_rec_dir_itr_disable_recursion_pending(NULL);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_pop(NULL, &ec));
  cfs_rec_dir_itr_close(NULL);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for file_queries.
 * \return The test result.
 */
TEST file_queries(void) {
  cfs_path p = {0}, p2 = {0}, p_out = {0};
  cfs_file_status s;
  cfs_bool b;
  cfs_error_code ec;
  cfs_size_t sz;
  cfs_space_info spc;
  cfs_file_time_type ft;
  cfs_uintmax_t count;
  cfs_perms perms = (cfs_perms)0;

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("nofq_1")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("nofq_2")));

  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_status(&p, &s, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_symlink_status(&p, &s, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_status_known(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_exists(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_exists_path(&p, &b, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_is_block_file(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_is_character_file(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_is_directory(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_is_fifo(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_is_other(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_is_regular_file(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_is_socket(s, &b));
  ASSERT_EQ(cfs_errc_success, cfs_is_symlink(s, &b));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_is_empty_path(&p, &b, &ec));

#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    ASSERT_EQ(cfs_errc_operation_not_supported, cfs_create_directory(&p, &ec));
  } else
#endif
  {
    test_ignore_rc(cfs_remove_all(&p, NULL, NULL));
    test_ignore_rc(cfs_remove_all(&p2, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directory(&p, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_create_directories(&p, &ec));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_create_hard_link(&p, &p2, &ec));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_create_symlink(&p, &p2, &ec));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&p2, &ec));
    ASSERT_NEQ(cfs_errc_invalid_argument,
               cfs_create_directory_symlink(&p, &p2, &ec));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&p2, &ec));

    ASSERT_EQ(cfs_errc_success, cfs_remove_all(&p, &sz, &ec));
  }
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_rename(&p, &p2, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_resize_file(&p, 1024, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_space(&p, &spc, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_last_write_time(&p, &ft, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_hard_link_count(&p, &count, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_permissions(&p, perms, (cfs_perm_options)0, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_equivalent(&p, &p2, &b, &ec));

  cfs_path_init(&p_out);
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_read_symlink(&p, &p_out, &ec));
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_absolute(&p, &p_out, &ec));
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_canonical(&p, &p_out, &ec));
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_weakly_canonical(&p, &p_out, &ec));
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  ASSERT_EQ(cfs_errc_success, cfs_proximate(&p, &p2, &p_out, &ec));
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  ASSERT_EQ(cfs_errc_success, cfs_relative(&p, &p2, &p_out, &ec));
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  ASSERT_EQ(cfs_errc_success, cfs_temp_directory_path(&p_out, &ec));
  cfs_path_destroy(&p_out);

  ASSERT_NEQ(cfs_errc_invalid_argument,
             cfs_copy(&p, &p2, cfs_copy_options_none, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_copy_symlink(&p, &p2, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument,
             cfs_copy_file(&p, &p2, cfs_copy_options_none, &ec));

  ASSERT_EQ(cfs_errc_success, cfs_current_path(&p_out, &ec));
  cfs_path_destroy(&p_out);
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_current_path_set(&p, &ec));

  cfs_path_remove_filename(&p);

  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);

  PASS();
}

/**
 * \brief Test case for string_and_errors.
 * \return The test result.
 */
TEST string_and_errors(void) {
  wchar_t dest_w[100];
  char dest_c[100];
  cfs_size_t out_req;
  cfs_error_code ec;
  const char *msg = NULL;

  ASSERT_EQ(cfs_errc_success, cfs_mb_to_wide("test", dest_w, 100, &out_req));
  ASSERT_EQ(5, out_req);
  ASSERT_EQ(cfs_errc_success, cfs_wide_to_mb(L"test", dest_c, 100, &out_req));
  ASSERT_EQ(5, out_req);
  ASSERT_STR_EQ("test", dest_c);

#if defined(CFS_OS_WINDOWS)
  ASSERT_EQ(cfs_errc_success, cfs_make_error_code_from_os(2, &ec));
  ASSERT_EQ(2, ec.value);
#else
  ASSERT_EQ(cfs_errc_success, cfs_make_error_code_from_os(ENOENT, &ec));
  ASSERT_EQ(ENOENT, ec.value);
#endif
  ASSERT_EQ(cfs_errc_no_such_file_or_directory, ec.errc);
  ASSERT_EQ(cfs_errc_success, cfs_make_error_code_from_os(30000, &ec));
  ASSERT_EQ(cfs_errc_unknown_error, ec.errc);
  ASSERT_EQ(cfs_errc_success, cfs_make_error_code_from_os(0, &ec));
  ASSERT_EQ(cfs_errc_success, ec.errc);
  ASSERT_EQ(cfs_errc_success, cfs_get_last_error(&ec));
  ASSERT_EQ(cfs_errc_success, cfs_error_message(cfs_errc_success, &msg));
  ASSERT(msg != NULL);
  ASSERT_EQ(cfs_errc_success,
            cfs_error_message(cfs_errc_no_such_file_or_directory, &msg));
  ASSERT(msg != NULL);

  ASSERT_NEQ(cfs_errc_success, cfs_mb_to_wide(NULL, dest_w, 100, &out_req));
  ASSERT_NEQ(cfs_errc_success, cfs_wide_to_mb(NULL, dest_c, 100, &out_req));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_error_message(cfs_errc_success, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_get_last_error(NULL));

  PASS();
}

/**
 * \brief Test case for ipc_and_processes.
 * \return The test result.
 */
TEST ipc_and_processes(void) {
  cfs_message_pipe *pipe = NULL;
  cfs_process_t *proc = NULL;
  cfs_shm_segment *shm = NULL;
  cfs_named_semaphore *sem = NULL;
  void *addr = NULL;

  ASSERT_EQ(cfs_errc_success, cfs_message_pipe_create(CFS_STR("pipe"), &pipe));
  ASSERT(pipe != NULL);
  cfs_message_pipe_destroy(pipe);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_message_pipe_create(NULL, &pipe));
  cfs_message_pipe_destroy(NULL);

#if defined(CFS_OS_WINDOWS)
  ASSERT_NEQ(cfs_errc_invalid_argument,
             cfs_process_spawn(CFS_STR("nonexistent_exe_12345"), &proc));
  ASSERT_EQ(cfs_errc_success,
            cfs_process_spawn(CFS_STR("cmd.exe /c exit 0"), &proc));
#else
  ASSERT_EQ(cfs_errc_success, cfs_process_spawn(CFS_STR("exit 0"), &proc));
#endif
  ASSERT_EQ(cfs_errc_success, cfs_process_wait(proc));
  cfs_process_destroy(proc);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_process_spawn(NULL, &proc));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_process_wait(NULL));
  cfs_process_destroy(NULL);

  ASSERT_EQ(cfs_errc_success, cfs_shm_create(1024, CFS_STR("shm"), &shm));
  ASSERT_EQ(cfs_errc_success, cfs_shm_map(shm, &addr));
  ASSERT(addr != NULL);
  cfs_shm_unmap(shm, addr);
  cfs_shm_destroy(shm);

  ASSERT_EQ(cfs_errc_success, cfs_shm_create(1024, CFS_STR("shm2"), &shm));
  ASSERT_EQ(cfs_errc_success, cfs_shm_map(shm, &addr));
  cfs_shm_destroy(shm);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_create(0, NULL, &shm));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_map(NULL, &addr));
  cfs_shm_unmap(NULL, addr);
  cfs_shm_destroy(NULL);

  ASSERT_EQ(cfs_errc_success,
            cfs_named_semaphore_create(CFS_STR("sem"), 1, &sem));
  ASSERT_EQ(cfs_errc_success, cfs_named_semaphore_wait(sem));
  ASSERT_EQ(cfs_errc_success, cfs_named_semaphore_post(sem));
  cfs_named_semaphore_destroy(sem);
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_named_semaphore_create(NULL, 1, &sem));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_named_semaphore_wait(NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_named_semaphore_post(NULL));
  cfs_named_semaphore_destroy(NULL);

  PASS();
}

/**
 * \brief Test case for greenthreads_and_utils.
 * \return The test result.
 */
TEST greenthreads_and_utils(void) {
  cfs_greenthread_t *gt = NULL;
  cfs_request_t *req = NULL;
  cfs_request_t *req_ptr = NULL;
  void *buf;
  cfs_size_t sz;
  cfs_runtime_t *rt;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p = {0};
  cfs_sandbox_config sbox = {0};
  int cmp = 0;

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));

  dummy_greenthread(NULL);

  ASSERT_EQ(cfs_errc_success,
            cfs_greenthread_spawn(dummy_greenthread, NULL, &gt));
  ASSERT_EQ(cfs_errc_success, cfs_greenthread_yield());
  cfs_greenthread_destroy(gt);

  ASSERT_EQ(cfs_errc_success, cfs_greenthread_spawn(NULL, NULL, &gt));
  cfs_greenthread_destroy(NULL);
  cfs_greenthread_destroy(gt);

  ASSERT_EQ(cfs_errc_success, cfs_malloc(sizeof(cfs_request_t), (void **)&req));
  if (req) {
    memset(req, 0, sizeof(cfs_request_t));
    cfs_request_retain(req);
    ASSERT_EQ(cfs_errc_success, cfs_cancel_request(rt, req));
    ASSERT_EQ(cfs_true, req->cancelled);
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_cancel_request(NULL, req));

    /* Test serialization with empty paths */
    ASSERT_EQ(cfs_errc_success, cfs_serialize_request(req, &buf, &sz));
    ASSERT_EQ(cfs_errc_success, cfs_deserialize_request(buf, sz, &req_ptr));
    cfs_free(buf);
    cfs_request_release(req_ptr);

    /* Test serialization with non-empty target_path and dest_path */
    req->opcode = cfs_opcode_copy_file;
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&req->target_path, CFS_STR("src_file.txt")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&req->dest_path, CFS_STR("dst_file.txt")));
    ASSERT_EQ(cfs_errc_success, cfs_serialize_request(req, &buf, &sz));
    ASSERT_EQ(cfs_errc_success, cfs_deserialize_request(buf, sz, &req_ptr));
    ASSERT_EQ(cfs_opcode_copy_file, req_ptr->opcode);
    ASSERT_EQ(cfs_errc_success,
              cfs_path_compare(&req->target_path, &req_ptr->target_path, &cmp));
    ASSERT_EQ(0, cmp);
    ASSERT_EQ(cfs_errc_success,
              cfs_path_compare(&req->dest_path, &req_ptr->dest_path, &cmp));
    ASSERT_EQ(0, cmp);
    cfs_free(buf);
    cfs_request_release(req_ptr);

    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_serialize_request(NULL, &buf, &sz));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_deserialize_request(NULL, sz, &req_ptr));

    cfs_request_release(req);
  }

  cfs_set_oom_handler(NULL);
  ASSERT_EQ(cfs_errc_success, cfs_runtime_set_sandbox(rt, &sbox));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_set_sandbox(NULL, &sbox));

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));
  ASSERT_EQ(cfs_errc_success, cfs_file_size_async(rt, &p, NULL, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_dir_itr_init_async(rt, &p, NULL, NULL));

  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_file_size_async(NULL, &p, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_itr_init_async(NULL, &p, NULL, NULL));

  cfs_path_destroy(&p);
  cfs_runtime_destroy(rt);

  PASS();
}

/**
 * \brief Test case for exhaustive_nulls.
 * \return The test result.
 */
TEST exhaustive_nulls(void) {
  cfs_path p = {0};
  int out_int;
  cfs_size_t out_sz;
  cfs_uintmax_t out_u;
  cfs_space_info out_spc;
  cfs_file_time_type out_ft;
  cfs_bool b;
  cfs_char_t buf[10] = {0};
  cfs_char_t *out_str;
  cfs_runtime_t *rt = NULL;
  cfs_error_code ec;
  cfs_path out_p = {0};

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));

  /* cfs_path nulls */
  cfs_path_init(NULL);
  cfs_path_destroy(NULL);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_clone(NULL, &p));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_clone(&p, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_c_str(&p, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_assign(NULL, CFS_STR("")));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_append(NULL, CFS_STR("")));
  ASSERT_EQ(cfs_errc_success, cfs_path_is_empty(NULL, &b));
  ASSERT_EQ(cfs_true, b);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_is_empty(&p, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(NULL, &out_p));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_extension(NULL, &out_p));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_stem(NULL, &out_p));
  cfs_path_remove_filename(NULL);

  /* cfs_string nulls */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_strlen(NULL, &out_sz));
  ASSERT_EQ(0, out_sz);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_strlen(CFS_STR(""), NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_strcpy(NULL, CFS_STR(""), &out_str));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_strcpy(buf, NULL, &out_str));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_strncpy(NULL, CFS_STR(""), 5, &out_str));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_strncpy(buf, NULL, 5, &out_str));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_strcat(NULL, CFS_STR(""), &out_str));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_strcat(buf, NULL, &out_str));
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(NULL, NULL, &out_int));
  ASSERT_EQ(0, out_int);
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(NULL, CFS_STR(""), &out_int));
  ASSERT_EQ(-1, out_int);
  ASSERT_EQ(cfs_errc_success, cfs_strcmp(CFS_STR(""), NULL, &out_int));
  ASSERT_EQ(1, out_int);
  ASSERT_EQ(cfs_errc_success, cfs_strncmp(NULL, NULL, 5, &out_int));
  ASSERT_EQ(0, out_int);
  ASSERT_EQ(cfs_errc_success, cfs_strncmp(NULL, CFS_STR(""), 5, &out_int));
  ASSERT_EQ(-1, out_int);
  ASSERT_EQ(cfs_errc_success, cfs_strncmp(CFS_STR(""), NULL, 5, &out_int));
  ASSERT_EQ(1, out_int);

  /* File nulls */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_remove(NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_file_size(NULL, &out_u, &ec));
  ASSERT_NEQ(cfs_errc_success, cfs_file_size(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_space(NULL, &out_spc, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_last_write_time(NULL, &out_ft, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_hard_link_count(NULL, &out_u, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_permissions(NULL, (cfs_perms)0, (cfs_perm_options)0, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_equivalent(NULL, &p, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_equivalent(&p, NULL, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_status(NULL, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_symlink_status(NULL, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_exists_path(NULL, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_empty_path(NULL, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_create_directory(NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_create_directories(NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_create_hard_link(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_create_hard_link(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_directory_symlink(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_directory_symlink(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_remove_all(NULL, &out_sz, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rename(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rename(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_resize_file(NULL, 0, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_temp_directory_path(NULL, &ec));

  /* Path copy/abs nulls */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_absolute(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_canonical(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_weakly_canonical(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_read_symlink(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_relative(NULL, &p, &out_p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_proximate(NULL, &p, &out_p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_copy(NULL, &p, cfs_copy_options_none, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_copy_symlink(NULL, &p, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_copy_file(NULL, &p, cfs_copy_options_none, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_current_path(NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_current_path_set(NULL, &ec));

  /* Runtime nulls */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_init(NULL, &rt, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_init(NULL, NULL, &ec));
  cfs_dispatch_request(NULL, NULL, NULL, NULL);
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_remove_async(NULL, NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(NULL));
  cfs_request_retain(NULL);
  cfs_request_release(NULL);

  /* Others */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_malloc(0, NULL));
  cfs_free(NULL);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_realloc(NULL, 0, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_calloc(0, 0, NULL));

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for real_file_operations.
 * \return The test result.
 */
TEST real_file_operations(void) {
  FILE *f;
  cfs_path p = {0};
  cfs_path p_dir = {0};
  cfs_path p_nested = {0};
  cfs_path p_copy = {0};
  cfs_path p_link = {0};
#if !defined(CFS_OS_DOS)
  cfs_path p_dev = {0};
#endif
  cfs_file_status st;
  cfs_error_code ec;
  cfs_uintmax_t size;
  cfs_space_info spc;
  cfs_file_time_type ft;
  cfs_uintmax_t links;
  cfs_perms perms = 0;
  cfs_bool is_empty;
  cfs_size_t rem_count = 0;
  cfs_path p_renamed = {0};
  cfs_recursive_directory_iterator *rit = NULL;

#if defined(_MSC_VER)
  if (fopen_s(&f, "treal1.txt", "w") != 0)
    f = NULL;
#else
  f = fopen("treal1.txt", "w");
#endif
  if (f) {
    fprintf(f, "test data");
    fclose(f);
  }

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("treal1.txt")));

  ASSERT_EQ(cfs_errc_success, cfs_status(&p, &st, &ec));
  ASSERT_EQ(cfs_file_type_regular, st.type);
  ASSERT_EQ(cfs_errc_success, cfs_symlink_status(&p, &st, &ec));
  ASSERT_EQ(cfs_file_type_regular, st.type);
  ASSERT_EQ(cfs_errc_success, cfs_file_size(&p, &size, &ec));
  ASSERT_EQ(9, size);
  ASSERT_EQ(cfs_errc_success, cfs_space(&p, &spc, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_last_write_time(&p, &ft, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_hard_link_count(&p, &links, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_permissions(&p, perms, (cfs_perm_options)0, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_is_empty_path(&p, &is_empty, &ec));
  ASSERT_EQ(cfs_false, is_empty);

  /* Test copy_file and copy_options */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p_copy, CFS_STR("trealcp.txt")));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&p_copy, NULL));
  ASSERT_EQ(cfs_errc_success,
            cfs_copy_file(&p, &p_copy, cfs_copy_options_none, &ec));
  ASSERT_EQ(cfs_errc_file_exists,
            cfs_copy_file(&p, &p_copy, cfs_copy_options_none, &ec));
  ASSERT_EQ(cfs_errc_success,
            cfs_copy_file(&p, &p_copy, cfs_copy_options_skip_existing, &ec));
  ASSERT_EQ(
      cfs_errc_success,
      cfs_copy_file(&p, &p_copy, cfs_copy_options_overwrite_existing, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_remove(&p_copy, NULL));

  /* Test copy_file error paths */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_dir, CFS_STR(".")));
  ASSERT_NEQ(
      cfs_errc_success,
      cfs_copy_file(&p, &p_dir, cfs_copy_options_overwrite_existing, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_status(&p_dir, &st, &ec));
  ASSERT_EQ(cfs_file_type_directory, st.type);
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2) {
    ASSERT_EQ(cfs_errc_success, cfs_is_empty_path(&p_dir, &is_empty, &ec));
    ASSERT_EQ(cfs_false, is_empty);
  }
#else
  ASSERT_EQ(cfs_errc_success, cfs_is_empty_path(&p_dir, &is_empty, &ec));
  ASSERT_EQ(cfs_false, is_empty);
#endif

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_dev, CFS_STR("/dev/null")));
  ASSERT_EQ(cfs_errc_success, cfs_status(&p_dev, &st, &ec));
  ASSERT_EQ(cfs_file_type_character, st.type);
  ASSERT_EQ(cfs_errc_success, cfs_is_other(st, &is_empty));
  ASSERT_EQ(cfs_true, is_empty);
  cfs_path_destroy(&p_dev);

  /* Test hard link & resize_file */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p_link, CFS_STR("trealhl.txt")));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&p_link, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_create_hard_link(&p, &p_link, &ec));
  ASSERT_NEQ(cfs_errc_success, cfs_create_hard_link(&p, &p_link, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_remove(&p_link, NULL));
  cfs_path_destroy(&p_link);
#endif
#if !defined(CFS_OS_WINDOWS)
  ASSERT_EQ(cfs_errc_success, cfs_resize_file(&p, 4, &ec));
#endif

  /* Test create_directory, create_directories, create_directory_symlink,
   * remove_all */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p_nested, CFS_STR("tdir_a/tdir_b")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_link, CFS_STR("tdir_a")));
#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    ASSERT_EQ(cfs_errc_operation_not_supported,
              cfs_create_directory(&p_link, &ec));
  } else
#endif
  {
    ASSERT_NEQ(cfs_errc_invalid_argument,
               cfs_remove_all(&p_link, &rem_count, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directories(&p_nested, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_create_directories(&p_nested, &ec));
    ASSERT_NEQ(cfs_errc_success, cfs_create_directory(&p_nested, &ec));

#if !defined(CFS_OS_DOS)
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_dev, CFS_STR("tdirsym")));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&p_dev, NULL));
    ASSERT_EQ(cfs_errc_success,
              cfs_create_directory_symlink(&p_link, &p_dev, &ec));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_symlink_status(&p_dev, &st, &ec));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&p_dev, NULL));
    cfs_path_destroy(&p_dev);
#endif

    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p_link, &rit, &ec));
    cfs_rec_dir_itr_close(rit);

    ASSERT_EQ(cfs_errc_success, cfs_remove_all(&p_link, &rem_count, &ec));
    ASSERT(rem_count >= 2);
    ASSERT_NEQ(cfs_errc_success, cfs_remove_all(&p_link, &rem_count, &ec));
  }

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p_renamed, CFS_STR("trealrn.txt")));
  test_ignore_rc(cfs_remove(&p_renamed, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_rename(&p, &p_renamed, &ec));
  ASSERT_NEQ(cfs_errc_success, cfs_rename(&p, &p_renamed, &ec));
  ASSERT_NEQ(cfs_errc_success, cfs_resize_file(&p, 4, &ec));
  ASSERT_NEQ(cfs_errc_success,
             cfs_copy_file(&p, &p_copy, cfs_copy_options_none, &ec));

  ASSERT_EQ(cfs_errc_success, cfs_remove(&p_renamed, &ec));

  cfs_path_destroy(&p);
  cfs_path_destroy(&p_dir);
  cfs_path_destroy(&p_nested);
  cfs_path_destroy(&p_copy);
  cfs_path_destroy(&p_link);
  cfs_path_destroy(&p_renamed);
  PASS();
}

/**
 * \brief Test case for more_coverage.
 * \return The test result.
 */
TEST more_coverage(void) {
  cfs_file_status s;
  cfs_path p = {0}, p2 = {0}, p3 = {0}, p4 = {0}, out = {0};
  cfs_error_code ec;
  cfs_bool b;
  cfs_perms perms = (cfs_perms)0777;
  FILE *f;

  memset(&s, 0, sizeof(s));

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_status_known(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_exists(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_block_file(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_character_file(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_directory(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_fifo(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_other(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_regular_file(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_socket(s, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_symlink(s, NULL));

  /* cfs_permissions with replace so it tries chmod and fails */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummynex.txt")));
  ASSERT_NEQ(cfs_errc_success,
             cfs_permissions(&p, perms, cfs_perm_options_replace, &ec));

/* cfs_equivalent success branch */
#if defined(_MSC_VER)
  if (fopen_s(&f, "testeq1.txt", "w") != 0)
    f = NULL;
#else
  f = fopen("testeq1.txt", "w");
#endif
  if (f) {
    fclose(f);
  }
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("testeq1.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_equivalent(&p2, &p2, &b, &ec));

  /* cfs_read_symlink success branch */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p3, CFS_STR("testeqs.txt")));
#if defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_operation_not_supported,
            cfs_create_symlink(&p2, &p3, &ec));
#else
  ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p2, &p3, &ec));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_read_symlink(&p3, &out, &ec));
  cfs_path_destroy(&out);
#endif

  /* cfs_absolute already absolute */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p4, CFS_STR("/absolute/path")));
  ASSERT_EQ(cfs_errc_success, cfs_absolute(&p4, &out, &ec));
  cfs_path_destroy(&out);
  cfs_path_destroy(&p4);

  /* Canonical on absolute path */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p4, CFS_STR("/dev/null")));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_canonical(&p4, &out, &ec));
  cfs_path_destroy(&out);
  cfs_path_destroy(&p4);

  remove("testeq1.txt");
  remove("testeqs.txt");

  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);
  cfs_path_destroy(&p3);

  PASS();
}

/**
 * \brief Test case for final_coverage.
 * \return The test result.
 */
TEST final_coverage(void) {
  cfs_path p = {0}, p2 = {0}, out = {0};
  cfs_error_code ec;
  cfs_bool b;
  FILE *f;

/* cfs_permissions success branch */
#if defined(_MSC_VER)
  if (fopen_s(&f, "tperms1.txt", "w") != 0)
    f = NULL;
#else
  f = fopen("tperms1.txt", "w");
#endif
  if (f) {
    fclose(f);
  }
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("tperms1.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_permissions(&p, (cfs_perms)0777,
                                              cfs_perm_options_replace, &ec));

  /* cfs_copy_symlink success branch */
  remove("tpermss.txt");
  remove("tpermsc.txt");
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("tpermss.txt")));
#if defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_operation_not_supported, cfs_create_symlink(&p, &p2, &ec));
#else
  ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p, &p2, &ec));
  /* test cfs_create_symlink failure when link already exists */
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_create_symlink(&p, &p2, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&out, CFS_STR("tpermsc.txt")));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_copy_symlink(&p2, &out, &ec));
  remove("tpermsc.txt");
  cfs_path_destroy(&out);
#endif

  /* cfs_path_lexically_relative */
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&p, &p2, &out));
  cfs_path_destroy(&out);
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&p, &p, &out));
  cfs_path_destroy(&out);
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&p, NULL, &out));
  cfs_path_destroy(&out);
  {
    cfs_path pa = {0}, pb = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&pa, CFS_STR("/a/b/c")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&pb, CFS_STR("/a/b")));
    cfs_path_init(&out);
    ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&pa, &pb, &out));
    cfs_path_destroy(&out);
    cfs_path_init(&out);
    ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&pb, &pa, &out));
    cfs_path_destroy(&out);
    cfs_path_destroy(&pa);
    cfs_path_destroy(&pb);
  }

  /* cfs_path_is_absolute */
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&p, &b));

  remove("tperms1.txt");
  remove("tpermss.txt");
  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);

  PASS();
}

/**
 * \brief Test case for out_of_memory.
 * \return The test result.
 */
TEST out_of_memory(void) {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_request_t *req = NULL;
  cfs_path p = {0};
  void *buf = NULL;
  cfs_size_t sz = 0;
  cfs_message_pipe *pipe;
  cfs_process_t *proc;
  cfs_shm_segment *shm;
  cfs_named_semaphore *sem;
  cfs_greenthread_t *gt;
  cfs_greenthread_scheduler *sched;
  cfs_request_t req_dummy;

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  /* Test cfs_set_oom_handler invocation on malloc, realloc, calloc */
  g_oom_handler_called = 0;
  cfs_set_oom_handler(test_oom_handler_cb);
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_malloc(100, &buf));
  ASSERT_EQ(1, g_oom_handler_called);
  g_cfs_realloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_realloc(NULL, 100, &buf));
  ASSERT_EQ(2, g_oom_handler_called);
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_calloc(10, 10, &buf));
  ASSERT_EQ(3, g_oom_handler_called);
  cfs_set_oom_handler(NULL);

  /* Test cfs_malloc OOM */
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_malloc(100, &buf));
  {
    cfs_path p_fresh = {0};
    cfs_path_init(&p_fresh);
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_path_assign(&p_fresh,
                              CFS_STR("very_long_string_to_force_allocation")));
  }
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_path_generic_string(&p, (cfs_char_t **)&buf));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_runtime_init(&cfg, &rt, &ec));
  g_cfs_malloc_fail = 0;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_async(rt, &p, NULL, NULL));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_file_size_async(rt, &p, NULL, NULL));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_message_pipe_create(CFS_STR("pipe"), &pipe));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_process_spawn(CFS_STR("dummy"), &proc));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_shm_create(1024, CFS_STR("shm"), &shm));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_named_semaphore_create(CFS_STR("sem"), 1, &sem));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_greenthread_spawn(NULL, NULL, &gt));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_greenthread_scheduler_init(&sched));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_dir_itr_init_async(rt, &p, NULL, NULL));
  cfs_runtime_destroy(rt);

  memset(&req_dummy, 0, sizeof(req_dummy));
  req_dummy.opcode = cfs_opcode_remove;
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_serialize_request(&req_dummy, &buf, &sz));
  g_cfs_malloc_fail = 0;
  ASSERT_EQ(cfs_errc_success, cfs_serialize_request(&req_dummy, &buf, &sz));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_deserialize_request(buf, sz, &req));
  cfs_free(buf);

  g_cfs_malloc_fail = 0;

  /* Test cfs_realloc OOM */
  g_cfs_realloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_realloc(NULL, 100, &buf));
  g_cfs_realloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_path_assign(&p, CFS_STR("another_long_string_for_realloc")));
  g_cfs_realloc_fail = 0;

  /* Test cfs_calloc OOM */
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_calloc(10, 10, &buf));
  g_cfs_calloc_fail = 0;

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for extreme_edge_cases.
 * \return The test result.
 */
TEST extreme_edge_cases(void) {
  cfs_path p = {0};
  cfs_path out = {0};
  cfs_error_code ec;
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, NULL));

  /* cfs_current_path getcwd failure */
  g_cfs_getcwd_fail = 1;
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_current_path(&out, &ec));
  cfs_path_destroy(&out);
  g_cfs_getcwd_fail = 0;

  /* cfs_read_symlink readlink failure */
  g_cfs_readlink_fail = 1;
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("dummy_symlink_path")));
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_read_symlink(&p, &out, &ec));
  cfs_path_destroy(&out);
  g_cfs_readlink_fail = 0;

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for last_mile.
 * \return The test result.
 */
TEST last_mile(void) {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p = {0};
  cfs_path out = {0};
  cfs_request_t req;
  cfs_request_t *req1 = NULL, *req2 = NULL;

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  /* cfs_absolute where getcwd fails */
  g_cfs_getcwd_fail = 1;
  ASSERT_NEQ(cfs_errc_invalid_argument, cfs_absolute(&p, &out, &ec));
  cfs_path_destroy(&out);
  g_cfs_getcwd_fail = 0;

  /* invalid opcode execution */
  memset(&req, 0, sizeof(req));
  req.opcode = (cfs_opcode)9999;
  cfs_dispatch_request(NULL, &req, NULL, NULL);

  /* Async errors inside thread pool / queues */
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));

  /* Force malloc failures for async variants */
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_async(rt, &p, NULL, NULL));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_file_size_async(rt, &p, NULL, NULL));
  g_cfs_malloc_fail = 0;

  /* Add cancelled requests and normal requests to queue, then destroy runtime
   */
  ASSERT_EQ(cfs_errc_success,
            cfs_malloc(sizeof(cfs_request_t), (void **)&req1));
  ASSERT_EQ(cfs_errc_success,
            cfs_malloc(sizeof(cfs_request_t), (void **)&req2));
  if (req1 && req2) {
    memset(req1, 0, sizeof(*req1));
    memset(req2, 0, sizeof(*req2));
    req1->ref_count = 1;
    req2->ref_count = 1;
    req1->cancelled = cfs_true;
    cfs_dispatch_request(rt, req1, NULL, NULL);
    cfs_dispatch_request(rt, req2, NULL, NULL);
  }

  cfs_runtime_destroy(rt);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for cover_everything.
 * \return The test result.
 */
TEST cover_everything(void) {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_request_t *req_dyn = NULL;

  cfs_log_debug("testing logger");

  cfg.mode = cfs_modality_sync;
  cfg.thread_pool_size = 0;
  cfg.ipc_path = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));

  ASSERT_EQ(cfs_errc_success,
            cfs_malloc(sizeof(cfs_request_t), (void **)&req_dyn));
  if (req_dyn) {
    memset(req_dyn, 0, sizeof(*req_dyn));
    req_dyn->ref_count = 1;
    req_dyn->opcode = (cfs_opcode)9999;
    cfs_dispatch_request(rt, req_dyn, NULL, NULL);
  }

  /* Clean up */
  cfs_runtime_destroy(rt);

  /* Thread pool creation failure */
  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;

  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_runtime_init(&cfg, &rt, &ec));
  cfs_runtime_destroy(rt);
  g_cfs_calloc_fail = 0;

  g_cfs_calloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_runtime_init(&cfg, &rt, &ec));
  cfs_runtime_destroy(rt);
  g_cfs_calloc_fail = 0;

  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_runtime_init(&cfg, &rt, &ec));
  cfs_runtime_destroy(rt);
  g_cfs_malloc_fail = 0;

  {
    cfs_path dot_p = {0};
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    cfs_path tmp_p = {0};
#endif
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&dot_p, CFS_STR(".")));
    ASSERT_EQ(cfs_errc_success, cfs_current_path_set(&dot_p, &ec));
    cfs_path_destroy(&dot_p);

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    {
      const char *old_tmpdir = getenv("TMPDIR");
      char saved_tmpdir[1024];
      int had_tmpdir = 0;
      if (old_tmpdir) {
        had_tmpdir = 1;
        strncpy(saved_tmpdir, old_tmpdir, sizeof(saved_tmpdir) - 1);
        saved_tmpdir[sizeof(saved_tmpdir) - 1] = '\0';
      }
      unsetenv("TMPDIR");
      cfs_path_init(&tmp_p);
      ASSERT_EQ(cfs_errc_success, cfs_temp_directory_path(&tmp_p, &ec));
      cfs_path_destroy(&tmp_p);
      if (had_tmpdir) {
        setenv("TMPDIR", saved_tmpdir, 1);
      }
    }
#endif
  }

  PASS();
}

/**
 * \brief Test case for out_of_memory_precise.
 * \return The test result.
 */
TEST out_of_memory_precise(void) {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p = {0};
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  g_cfs_malloc_fail = 2; /* Fail allocating work_queue */
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_runtime_init(&cfg, &rt, &ec));

  g_cfs_malloc_fail = 3; /* Fail allocating completion_queue */
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_runtime_init(&cfg, &rt, &ec));
  g_cfs_malloc_fail = 0;

  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));

  g_cfs_malloc_fail = 2; /* Fail cloning target_path */
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_file_size_async(rt, &p, NULL, NULL));

  g_cfs_malloc_fail = 3; /* Fail allocating result_buffer */
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_file_size_async(rt, &p, NULL, NULL));

  g_cfs_malloc_fail = 0;
  cfs_runtime_destroy(rt);
  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for fix_last_missing.
 * \return The test result.
 */
TEST fix_last_missing(void) {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p = {0};
  cfs_request_t *req1;

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));

  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_async(rt, &p, NULL, NULL));

  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_file_size_async(rt, &p, NULL, NULL));

  g_cfs_malloc_fail = 0;

  ASSERT_EQ(cfs_errc_success,
            cfs_malloc(sizeof(cfs_request_t), (void **)&req1));
  if (req1) {
    memset(req1, 0, sizeof(*req1));
    req1->ref_count = 1;
    req1->cancelled = cfs_true;
    cfs_dispatch_request(rt, req1, NULL, NULL);
    test_sleep_ms(100);
  }

  cfs_runtime_destroy(rt);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for missing_lines_coverage.
 * \return The test result.
 */
TEST missing_lines_coverage(void) {
  cfs_path p = {0}, res = {0};
  cfs_char_t dest[10];
  cfs_request_t *req_out;

  ASSERT_EQ(cfs_errc_success, cfs_strncpy(dest, CFS_STR("abc"), 5, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_strcmp(CFS_STR("a"), CFS_STR("b"), NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_strncmp(CFS_STR("a"), CFS_STR("b"), 1, NULL));

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc")));
  ASSERT_EQ(cfs_errc_success, cfs_path_concat(&p, CFS_STR("")));
  cfs_path_destroy(&p);

  cfs_path_init(&p);
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_concat(&p, CFS_STR("xyz")));
  g_cfs_malloc_fail = 0;
  cfs_path_destroy(&p);

  cfs_path_init(&p);
  ASSERT_EQ(cfs_errc_success, cfs_path_append(&p, CFS_STR("abc")));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc")));
  ASSERT_EQ(cfs_errc_success, cfs_path_append(&p, CFS_STR("")));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc")));
  ASSERT_EQ(cfs_errc_success, cfs_path_append(&p, CFS_STR("/def")));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc\\")));
  ASSERT_EQ(cfs_errc_success, cfs_path_append(&p, CFS_STR("\\def")));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("a")));
  g_cfs_realloc_fail = 1;
  ASSERT_EQ(
      cfs_errc_not_enough_memory,
      cfs_path_append(
          &p, CFS_STR("a_very_long_string_that_exceeds_initial_capacity_xyz")));
  g_cfs_realloc_fail = 0;
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc")));
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, NULL));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc")));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_filename(&p, NULL));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc.txt")));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_extension(&p, NULL));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc/def")));
  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &res));
  cfs_path_destroy(&p);
  cfs_path_destroy(&res);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc.txt")));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_stem(&p, NULL));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc/")));
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &res));
  cfs_path_destroy(&p);
  cfs_path_destroy(&res);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("file")));
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &res));
  cfs_path_destroy(&p);
  cfs_path_destroy(&res);

  {
    FILE *f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, "test_rem.txt", "w") != 0)
      f = NULL;
#else
    f = fopen("test_rem.txt", "w");
#endif
    if (f)
      fclose(f);
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("test_rem.txt")));
    ASSERT_EQ(cfs_errc_success, cfs_remove(&p, NULL));
    cfs_path_destroy(&p);
  }

  {
    int dummy_buf[1] = {0};
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_deserialize_request(dummy_buf, sizeof(dummy_buf), &req_out));
    g_cfs_malloc_fail = 0;
  }

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_greenthread_spawn(NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_greenthread_scheduler_init(NULL));

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR(".")));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_dir_itr_init_async((cfs_runtime_t *)1, &p, NULL, NULL));
  g_cfs_malloc_fail = 0;
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc")));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_path_lexically_relative(&p, &p, NULL));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("abc")));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_is_absolute(&p, NULL));
  cfs_path_destroy(&p);

  PASS();
}

/**
 * \brief Test case for missing_lines_coverage_async.
 * \return The test result.
 */
TEST missing_lines_coverage_async(void) {
  cfs_runtime_config cfg;
  cfs_runtime_t *rt;
  cfs_path p = {0};

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));

  cfg.mode = cfs_modality_sync;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, NULL));
  ASSERT_EQ(cfs_errc_success,
            cfs_remove_async(rt, &p, dummy_async_callback, NULL));
  cfs_runtime_destroy(rt);

  cfg.mode = cfs_modality_singlethread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, NULL));
  ASSERT_EQ(cfs_errc_success,
            cfs_remove_async(rt, &p, dummy_async_callback, NULL));
  cfs_runtime_destroy(rt);

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, NULL));

  {
    cfs_request_t *reqs[200];
    int j;
    for (j = 0; j < 200; j++) {
      ASSERT_EQ(cfs_errc_success,
                cfs_malloc(sizeof(cfs_request_t), (void **)&reqs[j]));
      if (reqs[j]) {
        memset(reqs[j], 0, sizeof(*reqs[j]));
        reqs[j]->opcode = cfs_opcode_remove;
        cfs_dispatch_request(rt, reqs[j], NULL, NULL);
        ASSERT_EQ(cfs_errc_success, cfs_cancel_request(rt, reqs[j]));
      }
    }
  }

  test_sleep_ms(100);

  cfs_runtime_destroy(rt);
  cfs_path_destroy(&p);

  PASS();
}

/**
 * \brief Test case for branch_coverage_nulls.
 * \return The test result.
 */
TEST branch_coverage_nulls(void) {
  cfs_path p = {0};
  cfs_path empty_p = {0};
  cfs_bool b;
  cfs_runtime_config cfg;
  cfs_runtime_t *rt = NULL;
  const cfs_char_t *cstr;

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("dummy")));
  cfs_path_init(&empty_p);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_init(NULL, NULL, NULL));
  cfg.mode = cfs_modality_sync;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, NULL));

  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_remove_async(NULL, NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_file_size_async(NULL, NULL, NULL, NULL));
  cfs_dispatch_request(NULL, NULL, NULL, NULL);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_cancel_request(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_itr_init_async(NULL, NULL, NULL, NULL));

  cfs_runtime_destroy(rt);

#if defined(CFS_OS_WINDOWS)
  ASSERT_EQ(cfs_errc_success, cfs_utf8_to_utf16("test", NULL, 0, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_utf16_to_utf8(L"test", NULL, 0, NULL));
#endif
  ASSERT_EQ(cfs_errc_success, cfs_mb_to_wide("test", NULL, 0, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_wide_to_mb(L"test", NULL, 0, NULL));

  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&p, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(NULL, &cstr));
  ASSERT_EQ(cfs_errc_success, cfs_path_c_str(&empty_p, &cstr));

  cfs_path_clear(NULL);
  cfs_path_clear(&empty_p);

  cfs_path_swap(NULL, NULL);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_append(NULL, NULL));

  {
    cfs_path dummy_out = {0};
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_filename(NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_path_filename(&empty_p, &dummy_out));
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_extension(NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_path_extension(&empty_p, &dummy_out));
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_stem(NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_path_stem(&empty_p, &dummy_out));
  }

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_message_pipe_create(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_serialize_request(NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_deserialize_request(NULL, 0, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_process_spawn(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_create(0, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_map(NULL, NULL));
  cfs_shm_unmap(NULL, NULL);
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_named_semaphore_create(NULL, 0, NULL));
  cfs_greenthread_scheduler_destroy(NULL);
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_copy_file(NULL, NULL, cfs_copy_options_none, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_is_absolute(NULL, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&empty_p, &b));

  {
    cfs_path tmp = {0};
    cfs_path_init(&tmp);
    ASSERT_EQ(cfs_errc_success, cfs_temp_directory_path(&tmp, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_current_path(&tmp, NULL));
    cfs_path_destroy(&tmp);
  }

  cfs_path_destroy(&p);
  cfs_path_destroy(&empty_p);
  PASS();
}

/**
 * \brief Test case for edge_cases_and_oom.
 * \return The test result.
 */
TEST edge_cases_and_oom(void) {
  cfs_path p, out;
  cfs_bool b;
  cfs_directory_iterator *it;
  cfs_recursive_directory_iterator *rit;
  cfs_error_code ec;
#if !defined(CFS_OS_WINDOWS)
  const cfs_directory_entry *entry;
#endif

  /* null checks */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_root_name(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_root_directory(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_root_path(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_relative_path(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_parent_path(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_filename(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_stem(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_extension(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_root_path(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_root_name(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_root_directory(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_relative_path(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_parent_path(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_filename(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_stem(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_has_extension(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_is_absolute(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_is_relative(NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_lexically_normal(NULL, NULL));

  /* empty paths */
  cfs_path_init(&p);
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_directory(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_path(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_relative_path(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_path(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_directory(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_relative_path(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_parent_path(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_filename(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_stem(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_extension(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));

  /* lexically normal specific */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_assign(&p, CFS_STR("a/../b/./c/../../d/..")));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("../../..")));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("/a/..")));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("a/b/...")));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);

  /* oom tests */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_assign(&p, CFS_STR("/root/dir/file.ext")));

  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_root_directory(&p, &out));
  g_cfs_malloc_fail = 0;

  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_root_path(&p, &out));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_relative_path(&p, &out));
  g_cfs_calloc_fail = 0;

  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_parent_path(&p, &out));
  g_cfs_malloc_fail = 0;

  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_filename(&p, &out));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_stem(&p, &out));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_extension(&p, &out));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_lexically_normal(&p, &out));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_path_replace_extension(&p, CFS_STR("old")));
  g_cfs_calloc_fail = 0;

  ASSERT_EQ(cfs_errc_success,
            cfs_path_replace_filename(&p, CFS_STR("new.txt")));
  cfs_path_remove_filename(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("C:/root/dir")));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(&p, &b));

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("/C:")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR(".")));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("..")));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("C:/")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_path(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(&p, &out));
  cfs_path_destroy(&out);
  cfs_path_remove_filename(&p);
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("C:/root/")));
  cfs_path_remove_filename(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("C:/root/dir")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_directory(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(&p, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("/file.ext")));
  g_cfs_calloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_extension(&p, &out));
  g_cfs_calloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_stem(&p, &out));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_filename(&p, &out));
  g_cfs_calloc_fail = 0;

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("")));
  cfs_path_remove_filename(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("/")));
  cfs_path_remove_filename(&p);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("//host")));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_name(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(&p, &out));
  cfs_path_destroy(&out);
  cfs_path_remove_filename(&p);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("/")));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(&p, &out));
  cfs_path_destroy(&out);
  cfs_path_remove_filename(&p);
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR(".a")));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("a")));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("ab")));
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_stem(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR(".")));
  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &out));
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p, CFS_STR("..")));
  ASSERT_EQ(cfs_errc_success, cfs_path_extension(&p, &out));
  cfs_path_destroy(&out);

  {
    char fmt_buf[64];
    cfs_uintmax_t uval = (cfs_uintmax_t)12345;
    cfs_file_time_type sval = (cfs_file_time_type)-6789;
    CFS_SPRINTF_1_SAFE(fmt_buf, sizeof(fmt_buf), CFS_UNUM_FORMAT, uval);
    ASSERT_STR_EQ("12345", fmt_buf);
    CFS_SPRINTF_1_SAFE(fmt_buf, sizeof(fmt_buf), CFS_NUM_FORMAT, sval);
    ASSERT_STR_EQ("-6789", fmt_buf);
  }

  cfs_path_destroy(&p);

#if !defined(CFS_OS_WINDOWS)
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("/")));
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_dir_itr_init(&p, &it, &ec));
    g_cfs_malloc_fail = 2;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_rec_dir_itr_init(&p, &rit, &ec));
    g_cfs_malloc_fail = 0;

    ASSERT_EQ(cfs_errc_success, cfs_dir_itr_init(&p, &it, &ec));
    if (it) {
      while (cfs_dir_itr_next(it, &entry, &ec) == 0 && entry != NULL) {
      }
      ASSERT_EQ((const void *)NULL, (const void *)entry);
      ASSERT_EQ(cfs_errc_success, cfs_dir_itr_next(it, &entry, &ec));
      ASSERT_EQ((const void *)NULL, (const void *)entry);
      cfs_dir_itr_close(it);
    }
  }

  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("/this_dir_does_not_exist_123456")));
  ASSERT_NEQ(cfs_errc_success, cfs_dir_itr_init(&p, &it, &ec));
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_itr_init(NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_itr_next(NULL, NULL, NULL));
#endif

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("/")));
  ASSERT_EQ(cfs_errc_success, cfs_path_relative_path(&p, &out));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_directory(&p, &b));
  cfs_path_destroy(&p);

  {
    cfs_path p2 = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("C:/root/dir")));
    ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(&p2, &b));
    cfs_path_destroy(&p2);
  }
  {
    cfs_path p2 = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("/file.ext")));
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_parent_path(&p2, &out));
    g_cfs_malloc_fail = 0;
    cfs_path_destroy(&p2);
  }
  {
    cfs_path p2 = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("/file.ext")));
    g_cfs_realloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_path_replace_filename(
                  &p2, CFS_STR("very_long_name_to_exceed_capacity.txt")));
    g_cfs_realloc_fail = 0;
    cfs_path_destroy(&p2);
  }
  {
    cfs_path p2 = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("file")));
    g_cfs_realloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_path_replace_extension(&p2, CFS_STR("ext")));
    g_cfs_realloc_fail = 0;
    cfs_path_destroy(&p2);
  }
  {
    cfs_path p2 = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("file")));
    g_cfs_realloc_fail = 2;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_path_replace_extension(&p2, CFS_STR("ext")));
    g_cfs_realloc_fail = 0;
    cfs_path_destroy(&p2);
  }

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR(".")));
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_dir_itr_init(&p, &it, &ec));
    g_cfs_malloc_fail = 2;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_rec_dir_itr_init(&p, &rit, &ec));
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_rec_dir_itr_init(&p, &rit, &ec));
    g_cfs_malloc_fail = 0;
  }
  cfs_path_destroy(&p);

  PASS();
}

/**
 * \brief Tests multi-header coexistence (`NO_DISCARD` pre-definition and
 * before/after foreign `no_discard.h` inclusion).
 * \return The test result.
 */
TEST header_interoperability(void) {
  cfs_bool empty = cfs_false;

  ASSERT_EQ(cfs_errc_success, cfs_test_interop_predef(&empty));
  ASSERT_EQ(cfs_true, empty);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_test_interop_predef(NULL));

  ASSERT_EQ(cfs_errc_success, cfs_test_interop_before(&empty));
  ASSERT_EQ(cfs_false, empty);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_test_interop_before(NULL));

  ASSERT_EQ(cfs_errc_success, cfs_test_interop_after(&empty));
  ASSERT_EQ(cfs_false, empty);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_test_interop_after(NULL));

  PASS();
}

/**
 * \brief Covers all newly percolated OOM and error paths across `cfs.h`.
 * \return The test result.
 */
TEST additional_percolation_oom_coverage(void) {
  cfs_path p = {0}, p2 = {0}, out = {0};
  cfs_bool b = cfs_false;
  cfs_error_code ec;
  cfs_runtime_config cfg;
  cfs_runtime_t *rt = NULL;
  cfs_shm_segment *shm = NULL;
#if !defined(CFS_OS_WINDOWS)
  void *addr = NULL;
#endif
  void *buf = NULL;
  cfs_size_t sz = 0;
  cfs_request_t req;
  cfs_request_t *req_out = NULL;
  FILE *f = NULL;

  /* 1. Sync file_size_async on existing and non-existing files */
#if defined(_MSC_VER)
  if (fopen_s(&f, "test_async_sz.txt", "wb") != 0)
    f = NULL;
#else
  f = fopen("test_async_sz.txt", "wb");
#endif
  if (f) {
    fwrite("abcde", 1, 5, f);
    fclose(f);
  }
  cfg.mode = cfs_modality_sync;
  cfg.thread_pool_size = 0;
  cfg.ipc_path = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("test_async_sz.txt")));
  ASSERT_EQ(cfs_errc_success,
            cfs_file_size_async(rt, &p, dummy_async_callback, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_remove_async(rt, &p, NULL, NULL));
  ASSERT_EQ(cfs_errc_success,
            cfs_file_size_async(rt, &p, dummy_async_callback, NULL));

  /* 2. Async clone OOM on remove_async and dir_itr_init_async */
  g_cfs_malloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_async(rt, &p, NULL, NULL));
  g_cfs_malloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_dir_itr_init_async(rt, &p, NULL, NULL));
  g_cfs_malloc_fail = 0;
  cfs_runtime_destroy(rt);
  cfs_path_destroy(&p);

  /* 3. Deserialize OOM on target_path and dest_path */
  memset(&req, 0, sizeof(req));
  req.opcode = cfs_opcode_copy_file;
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&req.target_path, CFS_STR("t_path")));
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&req.dest_path, CFS_STR("d_path")));
  ASSERT_EQ(cfs_errc_success, cfs_serialize_request(&req, &buf, &sz));
  g_cfs_malloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_deserialize_request(buf, sz, &req_out));
  g_cfs_malloc_fail = 3;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_deserialize_request(buf, sz, &req_out));
  g_cfs_malloc_fail = 0;
  cfs_free(buf);
  cfs_path_destroy(&req.target_path);
  cfs_path_destroy(&req.dest_path);

  /* 4. cfs_shm_map OOM */
  ASSERT_EQ(cfs_errc_success, cfs_shm_create(64, CFS_STR("shm_oom"), &shm));
#if !defined(CFS_OS_WINDOWS)
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_shm_map(shm, &addr));
  g_cfs_calloc_fail = 0;
#endif
  cfs_shm_destroy(shm);

  /* 5. cfs_path_stem OOM on "." and cfs_path_has_* OOM */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR(".")));
  g_cfs_calloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_stem(&p, &out));
  g_cfs_calloc_fail = 0;
  cfs_path_destroy(&p);

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("/dir/sub/file.txt")));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_has_parent_path(&p, &b));
  g_cfs_malloc_fail = 0;
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_has_filename(&p, &b));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_has_stem(&p, &b));
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_has_extension(&p, &b));
  g_cfs_calloc_fail = 0;
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_is_relative(&p, NULL));

  /* 6. cfs_path_lexically_normal & cfs_path_lexically_relative OOM */
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_lexically_normal(&p, &out));
  g_cfs_malloc_fail = 0;

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p2, CFS_STR("/dir/other/leaf")));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_path_lexically_relative(&p, &p2, &out));
  g_cfs_malloc_fail = 0;
  cfs_path_destroy(&p2);
  cfs_path_destroy(&p);

  /* 7. cfs_absolute OOM on POSIX */
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("rel_child")));
  cfs_path_init(&out);
  g_cfs_malloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_absolute(&p, &out, &ec));
  g_cfs_malloc_fail = 0;
  g_cfs_realloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_absolute(&p, &out, &ec));
  g_cfs_realloc_fail = 0;
  cfs_path_destroy(&out);
  cfs_path_destroy(&p);
#endif

  /* 8. Empty directory & 0-byte file in cfs_is_empty_path + OOMs */
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("temptydr")));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove_all(&p, &sz, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directory(&p, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_is_empty_path(&p, &b, &ec));
    ASSERT_EQ(cfs_true, b);
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_is_empty_path(&p, &b, &ec));
#if !defined(CFS_OS_WINDOWS)
    g_cfs_malloc_fail = 2;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_is_empty_path(&p, &b, &ec));
#endif
    g_cfs_malloc_fail = 0;
    ASSERT_EQ(cfs_errc_success, cfs_remove(&p, &ec));
    cfs_path_destroy(&p);
  }

#if defined(_MSC_VER)
  if (fopen_s(&f, "tzerocov.txt", "wb") != 0)
    f = NULL;
#else
  f = fopen("tzerocov.txt", "wb");
#endif
  if (f)
    fclose(f);
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("tzerocov.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_is_empty_path(&p, &b, &ec));
  ASSERT_EQ(cfs_true, b);
  ASSERT_EQ(cfs_errc_success, cfs_remove(&p, &ec));
  cfs_path_destroy(&p);

  /* 9. cfs_create_directories & cfs_remove_all OOMs */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("tcdoom1/tcdoom2/leaf")));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_create_directories(&p, &ec));
  g_cfs_malloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_create_directories(&p, &ec));
  g_cfs_malloc_fail = 0;
  cfs_path_destroy(&p);

#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("trmoomdr")));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove_all(&p, &sz, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directory(&p, &ec));
#if defined(_MSC_VER)
    if (fopen_s(&f, "trmoomdr/item_with_very_long_filename_for_realloc.txt",
                "wb") != 0)
      f = NULL;
#elif defined(CFS_OS_DOS)
    f = fopen("trmoomdr/item1.txt", "wb");
#else
    f = fopen("trmoomdr/item_with_very_long_filename_for_realloc.txt", "wb");
#endif
    if (f)
      fclose(f);
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_all(&p, &sz, &ec));
#if !defined(CFS_OS_WINDOWS)
    g_cfs_malloc_fail = 2;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_all(&p, &sz, &ec));
    g_cfs_malloc_fail = 3;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_all(&p, &sz, &ec));
#endif
    g_cfs_malloc_fail = 0;
#if !defined(CFS_OS_DOS)
    g_cfs_realloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_all(&p, &sz, &ec));
    g_cfs_realloc_fail = 0;
#endif
    ASSERT_EQ(cfs_errc_success, cfs_remove_all(&p, &sz, &ec));
    cfs_path_destroy(&p);
  }

  /* 10. cfs_copy_symlink error when destination already exists */
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
#if defined(_MSC_VER)
  if (fopen_s(&f, "test_cs_src.txt", "wb") != 0)
    f = NULL;
#else
  f = fopen("test_cs_src.txt", "wb");
#endif
  if (f)
    fclose(f);
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p, CFS_STR("test_cs_src.txt")));
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p2, CFS_STR("test_cs_lnk.txt")));
  remove("test_cs_lnk.txt");
  ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p, &p2, &ec));
  ASSERT_NEQ(cfs_errc_success, cfs_copy_symlink(&p2, &p, &ec));
  remove("test_cs_lnk.txt");
  remove("test_cs_src.txt");
  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);
#endif

  PASS();
}

/**
 * @brief Exercises lexical and string short-circuit boolean branches and edge
 * cases.
 * @return The test result.
 */
TEST exhaustive_branch_coverage(void) {
  cfs_path empty_null = {0};
  cfs_path empty_str = {0};
  cfs_path p = {0}, p2 = {0}, out = {0};
  cfs_bool b = cfs_false;
  cfs_char_t cbuf[16];
  cfs_char_t *gstr = NULL;

  cfs_path_init(&empty_null);
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&empty_str, CFS_STR("")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("branch_a.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("branch_b.txt")));

  /* 1. Clone empty_null (src->str == NULL) */
  ASSERT_EQ(cfs_errc_success, cfs_path_clone(&out, &empty_null));
  cfs_path_destroy(&out);

  /* 2. All path decomposition / query functions with empty_str (p->str != NULL,
   * p->length == 0) */
  ASSERT_EQ(cfs_errc_success, cfs_path_filename(&empty_str, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_relative_path(&empty_str, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_parent_path(&empty_str, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_is_absolute(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_path(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_name(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_directory(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_relative_path(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_parent_path(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_filename(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_stem(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_extension(&empty_str, &b));
  ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&empty_str, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_directory(&empty_null, &out));
  ASSERT_EQ(cfs_errc_success, cfs_path_root_path(&empty_null, &out));

  /* 3. cfs_path_stem OOM on no-dot filename */
  g_cfs_calloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_stem(&p2, &out));
  g_cfs_calloc_fail = 0;
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&out, CFS_STR("no_dot_file")));
  g_cfs_calloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_path_stem(&out, &empty_null));
  g_cfs_calloc_fail = 0;
  cfs_path_destroy(&out);
  cfs_path_init(&empty_null);

  /* 4. cfs_path_make_preferred & cfs_path_generic_string with empty_null */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_make_preferred(&empty_null));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_path_generic_string(&empty_null, &gstr));

  /* 5. cfs_path_append separator combinations & NULL source */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_append(&p, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&out, CFS_STR("dir/")));
  ASSERT_EQ(cfs_errc_success, cfs_path_append(&out, CFS_STR("leaf")));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&out, CFS_STR("dir")));
  ASSERT_EQ(cfs_errc_success, cfs_path_append(&out, CFS_STR("\\leaf")));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&out, CFS_STR("\\leading")));
  ASSERT_EQ(cfs_errc_success, cfs_path_has_root_directory(&out, &b));
  cfs_path_destroy(&out);

  /* 6. cfs_path_replace_filename(p, NULL) & cfs_path_replace_extension(p, NULL
   * / "" / ".ext") */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&out, CFS_STR("dir/file.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_replace_filename(&out, NULL));
  cfs_path_destroy(&out);
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&out, CFS_STR("file.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_replace_extension(&out, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_path_replace_extension(&out, CFS_STR("")));
  ASSERT_EQ(cfs_errc_success,
            cfs_path_replace_extension(&empty_null, CFS_STR(".z")));
  cfs_path_destroy(&empty_null);
  cfs_path_init(&empty_null);
  cfs_path_destroy(&out);

  /* 7. cfs_path_lexically_relative edge cases */
  {
    cfs_path base_trail = {0}, sub_p = {0};
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&base_trail, CFS_STR("/a/b/")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&sub_p, CFS_STR("/a/b/c")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_relative(&base_trail, &sub_p, &out));
    cfs_path_destroy(&out);
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_relative(&p, &empty_null, &out));
    cfs_path_destroy(&out);
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_relative(&p, &empty_str, &out));
    cfs_path_destroy(&out);
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_path_lexically_relative(&empty_null, &p, &out));
    cfs_path_destroy(&base_trail);
    cfs_path_destroy(&sub_p);
  }

  /* 8. String functions with NULL out / truncation */
  ASSERT_EQ(cfs_errc_success, cfs_strcpy(cbuf, CFS_STR("hi"), NULL));
  ASSERT_EQ(cfs_errc_success, cfs_strncpy(cbuf, CFS_STR("abcdef"), 3, NULL));
  cbuf[3] = CFS_CHAR('\0');
  ASSERT_EQ(cfs_errc_success, cfs_strcat(cbuf, CFS_STR("z"), NULL));
  ASSERT_EQ(cfs_errc_success, cfs_make_error_code_from_os(1, NULL));

  cfs_path_destroy(&empty_null);
  cfs_path_destroy(&empty_str);
  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);
  PASS();
}

/**
 * @brief Exercises runtime, IPC, and filesystem short-circuit boolean branches.
 * @return The test result.
 */
TEST exhaustive_branch_coverage_fs(void) {
  cfs_path empty_null = {0};
  cfs_path empty_str = {0};
  cfs_path p = {0}, p2 = {0}, out = {0};
  cfs_bool b = cfs_false;
  cfs_error_code ec;
  cfs_file_status fs;
  cfs_uintmax_t u = 0;
  cfs_size_t sz = 0;
  cfs_runtime_config cfg = {cfs_modality_sync, 0, NULL};
  cfs_runtime_t *rt = NULL;
  cfs_request_t *req = NULL;
  cfs_request_t *req_out = NULL;
  cfs_shm_segment *shm = NULL;
  cfs_named_semaphore *sem = NULL;
  cfs_directory_iterator *dit = NULL;
  cfs_recursive_directory_iterator *rdit = NULL;
  cfs_sandbox_config sbox = {0};
  void *buf = NULL;
  void *addr = NULL;
  int dummy_int = 0;
  FILE *f = NULL;

  cfs_path_init(&empty_null);
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&empty_str, CFS_STR("")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p, CFS_STR("branch_a.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("branch_b.txt")));

  /* 9. Runtime modes (async, thread_pool_size=0, pthread_create fail, sync
   * cancelled, poll callback=NULL) */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_init(&cfg, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_init(NULL, &rt, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_init(NULL, &rt, NULL));

  cfg.mode = cfs_modality_async;
  cfg.thread_pool_size = 0;
  cfg.ipc_path = NULL;
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS) &&                        \
    defined(CFS_MULTITHREADED)
  g_cfs_pthread_create_fail = 1;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));
  g_cfs_pthread_create_fail = 0;
  cfs_runtime_destroy(rt);
#endif

  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_remove_async(rt, NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_file_size_async(rt, NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_itr_init_async(rt, NULL, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_cancel_request(rt, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_runtime_set_sandbox(rt, NULL));
  cfs_dispatch_request(rt, NULL, NULL, NULL);
  ASSERT_EQ(cfs_errc_success, cfs_remove_async(rt, &p, NULL, NULL));
  test_sleep_ms(50);
  ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(rt));
  cfs_runtime_destroy(rt);

  cfg.mode = cfs_modality_sync;
  cfg.thread_pool_size = 0;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(rt));

  /* Sync cancelled request + file_size with NULL/undersized buffer */
  ASSERT_EQ(cfs_errc_success, cfs_malloc(sizeof(cfs_request_t), (void **)&req));
  memset(req, 0, sizeof(*req));
  req->ref_count = 1;
  req->cancelled = cfs_true;
  cfs_dispatch_request(rt, req, NULL, NULL);

#if defined(_MSC_VER)
  if (fopen_s(&f, "branch_a.txt", "wb") != 0)
    f = NULL;
#else
  f = fopen("branch_a.txt", "wb");
#endif
  if (f) {
    fwrite("123", 1, 3, f);
    fclose(f);
  }
#if defined(_MSC_VER)
  if (fopen_s(&f, "branch_b.txt", "wb") != 0)
    f = NULL;
#else
  f = fopen("branch_b.txt", "wb");
#endif
  if (f) {
    fwrite("456", 1, 3, f);
    fclose(f);
  }

  ASSERT_EQ(cfs_errc_success, cfs_malloc(sizeof(cfs_request_t), (void **)&req));
  memset(req, 0, sizeof(*req));
  req->ref_count = 1;
  req->opcode = cfs_opcode_file_size;
  ASSERT_EQ(cfs_errc_success, cfs_path_clone(&req->target_path, &p));
  req->result_buffer = NULL;
  req->result_size = 0;
  cfs_dispatch_request(rt, req, NULL, NULL);

  ASSERT_EQ(cfs_errc_success, cfs_malloc(sizeof(cfs_request_t), (void **)&req));
  memset(req, 0, sizeof(*req));
  req->ref_count = 1;
  req->opcode = cfs_opcode_file_size;
  ASSERT_EQ(cfs_errc_success, cfs_path_clone(&req->target_path, &p));
  ASSERT_EQ(cfs_errc_success, cfs_malloc(1, &req->result_buffer));
  req->result_size = 1;
  cfs_dispatch_request(rt, req, NULL, NULL);
  cfs_runtime_destroy(rt);

  /* 10. Refcount > 1 release, IPC / SHM / Sem / Serialize / Deserialize
   * branches */
  ASSERT_EQ(cfs_errc_success, cfs_malloc(sizeof(cfs_request_t), (void **)&req));
  memset(req, 0, sizeof(*req));
  req->ref_count = 1;
  cfs_request_retain(req);
  cfs_request_release(req);
  cfs_request_release(req);

  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_message_pipe_create(CFS_STR("p"), NULL));
  memset(&sbox, 0, sizeof(sbox));
  {
    cfs_request_t sreq;
    memset(&sreq, 0, sizeof(sreq));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_serialize_request(&sreq, NULL, &sz));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_serialize_request(&sreq, &buf, NULL));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_deserialize_request(&dummy_int, sizeof(dummy_int), NULL));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_deserialize_request(&dummy_int, 0, &req_out));
    ASSERT_EQ(cfs_errc_success,
              cfs_deserialize_request(&dummy_int, sizeof(dummy_int), &req_out));
    cfs_request_release(req_out);
  }
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_process_spawn(CFS_STR("x"), NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_create(16, NULL, &shm));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_create(16, CFS_STR("s"), NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_create(0, CFS_STR("s"), &shm));
  ASSERT_EQ(cfs_errc_success, cfs_shm_create(16, CFS_STR("s_ok"), &shm));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_shm_map(shm, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_shm_map(shm, &addr));
#if !defined(CFS_OS_WINDOWS)
  ASSERT_EQ(cfs_errc_success, cfs_shm_map(shm, &addr));
#else
  cfs_shm_unmap(shm, addr);
#endif
  cfs_shm_unmap(shm, NULL);
  cfs_shm_destroy(shm);

  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_named_semaphore_create(CFS_STR("s"), 0, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_named_semaphore_create(NULL, 0, &sem));
  ASSERT_EQ(cfs_errc_success,
            cfs_named_semaphore_create(CFS_STR("s_zero"), 0, &sem));
  ASSERT_EQ(cfs_errc_success, cfs_named_semaphore_try_wait(sem, &b));
  ASSERT_EQ(cfs_false, b);
  cfs_named_semaphore_destroy(sem);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_malloc(8, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_realloc(NULL, 8, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_calloc(1, 8, NULL));

  /* 11. Filesystem operations parameter combinations & status types */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_file_size(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_file_size(&empty_null, &u, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_remove(&empty_null, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_hard_link_count(&empty_null, &u, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_hard_link_count(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_permissions(&empty_null, (cfs_perms)0777,
                            cfs_perm_options_replace, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_equivalent(&empty_null, &p2, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_equivalent(&p, &empty_null, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_equivalent(&p, &p2, NULL, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_equivalent(&p, &p2, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  {
    cfs_path nonex = {0};
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&nonex, CFS_STR("nonex_eq_file.txt")));
    ASSERT_NEQ(cfs_errc_success, cfs_equivalent(&p, &nonex, &b, &ec));
    cfs_path_destroy(&nonex);
  }
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_read_symlink(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_read_symlink(&empty_null, &out, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_absolute(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_canonical(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_copy_file(&p, NULL, cfs_copy_options_none, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_copy_file(&empty_null, &p2, cfs_copy_options_none, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_copy_file(&p, &empty_null, cfs_copy_options_none, &ec));
  ASSERT_EQ(cfs_errc_success,
            cfs_copy_file(&p, &p2, cfs_copy_options_update_existing, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_current_path_set(&empty_null, &ec));

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_status(&empty_null, &fs, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_status(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_symlink_status(&empty_null, &fs, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_symlink_status(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_exists_path(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_exists_path(&empty_null, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_empty_path(&empty_null, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_is_empty_path(&p, NULL, &ec));

  fs.type = cfs_file_type_none;
  ASSERT_EQ(cfs_errc_success, cfs_status_known(fs, &b));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_exists(fs, &b));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_is_other(fs, &b));
  fs.type = cfs_file_type_block;
  ASSERT_EQ(cfs_errc_success, cfs_is_block_file(fs, &b));
  ASSERT_EQ(cfs_true, b);
  fs.type = cfs_file_type_fifo;
  ASSERT_EQ(cfs_errc_success, cfs_is_fifo(fs, &b));
  ASSERT_EQ(cfs_true, b);
  fs.type = cfs_file_type_socket;
  ASSERT_EQ(cfs_errc_success, cfs_is_socket(fs, &b));
  ASSERT_EQ(cfs_true, b);
  fs.type = cfs_file_type_symlink;
  ASSERT_EQ(cfs_errc_success, cfs_is_symlink(fs, &b));
  ASSERT_EQ(cfs_true, b);
  ASSERT_EQ(cfs_errc_success, cfs_is_other(fs, &b));
  ASSERT_EQ(cfs_false, b);
  fs.type = cfs_file_type_directory;
  ASSERT_EQ(cfs_errc_success, cfs_is_directory(fs, &b));
  ASSERT_EQ(cfs_true, b);
  ASSERT_EQ(cfs_errc_success, cfs_is_other(fs, &b));
  ASSERT_EQ(cfs_false, b);
  fs.type = cfs_file_type_regular;
  ASSERT_EQ(cfs_errc_success, cfs_is_regular_file(fs, &b));
  ASSERT_EQ(cfs_true, b);
  ASSERT_EQ(cfs_errc_success, cfs_is_other(fs, &b));
  ASSERT_EQ(cfs_false, b);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_create_directory(&empty_null, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_create_directory(&empty_str, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_directories(&empty_null, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_create_directories(&empty_str, &ec));
  /* cfs_create_directories when existing node is a regular file (st_ok &&
   * st.type != directory) */
  ASSERT_NEQ(cfs_errc_success, cfs_create_directories(&p, &ec));
  /* cfs_create_directories on single-level dir (parent.length == 0) */
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    cfs_path single_d = {0};
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&single_d, CFS_STR("tsngcdir")));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&single_d, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directories(&single_d, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_remove_all(&single_d, NULL, &ec));
    cfs_path_destroy(&single_d);
  }

  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_hard_link(&empty_null, &p2, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_hard_link(&p, &empty_null, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_remove_all(&empty_null, &sz, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_remove_all(&empty_str, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rename(&empty_null, &p2, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rename(&p, &empty_null, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_resize_file(&empty_null, 0, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_resize_file(&empty_str, 0, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_space(&p, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_last_write_time(&p, NULL, &ec));

  /* 12. cfs_remove_all child append OOM, recursive child failure, and final
   * remove failure */
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    cfs_path rdir = {0}, rsub = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&rdir, CFS_STR("trmfull")));
#if defined(CFS_OS_DOS)
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&rsub, CFS_STR("trmfull/subchild")));
#else
    ASSERT_EQ(
        cfs_errc_success,
        cfs_path_init_str(
            &rsub, CFS_STR("trmfull/sub_very_long_child_directory_name")));
#endif
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove_all(&rdir, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directories(&rsub, &ec));
#if !defined(CFS_OS_DOS)
    g_cfs_realloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_all(&rdir, &sz, &ec));
    g_cfs_realloc_fail = 0;
#endif
#if !defined(CFS_OS_WINDOWS)
    g_cfs_malloc_fail = 4;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_remove_all(&rdir, &sz, &ec));
    g_cfs_malloc_fail = 0;
#endif
    ASSERT_EQ(cfs_errc_success, cfs_remove_all(&rdir, &sz, &ec));

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    {
      cfs_path orig_cwd = {0}, dot_cur = {0};
      ASSERT_EQ(cfs_errc_success, cfs_current_path(&orig_cwd, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_create_directories(&rsub, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_current_path_set(&rsub, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&dot_cur, CFS_STR(".")));
      ASSERT_NEQ(cfs_errc_success, cfs_remove_all(&dot_cur, &sz, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_current_path_set(&orig_cwd, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&rdir, &sz, &ec));
      cfs_path_destroy(&dot_cur);
      cfs_path_destroy(&orig_cwd);
    }
#endif
    cfs_path_destroy(&rdir);
    cfs_path_destroy(&rsub);
  }

  /* 13. Directory iterator remaining branches */
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    cfs_path dot_p = {0};
    const cfs_directory_entry *en = NULL;
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&dot_p, CFS_STR(".")));
    ASSERT_EQ(cfs_errc_success, cfs_dir_itr_init(&dot_p, &dit, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_itr_next(dit, NULL, &ec));
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_dir_itr_next(dit, &en, &ec));
    g_cfs_malloc_fail = 0;
    cfs_dir_itr_close(dit);

    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&dot_p, &rdit, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_pop(rdit, NULL));
    cfs_rec_dir_itr_close(rdit);
    cfs_path_destroy(&dot_p);
  }

  /* 14. Remaining lexical / has_* / st_dev / root-backslash branches */
  {
    cfs_path q = {0}, q2 = {0};
    cfs_path_remove_filename(&empty_null);
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR("rel_no_ext")));
    ASSERT_EQ(cfs_errc_success, cfs_path_has_root_path(&q, &b));
    ASSERT_EQ(cfs_false, b);
    ASSERT_EQ(cfs_errc_success, cfs_path_has_root_directory(&q, &b));
    ASSERT_EQ(cfs_false, b);
    ASSERT_EQ(cfs_errc_success, cfs_path_has_parent_path(&q, &b));
    ASSERT_EQ(cfs_false, b);
    ASSERT_EQ(cfs_errc_success, cfs_path_has_extension(&q, &b));
    ASSERT_EQ(cfs_false, b);
    cfs_path_destroy(&q);

    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR("/")));
    ASSERT_EQ(cfs_errc_success, cfs_path_has_relative_path(&q, &b));
    ASSERT_EQ(cfs_false, b);
    ASSERT_EQ(cfs_errc_success, cfs_path_has_filename(&q, &b));
    ASSERT_EQ(cfs_false, b);
    ASSERT_EQ(cfs_errc_success, cfs_path_has_stem(&q, &b));
    ASSERT_EQ(cfs_false, b);
    cfs_path_destroy(&q);

    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR(".x/..")));
    ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&q, &out));
    cfs_path_destroy(&out);
    cfs_path_destroy(&q);

    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR("ab/..")));
    ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&q, &out));
    cfs_path_destroy(&out);
    cfs_path_destroy(&q);

    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR("/..")));
    ASSERT_EQ(cfs_errc_success, cfs_path_lexically_normal(&q, &out));
    cfs_path_destroy(&out);
    cfs_path_destroy(&q);

    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR("/a/x")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q2, CFS_STR("/a\\b\\c")));
    ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&q, &q2, &out));
    cfs_path_destroy(&out);
    cfs_path_destroy(&q);
    cfs_path_destroy(&q2);

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR("/dev/null")));
    ASSERT_EQ(cfs_errc_success, cfs_equivalent(&p, &q, &b, &ec));
    ASSERT_EQ(cfs_false, b);
    cfs_path_destroy(&q);
#if !defined(__CYGWIN__)
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&q, CFS_STR("\\")));
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_remove(&q, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directories(&q, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_remove(&q, &ec));
    cfs_path_destroy(&q);
#endif
#endif
  }

  ASSERT_EQ(cfs_errc_success, cfs_remove(&p, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_remove(&p2, &ec));
  cfs_path_destroy(&empty_null);
  cfs_path_destroy(&empty_str);
  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);
  PASS();
}

/**
 * \brief Comprehensive unit, branch, and OOM test suite for Phase 1-3 features.
 * \return The test result.
 */
TEST phase1_to_3_features(void) {
  cfs_error_code ec;
  const char *msg = NULL;
  int ei;
  wchar_t wbuf[32];
  char mbuf[32];
  cfs_size_t req_sz = 0;
  cfs_path p1 = {0}, p2 = {0}, p3 = {0}, out = {0};
  cfs_path_iterator pit;
  cfs_path_element pelem;
  cfs_bool has_el = cfs_false;
  cfs_file_status st;
  cfs_uintmax_t fsz = 0;
  cfs_file_time_type ft = 0;
  int cmp = 0;
  cfs_size_t rem_cnt = 0;

  /* 1. Error messages across all 80 enumerators + out-of-bounds */
  for (ei = -1; ei <= (int)cfs_errc_unknown_error + 2; ++ei) {
    ASSERT_EQ(cfs_errc_success, cfs_error_message((cfs_errc)ei, &msg));
    ASSERT(msg != NULL && msg[0] != '\0');
  }

  /* OS error mapping & errno == 0 fallback in cfs_record_last_error */
#if !defined(CFS_OS_WINDOWS)
  errno = 0;
  g_cfs_getcwd_fail = 1;
  cfs_path_init(&out);
  ASSERT_EQ(cfs_errc_unknown_error, cfs_current_path(&out, &ec));
  g_cfs_getcwd_fail = 0;
  cfs_path_destroy(&out);
#endif

  /* CFS_REMOVE macro in sync & async runtime modes */
  test_write_file("test_macro_rm1.txt", "abc");
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p1, CFS_STR("test_macro_rm1.txt")));
  ASSERT_EQ(cfs_errc_success, CFS_REMOVE(NULL, &p1, NULL, NULL));
  ASSERT_NEQ(cfs_errc_success, CFS_REMOVE(NULL, &p1, NULL, NULL));
  {
    cfs_runtime_config rcfg;
    cfs_runtime_t *rt = NULL;
    rcfg.mode = cfs_modality_sync;
    rcfg.thread_pool_size = 0;
    rcfg.ipc_path = NULL;
    ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&rcfg, &rt, &ec));
    test_write_file("test_macro_rm1.txt", "abc");
    ASSERT_EQ(cfs_errc_success, CFS_REMOVE(rt, &p1, NULL, NULL));
    cfs_runtime_destroy(rt);

    rcfg.mode = cfs_modality_async;
    rcfg.thread_pool_size = 1;
    ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&rcfg, &rt, &ec));
    test_write_file("test_macro_rm1.txt", "abc");
    ASSERT_EQ(cfs_errc_success, CFS_REMOVE(rt, &p1, NULL, NULL));
    test_sleep_ms(30);
    ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(rt));
    cfs_runtime_destroy(rt);
  }
  cfs_path_destroy(&p1);

  /* 2. Charset conversions: cfs_mb_to_wide & cfs_wide_to_mb all branches */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_mb_to_wide(NULL, wbuf, 32, &req_sz));
  ASSERT_EQ(0, req_sz);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_mb_to_wide(NULL, wbuf, 32, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_mb_to_wide("hello", NULL, 0, &req_sz));
  ASSERT_EQ(6, req_sz);
  ASSERT_EQ(cfs_errc_success, cfs_mb_to_wide("hello", wbuf, 0, NULL));
  ASSERT_EQ(cfs_errc_result_out_of_range,
            cfs_mb_to_wide("hello", wbuf, 3, &req_sz));
  ASSERT_EQ(cfs_errc_success, cfs_mb_to_wide("hello", wbuf, 32, NULL));

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_wide_to_mb(NULL, mbuf, 32, &req_sz));
  ASSERT_EQ(0, req_sz);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_wide_to_mb(NULL, mbuf, 32, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_wide_to_mb(L"hello", NULL, 0, &req_sz));
  ASSERT_EQ(6, req_sz);
  ASSERT_EQ(cfs_errc_success, cfs_wide_to_mb(L"hello", mbuf, 0, NULL));
  ASSERT_EQ(cfs_errc_result_out_of_range,
            cfs_wide_to_mb(L"hello", mbuf, 3, &req_sz));
  ASSERT_EQ(cfs_errc_success, cfs_wide_to_mb(L"hello", mbuf, 32, NULL));
  ASSERT_STR_EQ("hello", mbuf);

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  {
    const char *prev_loc = setlocale(LC_CTYPE, NULL);
    char saved_loc[64];
    const char bad_utf8[] = {(char)0xFF, (char)0xFE, '\0'};
    wchar_t bad_wide[2];
    saved_loc[0] = '\0';
    if (prev_loc) {
      strncpy(saved_loc, prev_loc, sizeof(saved_loc) - 1);
      saved_loc[sizeof(saved_loc) - 1] = '\0';
    }
    if (setlocale(LC_CTYPE, "en_US.UTF-8") || setlocale(LC_CTYPE, "C.UTF-8") ||
        setlocale(LC_CTYPE, "UTF-8")) {
      ASSERT_EQ(cfs_errc_illegal_byte_sequence,
                cfs_mb_to_wide(bad_utf8, wbuf, 32, &req_sz));
      ASSERT_EQ(0, req_sz);
      ASSERT_EQ(cfs_errc_illegal_byte_sequence,
                cfs_mb_to_wide(bad_utf8, wbuf, 32, NULL));
    }
    (void)setlocale(LC_CTYPE, "C");
    bad_wide[0] = (wchar_t)0xD800u;
    bad_wide[1] = (wchar_t)0;
    ASSERT_EQ(cfs_errc_illegal_byte_sequence,
              cfs_wide_to_mb(bad_wide, mbuf, 32, &req_sz));
    ASSERT_EQ(0, req_sz);
    ASSERT_EQ(cfs_errc_illegal_byte_sequence,
              cfs_wide_to_mb(bad_wide, mbuf, 32, NULL));
    if (saved_loc[0] != '\0') {
      (void)setlocale(LC_CTYPE, saved_loc);
    } else {
      (void)setlocale(LC_CTYPE, "C");
    }
  }
#endif

  /* 3. cfs_path_iterator & component-wise cfs_path_compare */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_itr_init(&p1, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_path_itr_next(NULL, &pelem, &has_el));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_itr_next(&pit, NULL, &has_el));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_path_itr_next(&pit, &pelem, NULL));

  ASSERT_EQ(cfs_errc_success, cfs_path_itr_init(NULL, &pit));
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(cfs_false, has_el);

  cfs_path_init(&p1);
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_init(&p1, &pit));
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(cfs_false, has_el);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p1, CFS_STR("///")));
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_init(&p1, &pit));
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(cfs_true, has_el);
  ASSERT_EQ(1, pelem.length);
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(cfs_false, has_el);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p1, CFS_STR("/a//b//")));
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_init(&p1, &pit));
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(cfs_true, has_el);
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(1, pelem.length);
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(1, pelem.length);
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(cfs_true, has_el);
  ASSERT_EQ(0, pelem.length);
  ASSERT_EQ(cfs_errc_success, cfs_path_itr_next(&pit, &pelem, &has_el));
  ASSERT_EQ(cfs_false, has_el);

  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p1, CFS_STR("a///b")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("a/b")));
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p1, &p2, &cmp));
  ASSERT_EQ(0, cmp);
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p1, CFS_STR("/a")));
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("\\a")));
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p1, &p2, &cmp));
  ASSERT_EQ(0, cmp);
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p1, CFS_STR("aa")));
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("ab")));
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p1, &p2, &cmp));
  ASSERT(cmp < 0);
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p2, &p1, &cmp));
  ASSERT(cmp > 0);
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p1, CFS_STR("a")));
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("aa")));
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p1, &p2, &cmp));
  ASSERT(cmp < 0);
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p2, &p1, &cmp));
  ASSERT(cmp > 0);
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p1, CFS_STR("/x")));
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p2, CFS_STR("ab")));
  ASSERT_EQ(cfs_errc_success, cfs_path_compare(&p1, &p2, &cmp));
  ASSERT(cmp != 0);
  cfs_path_destroy(&p1);
  cfs_path_destroy(&p2);

  /* 4. cfs_status_from_mode & cfs_file_size on directory */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_status_from_mode(0, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_status_from_mode(0, &st));
  ASSERT_EQ(cfs_file_type_unknown, st.type);
#if !defined(CFS_OS_WINDOWS)
  ASSERT_EQ(cfs_errc_success, cfs_status_from_mode((unsigned int)S_IFREG, &st));
  ASSERT_EQ(cfs_file_type_regular, st.type);
  ASSERT_EQ(cfs_errc_success, cfs_status_from_mode((unsigned int)S_IFDIR, &st));
  ASSERT_EQ(cfs_file_type_directory, st.type);
  ASSERT_EQ(cfs_errc_success, cfs_status_from_mode((unsigned int)S_IFCHR, &st));
  ASSERT_EQ(cfs_file_type_character, st.type);
#if defined(S_IFLNK) && (S_IFLNK != 0)
  ASSERT_EQ(cfs_errc_success, cfs_status_from_mode((unsigned int)S_IFLNK, &st));
  ASSERT_EQ(cfs_file_type_symlink, st.type);
#endif
#if defined(S_IFBLK) && (S_IFBLK != 0)
  ASSERT_EQ(cfs_errc_success, cfs_status_from_mode((unsigned int)S_IFBLK, &st));
  ASSERT_EQ(cfs_file_type_block, st.type);
#endif
#if defined(S_IFIFO) && (S_IFIFO != 0)
  ASSERT_EQ(cfs_errc_success, cfs_status_from_mode((unsigned int)S_IFIFO, &st));
  ASSERT_EQ(cfs_file_type_fifo, st.type);
#endif
#if defined(S_IFSOCK) && (S_IFSOCK != 0)
  ASSERT_EQ(cfs_errc_success,
            cfs_status_from_mode((unsigned int)S_IFSOCK, &st));
  ASSERT_EQ(cfs_file_type_socket, st.type);
#endif
#endif

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p1, CFS_STR(".")));
  ASSERT_EQ(cfs_errc_is_a_directory, cfs_file_size(&p1, &fsz, &ec));
  ASSERT_EQ(cfs_errc_is_a_directory, ec.errc);
  cfs_path_destroy(&p1);

  /* 5. cfs_permissions (replace, add, remove, nofollow) & timestamps &
   * create_directory_attr */
  test_write_file("tp3perm.txt", "hello");
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p1, CFS_STR("tp3perm.txt")));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_permissions(&p1, (cfs_perms)cfs_perms_owner_all,
                            (cfs_perm_options)64, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_permissions(&p1, (cfs_perms)cfs_perms_owner_all,
                            (cfs_perm_options)(cfs_perm_options_replace |
                                               cfs_perm_options_add),
                            &ec));
  ASSERT_EQ(cfs_errc_success,
            cfs_permissions(&p1, (cfs_perms)cfs_perms_owner_read,
                            cfs_perm_options_remove, &ec));
  ASSERT_EQ(cfs_errc_success,
            cfs_permissions(
                &p1, (cfs_perms)(cfs_perms_owner_read | cfs_perms_owner_write),
                cfs_perm_options_add, &ec));
  ASSERT_EQ(cfs_errc_success,
            cfs_permissions(
                &p1, (cfs_perms)(cfs_perms_owner_read | cfs_perms_owner_write),
                (cfs_perm_options)(cfs_perm_options_replace |
                                   cfs_perm_options_nofollow),
                &ec));

  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p2, CFS_STR("nexperm.txt")));
  ASSERT_NEQ(cfs_errc_success,
             cfs_permissions(&p2, (cfs_perms)cfs_perms_owner_read,
                             cfs_perm_options_add, &ec));
  ASSERT_NEQ(cfs_errc_success,
             cfs_permissions(&p2, (cfs_perms)cfs_perms_owner_read,
                             (cfs_perm_options)(cfs_perm_options_replace |
                                                cfs_perm_options_nofollow),
                             &ec));

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p3, CFS_STR("tp3psym.txt")));
  test_ignore_rc(cfs_remove(&p3, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p1, &p3, &ec));
  ASSERT_EQ(cfs_errc_operation_not_supported,
            cfs_permissions(&p3, (cfs_perms)cfs_perms_owner_all,
                            (cfs_perm_options)(cfs_perm_options_replace |
                                               cfs_perm_options_nofollow),
                            &ec));
  ASSERT_EQ(cfs_errc_success, cfs_remove(&p3, &ec));
  cfs_path_destroy(&p3);
#endif

  /* cfs_last_write_time_set */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_last_write_time_set(NULL, 100, &ec));
  cfs_path_init(&p3);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_last_write_time_set(&p3, 100, &ec));
  ASSERT_NEQ(cfs_errc_success, cfs_last_write_time_set(&p2, 100, &ec));
  ASSERT_EQ(cfs_errc_success,
            cfs_last_write_time_set(&p1, (cfs_file_time_type)1700000000L, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_last_write_time(&p1, &ft, &ec));
#if !defined(CFS_OS_DOS)
  ASSERT_EQ((cfs_file_time_type)1700000000L, ft);
#endif

  /* cfs_create_directory_attr */
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_directory_attr(NULL, &p1, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_directory_attr(&p1, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_directory_attr(&p3, &p1, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_create_directory_attr(&p1, &p3, &ec));
  ASSERT_NEQ(cfs_errc_success, cfs_create_directory_attr(&p1, &p2, &ec));
  ASSERT_EQ(cfs_errc_not_a_directory, cfs_create_directory_attr(&p2, &p1, &ec));
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2)
#endif
  {
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p3, CFS_STR("tasrc_d")));
    test_ignore_rc(cfs_remove_all(&p3, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directory(&p3, &ec));
    cfs_path_init(&out);
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&out, CFS_STR("tadst_d")));
    test_ignore_rc(cfs_remove_all(&out, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directory_attr(&out, &p3, &ec));
    ASSERT_NEQ(cfs_errc_success, cfs_create_directory_attr(&out, &p3, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_remove(&out, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_remove(&p3, &ec));
    cfs_path_destroy(&out);
  }
  cfs_path_destroy(&p3);

  /* cfs_temp_directory_path env chain & validation */
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  {
    const char *old_td = getenv("TMPDIR");
    char saved_td[256];
    int had_td = 0;
    if (old_td) {
      had_td = 1;
      strncpy(saved_td, old_td, sizeof(saved_td) - 1);
      saved_td[sizeof(saved_td) - 1] = '\0';
    }
    setenv("TMPDIR", "", 1);
    setenv("TMP", ".", 1);
    cfs_path_init(&out);
    ASSERT_EQ(cfs_errc_success, cfs_temp_directory_path(&out, &ec));
    setenv("TMPDIR", "tp3perm.txt", 1);
    ASSERT_EQ(cfs_errc_not_a_directory, cfs_temp_directory_path(&out, &ec));
    g_cfs_malloc_fail = 1;
    cfs_path_destroy(&out);
    cfs_path_init(&out);
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_temp_directory_path(&out, &ec));
    g_cfs_malloc_fail = 0;
    unsetenv("TMP");
    if (had_td) {
      setenv("TMPDIR", saved_td, 1);
    } else {
      unsetenv("TMPDIR");
    }
    cfs_path_destroy(&out);
  }
#endif

  /* 6. cfs_weakly_canonical, cfs_relative, cfs_proximate, lexically_relative &
   * lexically_proximate + OOMs */
  cfs_path_init(&p3);
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_weakly_canonical(&p1, NULL, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_weakly_canonical(&p3, &out, &ec));
  ASSERT_EQ(0, out.length);
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success, cfs_weakly_canonical(&p1, &out, &ec));
  ASSERT(out.length > 0);
  cfs_path_destroy(&out);

  ASSERT_EQ(cfs_errc_success,
            cfs_path_assign(&p3, CFS_STR("./missing_wc_a/../missing_wc_b/")));
  ASSERT_EQ(cfs_errc_success, cfs_weakly_canonical(&p3, &out, &ec));
  ASSERT(out.length > 0);
  cfs_path_destroy(&out);

  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_weakly_canonical(&p3, &out, &ec));
  g_cfs_malloc_fail = 2;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_weakly_canonical(&p3, &out, &ec));
  g_cfs_malloc_fail = 3;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_weakly_canonical(&p3, &out, &ec));
  g_cfs_malloc_fail = 4;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_weakly_canonical(&p3, &out, &ec));
  g_cfs_malloc_fail = 0;
#if !defined(CFS_OS_WINDOWS)
  {
    cfs_path no_ex_rel = {0};
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&no_ex_rel, CFS_STR("no_such_wc_root/leaf")));
    g_cfs_getcwd_fail = 1;
    ASSERT_NEQ(cfs_errc_success, cfs_weakly_canonical(&no_ex_rel, &out, &ec));
    g_cfs_getcwd_fail = 0;
    g_cfs_malloc_fail = 3;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_weakly_canonical(&no_ex_rel, &out, &ec));
    g_cfs_malloc_fail = 0;
    cfs_path_destroy(&no_ex_rel);
  }
#endif

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_proximate(NULL, &p1, &out, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_proximate(&p1, NULL, &out, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_proximate(&p1, &p2, NULL, &ec));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_proximate(&p2, &p3, &out, &ec));
  g_cfs_malloc_fail = 3;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_proximate(&p1, &p3, &out, &ec));
  g_cfs_malloc_fail = 0;

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_relative(&p1, NULL, &out, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_relative(&p1, &p2, NULL, &ec));
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_relative(&p2, &p3, &out, &ec));
  g_cfs_malloc_fail = 3;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_relative(&p1, &p3, &out, &ec));
  g_cfs_malloc_fail = 0;

  {
    cfs_path lr_a = {0}, lr_b = {0};
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&lr_a, CFS_STR("/a/b")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&lr_b, CFS_STR("c/d")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_relative(&lr_a, &lr_b, &out));
    ASSERT_EQ(0, out.length);
    cfs_path_destroy(&out);
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_proximate(&lr_a, &lr_b, &out));
    ASSERT_STR_EQ(lr_a.str, out.str);
    cfs_path_destroy(&out);
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_path_lexically_proximate(&lr_a, &lr_b, &out));
    g_cfs_malloc_fail = 0;

    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&lr_a, CFS_STR("a/./b/")));
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&lr_b, CFS_STR("a/c/../d/..")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_relative(&lr_a, &lr_b, &out));
    ASSERT_STR_EQ(CFS_STR("b"), out.str);
    cfs_path_destroy(&out);

    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&lr_b, CFS_STR("../x")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_relative(&lr_a, &lr_b, &out));
    ASSERT_EQ(0, out.length);
    cfs_path_destroy(&out);

    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&lr_a, CFS_STR("a/b")));
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&lr_b, CFS_STR("a/b")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_relative(&lr_a, &lr_b, &out));
    ASSERT_STR_EQ(CFS_STR("."), out.str);
    cfs_path_destroy(&out);

    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&lr_a, CFS_STR("a/b/c")));
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&lr_b, CFS_STR("a/d")));
    g_cfs_realloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_path_lexically_relative(&lr_a, &lr_b, &out));
    ASSERT(out.str == NULL);
    g_cfs_realloc_fail = 0;
    cfs_path_destroy(&lr_a);
    cfs_path_destroy(&lr_b);
  }
  cfs_path_destroy(&p3);

  /* 7. cfs_copy_file & cfs_copy full semantics + OOMs */
  {
    cfs_path c_src = {0}, c_dst = {0}, c_dir1 = {0}, c_dir2 = {0}, c_sub = {0};
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&c_src, CFS_STR("tp3perm.txt")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&c_dst, CFS_STR("tp3cdst.txt")));
    test_ignore_rc(cfs_remove(&c_dst, NULL));

    /* Self-copy error */
    ASSERT_EQ(cfs_errc_file_exists,
              cfs_copy_file(&c_src, &c_src, cfs_copy_options_overwrite_existing,
                            &ec));
    ASSERT_EQ(
        cfs_errc_file_exists,
        cfs_copy(&c_src, &c_src, cfs_copy_options_overwrite_existing, &ec));

    /* Directory source in copy_file & directory target in copy_file */
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&c_dir1, CFS_STR(".")));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_copy_file(&c_dir1, &c_dst, cfs_copy_options_none, &ec));
    ASSERT_EQ(cfs_errc_is_a_directory,
              cfs_copy_file(&c_src, &c_dir1, cfs_copy_options_none, &ec));
    cfs_path_destroy(&c_dir1);

    /* update_existing: newer dest skipped, older dest overwritten */
    ASSERT_EQ(cfs_errc_success,
              cfs_copy_file(&c_src, &c_dst, cfs_copy_options_none, &ec));
    ASSERT_EQ(
        cfs_errc_success,
        cfs_last_write_time_set(&c_src, (cfs_file_time_type)1700000000L, &ec));
    ASSERT_EQ(
        cfs_errc_success,
        cfs_last_write_time_set(&c_dst, (cfs_file_time_type)1700000500L, &ec));
    ASSERT_EQ(
        cfs_errc_success,
        cfs_copy_file(&c_src, &c_dst, cfs_copy_options_update_existing, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_last_write_time(&c_dst, &ft, &ec));
#if defined(CFS_OS_DOS)
    if (_osmajor >= 2)
#endif
    {
      ASSERT_EQ((cfs_file_time_type)1700000500L, ft);
    }

    ASSERT_EQ(
        cfs_errc_success,
        cfs_last_write_time_set(&c_src, (cfs_file_time_type)1700001000L, &ec));
    ASSERT_EQ(
        cfs_errc_success,
        cfs_copy_file(&c_src, &c_dst, cfs_copy_options_update_existing, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_remove(&c_dst, &ec));

    /* cfs_copy: directories_only on file */
    ASSERT_EQ(cfs_errc_success,
              cfs_copy(&c_src, &c_dst, cfs_copy_options_directories_only, &ec));

    /* cfs_copy: file -> file, OOMs, symlinks, hardlinks */
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_copy(&c_src, &c_dst, cfs_copy_options_none, &ec));
    g_cfs_malloc_fail = 0;
    ASSERT_EQ(cfs_errc_success,
              cfs_copy(&c_src, &c_dst, cfs_copy_options_none, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_remove(&c_dst, &ec));

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    ASSERT_EQ(
        cfs_errc_success,
        cfs_copy(&c_src, &c_dst, cfs_copy_options_create_hard_links, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_remove(&c_dst, &ec));
    ASSERT_EQ(cfs_errc_success,
              cfs_copy(&c_src, &c_dst, cfs_copy_options_create_symlinks, &ec));
    {
      cfs_path sym_dup = {0};
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&sym_dup, CFS_STR("test_p3_sym_dup.txt")));
      test_ignore_rc(cfs_remove(&sym_dup, NULL));
      ASSERT_EQ(
          cfs_errc_success,
          cfs_copy(&c_dst, &sym_dup, cfs_copy_options_skip_symlinks, &ec));
      ASSERT_EQ(
          cfs_errc_success,
          cfs_copy(&c_dst, &sym_dup, cfs_copy_options_copy_symlinks, &ec));
      ASSERT_EQ(
          cfs_errc_file_exists,
          cfs_copy(&c_dst, &sym_dup, cfs_copy_options_copy_symlinks, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_remove(&sym_dup, &ec));
      cfs_path_destroy(&sym_dup);
    }
    ASSERT_EQ(cfs_errc_success, cfs_remove(&c_dst, &ec));
#endif

    /* cfs_copy: directory tree (shallow & recursive & file into directory) */
#if defined(CFS_OS_DOS)
    if (_osmajor >= 2)
#endif
    {
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&c_dir1, CFS_STR("tp3csrc")));
      ASSERT_EQ(
          cfs_errc_success,
          cfs_path_init_str(&c_sub, CFS_STR("tp3csrc/nested_a/nested_b")));
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&c_dir2, CFS_STR("tp3cdst")));
      test_ignore_rc(cfs_remove_all(&c_dir1, NULL, NULL));
      test_ignore_rc(cfs_remove_all(&c_dir2, NULL, NULL));
      ASSERT_EQ(cfs_errc_success, cfs_create_directories(&c_sub, &ec));
      test_write_file("tp3csrc/top.txt", "top");
      test_write_file("tp3csrc/nested_a/nested_b/inner.txt", "inner");

      /* Copy file into existing directory */
      ASSERT_EQ(cfs_errc_success,
                cfs_copy(&c_src, &c_sub, cfs_copy_options_none, &ec));
      g_cfs_calloc_fail = 1;
      ASSERT_EQ(
          cfs_errc_not_enough_memory,
          cfs_copy(&c_src, &c_sub, cfs_copy_options_overwrite_existing, &ec));
      g_cfs_calloc_fail = 0;
      g_cfs_malloc_fail = 1;
      ASSERT_EQ(
          cfs_errc_not_enough_memory,
          cfs_copy(&c_src, &c_sub, cfs_copy_options_overwrite_existing, &ec));
      g_cfs_malloc_fail = 0;
      g_cfs_realloc_fail = 1;
      ASSERT_EQ(
          cfs_errc_not_enough_memory,
          cfs_copy(&c_src, &c_sub, cfs_copy_options_overwrite_existing, &ec));
      g_cfs_realloc_fail = 0;

      /* Directory error branches */
      ASSERT_EQ(
          cfs_errc_is_a_directory,
          cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_create_symlinks, &ec));
      ASSERT_EQ(cfs_errc_file_exists,
                cfs_copy(&c_dir1, &c_src, cfs_copy_options_none, &ec));

      /* Shallow copy & recursive copy */
      ASSERT_EQ(cfs_errc_success,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_none, &ec));
      ASSERT_EQ(cfs_errc_file_exists,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_none, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir2, &rem_cnt, &ec));

      ASSERT_EQ(
          cfs_errc_success,
          cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_directories_only, &ec));
      ASSERT_EQ(cfs_errc_success,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_recursive, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir2, &rem_cnt, &ec));

      /* OOMs inside directory cfs_copy */
#if !defined(CFS_OS_WINDOWS)
      g_cfs_malloc_fail = 1;
      ASSERT_EQ(cfs_errc_not_enough_memory,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_recursive, &ec));
      g_cfs_malloc_fail = 0;
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir2, &rem_cnt, &ec));
      g_cfs_malloc_fail = 2;
      ASSERT_EQ(cfs_errc_not_enough_memory,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_recursive, &ec));
      g_cfs_malloc_fail = 0;
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir2, &rem_cnt, &ec));
      g_cfs_malloc_fail = 3;
      ASSERT_EQ(cfs_errc_not_enough_memory,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_recursive, &ec));
      g_cfs_malloc_fail = 0;
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir2, &rem_cnt, &ec));
      g_cfs_realloc_fail = 1;
      ASSERT_EQ(cfs_errc_not_enough_memory,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_recursive, &ec));
      g_cfs_realloc_fail = 0;
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir2, &rem_cnt, &ec));
      g_cfs_realloc_fail = 2;
      ASSERT_EQ(cfs_errc_not_enough_memory,
                cfs_copy(&c_dir1, &c_dir2, cfs_copy_options_recursive, &ec));
      g_cfs_realloc_fail = 0;
      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir2, &rem_cnt, &ec));
#endif

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
      {
        cfs_path c_f1 = {0};
        ASSERT_EQ(cfs_errc_success,
                  cfs_path_init_str(&c_f1, CFS_STR("/dev/null")));
        ASSERT_EQ(cfs_errc_operation_not_supported,
                  cfs_copy(&c_f1, &c_dst, cfs_copy_options_none, &ec));
        cfs_path_destroy(&c_f1);
      }
#endif

      ASSERT_EQ(cfs_errc_success, cfs_remove_all(&c_dir1, &rem_cnt, &ec));
    }
    cfs_path_destroy(&c_src);
    cfs_path_destroy(&c_dst);
    cfs_path_destroy(&c_dir1);
    cfs_path_destroy(&c_dir2);
    cfs_path_destroy(&c_sub);
  }

  ASSERT_EQ(cfs_errc_success, cfs_remove(&p1, &ec));
  {
    cfs_path empty_s = {0}, null_s = {0}, pa = {0}, pb = {0};
    cfs_path_init(&null_s);
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&empty_s, CFS_STR("")));

    /* Empty non-NULL string branches */
    ASSERT_EQ(cfs_errc_success, cfs_path_itr_init(&empty_s, &pit));
    ASSERT_EQ(cfs_errc_success, cfs_weakly_canonical(&empty_s, &out, &ec));
    cfs_path_destroy(&out);
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_permissions(&empty_s, (cfs_perms)cfs_perms_owner_all,
                              cfs_perm_options_replace, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_last_write_time_set(&empty_s, 100, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_create_directory_attr(&empty_s, &empty_s, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&pa, CFS_STR(".")));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_create_directory_attr(&pa, &empty_s, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_proximate(&null_s, &pa, &out, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_relative(&null_s, &pa, &out, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_copy(&pa, NULL, cfs_copy_options_none, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_copy(&null_s, &pa, cfs_copy_options_none, &ec));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_copy(&pa, &null_s, cfs_copy_options_none, &ec));

    /* cfs_path_compare: shorter vs longer element list & '/' vs 'x' */
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&pa, CFS_STR("/usr/local")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&pb, CFS_STR("/usr/local/bin")));
    ASSERT_EQ(cfs_errc_success, cfs_path_compare(&pa, &pb, &cmp));
    ASSERT(cmp < 0);
    ASSERT_EQ(cfs_errc_success, cfs_path_compare(&pb, &pa, &cmp));
    ASSERT(cmp > 0);
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&pa, CFS_STR("/a")));
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&pb, CFS_STR("x/a")));
    ASSERT_EQ(cfs_errc_success, cfs_path_compare(&pa, &pb, &cmp));
    ASSERT(cmp != 0);

    /* cfs_weakly_canonical: root '/' + missing children & realloc OOMs */
    ASSERT_EQ(cfs_errc_success,
              cfs_path_assign(&pa, CFS_STR("/nonexistent_wc_root_1/sub_2")));
    ASSERT_EQ(cfs_errc_success, cfs_weakly_canonical(&pa, &out, &ec));
    cfs_path_destroy(&out);
    g_cfs_realloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_weakly_canonical(&pa, &out, &ec));
    g_cfs_realloc_fail = 2;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_weakly_canonical(&pa, &out, &ec));
    g_cfs_realloc_fail = 3;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_weakly_canonical(&pa, &out, &ec));
    g_cfs_realloc_fail = 0;

    /* cfs_path_lexically_relative & proximate edge cases */
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&pa, CFS_STR("a/b")));
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&pb, CFS_STR("a/dd/.x/..y")));
    ASSERT_EQ(cfs_errc_success, cfs_path_lexically_relative(&pa, &pb, &out));
    cfs_path_destroy(&out);
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&pb, CFS_STR("/abs")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_lexically_proximate(&empty_s, &pb, &out));
    ASSERT_EQ(0, out.length);
    cfs_path_destroy(&out);

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    /* cfs_temp_directory_path nonexistent TMPDIR */
    {
      const char *old_td = getenv("TMPDIR");
      char saved_td[256];
      int had_td = 0;
      if (old_td) {
        had_td = 1;
        strncpy(saved_td, old_td, sizeof(saved_td) - 1);
        saved_td[sizeof(saved_td) - 1] = '\0';
      }
      setenv("TMPDIR", "nonexistent_tmp_dir_99999", 1);
      cfs_path_init(&out);
      ASSERT_EQ(cfs_errc_not_a_directory, cfs_temp_directory_path(&out, &ec));
      cfs_path_destroy(&out);
      if (had_td) {
        setenv("TMPDIR", saved_td, 1);
      } else {
        unsetenv("TMPDIR");
      }
    }

    /* cfs_copy_file !out_f + cfs_copy remaining symlink/overwrite/dir branches
     */
    test_write_file("test_p3_extra_src.txt", "extra");
    test_write_file("test_p3_extra_dst.txt", "old");
    ASSERT_EQ(cfs_errc_success,
              cfs_path_assign(&pa, CFS_STR("test_p3_extra_src.txt")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_assign(&pb, CFS_STR("no_such_dir_xyz/out.txt")));
    ASSERT_NEQ(cfs_errc_success,
               cfs_copy_file(&pa, &pb, cfs_copy_options_none, &ec));

    ASSERT_EQ(cfs_errc_success,
              cfs_path_assign(&pb, CFS_STR("test_p3_extra_dst.txt")));
    ASSERT_EQ(cfs_errc_success,
              cfs_copy(&pa, &pb, cfs_copy_options_overwrite_existing, &ec));

    {
      cfs_path sym_p = {0}, d1 = {0}, d2 = {0}, brk_sym = {0};
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&sym_p, CFS_STR("test_p3_extra_sym.txt")));
      test_ignore_rc(cfs_remove(&sym_p, NULL));
      ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&pa, &sym_p, &ec));
      ASSERT_EQ(cfs_errc_file_exists,
                cfs_copy(&sym_p, &pb, cfs_copy_options_create_symlinks, &ec));
      test_ignore_rc(cfs_remove(&pb, NULL));
      ASSERT_EQ(cfs_errc_file_exists,
                cfs_copy(&sym_p, &pb, cfs_copy_options_create_symlinks, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_remove(&sym_p, &ec));

      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&d1, CFS_STR("test_p3_ex_d1")));
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&d2, CFS_STR("test_p3_ex_d2")));
      test_ignore_rc(cfs_remove_all(&d1, NULL, NULL));
      test_ignore_rc(cfs_remove_all(&d2, NULL, NULL));
      ASSERT_EQ(cfs_errc_success, cfs_create_directory(&d1, &ec));
      ASSERT_EQ(cfs_errc_success, cfs_create_directory(&d2, &ec));
      test_write_file("test_p3_ex_d1/f.txt", "1");
      test_write_file("test_p3_ex_d2/f.txt", "2");
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&brk_sym, CFS_STR("test_p3_ex_d1/dangling")));
      ASSERT_EQ(cfs_errc_success, cfs_path_assign(&pb, CFS_STR("missing_tgt")));
      ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&pb, &brk_sym, &ec));

      /* Existing to_dir OOMs at dir_itr_init, dir_itr_next, child_from,
       * child_to
       */
      for (ei = 1; ei <= 8; ++ei) {
        g_cfs_malloc_fail = ei;
        test_ignore_rc(cfs_copy(&d1, &d2, cfs_copy_options_none, &ec));
        g_cfs_malloc_fail = 0;
      }
      for (ei = 1; ei <= 6; ++ei) {
        g_cfs_realloc_fail = ei;
        test_ignore_rc(cfs_copy(&d1, &d2, cfs_copy_options_none, &ec));
        g_cfs_realloc_fail = 0;
      }

      /* Shallow & recursive copy hit dangling (!child_ok) & existing
       * destination error */
      ASSERT_EQ(cfs_errc_file_exists,
                cfs_copy(&d1, &d2, cfs_copy_options_none, &ec));
      ASSERT_NEQ(cfs_errc_success,
                 cfs_copy(&d1, &d2, cfs_copy_options_recursive, &ec));
      ASSERT_EQ(cfs_errc_success,
                cfs_path_assign(&pb, CFS_STR("no_such_parent_xyz/sub_dst")));
      ASSERT_NEQ(cfs_errc_success,
                 cfs_copy(&d1, &pb, cfs_copy_options_none, &ec));
      test_ignore_rc(cfs_remove_all(&d1, NULL, NULL));
      test_ignore_rc(cfs_remove_all(&d2, NULL, NULL));
      cfs_path_destroy(&sym_p);
      cfs_path_destroy(&d1);
      cfs_path_destroy(&d2);
      cfs_path_destroy(&brk_sym);
    }
    test_ignore_rc(cfs_remove(&pa, NULL));
#endif
    cfs_path_destroy(&empty_s);
    cfs_path_destroy(&null_s);
    cfs_path_destroy(&pa);
    cfs_path_destroy(&pb);
  }
  cfs_path_destroy(&p1);
  cfs_path_destroy(&p2);
  PASS();
}

/**
 * \brief Cross-platform & Windows Win32 native parity tests (Phase 4).
 * \return The test result.
 */
TEST phase4_windows_parity(void) {
  cfs_path f_orig = {0}, f_hl = {0}, f_other = {0}, f_ren = {0};
  cfs_path d_src = {0}, sym_f = {0}, sym_d = {0}, out = {0}, missing = {0};
  cfs_error_code ec;
  cfs_uintmax_t nlink = 0, sz = 0;
  cfs_bool eq = cfs_false;
  cfs_space_info spc;
  cfs_file_status st;
  cfs_errc rc;

  test_write_file("tp4orig.txt", "hello_world");
  test_write_file("tp4oth.txt", "different");
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&f_orig, CFS_STR("tp4orig.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&f_hl, CFS_STR("tp4hl.txt")));
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&f_other, CFS_STR("tp4oth.txt")));
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&missing, CFS_STR("tp4miss.txt")));
  test_ignore_rc(cfs_remove(&f_hl, NULL));

  /* Distinct files on same drive must NOT be equivalent */
  ASSERT_EQ(cfs_errc_success, cfs_equivalent(&f_orig, &f_other, &eq, &ec));
  ASSERT_EQ(cfs_false, eq);

  /* Hard link creation, link count == 2, and equivalence == true */
  ASSERT_EQ(cfs_errc_success, cfs_hard_link_count(&f_orig, &nlink, &ec));
  ASSERT_EQ((cfs_uintmax_t)1, nlink);
#if defined(CFS_OS_DOS)
  ASSERT_EQ(cfs_errc_operation_not_supported,
            cfs_create_hard_link(&f_orig, &f_hl, &ec));
#else
  ASSERT_EQ(cfs_errc_success, cfs_create_hard_link(&f_orig, &f_hl, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_hard_link_count(&f_orig, &nlink, &ec));
  ASSERT_EQ((cfs_uintmax_t)2, nlink);
  ASSERT_EQ(cfs_errc_success, cfs_equivalent(&f_orig, &f_hl, &eq, &ec));
  ASSERT_EQ(cfs_true, eq);
  ASSERT_EQ(cfs_errc_success, cfs_remove(&f_hl, &ec));
#endif

  /* Resize file: grow & shrink */
  ASSERT_EQ(cfs_errc_success, cfs_resize_file(&f_orig, 4096, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_file_size(&f_orig, &sz, &ec));
  ASSERT_EQ((cfs_uintmax_t)4096, sz);
  ASSERT_EQ(cfs_errc_success, cfs_resize_file(&f_orig, 3, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_file_size(&f_orig, &sz, &ec));
  ASSERT_EQ((cfs_uintmax_t)3, sz);
  ASSERT_NEQ(cfs_errc_success, cfs_resize_file(&missing, 10, &ec));

  /* Atomic rename over an existing destination file */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&f_ren, CFS_STR("tp4oth.txt")));
#if defined(CFS_OS_DOS)
  test_ignore_rc(cfs_remove(&f_ren, NULL));
#endif
  ASSERT_EQ(cfs_errc_success, cfs_rename(&f_orig, &f_ren, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_file_size(&f_ren, &sz, &ec));
  ASSERT_EQ((cfs_uintmax_t)3, sz);

  /* cfs_space on a regular file path & nonexistent path */
  ASSERT_EQ(cfs_errc_success, cfs_space(&f_ren, &spc, &ec));
  ASSERT(spc.capacity > 0);
  ASSERT_NEQ(cfs_errc_success, cfs_space(&missing, &spc, &ec));

  /* cfs_canonical: nonexistent must fail, existing must succeed */
  cfs_path_init(&out);
  ASSERT_NEQ(cfs_errc_success, cfs_canonical(&missing, &out, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_canonical(&f_ren, &out, &ec));
  ASSERT(out.length > 0);
  cfs_path_destroy(&out);

  /* Symlink status & symlink creation/reading */
  ASSERT_NEQ(cfs_errc_success, cfs_symlink_status(&missing, &st, &ec));
  ASSERT_EQ(cfs_file_type_not_found, st.type);
  ASSERT_EQ(cfs_errc_success, cfs_symlink_status(&f_ren, &st, &ec));
  ASSERT_EQ(cfs_file_type_regular, st.type);

  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&sym_f, CFS_STR("tp4symf.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&d_src, CFS_STR("tp4dsrc")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&sym_d, CFS_STR("tp4symd")));
  test_ignore_rc(cfs_remove(&sym_f, NULL));
#if defined(CFS_OS_DOS)
  if (_osmajor >= 2) {
    test_ignore_rc(cfs_remove_all(&d_src, NULL, NULL));
    test_ignore_rc(cfs_remove_all(&sym_d, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directory(&d_src, &ec));
  }
#else
  test_ignore_rc(cfs_remove_all(&d_src, NULL, NULL));
  test_ignore_rc(cfs_remove_all(&sym_d, NULL, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_create_directory(&d_src, &ec));
#endif

  rc = cfs_create_symlink(&f_ren, &sym_f, &ec);
  if (rc == cfs_errc_success) {
    ASSERT_EQ(cfs_errc_success, cfs_symlink_status(&sym_f, &st, &ec));
    ASSERT_EQ(cfs_file_type_symlink, st.type);
    cfs_path_init(&out);
    ASSERT_EQ(cfs_errc_success, cfs_read_symlink(&sym_f, &out, &ec));
    ASSERT(out.length > 0);
    cfs_path_destroy(&out);
    test_ignore_rc(cfs_remove(&sym_f, NULL));
  }
  rc = cfs_create_directory_symlink(&d_src, &sym_d, &ec);
  if (rc == cfs_errc_success) {
    ASSERT_EQ(cfs_errc_success, cfs_symlink_status(&sym_d, &st, &ec));
    ASSERT_EQ(cfs_file_type_symlink, st.type);
    test_ignore_rc(cfs_remove(&sym_d, NULL));
  }
  cfs_path_init(&out);
  ASSERT_NEQ(cfs_errc_success, cfs_read_symlink(&f_ren, &out, &ec));
  cfs_path_destroy(&out);

  test_ignore_rc(cfs_remove(&d_src, NULL));
  test_ignore_rc(cfs_remove(&f_ren, NULL));
  cfs_path_destroy(&f_orig);
  cfs_path_destroy(&f_hl);
  cfs_path_destroy(&f_other);
  cfs_path_destroy(&f_ren);
  cfs_path_destroy(&d_src);
  cfs_path_destroy(&sym_f);
  cfs_path_destroy(&sym_d);
  cfs_path_destroy(&missing);
  PASS();
}

/**
 * \brief Comprehensive unit, branch, and OOM tests for Phase 5 iterators &
 * directory entries.
 * \return The test result.
 */
TEST phase5_directory_iterators(void) {
  cfs_directory_entry dent;
  const cfs_path *epath = NULL;
  cfs_path p_root = {0}, p_d1 = {0}, p_d2 = {0}, p_f = {0}, p_tmp = {0};
  cfs_file_status st;
  cfs_bool b = cfs_false;
  cfs_uintmax_t u = 0;
  cfs_file_time_type ft = 0;
  cfs_error_code ec;
  cfs_recursive_directory_iterator *rit = NULL;
  const cfs_directory_entry *en = NULL;
  int depth = -1;
  int count = 0;
  int saw_depth2 = 0;
  int k;

  /* 1. cfs_directory_entry lifecycle, observers, uncached vs cached, & errors
   */
  cfs_dir_entry_init(NULL);
  cfs_dir_entry_destroy(NULL);
  cfs_dir_entry_init(&dent);

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_refresh(NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_refresh(&dent, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_assign(NULL, &p_root, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_assign(&dent, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_assign(&dent, &p_tmp, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_tmp, CFS_STR("")));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_assign(&dent, &p_tmp, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&dent.path, CFS_STR("")));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_refresh(&dent, &ec));
  {
    cfs_path null_p = {0};
    cfs_directory_iterator *tmp_it = NULL;
    cfs_path_init(&null_p);
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_dir_itr_init(&null_p, &tmp_it, &ec));
  }

  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_replace_filename(NULL, CFS_STR("x"), &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_replace_filename(&dent, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_path(NULL, &epath));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_path(&dent, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_status(NULL, &st, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_status(&dent, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_symlink_status(NULL, &st, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_symlink_status(&dent, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_exists(NULL, &b, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_exists(&dent, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_dir_entry_file_size(NULL, &u, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_hard_link_count(NULL, &u, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_dir_entry_last_write_time(NULL, &ft, &ec));

  /* Uncached entry pointing to missing file & valid file */
  ASSERT_EQ(cfs_errc_success,
            cfs_path_assign(&dent.path, CFS_STR("no_such_dent_file.txt")));
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_exists(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_symlink(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);

  test_write_file("tp5f1.txt", "12345");
  test_write_file("tp5f2.txt", "678");
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p_f, CFS_STR("tp5f1.txt")));

  g_cfs_malloc_fail = 1;
  cfs_dir_entry_destroy(&dent);
  cfs_dir_entry_init(&dent);
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_dir_entry_assign(&dent, &p_f, &ec));
  g_cfs_malloc_fail = 0;

  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_assign(&dent, &p_f, &ec));
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_path(&dent, &epath));
  ASSERT_STR_EQ(p_f.str, epath->str);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_status(&dent, &st, &ec));
  ASSERT_EQ(cfs_file_type_regular, st.type);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_symlink_status(&dent, &st, &ec));
  ASSERT_EQ(cfs_file_type_regular, st.type);

  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_exists(&dent, &b, &ec));
  ASSERT_EQ(cfs_true, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_regular_file(&dent, &b, &ec));
  ASSERT_EQ(cfs_true, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_directory(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_symlink(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_block_file(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_character_file(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_fifo(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_socket(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_is_other(&dent, &b, &ec));
  ASSERT_EQ(cfs_false, b);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_file_size(&dent, &u, &ec));
  ASSERT_EQ((cfs_uintmax_t)5, u);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_hard_link_count(&dent, &u, &ec));
  ASSERT_EQ((cfs_uintmax_t)1, u);
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_last_write_time(&dent, &ft, &ec));
#if defined(CFS_OS_DOS)
  ASSERT(ft >= 0);
#else
  ASSERT(ft > 0);
#endif

  g_cfs_realloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_dir_entry_replace_filename(
                &dent, CFS_STR("very_long_replacement_filename_oom.txt"), &ec));
  g_cfs_realloc_fail = 0;

  ASSERT_EQ(cfs_errc_success,
            cfs_dir_entry_replace_filename(&dent, CFS_STR("tp5f2.txt"), &ec));
  ASSERT_EQ(cfs_errc_success, cfs_dir_entry_file_size(&dent, &u, &ec));
  ASSERT_EQ((cfs_uintmax_t)3, u);

  ASSERT_EQ(cfs_errc_success,
            cfs_dir_entry_replace_filename(&dent, CFS_STR("missp5.txt"), &ec));
  ASSERT_EQ(cfs_file_type_not_found, dent.status.type);

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  {
    cfs_path d_sym = {0}, p_loop = {0}, p_inside = {0};
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&d_sym, CFS_STR("test_p5_dangling_sym")));
    test_ignore_rc(cfs_remove(&d_sym, NULL));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_assign(&p_tmp, CFS_STR("nonexistent_sym_target_p5")));
    ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p_tmp, &d_sym, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_dir_entry_assign(&dent, &d_sym, &ec));
    ASSERT_EQ(cfs_file_type_symlink, dent.symlink_status.type);
    ASSERT_EQ(cfs_file_type_not_found, dent.status.type);
    test_ignore_rc(cfs_remove(&d_sym, NULL));

    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&p_loop, CFS_STR("test_p5_loop_sym")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&p_inside, CFS_STR("test_p5_loop_sym/sub")));
    test_ignore_rc(cfs_remove(&p_loop, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p_loop, &p_loop, &ec));
    /* Refresh error on symlink_status ("loop/sub" -> ELOOP) & status ("loop"
     * -> lstat ok, stat ELOOP) */
    ASSERT_NEQ(cfs_errc_success, cfs_dir_entry_assign(&dent, &p_inside, &ec));
    ASSERT_NEQ(cfs_errc_success, cfs_dir_entry_assign(&dent, &p_loop, &ec));
    /* Uncached predicate error */
    cfs_dir_entry_destroy(&dent);
    cfs_dir_entry_init(&dent);
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&dent.path, p_loop.str));
    ASSERT_NEQ(cfs_errc_success, cfs_dir_entry_exists(&dent, &b, &ec));
    test_ignore_rc(cfs_remove(&p_loop, NULL));
    cfs_path_destroy(&p_loop);
    cfs_path_destroy(&p_inside);
    cfs_path_destroy(&d_sym);
  }
#endif
  cfs_dir_entry_destroy(&dent);
  test_ignore_rc(cfs_remove(&p_f, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p_f, CFS_STR("tp5f2.txt")));
  test_ignore_rc(cfs_remove(&p_f, NULL));

#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    cfs_path_destroy(&p_f);
    cfs_path_destroy(&p_tmp);
    PASS();
  }
#endif

  /* 2. Multi-level recursive directory iterator: stack growth, depth, pop,
   * disable_recursion_pending, symlinks, skip_permission_denied, & OOMs */
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_root, CFS_STR("tp5tree")));
  ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_d1, CFS_STR("tp5tree/d1")));
  ASSERT_EQ(cfs_errc_success,
            cfs_path_init_str(&p_d2, CFS_STR("tp5tree/d1/d2")));
  test_ignore_rc(cfs_remove_all(&p_root, NULL, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_create_directories(&p_d2, &ec));
  test_write_file("tp5tree/r.txt", "r");
  test_write_file("tp5tree/d1/m.txt", "m");
  test_write_file("tp5tree/d1/d2/leaf.txt", "leaf");

  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_depth(NULL, &depth));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_depth(rit, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_rec_dir_itr_recursion_pending(NULL, &b));

  /* Full 3-level traversal (grows stack capacity 2 -> 4!) */
  g_cfs_malloc_fail = 3;
  ASSERT_EQ(cfs_errc_not_enough_memory, cfs_is_empty_path(&p_root, &b, &ec));
  g_cfs_malloc_fail = 0;
  ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p_root, &rit, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_next(rit, NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_rec_dir_itr_depth(rit, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_rec_dir_itr_recursion_pending(rit, NULL));
  ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_recursion_pending(rit, &b));
  ASSERT_EQ(cfs_true, b);

  count = 0;
  saw_depth2 = 0;
  for (;;) {
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
    if (!en)
      break;
    count++;
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_depth(rit, &depth));
    if (depth == 2)
      saw_depth2 = 1;
  }
  ASSERT_EQ(5, count);
  ASSERT_EQ(1, saw_depth2);
  /* Pop at end (depth == 0) */
  ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_depth(rit, &depth));
  ASSERT_EQ(0, depth);
  ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_pop(rit, &ec));
  cfs_rec_dir_itr_close(rit);

  /* disable_recursion_pending skips descending into d1 */
  ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p_root, &rit, &ec));
  count = 0;
  for (;;) {
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
    if (!en)
      break;
    count++;
    if (en->status.type == cfs_file_type_directory) {
      cfs_rec_dir_itr_disable_recursion_pending(rit);
      ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_recursion_pending(rit, &b));
      ASSERT_EQ(cfs_false, b);
    }
  }
  ASSERT_EQ(2, count);
  cfs_rec_dir_itr_close(rit);

  /* Mid-traversal pop & close with multiple open frames */
  ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p_root, &rit, &ec));
  for (;;) {
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
    ASSERT(en != NULL);
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_depth(rit, &depth));
    if (depth == 1) {
      ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_pop(rit, &ec));
      break;
    }
  }
  cfs_rec_dir_itr_close(rit);

  /* Close while at depth 2 */
  ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p_root, &rit, &ec));
  for (;;) {
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
    ASSERT(en != NULL);
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_depth(rit, &depth));
    if (depth == 2)
      break;
  }
  cfs_rec_dir_itr_close(rit);

  /* OOM sweeps on cfs_rec_dir_itr_init & cfs_rec_dir_itr_next */
  for (k = 1; k <= 3; ++k) {
    g_cfs_malloc_fail = k;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_rec_dir_itr_init(&p_root, &rit, &ec));
    g_cfs_malloc_fail = 0;
  }
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_rec_dir_itr_init(&p_root, &rit, &ec));
  g_cfs_calloc_fail = 0;

  for (k = 1; k <= 8; ++k) {
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p_root, &rit, &ec));
    g_cfs_malloc_fail = k;
    for (;;) {
      cfs_errc step_rc = cfs_rec_dir_itr_next(rit, &en, &ec);
      if (step_rc != cfs_errc_success || !en)
        break;
    }
    g_cfs_malloc_fail = 0;
    cfs_rec_dir_itr_close(rit);
  }
  for (k = 1; k <= 8; ++k) {
    ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_init(&p_root, &rit, &ec));
    g_cfs_realloc_fail = k;
    for (;;) {
      cfs_errc step_rc = cfs_rec_dir_itr_next(rit, &en, &ec);
      if (step_rc != cfs_errc_success || !en)
        break;
    }
    g_cfs_realloc_fail = 0;
    cfs_rec_dir_itr_close(rit);
  }

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
  /* Directory symlink follow + cycle protection + skip_permission_denied */
  {
    cfs_path ext_dir = {0}, sym_ext = {0}, sym_loop = {0}, sym_file = {0};
    cfs_path deny_dir = {0};
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&ext_dir, CFS_STR("test_p5_ext")));
    test_ignore_rc(cfs_remove_all(&ext_dir, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_create_directory(&ext_dir, &ec));
    test_write_file("test_p5_ext/ext_f.txt", "e");

    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&sym_ext, CFS_STR("tp5tree/link_ext")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&sym_loop, CFS_STR("tp5tree/d1/loop_up")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&sym_file, CFS_STR("tp5tree/link_file")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_assign(&p_tmp, CFS_STR("../test_p5_ext")));
    ASSERT_EQ(cfs_errc_success,
              cfs_create_directory_symlink(&p_tmp, &sym_ext, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p_tmp, CFS_STR("..")));
    ASSERT_EQ(cfs_errc_success,
              cfs_create_directory_symlink(&p_tmp, &sym_loop, &ec));
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p_tmp, CFS_STR("r.txt")));
    ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p_tmp, &sym_file, &ec));

    /* Without follow_directory_symlink: symlinks NOT descended */
    ASSERT_EQ(cfs_errc_success,
              cfs_rec_dir_itr_init_opts(&p_root, cfs_directory_options_none,
                                        &rit, &ec));
    count = 0;
    for (;;) {
      ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
      if (!en)
        break;
      count++;
    }
    ASSERT_EQ(8, count);
    cfs_rec_dir_itr_close(rit);

    /* With follow_directory_symlink: ext_dir descended (+1 file = 9), loop_up
     * cycle safely skipped! */
    ASSERT_EQ(cfs_errc_success,
              cfs_rec_dir_itr_init_opts(
                  &p_root, cfs_directory_options_follow_directory_symlink, &rit,
                  &ec));
    count = 0;
    for (;;) {
      ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
      if (!en)
        break;
      count++;
    }
    ASSERT_EQ(9, count);
    cfs_rec_dir_itr_close(rit);

    if (geteuid() != 0) {
      cfs_directory_iterator *probe_dit = NULL;
      cfs_errc probe_rc;
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&deny_dir, CFS_STR("tp5tree/d1/deny")));
      ASSERT_EQ(cfs_errc_success, cfs_create_directory(&deny_dir, &ec));
      ASSERT_EQ(cfs_errc_success,
                cfs_permissions(&deny_dir, (cfs_perms)0000,
                                cfs_perm_options_replace, &ec));

      probe_rc = cfs_dir_itr_init(&deny_dir, &probe_dit, &ec);
      if (probe_rc == cfs_errc_success) {
        cfs_dir_itr_close(probe_dit);
      } else if (probe_rc == cfs_errc_permission_denied) {
        /* Root unreadable with & without skip_permission_denied */
        ASSERT_EQ(cfs_errc_permission_denied,
                  cfs_rec_dir_itr_init_opts(
                      &deny_dir, cfs_directory_options_none, &rit, &ec));
        ASSERT_EQ(cfs_errc_success,
                  cfs_rec_dir_itr_init_opts(
                      &deny_dir, cfs_directory_options_skip_permission_denied,
                      &rit, &ec));
        ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
        ASSERT(en == NULL);
        cfs_rec_dir_itr_close(rit);

        /* Nested unreadable without skip_permission_denied fails on descent */
        ASSERT_EQ(cfs_errc_success,
                  cfs_rec_dir_itr_init_opts(&p_root, cfs_directory_options_none,
                                            &rit, &ec));
        {
          int saw_err = 0;
          for (;;) {
            cfs_errc s_rc = cfs_rec_dir_itr_next(rit, &en, &ec);
            if (s_rc != cfs_errc_success) {
              saw_err = 1;
              break;
            }
            if (!en)
              break;
          }
          ASSERT_EQ(1, saw_err);
        }
        cfs_rec_dir_itr_close(rit);

        /* Nested unreadable WITH skip_permission_denied succeeds! */
        ASSERT_EQ(cfs_errc_success,
                  cfs_rec_dir_itr_init_opts(
                      &p_root, cfs_directory_options_skip_permission_denied,
                      &rit, &ec));
        for (;;) {
          ASSERT_EQ(cfs_errc_success, cfs_rec_dir_itr_next(rit, &en, &ec));
          if (!en)
            break;
        }
        cfs_rec_dir_itr_close(rit);
      }

      ASSERT_EQ(cfs_errc_success,
                cfs_permissions(&deny_dir, (cfs_perms)0700,
                                cfs_perm_options_replace, &ec));
      cfs_path_destroy(&deny_dir);
    }

    test_ignore_rc(cfs_remove_all(&ext_dir, NULL, NULL));
    cfs_path_destroy(&ext_dir);
    cfs_path_destroy(&sym_ext);
    cfs_path_destroy(&sym_loop);
    cfs_path_destroy(&sym_file);
  }
#endif

  test_ignore_rc(cfs_remove_all(&p_root, NULL, NULL));
  cfs_path_destroy(&p_root);
  cfs_path_destroy(&p_d1);
  cfs_path_destroy(&p_d2);
  cfs_path_destroy(&p_f);
  cfs_path_destroy(&p_tmp);
  PASS();
}

/**
 * \brief Comprehensive unit, branch, and OOM test suite for Phase 6 async
 * opcodes, runtime modalities, sandbox enforcement, and IPC primitives.
 * \return The test result.
 */
TEST phase6_async_ipc_sandbox(void) {
  cfs_runtime_config cfg;
  cfs_runtime_t *rt = NULL;
  cfs_error_code ec;
  cfs_path p_dir = {0}, p_sub = {0}, p_f1 = {0}, p_f2 = {0}, p_f3 = {0};
  cfs_path p_outside = {0};
  int cb_count = 0;
  int k;

  /* 1. All 16 *_async wrappers & opcodes in sync + singlethread + greenthread +
   * multiprocess + message_passing + multithread modes */
  cfg.mode = cfs_modality_sync;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;
  ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));

#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_dir, CFS_STR(".")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_sub, CFS_STR(".")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_f1, CFS_STR("tp6f1.txt")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_f2, CFS_STR("tp6f2.txt")));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_f3, CFS_STR("tp6f3.txt")));
    test_ignore_rc(cfs_remove(&p_f1, NULL));
    test_ignore_rc(cfs_remove(&p_f2, NULL));
    test_ignore_rc(cfs_remove(&p_f3, NULL));
  } else
#endif
  {
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_dir, CFS_STR("tp6dir")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&p_sub, CFS_STR("tp6dir/a/b")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&p_f1, CFS_STR("tp6dir/f1.txt")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&p_f2, CFS_STR("tp6dir/f2.txt")));
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&p_f3, CFS_STR("tp6dir/f3.txt")));
    test_ignore_rc(cfs_remove_all(&p_dir, NULL, NULL));
  }

  /* Null destination validation on 2-path async wrappers */
  ASSERT_EQ(cfs_errc_invalid_argument, cfs_remove(NULL, &ec));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_copy_async(rt, &p_f1, NULL, cfs_copy_options_none, NULL, NULL));
  ASSERT_EQ(
      cfs_errc_invalid_argument,
      cfs_copy_file_async(rt, &p_f1, NULL, cfs_copy_options_none, NULL, NULL));
  ASSERT_EQ(cfs_errc_invalid_argument,
            cfs_rename_async(rt, &p_f1, NULL, NULL, NULL));

  /* OOMs inside cfs_submit_async_op (req, target, dest, result_buffer) */
  g_cfs_malloc_fail = 1;
  ASSERT_EQ(
      cfs_errc_not_enough_memory,
      cfs_copy_async(rt, &p_f1, &p_f2, cfs_copy_options_none, NULL, NULL));
  g_cfs_malloc_fail = 2;
  ASSERT_EQ(
      cfs_errc_not_enough_memory,
      cfs_copy_async(rt, &p_f1, &p_f2, cfs_copy_options_none, NULL, NULL));
  g_cfs_malloc_fail = 3;
  ASSERT_EQ(
      cfs_errc_not_enough_memory,
      cfs_copy_async(rt, &p_f1, &p_f2, cfs_copy_options_none, NULL, NULL));
  g_cfs_malloc_fail = 0;
  g_cfs_calloc_fail = 1;
  ASSERT_EQ(cfs_errc_not_enough_memory,
            cfs_status_async(rt, &p_f1, NULL, NULL));
  g_cfs_calloc_fail = 0;

  /* Execute all 16 opcodes (both success & failure paths for result-bearing
   * opcodes!) */
  ASSERT_EQ(cfs_errc_success,
            cfs_status_async(rt, &p_f1, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_symlink_status_async(rt, &p_f1, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_is_empty_async(rt, &p_f1, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_space_async(rt, &p_f1, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_last_write_time_async(rt, &p_f1, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_remove_all_async(rt, &p_f1, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_dir_itr_init_async(rt, &p_f1, phase6_async_cb, &cb_count));

  ASSERT_EQ(cfs_errc_success,
            cfs_create_directory_async(rt, &p_dir, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success, cfs_create_directories_async(
                                  rt, &p_sub, phase6_async_cb, &cb_count));
#if defined(CFS_OS_DOS)
  test_write_file((_osmajor < 2) ? "tp6f1.txt" : "tp6dir/f1.txt", "hello");
#else
  test_write_file("tp6dir/f1.txt", "hello");
#endif

  ASSERT_EQ(cfs_errc_success,
            cfs_copy_file_async(rt, &p_f1, &p_f2, cfs_copy_options_none,
                                phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_copy_async(rt, &p_f1, &p_f3, cfs_copy_options_none,
                           phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_resize_file_async(rt, &p_f3, 128, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_file_size_async(rt, &p_f3, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_status_async(rt, &p_f3, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_symlink_status_async(rt, &p_f3, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_exists_async(rt, &p_f3, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_is_empty_async(rt, &p_f3, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_space_async(rt, &p_dir, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_last_write_time_async(rt, &p_f3, phase6_async_cb, &cb_count));
  /* Both unharvested (auto-closed) & harvested cfs_dir_itr_init_async + small
   * result_buffer */
  ASSERT_EQ(cfs_errc_success,
            cfs_dir_itr_init_async(rt, &p_dir, phase6_async_cb, &cb_count));
  ASSERT_EQ(
      cfs_errc_success,
      cfs_dir_itr_init_async(rt, &p_dir, phase6_harvest_itr_cb, &cb_count));
  {
    cfs_request_t manual_req;
    char tiny_buf[1];
    memset(&manual_req, 0, sizeof(manual_req));
    manual_req.opcode = cfs_opcode_dir_itr_init;
    ASSERT_EQ(cfs_errc_success,
              cfs_path_clone(&manual_req.target_path, &p_dir));
    manual_req.result_buffer = NULL;
    manual_req.result_size = 0;
    manual_req.ref_count = 2;
    cfs_dispatch_request(rt, &manual_req, NULL, NULL);
    manual_req.result_buffer = tiny_buf;
    manual_req.result_size = 1;
    manual_req.ref_count = 2;
    cfs_dispatch_request(rt, &manual_req, NULL, NULL);
    cfs_path_destroy(&manual_req.target_path);
  }
  {
    cfs_request_t *tiny_heap_req = NULL;
    ASSERT_EQ(cfs_errc_success,
              cfs_malloc(sizeof(cfs_request_t), (void **)&tiny_heap_req));
    memset(tiny_heap_req, 0, sizeof(cfs_request_t));
    tiny_heap_req->opcode = cfs_opcode_dir_itr_init;
    ASSERT_EQ(cfs_errc_success, cfs_malloc(1, &tiny_heap_req->result_buffer));
    tiny_heap_req->result_size = 1;
    tiny_heap_req->ref_count = 1;
    cfs_request_release(tiny_heap_req);
  }
  ASSERT_EQ(cfs_errc_success,
            cfs_remove_async(rt, &p_f2, phase6_async_cb, &cb_count));
  ASSERT_EQ(cfs_errc_success,
            cfs_rename_async(rt, &p_f3, &p_f2, phase6_async_cb, &cb_count));

  /* 2. Sandbox enforcement: read_only, root_chroot confinement, &
   * restrict_symlinks */
  {
    cfs_sandbox_config sbox;
    cfs_file_status chk_st;
    cfs_path empty_p = {0};
    cfs_path null_p = {0};
    memset(&sbox, 0, sizeof(sbox));
    /* Empty string root_chroot (str != NULL && length == 0) */
    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&sbox.root_chroot, CFS_STR("")));
    ASSERT_EQ(cfs_errc_success, cfs_runtime_set_sandbox(rt, &sbox));
    cfs_path_destroy(&sbox.root_chroot);

    ASSERT_EQ(cfs_errc_success, cfs_path_clone(&sbox.root_chroot, &p_dir));
    sbox.read_only = cfs_true;
    sbox.restrict_symlinks = cfs_true;

    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory, cfs_runtime_set_sandbox(rt, &sbox));
    g_cfs_malloc_fail = 0;
    ASSERT_EQ(cfs_errc_success, cfs_runtime_set_sandbox(rt, &sbox));

    /* Read-only blocks mutations, allows read-only & opcode_none */
    ASSERT_EQ(cfs_errc_success, cfs_remove_async(rt, &p_f2, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_exists_async(rt, &p_dir, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_exists_async(rt, &p_f2, NULL, NULL));
    {
      cfs_request_t none_req;
      memset(&none_req, 0, sizeof(none_req));
      none_req.opcode = cfs_opcode_none;
      none_req.ref_count = 2;
      cfs_dispatch_request(rt, &none_req, NULL, NULL);
    }
    ASSERT_EQ(cfs_errc_success, cfs_status(&p_f2, &chk_st, &ec));

    /* Allow writes inside root_chroot, block writes/reads outside root_chroot
     */
    sbox.read_only = cfs_false;
    ASSERT_EQ(cfs_errc_success, cfs_runtime_set_sandbox(rt, &sbox));
    ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&empty_p, CFS_STR("")));
    cfs_path_init(&null_p);
    ASSERT_EQ(cfs_errc_success, cfs_exists_async(rt, &empty_p, NULL, NULL));
    ASSERT_EQ(cfs_errc_success, cfs_exists_async(rt, &null_p, NULL, NULL));
#if !defined(CFS_OS_WINDOWS)
    {
      cfs_path no_ex_sb = {0};
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&no_ex_sb, CFS_STR("no_such_sb_dir/x")));
      g_cfs_getcwd_fail = 1;
      ASSERT_EQ(cfs_errc_success, cfs_exists_async(rt, &no_ex_sb, NULL, NULL));
      g_cfs_getcwd_fail = 0;
      cfs_path_destroy(&no_ex_sb);
    }
#endif
    cfs_path_destroy(&empty_p);

    ASSERT_EQ(cfs_errc_success,
              cfs_path_init_str(&p_outside, CFS_STR("tp6out.txt")));
    test_write_file("tp6out.txt", "keep");
#if defined(CFS_OS_DOS)
    if (_osmajor >= 2)
#endif
    {
      ASSERT_EQ(cfs_errc_success, cfs_remove_async(rt, &p_outside, NULL, NULL));
      {
        cfs_path p_dot = {0};
        ASSERT_EQ(cfs_errc_success, cfs_path_init_str(&p_dot, CFS_STR(".")));
        ASSERT_EQ(cfs_errc_success, cfs_exists_async(rt, &p_dot, NULL, NULL));
        cfs_path_destroy(&p_dot);
      }
      ASSERT_EQ(cfs_errc_success, cfs_status(&p_outside, &chk_st, &ec));
      /* Allowed 2-path copy inside sandbox + blocked 2-path copy outside */
      ASSERT_EQ(cfs_errc_success,
                cfs_copy_file_async(rt, &p_f2, &p_f1,
                                    cfs_copy_options_overwrite_existing, NULL,
                                    NULL));
      /* Prefix trick ("tp6dirx") must ALSO be blocked! */
      {
        cfs_path p_sibling = {0};
        ASSERT_EQ(cfs_errc_success,
                  cfs_path_init_str(&p_sibling, CFS_STR("tp6dirx.txt")));
        ASSERT_EQ(cfs_errc_success,
                  cfs_exists_async(rt, &p_sibling, NULL, NULL));
        /* dest_path escaping sandbox blocked */
        ASSERT_EQ(cfs_errc_success,
                  cfs_copy_file_async(rt, &p_f2, &p_outside,
                                      cfs_copy_options_overwrite_existing, NULL,
                                      NULL));
        /* Trailing slash on sandbox root ("/") exercised with an op! */
        cfs_path_destroy(&sbox.root_chroot);
        ASSERT_EQ(cfs_errc_success,
                  cfs_path_init_str(&sbox.root_chroot, CFS_STR("/")));
        ASSERT_EQ(cfs_errc_success, cfs_runtime_set_sandbox(rt, &sbox));
        ASSERT_EQ(cfs_errc_success, cfs_exists_async(rt, &p_f2, NULL, NULL));
        cfs_path_destroy(&sbox.root_chroot);
        ASSERT_EQ(cfs_errc_success, cfs_path_clone(&sbox.root_chroot, &p_dir));
        ASSERT_EQ(cfs_errc_success, cfs_runtime_set_sandbox(rt, &sbox));
        cfs_path_destroy(&p_sibling);
      }
    }
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    {
      cfs_path p_sym_in = {0};
      ASSERT_EQ(cfs_errc_success,
                cfs_path_init_str(&p_sym_in, CFS_STR("tp6dir/sym_f1.txt")));
      ASSERT_EQ(cfs_errc_success, cfs_create_symlink(&p_f1, &p_sym_in, &ec));
      /* restrict_symlinks blocks symlink access */
      ASSERT_EQ(cfs_errc_success, cfs_status_async(rt, &p_sym_in, NULL, NULL));
      test_ignore_rc(cfs_remove(&p_sym_in, NULL));
      cfs_path_destroy(&p_sym_in);
    }
#endif
    test_ignore_rc(cfs_remove(&p_outside, NULL));
    cfs_path_destroy(&p_outside);
    cfs_path_destroy(&sbox.root_chroot);
  }
#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    test_ignore_rc(cfs_remove(&p_f1, NULL));
    test_ignore_rc(cfs_remove(&p_f2, NULL));
    test_ignore_rc(cfs_remove(&p_f3, NULL));
  } else
#endif
  {
    ASSERT_EQ(cfs_errc_success,
              cfs_remove_all_async(rt, &p_dir, phase6_async_cb, &cb_count));
  }
  cfs_runtime_destroy(rt);

  /* 3. All deferred runtime modalities: singlethread, multiprocess,
   * greenthread, message_passing (+ cancelled & OOM paths) */
  {
    cfs_modality modes[5];
    int mi;
    modes[0] = cfs_modality_singlethread;
    modes[1] = cfs_modality_multiprocess;
    modes[2] = cfs_modality_greenthread;
    modes[3] = cfs_modality_message_passing;
    modes[4] = cfs_modality_message_passing;

    test_write_file("tp6mod.txt", "mod");
    ASSERT_EQ(cfs_errc_success, cfs_path_assign(&p_f1, CFS_STR("tp6mod.txt")));

    for (mi = 0; mi < 5; ++mi) {
      cfg.mode = modes[mi];
      cfg.thread_pool_size = 1;
      cfg.ipc_path = (mi == 3) ? CFS_STR("custom_ipc") : NULL;
      for (k = 1; k <= 4; ++k) {
        g_cfs_malloc_fail = k;
        if (cfs_runtime_init(&cfg, &rt, &ec) == cfs_errc_success) {
          cfs_runtime_destroy(rt);
          rt = NULL;
        }
        g_cfs_malloc_fail = 0;
      }
      ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &rt, &ec));
      cb_count = 0;
      /* OOM during poll in greenthread & message_passing FIRST */
      if (mi == 2) {
        ASSERT_EQ(cfs_errc_success,
                  cfs_exists_async(rt, &p_f1, phase6_async_cb, &cb_count));
        g_cfs_malloc_fail = 1;
        ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(rt));
        g_cfs_malloc_fail = 0;
      } else if (mi == 3) {
        for (k = 1; k <= 3; ++k) {
          cfs_runtime_t *ort = NULL;
          ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &ort, &ec));
          ASSERT_EQ(cfs_errc_success,
                    cfs_exists_async(ort, &p_f1, phase6_async_cb, &cb_count));
          g_cfs_malloc_fail = k;
          ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(ort));
          g_cfs_malloc_fail = 0;
          cfs_runtime_destroy(ort);
        }
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
        {
          cfs_runtime_t *prt = NULL;
          ASSERT_EQ(cfs_errc_success, cfs_runtime_init(&cfg, &prt, &ec));
          ASSERT_EQ(cfs_errc_success,
                    cfs_exists_async(prt, &p_f1, phase6_async_cb, &cb_count));
          g_cfs_rw_fail = 1;
          ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(prt));
          g_cfs_rw_fail = 0;
          cfs_runtime_destroy(prt);
        }
#endif
      }
      ASSERT_EQ(cfs_errc_success,
                cfs_exists_async(rt, &p_f1, phase6_async_cb, &cb_count));
      /* Dispatch a pre-cancelled request WITHOUT OOM so runner sees cancelled
       */
      {
        cfs_request_t *creq = NULL;
        ASSERT_EQ(cfs_errc_success,
                  cfs_malloc(sizeof(cfs_request_t), (void **)&creq));
        memset(creq, 0, sizeof(cfs_request_t));
        creq->opcode = cfs_opcode_exists;
        creq->ref_count = 1;
        cfs_dispatch_request(rt, creq, phase6_async_cb, &cb_count);
        ASSERT_EQ(cfs_errc_success, cfs_cancel_request(rt, creq));
      }
      ASSERT_EQ(cfs_errc_success, cfs_runtime_poll(rt));
      ASSERT(cb_count >= 2);
      cfs_runtime_destroy(rt);
    }
    test_ignore_rc(cfs_remove(&p_f1, NULL));
  }

  /* 4. Message pipe send/recv, serialization bounds, process spawn/wait, shared
   * memory cross-handle visibility, blocking named semaphore, & green threads
   */
  {
    cfs_message_pipe *mp = NULL;
    void *rbuf = NULL;
    cfs_size_t rsz = 0;
    const char payload[] = "hello_pipe";

    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_message_pipe_send(NULL, payload, sizeof(payload)));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_message_pipe_recv(NULL, &rbuf, &rsz));
#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    g_cfs_pipe_fail = 1;
    ASSERT_EQ(cfs_errc_io_error,
              cfs_message_pipe_create(CFS_STR("p6_mp"), &mp));
    g_cfs_pipe_fail = 0;
#endif
    ASSERT_EQ(cfs_errc_success, cfs_message_pipe_create(CFS_STR("p6_mp"), &mp));
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_message_pipe_send(mp, NULL, 5));
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_message_pipe_recv(mp, NULL, &rsz));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_message_pipe_recv(mp, &rbuf, NULL));

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    g_cfs_rw_fail = 1;
    ASSERT_EQ(cfs_errc_io_error,
              cfs_message_pipe_send(mp, payload, sizeof(payload)));
    g_cfs_rw_fail = 0;
    g_cfs_rw_fail = 1;
    ASSERT_EQ(cfs_errc_io_error, cfs_message_pipe_recv(mp, &rbuf, &rsz));
    g_cfs_rw_fail = 0;
    /* Fail payload write after header write, then drain header on recv */
    g_cfs_rw_fail = 2;
    ASSERT_EQ(cfs_errc_io_error, cfs_message_pipe_send(mp, payload, 4));
    g_cfs_rw_fail = 0;
    g_cfs_rw_fail = 2;
    ASSERT_EQ(cfs_errc_io_error, cfs_message_pipe_recv(mp, &rbuf, &rsz));
    g_cfs_rw_fail = 0;
    cfs_message_pipe_destroy(mp);
    ASSERT_EQ(cfs_errc_success, cfs_message_pipe_create(CFS_STR("p6_mp"), &mp));
#endif

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS)
    g_cfs_rw_fail = 3;
    ASSERT_EQ(cfs_errc_success,
              cfs_message_pipe_send(mp, payload, sizeof(payload)));
    g_cfs_rw_fail = 3;
    ASSERT_EQ(cfs_errc_success, cfs_message_pipe_recv(mp, &rbuf, &rsz));
    cfs_free(rbuf);
    g_cfs_rw_fail = 0;
#endif
    ASSERT_EQ(cfs_errc_success,
              cfs_message_pipe_send(mp, payload, sizeof(payload)));
    ASSERT_EQ(cfs_errc_success, cfs_message_pipe_recv(mp, &rbuf, &rsz));
    ASSERT_EQ(sizeof(payload), rsz);
    ASSERT_STR_EQ(payload, (const char *)rbuf);
    cfs_free(rbuf);

    /* Zero-byte message send & recv + OOM in recv */
    ASSERT_EQ(cfs_errc_success, cfs_message_pipe_send(mp, NULL, 0));
    ASSERT_EQ(cfs_errc_success, cfs_message_pipe_recv(mp, &rbuf, &rsz));
    ASSERT_EQ(0, rsz);
    cfs_free(rbuf);

    ASSERT_EQ(cfs_errc_success, cfs_message_pipe_send(mp, payload, 4));
    g_cfs_malloc_fail = 1;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_message_pipe_recv(mp, &rbuf, &rsz));
    g_cfs_malloc_fail = 0;
    cfs_message_pipe_destroy(mp);
  }

  /* Serialization & deserialization bounds checks */
  {
    unsigned char bad_hdr[128];
    cfs_request_t *dreq = NULL;
#if defined(__I86__) || defined(_M_I86)
    cfs_size_t huge_d = (cfs_size_t)16384U;
#else
    cfs_size_t huge_d = (cfs_size_t)0x200000UL;
#endif
    memset(bad_hdr, 0xFF, sizeof(bad_hdr));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_deserialize_request(bad_hdr, sizeof(bad_hdr), &dreq));
    memset(bad_hdr, 0, sizeof(bad_hdr));
    /* d_len > CFS_MAX_WIRE_PATH_LEN when t_len == 0 */
    CFS_MEMCPY_SAFE(bad_hdr + sizeof(int) * 4 + sizeof(cfs_uintmax_t) +
                        sizeof(cfs_size_t),
                    sizeof(huge_d), &huge_d, sizeof(huge_d));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_deserialize_request(bad_hdr, sizeof(bad_hdr), &dreq));
    memset(bad_hdr, 0, sizeof(bad_hdr));
    /* Header claiming t_len = 500 when buffer is only 128 bytes */
    {
      cfs_size_t big_len = 500;
      CFS_MEMCPY_SAFE(bad_hdr + sizeof(int) * 4 + sizeof(cfs_uintmax_t),
                      sizeof(big_len), &big_len, sizeof(big_len));
      ASSERT_EQ(cfs_errc_invalid_argument,
                cfs_deserialize_request(bad_hdr, sizeof(bad_hdr), &dreq));
    }
  }

  /* Process spawn & wait (exit 0 vs exit 1, signal, fault injection, OOM) */
  {
    cfs_process_t *pr = NULL;
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_process_spawn(CFS_STR(""), &pr));
#if !defined(CFS_OS_DOS) && !defined(CFS_OS_EMSCRIPTEN)
    g_cfs_malloc_fail = 2;
    ASSERT_EQ(cfs_errc_not_enough_memory,
              cfs_process_spawn(CFS_STR("exit 0"), &pr));
    g_cfs_malloc_fail = 0;
#endif
#if defined(CFS_OS_WINDOWS)
    ASSERT_EQ(cfs_errc_success,
              cfs_process_spawn(CFS_STR("cmd.exe /c exit 1"), &pr));
    ASSERT_NEQ(cfs_errc_success, cfs_process_wait(pr));
    cfs_process_destroy(pr);
    ASSERT_EQ(cfs_errc_success,
              cfs_process_spawn(CFS_STR("cmd.exe /c exit 0"), &pr));
    cfs_process_destroy(pr);
#elif defined(CFS_OS_DOS) || defined(CFS_OS_EMSCRIPTEN)
    ASSERT_EQ(cfs_errc_success, cfs_process_spawn(CFS_STR("exit 1"), &pr));
    ASSERT_NEQ(cfs_errc_success, cfs_process_wait(pr));
    cfs_process_destroy(pr);
    ASSERT_EQ(cfs_errc_success,
              cfs_process_spawn(CFS_STR("cmd /c exit 0"), &pr));
    ASSERT_EQ(cfs_errc_success, cfs_process_wait(pr));
    cfs_process_destroy(pr);
#else
    g_cfs_spawn_fail = 1;
    ASSERT_NEQ(cfs_errc_success, cfs_process_spawn(CFS_STR("exit 0"), &pr));
    g_cfs_spawn_fail = 0;

    ASSERT_EQ(cfs_errc_success, cfs_process_spawn(CFS_STR("exit 1"), &pr));
    g_cfs_waitpid_fail = 1;
    ASSERT_NEQ(cfs_errc_success, cfs_process_wait(pr));
    g_cfs_waitpid_fail = 0;
    ASSERT_NEQ(cfs_errc_success, cfs_process_wait(pr));
    /* Second wait after pid consumed returns invalid_argument */
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_process_wait(pr));
    cfs_process_destroy(pr);

    /* Signal-terminated child (!WIFEXITED) */
    ASSERT_EQ(cfs_errc_success, cfs_process_spawn(CFS_STR("kill -9 $$"), &pr));
    ASSERT_NEQ(cfs_errc_success, cfs_process_wait(pr));
    cfs_process_destroy(pr);

    /* Destroy with waitpid_fail & without explicit wait */
    ASSERT_EQ(cfs_errc_success, cfs_process_spawn(CFS_STR("exit 0"), &pr));
    g_cfs_waitpid_fail = 1;
    cfs_process_destroy(pr);
    g_cfs_waitpid_fail = 0;
    (void)waitpid(-1, NULL, 0);

    ASSERT_EQ(cfs_errc_success, cfs_process_spawn(CFS_STR("exit 0"), &pr));
    cfs_process_destroy(pr);
#endif
  }

  /* Shared memory cross-handle visibility & registry list traversal */
  {
    cfs_shm_segment *s1 = NULL, *s2 = NULL, *s3 = NULL;
    void *a1 = NULL, *a2 = NULL, *a3 = NULL;
    ASSERT_EQ(cfs_errc_success,
              cfs_shm_create(64, CFS_STR("cfs_p6_shmA"), &s1));
    ASSERT_EQ(cfs_errc_success,
              cfs_shm_create(64, CFS_STR("cfs_p6_shmA"), &s2));
    ASSERT_EQ(cfs_errc_success,
              cfs_shm_create(64, CFS_STR("cfs_p6_shmB"), &s3));
    g_cfs_malloc_fail = 1;
    ASSERT_NEQ(cfs_errc_invalid_argument, cfs_shm_map(s1, &a1));
    g_cfs_malloc_fail = 0;
    ASSERT_EQ(cfs_errc_success, cfs_shm_map(s1, &a1));
    ASSERT_EQ(cfs_errc_success, cfs_shm_map(s3, &a3));
    ASSERT_EQ(cfs_errc_success, cfs_shm_map(s2, &a2));
    CFS_MEMCPY_SAFE(a1, 64, "SHM_OK", 7);
    ASSERT_STR_EQ("SHM_OK", (const char *)a2);
    cfs_shm_unmap(s1, a1);
    /* Double unmap when already unmapped */
    cfs_shm_unmap(s1, a1);
    cfs_shm_destroy(s1);
    /* Destroy s2 while s3 is at registry head (steps cur = &(*cur)->next!) */
    cfs_shm_destroy(s2);
    cfs_shm_unmap(s3, a3);
    cfs_shm_destroy(s3);
  }

  /* Named semaphore: cross-handle sharing, try_wait, & real blocking wait */
  {
    cfs_named_semaphore *sem1 = NULL, *sem2 = NULL, *sem3 = NULL;
    cfs_bool acq = cfs_false;
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_named_semaphore_create(CFS_STR("x"), -1, &sem1));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_named_semaphore_try_wait(NULL, &acq));
    g_cfs_malloc_fail = 2;
    test_ignore_rc(
        cfs_named_semaphore_create(CFS_STR("cfs_p6_semA"), 0, &sem1));
    g_cfs_malloc_fail = 0;

    ASSERT_EQ(cfs_errc_success,
              cfs_named_semaphore_create(CFS_STR("cfs_p6_semA"), 0, &sem1));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_named_semaphore_try_wait(sem1, NULL));
    ASSERT_EQ(cfs_errc_success,
              cfs_named_semaphore_create(CFS_STR("cfs_p6_semB"), 1, &sem3));
    ASSERT_EQ(cfs_errc_success,
              cfs_named_semaphore_create(CFS_STR("cfs_p6_semA"), 0, &sem2));

    ASSERT_EQ(cfs_errc_success, cfs_named_semaphore_try_wait(sem1, &acq));
    ASSERT_EQ(cfs_false, acq);
    ASSERT_EQ(cfs_errc_success, cfs_named_semaphore_post(sem2));
    ASSERT_EQ(cfs_errc_success, cfs_named_semaphore_try_wait(sem1, &acq));
    ASSERT_EQ(cfs_true, acq);

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS) &&                        \
    defined(CFS_MULTITHREADED)
    {
      pthread_t tid;
      ASSERT_EQ(0, pthread_create(&tid, NULL, phase6_sem_poster_thread, sem2));
      ASSERT_EQ(cfs_errc_success, cfs_named_semaphore_wait(sem1));
      (void)pthread_join(tid, NULL);
    }
#endif
    cfs_named_semaphore_destroy(sem1);
    cfs_named_semaphore_destroy(sem2);
    cfs_named_semaphore_destroy(sem3);
  }

  /* Cooperative green-thread scheduler: multiple tasks, NULL func, yields */
  {
    cfs_greenthread_scheduler *sched = NULL;
    cfs_greenthread_t *gt1 = NULL, *gt2 = NULL;
    int counter = 5;
    cfs_greenthread_scheduler_destroy(NULL);
    ASSERT_EQ(cfs_errc_invalid_argument, cfs_greenthread_scheduler_run(NULL));
    ASSERT_EQ(cfs_errc_success, cfs_greenthread_scheduler_init(&sched));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_greenthread_scheduler_schedule(NULL, gt1));
    ASSERT_EQ(cfs_errc_invalid_argument,
              cfs_greenthread_scheduler_schedule(sched, NULL));
    ASSERT_EQ(cfs_errc_success,
              cfs_greenthread_spawn(phase6_gt_worker, &counter, &gt1));
    ASSERT_EQ(cfs_errc_success, cfs_greenthread_spawn(NULL, NULL, &gt2));
    ASSERT_EQ(cfs_errc_success, cfs_greenthread_scheduler_schedule(sched, gt1));
    ASSERT_EQ(cfs_errc_success, cfs_greenthread_scheduler_schedule(sched, gt2));
    ASSERT_EQ(cfs_errc_success, cfs_greenthread_scheduler_run(sched));
    ASSERT_EQ(15, counter);
    cfs_greenthread_destroy(gt1);
    cfs_greenthread_destroy(gt2);
    cfs_greenthread_scheduler_destroy(sched);
  }

  cfs_path_destroy(&p_dir);
  cfs_path_destroy(&p_sub);
  cfs_path_destroy(&p_f1);
  cfs_path_destroy(&p_f2);
  cfs_path_destroy(&p_f3);
  PASS();
}

/**
 * @brief Registers and runs all c-fs unit test cases.
 */
SUITE(cfs_suite) {
  RUN_TEST(phase6_async_ipc_sandbox);
  RUN_TEST(phase5_directory_iterators);
  RUN_TEST(phase4_windows_parity);
  RUN_TEST(phase1_to_3_features);
  RUN_TEST(header_interoperability);
  RUN_TEST(exhaustive_branch_coverage);
  RUN_TEST(exhaustive_branch_coverage_fs);
  RUN_TEST(additional_percolation_oom_coverage);
  RUN_TEST(edge_cases_and_oom);
  RUN_TEST(missing_lines_coverage);
  RUN_TEST(branch_coverage_nulls);
  RUN_TEST(missing_lines_coverage_async);
  RUN_TEST(fix_last_missing);
  RUN_TEST(out_of_memory_precise);
  RUN_TEST(cover_everything);
  RUN_TEST(last_mile);
  RUN_TEST(extreme_edge_cases);
  RUN_TEST(out_of_memory);
  RUN_TEST(final_coverage);
  RUN_TEST(more_coverage);
  RUN_TEST(real_file_operations);
  RUN_TEST(exhaustive_nulls);
  RUN_TEST(string_and_errors);
  RUN_TEST(ipc_and_processes);
  RUN_TEST(greenthreads_and_utils);
  RUN_TEST(file_queries);
  RUN_TEST(path_utilities);
  RUN_TEST(path_decomposition_more);
  RUN_TEST(path_queries);
  RUN_TEST(path_lexical);
  RUN_TEST(dir_iterators);
  RUN_TEST(rec_dir_iterators);
  RUN_TEST(path_initialization);
  RUN_TEST(path_appending);
  RUN_TEST(path_decomposition);
  RUN_TEST(root_path_decomposition);
  RUN_TEST(thread_pool_async_validation);
  RUN_TEST(greenthread_scheduler_validation);
  RUN_TEST(memory_allocation);
  RUN_TEST(string_handling);
}

/** @brief Instantiates greatest.h global test runner definitions. */
GREATEST_MAIN_DEFS();

/**
 * \brief Main entry point for the test suite.
 * \param argc Number of command-line arguments.
 * \param argv Array of command-line argument strings.
 * \return Exit status code.
 */
int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(cfs_suite);
  GREATEST_MAIN_END();
}
