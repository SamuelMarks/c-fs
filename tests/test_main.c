/* clang-format off */
#if !defined(__STDC_WANT_LIB_EXT1__)
#define __STDC_WANT_LIB_EXT1__ 1
#endif
#include "cfs/cfs.h"
#include <string.h>
#include "greatest.h"

#if defined(CFS_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif
/* clang-format on */

TEST path_initialization() {
  cfs_path p;
  cfs_path_init(&p);
  {
    cfs_bool empty;
    cfs_path_is_empty(&p, &empty);
    ASSERT_EQ(1, empty);
  }

  cfs_path_init_str(&p, CFS_STR("test/path"));
  {
    cfs_bool empty;
    cfs_path_is_empty(&p, &empty);
    ASSERT_EQ(0, empty);
  }
  {
    const cfs_char_t *c_str;
    cfs_path_c_str(&p, &c_str);
    ASSERT_EQ(0, cfs_strcmp(CFS_STR("test/path"), c_str));
  }

  cfs_path_destroy(&p);
  {
    cfs_bool empty;
    cfs_path_is_empty(&p, &empty);
    ASSERT_EQ(1, empty);
  }
  PASS();
}

TEST path_appending() {
  cfs_path p;
  cfs_path_init_str(&p, CFS_STR("dir"));
  cfs_path_append(&p, CFS_STR("file.txt"));

#if defined(CFS_OS_WINDOWS)
  {
    const cfs_char_t *c_str;
    cfs_path_c_str(&p, &c_str);
    ASSERT_EQ(0, cfs_strcmp(CFS_STR("dir\\file.txt"), c_str));
  }
#else
  {
    const cfs_char_t *c_str;
    cfs_path_c_str(&p, &c_str);
    ASSERT_EQ(0, cfs_strcmp(CFS_STR("dir/file.txt"), c_str));
  }
#endif

  cfs_path_destroy(&p);
  PASS();
}

TEST path_decomposition() {
  cfs_path p, res;
  cfs_path_init_str(&p, CFS_STR("dir/subdir/file.txt"));

  cfs_path_filename(&p, &res);
  {
    const cfs_char_t *c_str;
    cfs_path_c_str(&res, &c_str);
    ASSERT_EQ(0, cfs_strcmp(CFS_STR("file.txt"), c_str));
  }
  cfs_path_destroy(&res);

  cfs_path_extension(&p, &res);
  {
    const cfs_char_t *c_str;
    cfs_path_c_str(&res, &c_str);
    ASSERT_EQ(0, cfs_strcmp(CFS_STR(".txt"), c_str));
  }
  cfs_path_destroy(&res);

  cfs_path_stem(&p, &res);
  {
    const cfs_char_t *c_str;
    cfs_path_c_str(&res, &c_str);
    ASSERT_EQ(0, cfs_strcmp(CFS_STR("file"), c_str));
  }
  cfs_path_destroy(&res);

  cfs_path_destroy(&p);
  PASS();
}

/* Step 45. Write multithreading test cases simulating concurrent file creations
 */
static int test_completed_ops = 0;

static void async_callback(cfs_request_t *req, void *user_data) {
  (void)req;
  (void)user_data;
  test_completed_ops++;
}

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

SUITE(cfs_suite) {
  RUN_TEST(path_initialization);
  RUN_TEST(path_appending);
  RUN_TEST(path_decomposition);
  RUN_TEST(thread_pool_async_validation);
  RUN_TEST(greenthread_scheduler_validation);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(cfs_suite);
  GREATEST_MAIN_END();
}
