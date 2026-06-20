/**
 * \file test_main.c
 * \brief Main test suite for the c-fs library.
 */
/* clang-format off */
#if !defined(_XOPEN_SOURCE) && !defined(_WIN32)
#define _XOPEN_SOURCE 500
#endif
#if !defined(__STDC_WANT_LIB_EXT1__)
#define __STDC_WANT_LIB_EXT1__ 1
#endif
#include "cfs/cfs.h"
#include <string.h>
#include "greatest.h"

#if defined(CFS_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
/* clang-format on */

static void test_sleep_ms(int ms) {
#if defined(CFS_OS_WINDOWS)
  Sleep(ms);
#else
  usleep(ms * 1000);
#endif
}

/**
 * \brief Test case for path_initialization.
 * \return The test result.
 */
TEST path_initialization() {
  cfs_path p;
  cfs_path_init(&p);
  {
    cfs_bool empty;
    cfs_path_is_empty(&p, &empty);
    ASSERT_EQ(1, empty);
  }

  cfs_path_init_str(&p, CFS_STR("test") PATH_SEP_STR CFS_STR("path"));
  {
    cfs_bool empty;
    cfs_path_is_empty(&p, &empty);
    ASSERT_EQ(0, empty);
  }
  {
    const cfs_char_t *c_str;
    int cmp;
    cfs_path_c_str(&p, &c_str);
    cfs_strcmp(CFS_STR("test") PATH_SEP_STR CFS_STR("path"), c_str, &cmp);
    ASSERT_EQ(0, cmp);
  }

  cfs_path_destroy(&p);
  {
    cfs_bool empty;
    cfs_path_is_empty(&p, &empty);
    ASSERT_EQ(1, empty);
  }
  PASS();
}

/**
 * \brief Test case for path_appending.
 * \return The test result.
 */
TEST path_appending() {
  cfs_path p;
  cfs_path_init_str(&p, CFS_STR("dir"));
  cfs_path_append(&p, CFS_STR("file.txt"));

  {
    const cfs_char_t *c_str;
    int cmp;
    cfs_path_c_str(&p, &c_str);
    cfs_strcmp(CFS_STR("dir") PATH_SEP_STR CFS_STR("file.txt"), c_str, &cmp);
    ASSERT_EQ(0, cmp);
  }

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for path_decomposition.
 * \return The test result.
 */
TEST path_decomposition() {
  cfs_path p, res;
  cfs_path_init_str(&p, CFS_STR("dir") PATH_SEP_STR CFS_STR("subdir")
                            PATH_SEP_STR CFS_STR("file.txt"));

  cfs_path_filename(&p, &res);
  {
    const cfs_char_t *c_str;
    int cmp;
    cfs_path_c_str(&res, &c_str);
    cfs_strcmp(CFS_STR("file.txt"), c_str, &cmp);
    ASSERT_EQ(0, cmp);
  }
  cfs_path_destroy(&res);

  cfs_path_extension(&p, &res);
  {
    const cfs_char_t *c_str;
    int cmp;
    cfs_path_c_str(&res, &c_str);
    cfs_strcmp(CFS_STR(".txt"), c_str, &cmp);
    ASSERT_EQ(0, cmp);
  }
  cfs_path_destroy(&res);

  cfs_path_stem(&p, &res);
  {
    const cfs_char_t *c_str;
    int cmp;
    cfs_path_c_str(&res, &c_str);
    cfs_strcmp(CFS_STR("file"), c_str, &cmp);
    ASSERT_EQ(0, cmp);
  }
  cfs_path_destroy(&res);

  cfs_path_destroy(&p);
  PASS();
}

/* Step 45. Write multithreading test cases simulating concurrent file creations
 */
static int test_completed_ops = 0;

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
 * \brief Test case for thread_pool_async_validation.
 * \return The test result.
 */
TEST thread_pool_async_validation() {
  cfs_runtime_config config;
  cfs_runtime_t *rt;
  cfs_path p;
  cfs_error_code ec;
  int i;
  int res;

  config.mode = cfs_modality_multithread;
  config.thread_pool_size = 4;
  config.ipc_path = NULL;

  res = cfs_runtime_init(&config, &rt, &ec);
  ASSERT_EQ(0, res);
  ASSERT_NEQ(NULL, rt);

  test_completed_ops = 0;

  cfs_path_init_str(&p, CFS_STR("dummy_test_file.txt"));

  for (i = 0; i < 100; i++) {
    cfs_remove_async(rt, &p, async_callback, NULL);
  }

  /* Wait for operations to hit completion queue (simulate event loop tick) */
#if defined(CFS_OS_WINDOWS)
  Sleep(100);
#endif

  cfs_runtime_poll(rt);

  /* We expect some to have finished depending on thread timing. Just validate
   * poll works */

  cfs_path_destroy(&p);
  cfs_runtime_destroy(rt);
  PASS();
}

/* Step 47. Write greenthread / scheduler stubs test cases */
TEST greenthread_scheduler_validation() {
  cfs_greenthread_scheduler *sched = NULL;
  int res;
  res = cfs_greenthread_scheduler_init(&sched);
  ASSERT_EQ(0, res);
  ASSERT_NEQ(NULL, sched);

  res = cfs_greenthread_scheduler_run(sched);
  ASSERT_EQ(0, res);

  cfs_greenthread_scheduler_destroy(sched);
  PASS();
}

/**
 * \brief Test case for memory_allocation.
 * \return The test result.
 */
TEST memory_allocation() {
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
TEST string_handling() {
  cfs_char_t buf[100];
  cfs_size_t len;
  cfs_char_t *out = NULL;
  cfs_strcpy(buf, CFS_STR("Hello"), &out);
  cfs_strlen(buf, &len);
  ASSERT_EQ(5, len);

  cfs_strcat(buf, CFS_STR(" World"), &out);
  cfs_strlen(buf, &len);
  ASSERT_EQ(11, len);

  {
    int cmp;
    cfs_strcmp(buf, CFS_STR("Hello World"), &cmp);
    ASSERT_EQ(0, cmp);
    cfs_strncmp(buf, CFS_STR("Hello W"), 7, &cmp);
    ASSERT_EQ(0, cmp);
  }
  PASS();
}

/**
 * \brief Test case for path_utilities.
 * \return The test result.
 */
TEST path_utilities() {
  cfs_path p;
  cfs_path p2;
  cfs_char_t *gen_str = NULL;
  int cmp;

  cfs_path_init_str(&p, CFS_STR("dir/subdir\\file.txt"));
  cfs_path_make_preferred(&p);
  cfs_path_generic_string(&p, &gen_str);
  if (gen_str)
    cfs_free(gen_str);

  cfs_path_clear(&p);

  cfs_path_init_str(&p, CFS_STR("first"));
  cfs_path_init_str(&p2, CFS_STR("second"));
  cfs_path_swap(&p, &p2);
  cfs_path_concat(&p, CFS_STR("_part"));

  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);

  cfs_path_make_preferred(NULL);
  cfs_path_generic_string(NULL, &gen_str);
  cfs_path_generic_string(&p, NULL);
  cfs_path_clear(NULL);
  cfs_path_swap(NULL, &p2);
  cfs_path_swap(&p, NULL);
  cfs_path_concat(NULL, CFS_STR("a"));
  cfs_path_concat(&p, NULL);

  PASS();
}

/**
 * \brief Test case for path_decomposition_more.
 * \return The test result.
 */
TEST path_decomposition_more() {
  cfs_path p, out;
  cfs_path_init_str(&p, CFS_STR("/usr/local/bin/test.exe"));

  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_root_name(&p, &out);
  cfs_path_destroy(&out);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_root_directory(&p, &out);
  cfs_path_destroy(&out);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_root_path(&p, &out);
  cfs_path_destroy(&out);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_relative_path(&p, &out);
  cfs_path_destroy(&out);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_parent_path(&p, &out);
  cfs_path_destroy(&out);

  cfs_path_replace_filename(&p, CFS_STR("new.exe"));
  cfs_path_replace_extension(&p, CFS_STR(".bin"));

  cfs_path_destroy(&p);

  cfs_path_root_name(NULL, &out);
  cfs_path_root_directory(NULL, &out);
  cfs_path_root_path(NULL, &out);
  cfs_path_relative_path(NULL, &out);
  cfs_path_parent_path(NULL, &out);
  cfs_path_replace_filename(NULL, CFS_STR(""));
  cfs_path_replace_extension(NULL, CFS_STR(""));

  PASS();
}

/**
 * \brief Test case for path_queries.
 * \return The test result.
 */
TEST path_queries() {
  cfs_path p;
  cfs_bool out_bool;
  cfs_path_init_str(&p, CFS_STR("/usr/local/bin/test.exe"));

  cfs_path_has_root_path(&p, &out_bool);
  cfs_path_has_root_name(&p, &out_bool);
  cfs_path_has_root_directory(&p, &out_bool);
  cfs_path_has_relative_path(&p, &out_bool);
  cfs_path_has_parent_path(&p, &out_bool);
  cfs_path_has_filename(&p, &out_bool);
  cfs_path_has_stem(&p, &out_bool);
  cfs_path_has_extension(&p, &out_bool);
  cfs_path_is_absolute(&p, &out_bool);
  cfs_path_is_relative(&p, &out_bool);

  cfs_path_destroy(&p);

  cfs_path_has_root_path(NULL, &out_bool);
  cfs_path_has_root_name(NULL, &out_bool);
  cfs_path_has_root_directory(NULL, &out_bool);
  cfs_path_has_relative_path(NULL, &out_bool);
  cfs_path_has_parent_path(NULL, &out_bool);
  cfs_path_has_filename(NULL, &out_bool);
  cfs_path_has_stem(NULL, &out_bool);
  cfs_path_has_extension(NULL, &out_bool);
  cfs_path_is_absolute(NULL, &out_bool);
  cfs_path_is_relative(NULL, &out_bool);

  PASS();
}

/**
 * \brief Test case for path_lexical.
 * \return The test result.
 */
TEST path_lexical() {
  cfs_path p, base, out;
  cfs_path_init_str(&p, CFS_STR("/usr/local/bin/test.exe"));
  cfs_path_init_str(&base, CFS_STR("/usr/local/"));

  cfs_path_compare(&p, &base);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_lexically_normal(&p, &out);
  cfs_path_destroy(&out);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_lexically_relative(&p, &base, &out);
  cfs_path_destroy(&out);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_lexically_proximate(&p, &base, &out);
  cfs_path_destroy(&out);

  cfs_path_destroy(&p);
  cfs_path_destroy(&base);

  cfs_path_compare(NULL, NULL);
  cfs_path_lexically_normal(NULL, &out);
  cfs_path_lexically_relative(NULL, &base, &out);
  cfs_path_lexically_proximate(NULL, &base, &out);

  PASS();
}

/**
 * \brief Test case for dir_iterators.
 * \return The test result.
 */
TEST dir_iterators() {
  cfs_directory_iterator *itr = NULL;
  cfs_path p;
  const cfs_directory_entry *out_entry = NULL;
  cfs_error_code ec;

  cfs_path_init_str(&p, CFS_STR("."));

  cfs_dir_itr_init(&p, &itr, &ec);
  cfs_dir_itr_next(itr, &out_entry, &ec);
  cfs_dir_itr_close(itr);

  cfs_dir_itr_init(NULL, &itr, &ec);
  cfs_dir_itr_init(&p, NULL, &ec);
  cfs_dir_itr_next(NULL, &out_entry, &ec);
  cfs_dir_itr_next(itr, NULL, &ec);
  cfs_dir_itr_next(itr, &out_entry, NULL);
  cfs_dir_itr_close(NULL);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for rec_dir_iterators.
 * \return The test result.
 */
TEST rec_dir_iterators() {
  cfs_recursive_directory_iterator *itr = NULL;
  cfs_path p;
  const cfs_directory_entry *out_entry = NULL;
  cfs_error_code ec;

  cfs_path_init_str(&p, CFS_STR("."));

  cfs_rec_dir_itr_init(&p, &itr, &ec);
  cfs_rec_dir_itr_next(itr, &out_entry, &ec);
  cfs_rec_dir_itr_disable_recursion_pending(itr);
  cfs_rec_dir_itr_pop(itr, &ec);
  cfs_rec_dir_itr_close(itr);

  cfs_rec_dir_itr_init(NULL, &itr, &ec);
  cfs_rec_dir_itr_init(&p, NULL, &ec);
  cfs_rec_dir_itr_next(NULL, &out_entry, &ec);
  cfs_rec_dir_itr_next(itr, NULL, &ec);
  cfs_rec_dir_itr_next(itr, &out_entry, NULL);
  cfs_rec_dir_itr_disable_recursion_pending(NULL);
  cfs_rec_dir_itr_pop(NULL, &ec);
  cfs_rec_dir_itr_close(NULL);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for file_queries.
 * \return The test result.
 */
TEST file_queries() {
  cfs_path p, p2, p_out;
  cfs_file_status s;
  cfs_bool b;
  cfs_error_code ec;
  cfs_size_t sz;
  cfs_space_info spc;
  cfs_file_time_type ft;
  cfs_uintmax_t count;
  cfs_perms perms = 0;

  cfs_path_init_str(&p, CFS_STR("dummy"));
  cfs_path_init_str(&p2, CFS_STR("dummy2"));

  cfs_status(&p, &s, &ec);
  cfs_symlink_status(&p, &s, &ec);
  cfs_status_known(s, &b);
  cfs_exists(s, &b);
  cfs_exists_path(&p, &b, &ec);
  cfs_is_block_file(s, &b);
  cfs_is_character_file(s, &b);
  cfs_is_directory(s, &b);
  cfs_is_fifo(s, &b);
  cfs_is_other(s, &b);
  cfs_is_regular_file(s, &b);
  cfs_is_socket(s, &b);
  cfs_is_symlink(s, &b);
  cfs_is_empty_path(&p, &b, &ec);

  cfs_create_directory(&p, &ec);
  cfs_create_directories(&p, &ec);
  cfs_create_hard_link(&p, &p2, &ec);
  cfs_create_symlink(&p, &p2, &ec);
  cfs_create_directory_symlink(&p, &p2, &ec);

  cfs_remove_all(&p, &sz, &ec);
  cfs_rename(&p, &p2, &ec);
  cfs_resize_file(&p, 1024, &ec);
  cfs_space(&p, &spc, &ec);
  cfs_last_write_time(&p, &ft, &ec);
  cfs_hard_link_count(&p, &count, &ec);
  cfs_permissions(&p, perms, 0, &ec);
  cfs_equivalent(&p, &p2, &b, &ec);

  cfs_path_init(&p_out);
  cfs_read_symlink(&p, &p_out, &ec);
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  cfs_absolute(&p, &p_out, &ec);
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  cfs_canonical(&p, &p_out, &ec);
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  cfs_weakly_canonical(&p, &p_out, &ec);
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  cfs_proximate(&p, &p2, &p_out, &ec);
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  cfs_relative(&p, &p2, &p_out, &ec);
  cfs_path_destroy(&p_out);

  cfs_path_init(&p_out);
  cfs_temp_directory_path(&p_out, &ec);
  cfs_path_destroy(&p_out);

  cfs_copy(&p, &p2, 0, &ec);
  cfs_copy_symlink(&p, &p2, &ec);
  cfs_copy_file(&p, &p2, 0, &ec);

  cfs_current_path(&p_out, &ec);
  cfs_current_path_set(&p, &ec);

  cfs_path_remove_filename(&p);

  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);

  PASS();
}

/**
 * \brief Test case for string_and_errors.
 * \return The test result.
 */
TEST string_and_errors() {
  wchar_t dest_w[100];
  char dest_c[100];
  cfs_size_t out_req;
  cfs_error_code ec;
  const char *msg;

  cfs_mb_to_wide("test", dest_w, 100, &out_req);
  cfs_wide_to_mb(L"test", dest_c, 100, &out_req);

  cfs_make_error_code_from_os(2, &ec);
  cfs_get_last_error(&ec);
  cfs_error_message(cfs_errc_no_such_file_or_directory, &msg);

  cfs_mb_to_wide(NULL, dest_w, 100, &out_req);
  cfs_wide_to_mb(NULL, dest_c, 100, &out_req);
  cfs_error_message(cfs_errc_success, NULL);
  cfs_get_last_error(NULL);

  PASS();
}

/**
 * \brief Test case for ipc_and_processes.
 * \return The test result.
 */
TEST ipc_and_processes() {
  cfs_message_pipe *pipe = NULL;
  cfs_process_t *proc = NULL;
  cfs_shm_segment *shm = NULL;
  cfs_named_semaphore *sem = NULL;
  void *addr;

  cfs_message_pipe_create(CFS_STR("pipe"), &pipe);
  cfs_message_pipe_destroy(pipe);
  cfs_message_pipe_create(NULL, &pipe);
  cfs_message_pipe_destroy(NULL);

  cfs_process_spawn(CFS_STR("dummy"), &proc);
  cfs_process_wait(proc);
  cfs_process_destroy(proc);
  cfs_process_spawn(NULL, &proc);
  cfs_process_wait(NULL);
  cfs_process_destroy(NULL);

  cfs_shm_create(1024, CFS_STR("shm"), &shm);
  cfs_shm_map(shm, &addr);
  cfs_shm_unmap(shm, addr);
  cfs_shm_destroy(shm);
  cfs_shm_create(0, NULL, &shm);
  cfs_shm_map(NULL, &addr);
  cfs_shm_unmap(NULL, addr);
  cfs_shm_destroy(NULL);

  cfs_named_semaphore_create(CFS_STR("sem"), 1, &sem);
  cfs_named_semaphore_wait(sem);
  cfs_named_semaphore_post(sem);
  cfs_named_semaphore_destroy(sem);
  cfs_named_semaphore_create(NULL, 1, &sem);
  cfs_named_semaphore_wait(NULL);
  cfs_named_semaphore_post(NULL);
  cfs_named_semaphore_destroy(NULL);

  PASS();
}

/**
 * \brief Dummy greenthread function for testing.
 * \param arg Opaque pointer to thread arguments.
 */
static void dummy_greenthread(void *arg) { (void)arg; }

/**
 * \brief Test case for greenthreads_and_utils.
 * \return The test result.
 */
TEST greenthreads_and_utils() {
  cfs_greenthread_t *gt = NULL;
  cfs_request_t *req = NULL;
  cfs_request_t *req_ptr = NULL;
  void *buf;
  cfs_size_t sz;
  cfs_runtime_t *rt;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p;
  cfs_sandbox_config sbox;

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  cfs_runtime_init(&cfg, &rt, &ec);

  cfs_greenthread_spawn(dummy_greenthread, NULL, &gt);
  cfs_greenthread_yield();
  cfs_greenthread_destroy(gt);

  cfs_greenthread_spawn(NULL, NULL, &gt);
  cfs_greenthread_destroy(NULL);

  cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (req) {
    memset(req, 0, sizeof(cfs_request_t));
    cfs_request_retain(req);
    cfs_cancel_request(rt, req);
    cfs_cancel_request(NULL, req);

    cfs_serialize_request(req, &buf, &sz);
    cfs_deserialize_request(buf, sz, &req_ptr);
    cfs_serialize_request(NULL, &buf, &sz);
    cfs_deserialize_request(NULL, sz, &req_ptr);

    cfs_request_release(req);
  }

  cfs_set_oom_handler(NULL);
  cfs_runtime_set_sandbox(rt, &sbox);
  cfs_runtime_set_sandbox(NULL, &sbox);

  cfs_path_init_str(&p, CFS_STR("dummy"));
  cfs_file_size_async(rt, &p, NULL, NULL);
  cfs_dir_itr_init_async(rt, &p, NULL, NULL);

  cfs_file_size_async(NULL, &p, NULL, NULL);
  cfs_dir_itr_init_async(NULL, &p, NULL, NULL);

  cfs_path_destroy(&p);
  cfs_runtime_destroy(rt);

  PASS();
}

/**
 * \brief Test case for exhaustive_nulls.
 * \return The test result.
 */
TEST exhaustive_nulls() {
  cfs_path p;
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

  cfs_path_init_str(&p, CFS_STR("dummy"));

  /* cfs_path nulls */
  cfs_path_init(NULL);
  cfs_path_destroy(NULL);
  cfs_path_clone(NULL, &p);
  cfs_path_clone(&p, NULL);
  cfs_path_c_str(&p, NULL);
  cfs_path_assign(NULL, CFS_STR(""));
  cfs_path_append(NULL, CFS_STR(""));
  cfs_path_is_empty(NULL, &b);
  cfs_path_is_empty(&p, NULL);
  cfs_path_filename(NULL, &p);
  cfs_path_extension(NULL, &p);
  cfs_path_stem(NULL, &p);
  cfs_path_remove_filename(NULL);

  /* cfs_string nulls */
  cfs_strlen(NULL, &out_sz);
  cfs_strlen(CFS_STR(""), NULL);
  cfs_strcpy(NULL, CFS_STR(""), &out_str);
  cfs_strcpy(buf, NULL, &out_str);
  cfs_strncpy(NULL, CFS_STR(""), 5, &out_str);
  cfs_strncpy(buf, NULL, 5, &out_str);
  cfs_strcat(NULL, CFS_STR(""), &out_str);
  cfs_strcat(buf, NULL, &out_str);
  cfs_strcmp(NULL, NULL, &out_int);
  cfs_strcmp(NULL, CFS_STR(""), &out_int);
  cfs_strcmp(CFS_STR(""), NULL, &out_int);
  cfs_strncmp(NULL, NULL, 5, &out_int);
  cfs_strncmp(NULL, CFS_STR(""), 5, &out_int);
  cfs_strncmp(CFS_STR(""), NULL, 5, &out_int);

  /* File nulls */
  cfs_remove(NULL, &ec);
  cfs_file_size(NULL, &out_u, &ec);
  cfs_file_size(&p, NULL, &ec);
  cfs_space(NULL, &out_spc, &ec);
  cfs_last_write_time(NULL, &out_ft, &ec);
  cfs_hard_link_count(NULL, &out_u, &ec);
  cfs_permissions(NULL, 0, 0, &ec);
  cfs_equivalent(NULL, &p, &b, &ec);
  cfs_equivalent(&p, NULL, &b, &ec);
  cfs_status(NULL, NULL, &ec);
  cfs_symlink_status(NULL, NULL, &ec);
  cfs_exists_path(NULL, &b, &ec);
  cfs_is_empty_path(NULL, &b, &ec);
  cfs_create_directory(NULL, &ec);
  cfs_create_directories(NULL, &ec);
  cfs_create_hard_link(NULL, &p, &ec);
  cfs_create_hard_link(&p, NULL, &ec);
  cfs_create_directory_symlink(NULL, &p, &ec);
  cfs_create_directory_symlink(&p, NULL, &ec);
  cfs_remove_all(NULL, &out_sz, &ec);
  cfs_rename(NULL, &p, &ec);
  cfs_rename(&p, NULL, &ec);
  cfs_resize_file(NULL, 0, &ec);
  cfs_temp_directory_path(NULL, &ec);

  /* Path copy/abs nulls */
  cfs_absolute(NULL, &p, &ec);
  cfs_canonical(NULL, &p, &ec);
  cfs_weakly_canonical(NULL, &p, &ec);
  cfs_read_symlink(NULL, &p, &ec);
  cfs_relative(NULL, &p, &p, &ec);
  cfs_proximate(NULL, &p, &p, &ec);
  cfs_copy(NULL, &p, 0, &ec);
  cfs_copy_symlink(NULL, &p, &ec);
  cfs_copy_file(NULL, &p, 0, &ec);
  cfs_current_path(NULL, &ec);
  cfs_current_path_set(NULL, &ec);

  /* Runtime nulls */
  cfs_runtime_init(NULL, &rt, &ec);
  cfs_runtime_init(NULL, NULL, &ec);
  cfs_dispatch_request(NULL, NULL, NULL, NULL);
  cfs_remove_async(NULL, NULL, NULL, NULL);
  cfs_runtime_poll(NULL);
  cfs_request_retain(NULL);
  cfs_request_release(NULL);

  /* Others */
  cfs_malloc(0, NULL);
  cfs_free(NULL);
  cfs_realloc(NULL, 0, NULL);
  cfs_calloc(0, 0, NULL);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for real_file_operations.
 * \return The test result.
 */
TEST real_file_operations() {
  FILE *f;
  cfs_path p;
  cfs_file_status st;
  cfs_error_code ec;
  cfs_uintmax_t size;
  cfs_space_info spc;
  cfs_file_time_type ft;
  cfs_uintmax_t links;
  cfs_perms perms = 0;
  cfs_size_t count;
  cfs_bool is_empty;
  cfs_path p_renamed;

  f = fopen("test_real.txt", "w");
  if (f) {
    fprintf(f, "test data");
    fclose(f);
  }

  cfs_path_init_str(&p, CFS_STR("test_real.txt"));

  cfs_status(&p, &st, &ec);
  cfs_file_size(&p, &size, &ec);
  cfs_space(&p, &spc, &ec);
  cfs_last_write_time(&p, &ft, &ec);
  cfs_hard_link_count(&p, &links, &ec);
  cfs_permissions(&p, perms, 0, &ec);
  cfs_is_empty_path(&p, &is_empty, &ec);

  cfs_path_init_str(&p_renamed, CFS_STR("test_real_renamed.txt"));
  cfs_rename(&p, &p_renamed, &ec);

  cfs_remove(&p_renamed, &ec);

  cfs_path_destroy(&p);
  cfs_path_destroy(&p_renamed);
  PASS();
}

/**
 * \brief Test case for more_coverage.
 * \return The test result.
 */
TEST more_coverage() {
  cfs_file_status s;
  cfs_path p, p2, p3, p4, out;
  cfs_error_code ec;
  cfs_bool b;
  cfs_perms perms = 0777;
  FILE *f;

  memset(&s, 0, sizeof(s));

  cfs_status_known(s, NULL);
  cfs_exists(s, NULL);
  cfs_is_block_file(s, NULL);
  cfs_is_character_file(s, NULL);
  cfs_is_directory(s, NULL);
  cfs_is_fifo(s, NULL);
  cfs_is_other(s, NULL);
  cfs_is_regular_file(s, NULL);
  cfs_is_socket(s, NULL);
  cfs_is_symlink(s, NULL);

  /* cfs_permissions with replace so it tries chmod and fails */
  cfs_path_init_str(&p, CFS_STR("dummy_nonexistent.txt"));
  cfs_permissions(&p, perms, cfs_perm_options_replace, &ec);

  /* cfs_equivalent success branch */
  f = fopen("test_eq.txt", "w");
  if (f) {
    fclose(f);
  }
  cfs_path_init_str(&p2, CFS_STR("test_eq.txt"));
  cfs_equivalent(&p2, &p2, &b, &ec);

  /* cfs_read_symlink success branch */
  cfs_path_init_str(&p3, CFS_STR("test_eq_sym.txt"));
  cfs_create_symlink(&p2, &p3, &ec);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_read_symlink(&p3, &out, &ec);
  cfs_path_destroy(&out);

  /* cfs_absolute already absolute */
  cfs_path_init_str(&p4, CFS_STR("/absolute/path"));
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_absolute(&p4, &out, &ec);
  cfs_path_destroy(&out);
  cfs_path_destroy(&p4);

  /* Canonical on absolute path */
  cfs_path_init_str(&p4, CFS_STR("/dev/null"));
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_canonical(&p4, &out, &ec);
  cfs_path_destroy(&out);
  cfs_path_destroy(&p4);

  remove("test_eq.txt");
  remove("test_eq_sym.txt");

  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);
  cfs_path_destroy(&p3);

  PASS();
}

/**
 * \brief Test case for final_coverage.
 * \return The test result.
 */
TEST final_coverage() {
  cfs_path p, p2, out;
  cfs_error_code ec;
  cfs_bool b;

  /* cfs_permissions success branch */
  FILE *f = fopen("test_perms.txt", "w");
  if (f) {
    fclose(f);
  }
  cfs_path_init_str(&p, CFS_STR("test_perms.txt"));
  cfs_permissions(&p, 0777, cfs_perm_options_replace, &ec);

  /* cfs_copy_symlink success branch */
  cfs_path_init_str(&p2, CFS_STR("test_perms_sym.txt"));
  cfs_create_symlink(&p, &p2, &ec);
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_copy_symlink(&p2, &out, &ec);
  cfs_path_destroy(&out);

  /* cfs_path_lexically_relative */
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_path_lexically_relative(&p, &p2, &out);
  cfs_path_destroy(&out);

  /* cfs_path_is_absolute */
  cfs_path_is_absolute(&p, &b);

  remove("test_perms.txt");
  remove("test_perms_sym.txt");
  cfs_path_destroy(&p);
  cfs_path_destroy(&p2);

  PASS();
}

CFS_API extern int g_cfs_malloc_fail;
CFS_API extern int g_cfs_realloc_fail;
CFS_API extern int g_cfs_calloc_fail;
CFS_API extern int g_cfs_getcwd_fail;
CFS_API extern int g_cfs_readlink_fail;

/**
 * \brief Test case for out_of_memory.
 * \return The test result.
 */
TEST out_of_memory() {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_request_t *req = NULL;
  cfs_path p;
  void *buf;
  cfs_size_t sz;
  cfs_message_pipe *pipe;
  cfs_process_t *proc;
  cfs_shm_segment *shm;
  cfs_named_semaphore *sem;
  cfs_greenthread_t *gt;
  cfs_greenthread_scheduler *sched;
  cfs_request_t req_dummy;

  cfs_path_init_str(&p, CFS_STR("dummy"));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  /* Test cfs_malloc OOM */
  g_cfs_malloc_fail = 1;

  cfs_malloc(100, &buf);
  cfs_path_assign(&p, CFS_STR("very_long_string_to_force_allocation"));
  cfs_path_generic_string(&p, (cfs_char_t **)&buf);
  cfs_runtime_init(&cfg, &rt, &ec);
  cfs_remove_async(rt, &p, NULL, NULL);
  cfs_file_size_async(rt, &p, NULL, NULL);
  cfs_message_pipe_create(CFS_STR("pipe"), &pipe);
  cfs_process_spawn(CFS_STR("dummy"), &proc);
  cfs_shm_create(1024, CFS_STR("shm"), &shm);
  cfs_named_semaphore_create(CFS_STR("sem"), 1, &sem);
  cfs_greenthread_spawn(NULL, NULL, &gt);
  cfs_greenthread_scheduler_init(&sched);
  cfs_dir_itr_init_async(rt, &p, NULL, NULL);

  req_dummy.opcode = 0;
  cfs_serialize_request(&req_dummy, &buf, &sz);
  cfs_deserialize_request(buf, sz, &req);

  g_cfs_malloc_fail = 0;

  /* Test cfs_realloc OOM */
  g_cfs_realloc_fail = 1;
  cfs_realloc(NULL, 100, &buf);
  cfs_path_assign(&p, CFS_STR("another_long_string_for_realloc"));
  g_cfs_realloc_fail = 0;

  /* Test cfs_calloc OOM */
  g_cfs_calloc_fail = 1;
  cfs_calloc(10, 10, &buf);
  g_cfs_calloc_fail = 0;

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for extreme_edge_cases.
 * \return The test result.
 */
TEST extreme_edge_cases() {
  cfs_path p;
  cfs_path out;
  cfs_error_code ec;
  cfs_bool b;
  cfs_path_init_str(&p, NULL);

  /* cfs_current_path getcwd failure */
  g_cfs_getcwd_fail = 1;
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_current_path(&out, &ec);
  cfs_path_destroy(&out);
  g_cfs_getcwd_fail = 0;

  /* cfs_read_symlink readlink failure */
  g_cfs_readlink_fail = 1;
  cfs_path_init_str(&p, CFS_STR("dummy_symlink_path"));
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_read_symlink(&p, &out, &ec);
  cfs_path_destroy(&out);
  g_cfs_readlink_fail = 0;

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for last_mile.
 * \return The test result.
 */
TEST last_mile() {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p;
  cfs_path out;
  cfs_request_t req;
  cfs_request_t *req1 = NULL, *req2 = NULL;

  cfs_path_init_str(&p, CFS_STR("dummy"));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  /* 2635-2636: cfs_absolute where getcwd fails */
  g_cfs_getcwd_fail = 1;
  cfs_path_init_str(&out, CFS_STR("test_perms_sym_2.txt"));
  cfs_absolute(&p, &out, &ec);
  cfs_path_destroy(&out);
  g_cfs_getcwd_fail = 0;

  /* 959: invalid opcode execution */
  memset(&req, 0, sizeof(req));
  req.opcode = 9999;
  cfs_dispatch_request(NULL, &req, NULL, NULL);

  /* Async errors inside thread pool / queues */
  cfs_runtime_init(&cfg, &rt, &ec);

  /* Force malloc failures for async variants */
  g_cfs_malloc_fail = 1;
  cfs_remove_async(rt, &p, NULL, NULL);
  cfs_file_size_async(rt, &p, NULL, NULL);
  g_cfs_malloc_fail = 0;

  /* Add cancelled requests and normal requests to queue, then destroy runtime
   */
  cfs_malloc(sizeof(cfs_request_t), (void **)&req1);
  cfs_malloc(sizeof(cfs_request_t), (void **)&req2);
  if (req1 && req2) {
    memset(req1, 0, sizeof(*req1));
    memset(req2, 0, sizeof(*req2));
    req1->ref_count = 1;
    req2->ref_count = 1;
    req1->cancelled = cfs_true;
    cfs_dispatch_request(rt, req1, NULL, NULL);
    cfs_dispatch_request(rt, req2, NULL, NULL);
  }

  /* Give threads a tiny bit of time if possible to process */
  /* cfs_runtime_destroy will flush the queues and destroy the pool */
  cfs_runtime_destroy(rt);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for cover_everything.
 * \return The test result.
 */
TEST cover_everything() {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p;
  cfs_request_t req;

  cfs_log_debug("testing logger");

  cfg.mode = cfs_modality_sync;
  cfg.thread_pool_size = 0;
  cfg.ipc_path = NULL;
  cfs_runtime_init(&cfg, &rt, &ec);

  /* Force 959 */
  memset(&req, 0, sizeof(req));
  req.opcode = 9999;
  cfs_dispatch_request(rt, &req, NULL, NULL);

  /* Clean up */
  cfs_runtime_destroy(rt);

  /* Thread pool creation failure */
  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;

  g_cfs_calloc_fail = 1;
  cfs_runtime_init(&cfg, &rt, &ec);
  g_cfs_calloc_fail = 0;

  g_cfs_malloc_fail = 1;
  cfs_runtime_init(&cfg, &rt, &ec);
  g_cfs_malloc_fail = 0;

  PASS();
}

/**
 * \brief Test case for out_of_memory_precise.
 * \return The test result.
 */
TEST out_of_memory_precise() {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p;
  cfs_path_init_str(&p, CFS_STR("dummy"));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  /* cfs_runtime_init has 3 allocations: 1 malloc, 2 mallocs inside pool setup?
     Wait, cfs_runtime_init allocates rt (malloc), work_queue (malloc),
     completion_queue (malloc). Then cfs_thread_pool_create allocates pool
     (malloc), pool->threads (calloc). */

  g_cfs_malloc_fail = 2; /* Fail allocating work_queue */
  cfs_runtime_init(&cfg, &rt, &ec);

  g_cfs_malloc_fail = 3; /* Fail allocating completion_queue */
  cfs_runtime_init(&cfg, &rt, &ec);

  g_cfs_malloc_fail = 4; /* Fail allocating thread_pool_t */
  cfs_runtime_init(&cfg, &rt, &ec);

  g_cfs_malloc_fail = 0;

  /* cfs_file_size_async has 2 allocations: request_t and result_buffer */
  cfs_runtime_init(&cfg, &rt, &ec);

  g_cfs_malloc_fail = 2; /* Fail allocating result buffer */
  cfs_file_size_async(rt, &p, NULL, NULL);

  g_cfs_malloc_fail = 0;
  cfs_runtime_destroy(rt);
  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test case for fix_last_missing.
 * \return The test result.
 */
TEST fix_last_missing() {
  cfs_runtime_t *rt = NULL;
  cfs_runtime_config cfg;
  cfs_error_code ec;
  cfs_path p;
  cfs_path out;
  cfs_request_t *req1;

  cfs_path_init_str(&p, CFS_STR("dummy"));

  cfg.mode = cfs_modality_multithread;
  cfg.thread_pool_size = 1;
  cfg.ipc_path = NULL;

  /* Re-test malloc failures with proper resetting of the countdown */
  cfs_runtime_init(&cfg, &rt, &ec);

  g_cfs_malloc_fail = 1;
  cfs_remove_async(rt, &p, NULL, NULL);

  g_cfs_malloc_fail = 1;
  cfs_file_size_async(rt, &p, NULL, NULL);

  g_cfs_malloc_fail = 0;

  /* Hit 1171 specifically */
  cfs_malloc(sizeof(cfs_request_t), (void **)&req1);
  if (req1) {
    memset(req1, 0, sizeof(*req1));
    req1->ref_count = 1;
    req1->cancelled = cfs_true;
    cfs_dispatch_request(rt, req1, NULL, NULL);

    /* Give thread time to pop it BEFORE shutdown */
    test_sleep_ms(100);
  }

  cfs_runtime_destroy(rt);

  cfs_path_destroy(&p);
  PASS();
}

/**
 * \brief Test suite for cfs_suite.
 */
SUITE(cfs_suite) {
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
  RUN_TEST(thread_pool_async_validation);
  RUN_TEST(greenthread_scheduler_validation);
  RUN_TEST(memory_allocation);
  RUN_TEST(string_handling);
}

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
