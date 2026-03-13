/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>
#include "cfs/cfs.h"
#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#elif !defined(__WATCOMC__) && !defined(__MSDOS__) && !defined(CFS_OS_DOS)
#include <pthread.h>
#include <unistd.h>
#endif
/* clang-format on */

/* 38. Architectural #ifdef blocks delegating to MSVC native vs POSIX */
#if defined(CFS_OS_WINDOWS)
/* Windows native includes */
#elif defined(CFS_OS_LINUX) || defined(CFS_OS_MACOS) || defined(CFS_OS_BSD) || \
    defined(CFS_ENV_CYGWIN)
/* POSIX native includes */
#elif defined(CFS_OS_DOS)
/* DOS stub includes */
#else
/* Fallback includes */
#endif

/* Implementation details */

static int cfs_is_separator(cfs_char_t c, cfs_bool *out);
static int cfs_path_reserve(cfs_path *p, cfs_size_t new_cap);

/* Phase 5.5: Execution Context & Modality Implementations */

struct cfs_runtime_t {
  cfs_runtime_config config;
  cfs_queue_t *work_queue;
  cfs_queue_t *completion_queue;
  cfs_thread_pool_t *thread_pool;
};

/* Phase 5.6: Deferred Execution and Asynchronous Base Implementations */

/* 12. Generic Opcode Execution */
static void cfs_execute_op_inline(cfs_request_t *req) {
  if (!req)
    return;

  switch (req->opcode) {
  case cfs_opcode_remove:
    cfs_remove(&req->target_path, &req->error);
    break;
  case cfs_opcode_file_size: {
    cfs_uintmax_t size = 0;
    cfs_file_size(&req->target_path, &size, &req->error);
    if (req->result_buffer && req->result_size >= sizeof(cfs_uintmax_t)) {
      *((cfs_uintmax_t *)req->result_buffer) = size;
    }
    break;
  }
  /* Add other mappings as required */
  default:
    break;
  }
}

/* Platform specific Mutex/Cond/Thread wrappers for C89 */
#if defined(CFS_OS_WINDOWS)
/* Defined via winsock2.h earlier */
struct cfs_mutex_t {
  CRITICAL_SECTION cs;
};
struct cfs_thread_t {
  HANDLE h;
};

#if defined(_MSC_VER) && _MSC_VER < 1500
/* MSVC 2005 fallback */
struct cfs_cond_t {
  HANDLE event;
};
static void cfs_cond_init(cfs_cond_t *c) {
  c->event = CreateEvent(NULL, FALSE, FALSE, NULL);
}
static void cfs_cond_destroy(cfs_cond_t *c) { CloseHandle(c->event); }
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  LeaveCriticalSection(&m->cs);
  WaitForSingleObject(c->event, INFINITE);
  EnterCriticalSection(&m->cs);
}
static void cfs_cond_signal(cfs_cond_t *c) { SetEvent(c->event); }
static void cfs_cond_broadcast(cfs_cond_t *c) { SetEvent(c->event); }
#else
struct cfs_cond_t {
  CONDITION_VARIABLE cv;
};
static void cfs_cond_init(cfs_cond_t *c) {
  InitializeConditionVariable(&c->cv);
}
static void cfs_cond_destroy(cfs_cond_t *c) { (void)c; } /* No-op on Windows */
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  SleepConditionVariableCS(&c->cv, &m->cs, INFINITE);
}
static void cfs_cond_signal(cfs_cond_t *c) { WakeConditionVariable(&c->cv); }
static void cfs_cond_broadcast(cfs_cond_t *c) {
  WakeAllConditionVariable(&c->cv);
}
#endif

static void cfs_mutex_init(cfs_mutex_t *m) {
  InitializeCriticalSection(&m->cs);
}
static void cfs_mutex_destroy(cfs_mutex_t *m) { DeleteCriticalSection(&m->cs); }
static void cfs_mutex_lock(cfs_mutex_t *m) { EnterCriticalSection(&m->cs); }
static void cfs_mutex_unlock(cfs_mutex_t *m) { LeaveCriticalSection(&m->cs); }
#elif defined(CFS_OS_DOS)
struct cfs_mutex_t {
  int dummy;
};
struct cfs_cond_t {
  int dummy;
};
struct cfs_thread_t {
  int dummy;
};

static void cfs_mutex_init(cfs_mutex_t *m) { (void)m; }
static void cfs_mutex_destroy(cfs_mutex_t *m) { (void)m; }
static void cfs_mutex_lock(cfs_mutex_t *m) { (void)m; }
static void cfs_mutex_unlock(cfs_mutex_t *m) { (void)m; }

static void cfs_cond_init(cfs_cond_t *c) { (void)c; }
static void cfs_cond_destroy(cfs_cond_t *c) { (void)c; }
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  (void)c;
  (void)m;
}
static void cfs_cond_signal(cfs_cond_t *c) { (void)c; }
static void cfs_cond_broadcast(cfs_cond_t *c) { (void)c; }
#else
struct cfs_mutex_t {
  pthread_mutex_t m;
};
struct cfs_cond_t {
  pthread_cond_t c;
};
struct cfs_thread_t {
  pthread_t t;
};

static void cfs_mutex_init(cfs_mutex_t *m) { pthread_mutex_init(&m->m, NULL); }
static void cfs_mutex_destroy(cfs_mutex_t *m) { pthread_mutex_destroy(&m->m); }
static void cfs_mutex_lock(cfs_mutex_t *m) { pthread_mutex_lock(&m->m); }
static void cfs_mutex_unlock(cfs_mutex_t *m) { pthread_mutex_unlock(&m->m); }

static void cfs_cond_init(cfs_cond_t *c) { pthread_cond_init(&c->c, NULL); }
static void cfs_cond_destroy(cfs_cond_t *c) { pthread_cond_destroy(&c->c); }
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  pthread_cond_wait(&c->c, &m->m);
}
static void cfs_cond_signal(cfs_cond_t *c) { pthread_cond_signal(&c->c); }
static void cfs_cond_broadcast(cfs_cond_t *c) { pthread_cond_broadcast(&c->c); }
#endif

/* 15, 18. Thread-Safe FIFO Queue */
struct cfs_queue_t {
  cfs_request_t *head;
  cfs_request_t *tail;
  cfs_mutex_t lock;
  cfs_cond_t cond;
  cfs_bool shutdown;
};

static void cfs_queue_init(cfs_queue_t *q) {
  q->head = NULL;
  q->tail = NULL;
  q->shutdown = cfs_false;
  cfs_mutex_init(&q->lock);
  cfs_cond_init(&q->cond);
}

static void cfs_queue_destroy(cfs_queue_t *q) {
  cfs_mutex_destroy(&q->lock);
  cfs_cond_destroy(&q->cond);
}

static void cfs_queue_push(cfs_queue_t *q, cfs_request_t *req) {
  cfs_mutex_lock(&q->lock);
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;
  if (q->tail) {
    q->tail->next = req;
  } else {
    q->head = req;
  }
  q->tail = req;
  cfs_cond_signal(&q->cond);
  cfs_mutex_unlock(&q->lock);
}

static int cfs_queue_pop(cfs_queue_t *q, cfs_bool wait_for_data,
                         cfs_request_t **out_req) {
  cfs_request_t *req = NULL;
  if (!out_req)
    return -1;
  *out_req = NULL;
  cfs_mutex_lock(&q->lock);

  while (q->head == NULL && !q->shutdown && wait_for_data) {
    cfs_cond_wait(&q->cond, &q->lock);
  }

  if (q->head != NULL) {
    req = q->head;
    q->head = req->next;
    if (q->head == NULL) {
      q->tail = NULL;
    }
  }

  cfs_mutex_unlock(&q->lock);
  *out_req = req;
  return req ? 0 : -1;
}

static void cfs_queue_shutdown(cfs_queue_t *q) {
  cfs_mutex_lock(&q->lock);
  q->shutdown = cfs_true;
  cfs_cond_broadcast(&q->cond);
  cfs_mutex_unlock(&q->lock);
}

/* 16. Thread Pool */
struct cfs_thread_pool_t {
  cfs_thread_t *threads;
  cfs_size_t num_threads;
  cfs_queue_t *work_queue;
  cfs_queue_t *completion_queue;
};

/* 17. Worker Thread Loop */
#if defined(CFS_OS_WINDOWS)
static DWORD WINAPI cfs_worker_thread(LPVOID arg) {
#elif defined(CFS_OS_DOS)
static void *cfs_worker_thread(void *arg) {
#else
static void *cfs_worker_thread(void *arg) {
#endif
  cfs_thread_pool_t *pool = (cfs_thread_pool_t *)arg;
  cfs_request_t *req = NULL;

  while (1) {
    if (cfs_queue_pop(pool->work_queue, cfs_true, &req) != 0 || !req) {
      /* Cascade shutdown signal to wake up other waiting threads when using
       * event fallback */
      cfs_cond_broadcast(&pool->work_queue->cond);
      break; /* Shutdown condition */
    }

    if (!req->cancelled) {
      cfs_execute_op_inline(req);
    } else {
      cfs_make_error_code_from_os(125, &req->error); /* ECANCELED fallback */
    }

    cfs_queue_push(pool->completion_queue, req);
  }

#if defined(CFS_OS_WINDOWS)
  return 0;
#else
  return NULL;
#endif
}

static int cfs_thread_pool_create(cfs_size_t num_threads, cfs_queue_t *work,
                                  cfs_queue_t *comp,
                                  cfs_thread_pool_t **out_pool) {
  cfs_size_t i;
  cfs_thread_pool_t *pool = NULL;
  if (!out_pool)
    return -1;
  *out_pool = NULL;

  cfs_malloc(sizeof(cfs_thread_pool_t), (void **)&pool);
  if (!pool)
    return -1;

  pool->num_threads = num_threads;
  pool->work_queue = work;
  pool->completion_queue = comp;
  cfs_calloc(num_threads, sizeof(cfs_thread_t), (void **)&(pool->threads));

  if (!pool->threads) {
    cfs_free(pool);
    return -1;
  }

  for (i = 0; i < num_threads; ++i) {
#if defined(CFS_OS_WINDOWS)
    pool->threads[i].h =
        CreateThread(NULL, 0, cfs_worker_thread, pool, 0, NULL);
#elif defined(CFS_OS_DOS)
    (void)pool;
    (void)i;
    (void)cfs_worker_thread;
#else
    pthread_create(&pool->threads[i].t, NULL, cfs_worker_thread, pool);
#endif
  }

  *out_pool = pool;
  return 0;
}

static void cfs_thread_pool_destroy(cfs_thread_pool_t *pool) {
  cfs_size_t i;
  if (!pool)
    return;

  cfs_queue_shutdown(pool->work_queue);

  for (i = 0; i < pool->num_threads; ++i) {
#if defined(CFS_OS_WINDOWS)
    if (pool->threads[i].h) {
      WaitForSingleObject(pool->threads[i].h, INFINITE);
      CloseHandle(pool->threads[i].h);
    }
#elif defined(CFS_OS_DOS)
    (void)pool;
    (void)i;
#else
    pthread_join(pool->threads[i].t, NULL);
#endif
  }

  cfs_free(pool->threads);
  cfs_free(pool);
}

/* 13. Non-blocking API variants */
CFS_API int cfs_remove_async(cfs_runtime_t *rt, const cfs_path *p,
                             cfs_callback_t cb, void *user_data) {
  cfs_request_t *req;

  if (!rt || !p)
    return -1;

  cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (!req)
    return -1;

  req->opcode = cfs_opcode_remove;
  cfs_path_init(&req->target_path);
  cfs_path_clone(&req->target_path, p);
  cfs_path_init(&req->dest_path);
  req->result_buffer = NULL;
  req->result_size = 0;
  cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;

  cfs_dispatch_request(rt, req, cb, user_data);
  return 0;
}

CFS_API int cfs_file_size_async(cfs_runtime_t *rt, const cfs_path *p,
                                cfs_callback_t cb, void *user_data) {
  cfs_request_t *req;

  if (!rt || !p)
    return -1;

  cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (!req)
    return -1;

  req->opcode = cfs_opcode_file_size;
  cfs_path_init(&req->target_path);
  cfs_path_clone(&req->target_path, p);
  cfs_path_init(&req->dest_path);

  req->result_size = sizeof(cfs_uintmax_t);
  cfs_malloc(req->result_size, (void **)&req->result_buffer);
  cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;

  cfs_dispatch_request(rt, req, cb, user_data);
  return 0;
}

CFS_API int cfs_runtime_init(const cfs_runtime_config *config,
                             cfs_runtime_t **out_rt, cfs_error_code *ec) {
  cfs_runtime_t *rt;
  if (ec)
    cfs_clear_error(ec);
  if (!out_rt) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return -1;
  }
  *out_rt = NULL;

  if (!config) {
    if (ec)
      cfs_make_error_code_from_os(22, ec); /* EINVAL fallback */
    return -1;
  }

  cfs_malloc(sizeof(cfs_runtime_t), (void **)&rt);
  if (!rt) {
    if (ec)
      cfs_make_error_code_from_os(12, ec); /* ENOMEM fallback */
    return -1;
  }

  rt->config = *config;
  rt->work_queue = NULL;
  rt->completion_queue = NULL;
  rt->thread_pool = NULL;

  /* TODO: initialize thread pools, IPC, etc based on config->mode */
  if (rt->config.mode == cfs_modality_async ||
      rt->config.mode == cfs_modality_multithread) {
    cfs_malloc(sizeof(cfs_queue_t), (void **)&(rt->work_queue));
    cfs_malloc(sizeof(cfs_queue_t), (void **)&(rt->completion_queue));
    cfs_queue_init(rt->work_queue);
    cfs_queue_init(rt->completion_queue);

    cfs_thread_pool_create(
        rt->config.thread_pool_size > 0 ? rt->config.thread_pool_size : 4,
        rt->work_queue, rt->completion_queue, &rt->thread_pool);
  }

  *out_rt = rt;
  return 0;
}

CFS_API void cfs_runtime_destroy(cfs_runtime_t *runtime) {
  if (!runtime)
    return;

  if (runtime->thread_pool) {
    cfs_thread_pool_destroy(runtime->thread_pool);
  }
  if (runtime->work_queue) {
    cfs_queue_destroy(runtime->work_queue);
    cfs_free(runtime->work_queue);
  }
  if (runtime->completion_queue) {
    cfs_queue_destroy(runtime->completion_queue);
    cfs_free(runtime->completion_queue);
  }

  cfs_free(runtime);
}

CFS_API void cfs_dispatch_request(cfs_runtime_t *runtime, cfs_request_t *req,
                                  cfs_callback_t cb, void *user_data) {
  if (!runtime || !req)
    return;

  req->callback = cb;
  req->user_data = user_data;

  if (runtime->config.mode == cfs_modality_sync) {
    if (!req->cancelled)
      cfs_execute_op_inline(req);
    if (req->callback) {
      req->callback(req, req->user_data);
    }
    cfs_request_release(req);
  } else {
    if (runtime->work_queue) {
      cfs_queue_push(runtime->work_queue, req);
    } else {
      cfs_request_release(req);
    }
  }
}

/* 19. cfs_runtime_poll() */
CFS_API int cfs_runtime_poll(cfs_runtime_t *rt) {
  int processed = 0;
  cfs_request_t *req = NULL;
  if (!rt || !rt->completion_queue)
    return 0;

  while (cfs_queue_pop(rt->completion_queue, cfs_false, &req) == 0 &&
         req != NULL) {
    if (req->callback) {
      req->callback(req, req->user_data);
    }
    cfs_request_release(req);
    processed++;
  }
  return processed;
}

/* Re-implementing lost Phase 6, 7, 8, 9 functions */

/* Phase 6: String Handling */
CFS_API int cfs_strlen(const cfs_char_t *str, cfs_size_t *out) {
  cfs_size_t len = 0;
  if (!out)
    return -1;
  *out = 0;
  if (!str)
    return -1;
  while (str[len])
    len++;
  *out = len;
  return 0;
}

CFS_API int cfs_strcpy(cfs_char_t *dest, const cfs_char_t *src,
                       cfs_char_t **out) {
  cfs_size_t i = 0;
  if (out)
    *out = dest;
  if (!dest || !src)
    return -1;
  while ((dest[i] = src[i]) != 0)
    i++;
  return 0;
}

CFS_API int cfs_strncpy(cfs_char_t *dest, const cfs_char_t *src, cfs_size_t n,
                        cfs_char_t **out) {
  cfs_size_t i;
  if (out)
    *out = dest;
  if (!dest || !src)
    return -1;
  for (i = 0; i < n && src[i] != 0; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = 0;
  return 0;
}

CFS_API int cfs_strcat(cfs_char_t *dest, const cfs_char_t *src,
                       cfs_char_t **out) {
  cfs_size_t dest_len = 0;
  cfs_size_t i = 0;
  if (out)
    *out = dest;
  cfs_strlen(dest, &dest_len);
  if (!dest || !src)
    return -1;
  while ((dest[dest_len + i] = src[i]) != 0)
    i++;
  return 0;
}

CFS_API int cfs_strcmp(const cfs_char_t *lhs, const cfs_char_t *rhs) {
  if (!lhs && !rhs)
    return 0;
  if (!lhs)
    return -1;
  if (!rhs)
    return 1;
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
  return wcscmp(lhs, rhs);
#else
  return strcmp(lhs, rhs);
#endif
}

CFS_API int cfs_strncmp(const cfs_char_t *lhs, const cfs_char_t *rhs,
                        cfs_size_t count) {
  if (!lhs && !rhs)
    return 0;
  if (!lhs)
    return -1;
  if (!rhs)
    return 1;
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
  return wcsncmp(lhs, rhs, count);
#else
  return strncmp(lhs, rhs, count);
#endif
}

#if defined(CFS_OS_WINDOWS)
CFS_API int cfs_utf8_to_utf16(const char *utf8_str, wchar_t *dest,
                              cfs_size_t dest_len, cfs_size_t *out_req) {
  int req = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, dest, (int)dest_len);
  if (out_req) {
    *out_req = (req > 0) ? (cfs_size_t)req : 0;
  }
  return (req > 0) ? 0 : -1;
}

CFS_API int cfs_utf16_to_utf8(const wchar_t *utf16_str, char *dest,
                              cfs_size_t dest_len, cfs_size_t *out_req) {
  int req = WideCharToMultiByte(CP_UTF8, 0, utf16_str, -1, dest, (int)dest_len,
                                NULL, NULL);
  if (out_req) {
    *out_req = (req > 0) ? (cfs_size_t)req : 0;
  }
  return (req > 0) ? 0 : -1;
}
#endif

CFS_API int cfs_mb_to_wide(const char *mb_str, wchar_t *dest,
                           cfs_size_t dest_len, cfs_size_t *out_req) {
#if defined(CFS_OS_WINDOWS)
  return cfs_utf8_to_utf16(mb_str, dest, dest_len, out_req);
#else
  if (out_req) {
    *out_req = 0; /* Fallback not used on POSIX where cfs_char_t is char */
  }
  return -1;
#endif
}

CFS_API int cfs_wide_to_mb(const wchar_t *wide_str, char *dest,
                           cfs_size_t dest_len, cfs_size_t *out_req) {
#if defined(CFS_OS_WINDOWS)
  return cfs_utf16_to_utf8(wide_str, dest, dest_len, out_req);
#else
  if (out_req) {
    *out_req = 0; /* Fallback not used on POSIX */
  }
  return -1;
#endif
}

/* Phase 7: Error Handling */
CFS_API void cfs_set_error(cfs_error_code *ec, int os_value,
                           cfs_errc standard_value) {
  if (ec) {
    ec->value = os_value;
    ec->errc = standard_value;
  }
}

CFS_API void cfs_clear_error(cfs_error_code *ec) {
  cfs_set_error(ec, 0, cfs_errc_success);
}

CFS_API int cfs_make_error_code_from_os(int os_error, cfs_error_code *out) {
  if (out) {
    out->value = os_error;
    out->errc = os_error == 0 ? cfs_errc_success : cfs_errc_unknown_error;
  }
  return 0;
}

CFS_API int cfs_get_last_error(cfs_error_code *out) {
#if defined(CFS_OS_WINDOWS)
  return cfs_make_error_code_from_os(GetLastError(), out);
#else
  /* missing errno include but simplify for now */
  return cfs_make_error_code_from_os(1, out);
#endif
}

CFS_API int cfs_error_message(cfs_errc err, const char **out) {
  (void)err;
  if (!out)
    return -1;
  *out = "Error";
  return 0;
}

/* Phase 8 & 9: Path Struct Basics */
CFS_API void cfs_path_init(cfs_path *p) {
  if (!p)
    return;
  p->str = NULL;
  p->length = 0;
  p->capacity = 0;
}

CFS_API int cfs_path_init_str(cfs_path *p, const cfs_char_t *source) {
  cfs_path_init(p);
  if (source) {
    return cfs_path_assign(p, source);
  }
  return 0;
}

CFS_API void cfs_path_destroy(cfs_path *p) {
  if (!p)
    return;
  if (p->str)
    cfs_free(p->str);
  p->str = NULL;
  p->length = 0;
  p->capacity = 0;
}

CFS_API int cfs_path_clone(cfs_path *dest, const cfs_path *src) {
  if (!dest || !src)
    return -1;
  cfs_path_init(dest);
  if (src->str) {
    return cfs_path_assign(dest, src->str);
  }
  return 0;
}

CFS_API int cfs_path_c_str(const cfs_path *p, const cfs_char_t **out) {
  if (!out)
    return -1;
  *out = (p && p->str) ? p->str : CFS_STR("");
  return 0;
}

CFS_API int cfs_path_make_preferred(cfs_path *p) {
  cfs_size_t i;
  if (!p || !p->str)
    return -1;
  for (i = 0; i < p->length; i++) {
#if defined(CFS_OS_WINDOWS)
    if (p->str[i] == CFS_CHAR('/'))
      p->str[i] = CFS_CHAR('\\');
#else
    if (p->str[i] == CFS_CHAR('\\'))
      p->str[i] = CFS_CHAR('/');
#endif
  }
  return 0;
}

CFS_API int cfs_path_generic_string(const cfs_path *p, cfs_char_t **out) {
  cfs_char_t *res;
  cfs_size_t i;
  if (!out)
    return -1;
  *out = NULL;
  if (!p || !p->str)
    return -1;
  cfs_malloc((p->length + 1) * sizeof(cfs_char_t), (void **)&res);
  if (!res)
    return -1;
  CFS_STRCPY_SAFE(res, p->length + 1, p->str);
  for (i = 0; i < p->length; i++) {
#if defined(CFS_OS_WINDOWS)
    if (res[i] == CFS_CHAR('\\'))
      res[i] = CFS_CHAR('/');
#endif
  }
  *out = res;
  return 0;
}

CFS_API void cfs_path_clear(cfs_path *p) {
  if (!p)
    return;
  if (p->str)
    p->str[0] = 0;
  p->length = 0;
}

CFS_API void cfs_path_swap(cfs_path *lhs, cfs_path *rhs) {
  cfs_path temp;
  if (!lhs || !rhs)
    return;
  temp = *lhs;
  *lhs = *rhs;
  *rhs = temp;
}

CFS_API int cfs_path_concat(cfs_path *p, const cfs_char_t *source) {
  cfs_size_t src_len;
  cfs_size_t new_len;
  if (!p || !source)
    return -1;
  cfs_strlen(source, &src_len);
  if (src_len == 0)
    return 0;
  new_len = p->length + src_len;
  if (cfs_path_reserve(p, new_len + 1) != 0)
    return -1;
  CFS_STRCPY_SAFE(p->str + p->length, p->capacity - p->length, source);
  p->length = new_len;
  return 0;
}

/* cfs_path_append implementation from recovered file ending */
CFS_API int cfs_path_append(cfs_path *p, const cfs_char_t *source) {
  cfs_size_t src_len;
  cfs_bool p_has_sep = cfs_false;
  cfs_bool src_has_sep = cfs_false;
  cfs_size_t new_len;
#if defined(CFS_OS_WINDOWS)
  cfs_size_t len = 0;
#endif

  if (!p || !source)
    return -1;

  {
    cfs_bool empty;
    cfs_path_is_empty(p, &empty);
    if (empty) {
      return cfs_path_assign(p, source);
    }
  }

  if (source[0] == 0)
    return 0;

#if defined(CFS_OS_WINDOWS)
  if (source[0] == CFS_CHAR('\\') || source[0] == CFS_CHAR('/') ||
      ((cfs_strlen(source, &len), len >= 2) && source[1] == CFS_CHAR(':'))) {
    return cfs_path_assign(p, source);
  }
#else
  if (source[0] == CFS_CHAR('/')) {
    return cfs_path_assign(p, source);
  }
#endif

  cfs_strlen(source, &src_len);
  if (p->length > 0)
    cfs_is_separator(p->str[p->length - 1], &p_has_sep);
  cfs_is_separator(source[0], &src_has_sep);

  new_len = p->length + src_len;
  if (!p_has_sep && !src_has_sep)
    new_len++;
  else if (p_has_sep && src_has_sep)
    new_len--;

  if (cfs_path_reserve(p, new_len + 1) != 0)
    return -1;

  if (!p_has_sep && !src_has_sep) {
    p->str[p->length] = CFS_PREFERRED_SEPARATOR;
    p->str[p->length + 1] = 0;
    p->length++;
  } else if (p_has_sep && src_has_sep) {
    source++;
  }

  CFS_STRCPY_SAFE(p->str + p->length, p->capacity - p->length, source);
  p->length = new_len;
  return 0;
}

static int cfs_is_separator(cfs_char_t c, cfs_bool *out) {
  if (!out)
    return -1;
  *out = (c == CFS_CHAR('/') || c == CFS_CHAR('\\')) ? cfs_true : cfs_false;
  return 0;
}

static int cfs_path_reserve(cfs_path *p, cfs_size_t new_cap) {
  cfs_char_t *new_str;
  if (!p)
    return -1;
  if (new_cap <= p->capacity)
    return 0;

  if (p->str) {
    cfs_realloc(p->str, new_cap * sizeof(cfs_char_t), (void **)&new_str);
  } else {
    cfs_malloc(new_cap * sizeof(cfs_char_t), (void **)&new_str);
  }

  if (!new_str)
    return -1;

  p->str = new_str;
  p->capacity = new_cap;
  return 0;
}

CFS_API int cfs_path_is_empty(const cfs_path *p, cfs_bool *out) {
  if (!out)
    return -1;
  *out = (!p || p->length == 0);
  return 0;
}

CFS_API int cfs_path_assign(cfs_path *p, const cfs_char_t *source) {
  cfs_size_t len;
  if (!p)
    return -1;
  if (!source) {
    cfs_path_clear(p);
    return 0;
  }
  cfs_strlen(source, &len);
  if (cfs_path_reserve(p, len + 1) != 0)
    return -1;
  CFS_STRCPY_SAFE(p->str, p->capacity, source);
  p->length = len;
  return 0;
}

CFS_API int cfs_malloc(cfs_size_t size, void **out) {
  if (out)
    *out = malloc(size);
  return (out && *out) ? 0 : -1;
}
CFS_API void cfs_free(void *ptr) { free(ptr); }
CFS_API int cfs_realloc(void *ptr, cfs_size_t size, void **out) {
  if (out)
    *out = realloc(ptr, size);
  return (out && *out) ? 0 : -1;
}
CFS_API int cfs_calloc(cfs_size_t num, cfs_size_t size, void **out) {
  if (out)
    *out = calloc(num, size);
  return (out && *out) ? 0 : -1;
}

CFS_API int cfs_path_filename(const cfs_path *p, cfs_path *out) {
  cfs_size_t i;
  if (!out)
    return -1;
  cfs_path_init(out);
  if (!p || p->length == 0)
    return -1;

  for (i = p->length; i > 0; i--) {
    cfs_bool is_sep = cfs_false;
    cfs_is_separator(p->str[i - 1], &is_sep);
    if (is_sep)
      break;
  }

  if (i < p->length) {
    cfs_path_assign(out, p->str + i);
  }
  return 0;
}

CFS_API int cfs_path_extension(const cfs_path *p, cfs_path *out) {
  cfs_size_t i;
  if (!out)
    return -1;
  cfs_path_init(out);
  if (!p || p->length == 0)
    return -1;

  for (i = p->length; i > 0; i--) {
    if (p->str[i - 1] == CFS_CHAR('.')) {
      if (i > 1) {
        cfs_bool is_sep = cfs_false;
        cfs_is_separator(p->str[i - 2], &is_sep);
        if (!is_sep) {
          cfs_path_assign(out, p->str + i - 1);
          return 0;
        }
      }
    }
    {
      cfs_bool is_sep = cfs_false;
      cfs_is_separator(p->str[i - 1], &is_sep);
      if (is_sep)
        break;
    }
  }
  return 0;
}

CFS_API int cfs_path_stem(const cfs_path *p, cfs_path *out) {
  cfs_path fn;
  cfs_size_t i;
  if (!out)
    return -1;
  cfs_path_init(out);
  if (!p || p->length == 0)
    return -1;

  cfs_path_filename(p, &fn);
  if (fn.length == 0) {
    *out = fn;
    return 0;
  }

  for (i = fn.length; i > 0; i--) {
    if (fn.str[i - 1] == CFS_CHAR('.')) {
      if (i > 1) {
        cfs_path_reserve(out, i);
        CFS_STRNCPY_SAFE(out->str, out->capacity, fn.str, i - 1);
        out->str[i - 1] = 0;
        out->length = i - 1;
        cfs_path_destroy(&fn);
        return 0;
      }
    }
  }
  *out = fn;
  return 0;
}

CFS_API int cfs_remove(const cfs_path *p, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || p->length == 0)
    return -1;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (_wremove(p->str) == 0)
    return 0;
  if (RemoveDirectoryW(p->str))
    return 0;
#else
  if (remove(p->str) == 0)
    return 0;
  if (RemoveDirectoryA(p->str))
    return 0;
#endif
#else
  if (remove(p->str) == 0)
    return 0;
#endif
  if (ec)
    cfs_get_last_error(ec);
  return -1;
}

CFS_API int cfs_file_size(const cfs_path *p, cfs_uintmax_t *out,
                          cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || p->length == 0)
    return -1;
#if defined(CFS_OS_WINDOWS)
  {
    struct _stat64 st;
#if defined(CFS_UNICODE)
    if (_wstat64(p->str, &st) == 0) {
      if (out)
        *out = (cfs_uintmax_t)st.st_size;
      return 0;
    }
#else
    if (_stat64(p->str, &st) == 0) {
      if (out)
        *out = (cfs_uintmax_t)st.st_size;
      return 0;
    }
#endif
  }
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      if (out)
        *out = (cfs_uintmax_t)st.st_size;
      return 0;
    }
  }
#endif
  if (ec)
    cfs_get_last_error(ec);
  return -1;
}

/* Phase 3: Platform-Specific Async & Message Passing Implementations */

CFS_API void cfs_request_retain(cfs_request_t *req) {
  if (req) {
#if defined(CFS_OS_WINDOWS)
    InterlockedIncrement((LONG volatile *)&req->ref_count);
#elif defined(CFS_COMPILER_GCC) || defined(CFS_COMPILER_CLANG)
    __atomic_fetch_add(&req->ref_count, 1, __ATOMIC_SEQ_CST);
#else
    req->ref_count++; /* Unsafe fallback */
#endif
  }
}

static void cfs_request_destroy_internal(cfs_request_t *req) {
  if (!req)
    return;
  cfs_path_destroy(&req->target_path);
  cfs_path_destroy(&req->dest_path);
  if (req->result_buffer)
    cfs_free(req->result_buffer);
  cfs_free(req);
}

CFS_API void cfs_request_release(cfs_request_t *req) {
  int new_val;
  if (!req)
    return;

#if defined(CFS_OS_WINDOWS)
  new_val = InterlockedDecrement((LONG volatile *)&req->ref_count);
#elif defined(CFS_COMPILER_GCC) || defined(CFS_COMPILER_CLANG)
  new_val = __atomic_sub_fetch(&req->ref_count, 1, __ATOMIC_SEQ_CST);
#else
  new_val = --req->ref_count;
#endif

  if (new_val == 0) {
    cfs_request_destroy_internal(req);
  }
}

CFS_API int cfs_cancel_request(cfs_runtime_t *rt, cfs_request_t *req) {
  if (!rt || !req)
    return -1;
  /* Basic cancellation just marks it. The worker thread will skip execution if
   * it sees this. */
  /* Thread-safe boolean assignment (assuming aligned int is atomic enough for
   * this) */
  req->cancelled = cfs_true;
  return 0;
}

/* Phase 3: Message Passing and Async Backend Stubs */

struct cfs_message_pipe {
  void *handle;
};

CFS_API int cfs_message_pipe_create(const cfs_char_t *path,
                                    cfs_message_pipe **out_pipe) {
  if (!path || !out_pipe)
    return -1;
  cfs_malloc(sizeof(cfs_message_pipe), (void **)out_pipe);
  if (*out_pipe)
    (*out_pipe)->handle = NULL;
  return (*out_pipe) ? 0 : -1;
}

CFS_API void cfs_message_pipe_destroy(cfs_message_pipe *pipe) {
  if (pipe)
    cfs_free(pipe);
}

CFS_API int cfs_serialize_request(const cfs_request_t *req, void **buffer,
                                  cfs_size_t *size) {
  if (!req || !buffer || !size)
    return -1;
  /* Simplistic serialization simulation */
  *size = sizeof(int) + sizeof(cfs_size_t) * 2; /* Opcode + path lengths */
  cfs_malloc(*size, (void **)buffer);
  if (!*buffer)
    return -1;
  ((int *)*buffer)[0] = req->opcode;
  return 0;
}

CFS_API int cfs_deserialize_request(const void *buffer, cfs_size_t size,
                                    cfs_request_t **req) {
  if (!buffer || !req || size < sizeof(int))
    return -1;
  cfs_malloc(sizeof(cfs_request_t), (void **)req);
  if (!*req)
    return -1;
  (*req)->opcode = ((int *)buffer)[0];
  (*req)->ref_count = 1;
  (*req)->cancelled = cfs_false;
  (*req)->result_buffer = NULL;
  (*req)->result_size = 0;
  cfs_path_init(&(*req)->target_path);
  cfs_path_init(&(*req)->dest_path);
  (*req)->callback = NULL;
  (*req)->user_data = NULL;
  return 0;
}

/* Phase 4: Multiprocessing and Greenthreads Implementations */

struct cfs_process_t {
#if defined(CFS_OS_WINDOWS)
  HANDLE process_handle;
  HANDLE thread_handle;
#else
  pid_t pid;
#endif
};

CFS_API int cfs_process_spawn(const cfs_char_t *executable,
                              cfs_process_t **out_proc) {
  if (!executable || !out_proc)
    return -1;
  cfs_malloc(sizeof(cfs_process_t), (void **)out_proc);
  if (!*out_proc)
    return -1;

#if defined(CFS_OS_WINDOWS)
  {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, (LPWSTR)executable, NULL, NULL, FALSE, 0, NULL,
                        NULL, &si, &pi)) {
      cfs_free(*out_proc);
      *out_proc = NULL;
      return -1;
    }
    (*out_proc)->process_handle = pi.hProcess;
    (*out_proc)->thread_handle = pi.hThread;
  }
#else
  /* Fork/Exec stub for POSIX */
  (*out_proc)->pid = -1; /* Implement full fork/exec */
#endif
  return 0;
}

CFS_API int cfs_process_wait(cfs_process_t *proc) {
  if (!proc)
    return -1;
#if defined(CFS_OS_WINDOWS)
  WaitForSingleObject(proc->process_handle, INFINITE);
  return 0;
#else
  /* waitpid stub */
  return 0;
#endif
}

CFS_API void cfs_process_destroy(cfs_process_t *proc) {
  if (!proc)
    return;
#if defined(CFS_OS_WINDOWS)
  CloseHandle(proc->process_handle);
  CloseHandle(proc->thread_handle);
#endif
  cfs_free(proc);
}

struct cfs_shm_segment {
#if defined(CFS_OS_WINDOWS)
  HANDLE map_handle;
#else
  int fd;
#endif
  cfs_size_t size;
};

CFS_API int cfs_shm_create(cfs_size_t size, const cfs_char_t *name,
                           cfs_shm_segment **out_shm) {
  if (!name || !out_shm || size == 0)
    return -1;
  cfs_malloc(sizeof(cfs_shm_segment), (void **)out_shm);
  if (!*out_shm)
    return -1;
  (*out_shm)->size = size;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  (*out_shm)->map_handle = CreateFileMappingW(
      INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      (DWORD)(((cfs_uintmax_t)size) >> 32), (DWORD)(size & 0xFFFFFFFF), name);
#else
  (*out_shm)->map_handle = CreateFileMappingA(
      INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      (DWORD)(((cfs_uintmax_t)size) >> 32), (DWORD)(size & 0xFFFFFFFF), name);
#endif
  if (!(*out_shm)->map_handle) {
    cfs_free(*out_shm);
    *out_shm = NULL;
    return -1;
  }
#else
  /* shm_open stub */
  (*out_shm)->fd = -1;
#endif
  return 0;
}

CFS_API int cfs_shm_map(cfs_shm_segment *shm, void **out) {
  if (!shm || !out)
    return -1;
#if defined(CFS_OS_WINDOWS)
  *out = MapViewOfFile(shm->map_handle, FILE_MAP_ALL_ACCESS, 0, 0, shm->size);
  return *out ? 0 : -1;
#else
  *out = NULL; /* mmap stub */
  return -1;
#endif
}

CFS_API void cfs_shm_unmap(cfs_shm_segment *shm, void *addr) {
  if (!shm || !addr)
    return;
#if defined(CFS_OS_WINDOWS)
  UnmapViewOfFile(addr);
#else
  /* munmap stub */
#endif
}

CFS_API void cfs_shm_destroy(cfs_shm_segment *shm) {
  if (!shm)
    return;
#if defined(CFS_OS_WINDOWS)
  CloseHandle(shm->map_handle);
#else
  /* close stub */
#endif
  cfs_free(shm);
}

struct cfs_named_semaphore {
#if defined(CFS_OS_WINDOWS)
  HANDLE handle;
#else
  void *sem; /* sem_t* */
#endif
};

CFS_API int cfs_named_semaphore_create(const cfs_char_t *name,
                                       int initial_count,
                                       cfs_named_semaphore **out_sem) {
  if (!name || !out_sem)
    return -1;
  cfs_malloc(sizeof(cfs_named_semaphore), (void **)out_sem);
  if (!*out_sem)
    return -1;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  (*out_sem)->handle = CreateSemaphoreW(NULL, initial_count, 0x7FFFFFFF, name);
#else
  (*out_sem)->handle = CreateSemaphoreA(NULL, initial_count, 0x7FFFFFFF, name);
#endif
  if (!(*out_sem)->handle) {
    cfs_free(*out_sem);
    *out_sem = NULL;
    return -1;
  }
#else
  /* sem_open stub */
  (*out_sem)->sem = NULL;
#endif
  return 0;
}

CFS_API int cfs_named_semaphore_wait(cfs_named_semaphore *sem) {
  if (!sem)
    return -1;
#if defined(CFS_OS_WINDOWS)
  return WaitForSingleObject(sem->handle, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
#else
  return -1; /* sem_wait stub */
#endif
}

CFS_API int cfs_named_semaphore_post(cfs_named_semaphore *sem) {
  if (!sem)
    return -1;
#if defined(CFS_OS_WINDOWS)
  return ReleaseSemaphore(sem->handle, 1, NULL) ? 0 : -1;
#else
  return -1; /* sem_post stub */
#endif
}

CFS_API void cfs_named_semaphore_destroy(cfs_named_semaphore *sem) {
  if (!sem)
    return;
#if defined(CFS_OS_WINDOWS)
  CloseHandle(sem->handle);
#else
  /* sem_close stub */
#endif
  cfs_free(sem);
}

/* Greenthread basic stubs (Platform implementation requires assembly/ucontext
 * or setjmp logic) */
struct cfs_greenthread_t {
  void *context;
};

struct cfs_greenthread_scheduler {
  cfs_greenthread_t *current;
};

CFS_API int cfs_greenthread_spawn(cfs_greenthread_func func, void *arg,
                                  cfs_greenthread_t **out_gt) {
  (void)func;
  (void)arg;
  if (!out_gt)
    return -1;
  cfs_malloc(sizeof(cfs_greenthread_t), (void **)out_gt);
  if (*out_gt)
    (*out_gt)->context = NULL;
  return (*out_gt) ? 0 : -1;
}

CFS_API int cfs_greenthread_yield(void) { return 0; /* stub */ }

CFS_API void cfs_greenthread_destroy(cfs_greenthread_t *gt) {
  if (gt)
    cfs_free(gt);
}

CFS_API int
cfs_greenthread_scheduler_init(cfs_greenthread_scheduler **out_sched) {
  if (!out_sched)
    return -1;
  cfs_malloc(sizeof(cfs_greenthread_scheduler), (void **)out_sched);
  if (*out_sched)
    (*out_sched)->current = NULL;
  return (*out_sched) ? 0 : -1;
}

CFS_API int cfs_greenthread_scheduler_run(cfs_greenthread_scheduler *sched) {
  return sched ? 0 : -1;
}

CFS_API void
cfs_greenthread_scheduler_destroy(cfs_greenthread_scheduler *sched) {
  if (sched)
    cfs_free(sched);
}

/* Phase 5.7: Integration Implementations */

CFS_API int cfs_dir_itr_init_async(cfs_runtime_t *rt, const cfs_path *p,
                                   cfs_callback_t cb, void *user_data) {
  cfs_request_t *req;
  if (!rt || !p)
    return -1;

  cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (!req)
    return -1;

  /* Reusing a non-existent opcode for iterator init, but in reality we'd add
   * cfs_opcode_dir_itr_init */
  req->opcode = 999;
  cfs_path_init(&req->target_path);
  cfs_path_clone(&req->target_path, p);
  cfs_path_init(&req->dest_path);
  req->result_buffer = NULL;
  req->result_size = 0;
  cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->ref_count = 1;
  req->cancelled = cfs_false;
  req->next = NULL;

  cfs_dispatch_request(rt, req, cb, user_data);
  return 0;
}

CFS_API int cfs_runtime_set_sandbox(cfs_runtime_t *rt,
                                    const cfs_sandbox_config *config) {
  /* 43. Set internal sandbox bounds. This just stubs out the validation
   * structure. */
  if (!rt || !config)
    return -1;
  return 0;
}
CFS_API int cfs_status_known(cfs_file_status s, cfs_bool *out) {
  if (!out)
    return -1;
  *out = (s.type != cfs_file_type_none);
  return 0;
}

CFS_API int cfs_hard_link_count(const cfs_path *p, cfs_uintmax_t *out,
                                cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || p->length == 0 || !out) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return -1;
  }
#if defined(CFS_OS_WINDOWS)
  {
    struct _stat64 st;
#if defined(CFS_UNICODE)
    if (_wstat64(p->str, &st) == 0) {
#else
    if (_stat64(p->str, &st) == 0) {
#endif
      *out = (cfs_uintmax_t)st.st_nlink;
      return 0;
    }
  }
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      *out = (cfs_uintmax_t)st.st_nlink;
      return 0;
    }
  }
#endif
  if (ec)
    cfs_get_last_error(ec);
  return -1;
}

CFS_API int cfs_permissions(const cfs_path *p, cfs_perms prms,
                            cfs_perm_options opts, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || p->length == 0 || opts != cfs_perm_options_replace) {
    if (ec)
      cfs_set_error(
          ec, 0, cfs_errc_invalid_argument); /* Only support replace for now */
    return -1;
  }
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (_wchmod(p->str, (int)prms) == 0) {
#else
  if (_chmod(p->str, (int)prms) == 0) {
#endif
    return 0;
  }
#else
  if (chmod(p->str, (mode_t)prms) == 0) {
    return 0;
  }
#endif
  if (ec)
    cfs_get_last_error(ec);
  return -1;
}

CFS_API int cfs_equivalent(const cfs_path *p1, const cfs_path *p2,
                           cfs_bool *out, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p1 || !p2 || p1->length == 0 || p2->length == 0 || !out) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return -1;
  }
#if defined(CFS_OS_WINDOWS)
  {
    HANDLE h1;
    HANDLE h2;
#if defined(CFS_UNICODE)
    h1 = CreateFileW(p1->str, 0,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    h2 = CreateFileW(p2->str, 0,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#else
    h1 = CreateFileA(p1->str, 0,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    h2 = CreateFileA(p2->str, 0,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#endif
    if (h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE) {
      BY_HANDLE_FILE_INFORMATION i1, i2;
      if (GetFileInformationByHandle(h1, &i1) &&
          GetFileInformationByHandle(h2, &i2)) {
        *out = (i1.dwVolumeSerialNumber == i2.dwVolumeSerialNumber &&
                i1.nFileIndexHigh == i2.nFileIndexHigh &&
                i1.nFileIndexLow == i2.nFileIndexLow)
                   ? cfs_true
                   : cfs_false;
        CloseHandle(h1);
        CloseHandle(h2);
        return 0;
      }
    }
    if (ec)
      cfs_get_last_error(ec);
    if (h1 != INVALID_HANDLE_VALUE)
      CloseHandle(h1);
    if (h2 != INVALID_HANDLE_VALUE)
      CloseHandle(h2);
    return -1;
  }
#else
  {
    struct stat s1, s2;
    if (stat(p1->str, &s1) != 0 || stat(p2->str, &s2) != 0) {
      if (ec)
        cfs_get_last_error(ec); /* Or make error from errno */
      return -1;
    }
    *out = (s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino) ? cfs_true
                                                              : cfs_false;
    return 0;
  }
#endif
}

CFS_API int cfs_read_symlink(const cfs_path *p, cfs_path *out,
                             cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || !out || p->length == 0) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return -1;
  }
#if defined(CFS_OS_WINDOWS)
  {
    /* Minimal stub for Windows C89, symlinks require reparse points parsing */
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return -1;
  }
#elif defined(CFS_OS_DOS)
  {
    /* Minimal stub for DOS C89, symlinks require reparse points parsing */
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return -1;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    ssize_t len;
    len = readlink(p->str, buf, sizeof(buf) - 1);
    if (len != -1) {
      buf[len] = '\0';
      cfs_path_assign(out, buf);
      return 0;
    }
    if (ec)
      cfs_get_last_error(ec);
    return -1;
  }
#endif
}

CFS_API int cfs_absolute(const cfs_path *p, cfs_path *out, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || !out) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return -1;
  }
#if defined(CFS_OS_WINDOWS)
  {
    cfs_char_t buf[CFS_MAX_PATH];
    DWORD len;
#if defined(CFS_UNICODE)
    len = GetFullPathNameW(p->str, CFS_MAX_PATH, buf, NULL);
#else
    len = GetFullPathNameA(p->str, CFS_MAX_PATH, buf, NULL);
#endif
    if (len > 0 && len < CFS_MAX_PATH) {
      cfs_path_assign(out, buf);
      return 0;
    }
    if (ec)
      cfs_get_last_error(ec);
    return -1;
  }
#elif defined(CFS_OS_DOS)
  {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return -1;
  }
#else
  {
    cfs_bool is_abs = cfs_false;
    cfs_path cp;
    if (cfs_path_is_absolute(p, &is_abs) == 0 && is_abs) {
      cfs_path_assign(out, p->str);
      return 0;
    }
    cfs_path_init(&cp);
    if (cfs_current_path(&cp, ec) == 0) {
      cfs_path_assign(out, cp.str);
      cfs_path_append(out, p->str);
      cfs_path_destroy(&cp);
      return 0;
    }
    cfs_path_destroy(&cp);
    return -1;
  }
#endif
}

CFS_API int cfs_canonical(const cfs_path *p, cfs_path *out,
                          cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || !out) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return -1;
  }
#if defined(CFS_OS_WINDOWS)
  {
    cfs_char_t buf[CFS_MAX_PATH];
    DWORD len;
#if defined(CFS_UNICODE)
    len = GetFullPathNameW(p->str, CFS_MAX_PATH, buf, NULL);
#else
    len = GetFullPathNameA(p->str, CFS_MAX_PATH, buf, NULL);
#endif
    if (len > 0 && len < CFS_MAX_PATH) {
      cfs_path_assign(out, buf);
      return 0;
    }
    if (ec)
      cfs_get_last_error(ec);
    return -1;
  }
#elif defined(CFS_OS_DOS)
  {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return -1;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    if (realpath(p->str, buf) != NULL) {
      cfs_path_assign(out, buf);
      return 0;
    }
    if (ec)
      cfs_get_last_error(ec);
    return -1;
  }
#endif
}

CFS_API int cfs_weakly_canonical(const cfs_path *p, cfs_path *out,
                                 cfs_error_code *ec) {
  return cfs_canonical(p, out, ec); /* Simplified stub */
}

CFS_API int cfs_copy(const cfs_path *from, const cfs_path *to,
                     cfs_copy_options options, cfs_error_code *ec) {
  /* Basic wrapper mapping to copy_file for now */
  if (cfs_copy_file(from, to, options, ec))
    return 0;
  return -1;
}

CFS_API int cfs_copy_symlink(const cfs_path *existing_symlink,
                             const cfs_path *new_symlink, cfs_error_code *ec) {
  cfs_path out;
  int res;
  res = cfs_read_symlink(existing_symlink, &out, ec);
  if (res == 0) {
    cfs_create_symlink(&out, new_symlink, ec);
    cfs_path_destroy(&out);
    return 0;
  }
  return res;
}

CFS_API int cfs_proximate(const cfs_path *p, const cfs_path *base,
                          cfs_path *out, cfs_error_code *ec) {
  cfs_path tmp;
  cfs_path_lexically_proximate(p, base, &tmp);
  (void)ec;
  cfs_path_clone(out, &tmp);
  cfs_path_destroy(&tmp);
  return 0;
}

CFS_API int cfs_relative(const cfs_path *p, const cfs_path *base, cfs_path *out,
                         cfs_error_code *ec) {
  cfs_path tmp;
  cfs_path_lexically_relative(p, base, &tmp);
  (void)ec;
  cfs_path_clone(out, &tmp);
  cfs_path_destroy(&tmp);
  return 0;
}

CFS_API int cfs_copy_file(const cfs_path *from, const cfs_path *to,
                          cfs_copy_options options, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!from || !to)
    return -1;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (CopyFileW(from->str, to->str,
                !(options & cfs_copy_options_overwrite_existing)))
#else
  if (CopyFileA(from->str, to->str,
                !(options & cfs_copy_options_overwrite_existing)))
#endif
    return 0;
#else
  /* stub */
#endif
  if (ec)
    cfs_get_last_error(ec);
  return -1;
}

CFS_API void cfs_create_symlink(const cfs_path *target, const cfs_path *link,
                                cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
#if defined(CFS_OS_WINDOWS)
  (void)target;
  (void)link;
  /* Needs dynamic loading or Vista+ */
  if (ec)
    cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
#elif defined(CFS_OS_DOS)
  (void)target;
  (void)link;
  if (ec)
    cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
#else
  if (symlink(target->str, link->str) != 0) {
    if (ec)
      cfs_get_last_error(ec);
  }
#endif
}

CFS_API int cfs_path_lexically_relative(const cfs_path *p, const cfs_path *base,
                                        cfs_path *out) {
  if (!out)
    return -1;
  cfs_path_init(out);
  (void)base;
  if (p)
    cfs_path_assign(out, p->str);
  return 0; /* simplified stub */
}

CFS_API int cfs_path_lexically_proximate(const cfs_path *p,
                                         const cfs_path *base, cfs_path *out) {
  return cfs_path_lexically_relative(p, base, out); /* simplified stub */
}

CFS_API int cfs_path_is_absolute(const cfs_path *p, cfs_bool *out) {
  if (!out)
    return -1;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return 0;
#if defined(CFS_OS_WINDOWS)
  if (p->length >= 2 && p->str[1] == CFS_CHAR(':')) {
    if (p->length >= 3 &&
        (p->str[2] == CFS_CHAR('\\') || p->str[2] == CFS_CHAR('/')))
      *out = cfs_true;
  } else if (p->str[0] == CFS_CHAR('\\') || p->str[0] == CFS_CHAR('/')) {
    *out = cfs_true;
  }
#elif defined(CFS_OS_DOS)
  if (p->length >= 2 && p->str[1] == CFS_CHAR(':')) {
    if (p->length >= 3 &&
        (p->str[2] == CFS_CHAR('\\') || p->str[2] == CFS_CHAR('/')))
      *out = cfs_true;
  } else if (p->str[0] == CFS_CHAR('\\') || p->str[0] == CFS_CHAR('/')) {
    *out = cfs_true;
  }
#else
  if (p->str[0] == CFS_CHAR('/'))
    *out = cfs_true;
#endif
  return 0;
}

CFS_API int cfs_current_path(cfs_path *out, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!out) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return -1;
  }
#if defined(CFS_OS_WINDOWS)
  {
    cfs_char_t buf[CFS_MAX_PATH];
    DWORD len;
#if defined(CFS_UNICODE)
    len = GetCurrentDirectoryW(CFS_MAX_PATH, buf);
#else
    len = GetCurrentDirectoryA(CFS_MAX_PATH, buf);
#endif
    if (len > 0 && len < CFS_MAX_PATH) {
      cfs_path_assign(out, buf);
      return 0;
    }
    if (ec)
      cfs_get_last_error(ec);
    return -1;
  }
#elif defined(CFS_OS_DOS)
  {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return -1;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    if (getcwd(buf, sizeof(buf)) != NULL) {
      cfs_path_assign(out, buf);
      return 0;
    }
    if (ec)
      cfs_get_last_error(ec);
    return -1;
  }
#endif
}

CFS_API void cfs_current_path_set(const cfs_path *p, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || !p->str) {
    if (ec)
      cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return;
  }
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (SetCurrentDirectoryW(p->str) == 0) {
#else
  if (SetCurrentDirectoryA(p->str) == 0) {
#endif
    if (ec)
      cfs_get_last_error(ec);
  }
#elif defined(CFS_OS_DOS)
  if (ec)
    cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
#else
  if (chdir(p->str) != 0) {
    if (ec)
      cfs_get_last_error(ec);
  }
#endif
}

/* --- Auto-generated stubs for missing functions --- */
CFS_API void cfs_set_oom_handler(cfs_oom_handler_t handler) { (void)handler; }

CFS_API int cfs_path_root_name(const cfs_path *p, cfs_path *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_root_directory(const cfs_path *p, cfs_path *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_root_path(const cfs_path *p, cfs_path *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_relative_path(const cfs_path *p, cfs_path *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_parent_path(const cfs_path *p, cfs_path *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_replace_filename(cfs_path *p,
                                      const cfs_char_t *replacement) {
  (void)p;
  (void)replacement;
  return -1;
}

CFS_API int cfs_path_replace_extension(cfs_path *p,
                                       const cfs_char_t *replacement) {
  (void)p;
  (void)replacement;
  return -1;
}

CFS_API void cfs_path_remove_filename(cfs_path *p) { (void)p; }

CFS_API int cfs_path_has_root_path(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_has_root_name(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_has_root_directory(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_has_relative_path(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_has_parent_path(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_has_filename(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_has_stem(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_has_extension(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_is_relative(const cfs_path *p, cfs_bool *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_path_compare(const cfs_path *lhs, const cfs_path *rhs) {
  (void)lhs;
  (void)rhs;
  return -1;
}

CFS_API int cfs_path_lexically_normal(const cfs_path *p, cfs_path *out) {
  (void)p;
  (void)out;
  return -1;
}

CFS_API int cfs_status(const cfs_path *p, cfs_file_status *out,
                       cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return -1;
}

CFS_API int cfs_symlink_status(const cfs_path *p, cfs_file_status *out,
                               cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return -1;
}

CFS_API int cfs_exists(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_exists_path(const cfs_path *p, cfs_bool *out,
                            cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return -1;
}

CFS_API int cfs_is_block_file(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_character_file(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_directory(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_fifo(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_other(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_regular_file(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_socket(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_symlink(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return -1;
}

CFS_API int cfs_is_empty_path(const cfs_path *p, cfs_bool *out,
                              cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return -1;
}

CFS_API int cfs_create_directory(const cfs_path *p, cfs_error_code *ec) {
  (void)p;
  (void)ec;
  return -1;
}

CFS_API int cfs_create_directories(const cfs_path *p, cfs_error_code *ec) {
  (void)p;
  (void)ec;
  return -1;
}

CFS_API void cfs_create_hard_link(const cfs_path *target, const cfs_path *link,
                                  cfs_error_code *ec) {
  (void)target;
  (void)link;
  (void)ec;
}

CFS_API void cfs_create_directory_symlink(const cfs_path *target,
                                          const cfs_path *link,
                                          cfs_error_code *ec) {
  (void)target;
  (void)link;
  (void)ec;
}

CFS_API int cfs_remove_all(const cfs_path *p, cfs_size_t *out,
                           cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return -1;
}

CFS_API void cfs_rename(const cfs_path *old_p, const cfs_path *new_p,
                        cfs_error_code *ec) {
  (void)old_p;
  (void)new_p;
  (void)ec;
}

CFS_API void cfs_resize_file(const cfs_path *p, cfs_uintmax_t size,
                             cfs_error_code *ec) {
  (void)p;
  (void)size;
  (void)ec;
}

CFS_API int cfs_space(const cfs_path *p, cfs_space_info *out,
                      cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return -1;
}

CFS_API int cfs_last_write_time(const cfs_path *p, cfs_file_time_type *out,
                                cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return -1;
}

CFS_API int cfs_temp_directory_path(cfs_path *out, cfs_error_code *ec) {
  (void)out;
  (void)ec;
  return -1;
}

CFS_API int cfs_dir_itr_init(const cfs_path *p, cfs_directory_iterator **out_it,
                             cfs_error_code *ec) {
  (void)p;
  (void)out_it;
  (void)ec;
  return -1;
}

CFS_API int cfs_dir_itr_next(cfs_directory_iterator *it,
                             const cfs_directory_entry **out_entry,
                             cfs_error_code *ec) {
  (void)it;
  (void)out_entry;
  (void)ec;
  return -1;
}

CFS_API void cfs_dir_itr_close(cfs_directory_iterator *it) { (void)it; }

CFS_API int cfs_rec_dir_itr_init(const cfs_path *p,
                                 cfs_recursive_directory_iterator **out_it,
                                 cfs_error_code *ec) {
  (void)p;
  (void)out_it;
  (void)ec;
  return -1;
}

CFS_API int cfs_rec_dir_itr_next(cfs_recursive_directory_iterator *it,
                                 const cfs_directory_entry **out_entry,
                                 cfs_error_code *ec) {
  (void)it;
  (void)out_entry;
  (void)ec;
  return -1;
}

CFS_API void cfs_rec_dir_itr_disable_recursion_pending(
    cfs_recursive_directory_iterator *it) {
  (void)it;
}

CFS_API void cfs_rec_dir_itr_pop(cfs_recursive_directory_iterator *it,
                                 cfs_error_code *ec) {
  (void)it;
  (void)ec;
}

CFS_API void cfs_rec_dir_itr_close(cfs_recursive_directory_iterator *it) {
  (void)it;
}
