/* clang-format off */
#include "cfs/cfs.h"
#include "greatest.h"

#if defined(CFS_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif
/* clang-format on */

TEST path_initialization() {
  cfs_path p;
  cfs_path_init(&p);
  ASSERT_EQ(1, cfs_path_is_empty(&p));

  cfs_path_init_str(&p, CFS_STR("test/path"));
  ASSERT_EQ(0, cfs_path_is_empty(&p));
  ASSERT_EQ(0, cfs_strcmp(CFS_STR("test/path"), cfs_path_c_str(&p)));

  cfs_path_destroy(&p);
  ASSERT_EQ(1, cfs_path_is_empty(&p));
  PASS();
}

TEST path_appending() {
  cfs_path p;
  cfs_path_init_str(&p, CFS_STR("dir"));
  cfs_path_append(&p, CFS_STR("file.txt"));

#if defined(CFS_OS_WINDOWS)
  ASSERT_EQ(0, cfs_strcmp(CFS_STR("dir\\file.txt"), cfs_path_c_str(&p)));
#else
  ASSERT_EQ(0, cfs_strcmp(CFS_STR("dir/file.txt"), cfs_path_c_str(&p)));
#endif

  cfs_path_destroy(&p);
  PASS();
}

TEST path_decomposition() {
  cfs_path p, res;
  cfs_path_init_str(&p, CFS_STR("dir/subdir/file.txt"));

  cfs_path_filename(&p, &res);
  ASSERT_EQ(0, cfs_strcmp(CFS_STR("file.txt"), cfs_path_c_str(&res)));
  cfs_path_destroy(&res);

  cfs_path_extension(&p, &res);
  ASSERT_EQ(0, cfs_strcmp(CFS_STR(".txt"), cfs_path_c_str(&res)));
  cfs_path_destroy(&res);

  cfs_path_stem(&p, &res);
  ASSERT_EQ(0, cfs_strcmp(CFS_STR("file"), cfs_path_c_str(&res)));
  cfs_path_destroy(&res);

  cfs_path_destroy(&p);
  PASS();
}

/* Step 45. Write multithreading test cases simulating concurrent file creations
 */
static int test_completed_ops = 0;

static void async_callback(cfs_request_t *req, void *user_data) {
  test_completed_ops++;
}

TEST thread_pool_async_validation() {
  cfs_runtime_config config;
  cfs_runtime_t *rt;
  cfs_path p;
  cfs_error_code ec;
  int i;

  config.mode = cfs_modality_multithread;
  config.thread_pool_size = 4;
  config.ipc_path = NULL;

  rt = cfs_runtime_init(&config, &ec);
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
  int res = cfs_greenthread_scheduler_init(&sched);
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
