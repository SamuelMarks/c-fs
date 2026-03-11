#ifndef CFS_H
#define CFS_H

/* clang-format off */
#include <stddef.h>
#ifdef CFS_IMPLEMENTATION
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/stat.h>
    #include <wchar.h>
    #if defined(_WIN32) || defined(_WIN64)
        #define WIN32_LEAN_AND_MEAN
        #include <winsock2.h>
    #endif
    #if !defined(_WIN32) && !defined(_WIN64) && !defined(CFS_ENV_CYGWIN) /* Assuming POSIX otherwise */
        #include <pthread.h>
    #endif
#endif
/* clang-format on */

/* Phase 3: Platform & Compiler Detection Macros */

/* 22-25. OS Detection */
#if defined(_WIN32) || defined(_WIN64)
#define CFS_OS_WINDOWS
#elif defined(__linux__)
#define CFS_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define CFS_OS_MACOS
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__bsdi__) || defined(__DragonFly__)
#define CFS_OS_BSD
#else
/* Unknown OS fallback (assume POSIX by default if needed) */
#endif

/* 29-30. Environment Detection */
#if defined(__MINGW32__) || defined(__MINGW64__)
#define CFS_ENV_MINGW
#elif defined(__CYGWIN__)
#define CFS_ENV_CYGWIN
#endif

/* 26-28. Compiler Detection */
#if defined(_MSC_VER)
#define CFS_COMPILER_MSVC _MSC_VER
#elif defined(__clang__)
#define CFS_COMPILER_CLANG __clang_major__
#elif defined(__GNUC__) || defined(__GNUG__)
#define CFS_COMPILER_GCC __GNUC__
#else
/* Unknown Compiler */
#endif
/* Phase 4: Distribution Modes Scaffolding */

/* 31. Implement CFS_API macro */
#if defined(CFS_OS_WINDOWS) || defined(__CYGWIN__)
#if defined(CFS_BUILD_SHARED)
#if defined(CFS_EXPORTS)
#define CFS_API __declspec(dllexport)
#else
#define CFS_API __declspec(dllimport)
#endif
#else
#define CFS_API
#endif
#else
#if defined(CFS_BUILD_SHARED) && defined(__GNUC__) && __GNUC__ >= 4
#define CFS_API __attribute__((visibility("default")))
#else
#define CFS_API
#endif
#endif

/* 32. Implement CFS_INLINE macro */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define CFS_INLINE inline
#elif defined(__GNUC__)
#define CFS_INLINE __inline__
#elif defined(_MSC_VER)
#define CFS_INLINE __inline
#else
#define CFS_INLINE static
#endif

/* 34. Define CFS_EXTERN_C_BEGIN and CFS_EXTERN_C_END */
#ifdef __cplusplus
#define CFS_EXTERN_C_BEGIN extern "C" {
#define CFS_EXTERN_C_END }
#else
#define CFS_EXTERN_C_BEGIN
#define CFS_EXTERN_C_END
#endif

/* 37. Thread-local storage macros */
#if defined(CFS_MULTITHREADED)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L &&                \
    !defined(__STDC_NO_THREADS__)
#define CFS_THREAD_LOCAL _Thread_local
#elif defined(CFS_COMPILER_MSVC)
#define CFS_THREAD_LOCAL __declspec(thread)
#elif defined(CFS_COMPILER_GCC) || defined(CFS_COMPILER_CLANG)
#define CFS_THREAD_LOCAL __thread
#else
#define CFS_THREAD_LOCAL
#endif
#else
#define CFS_THREAD_LOCAL
#endif

/* 39. LTO Attribute Macros */
#if defined(CFS_ENABLE_LTO) &&                                                 \
    (defined(CFS_COMPILER_GCC) || defined(CFS_COMPILER_CLANG))
#define CFS_EXTERNALLY_VISIBLE __attribute__((externally_visible))
#else
#define CFS_EXTERNALLY_VISIBLE
#endif

CFS_EXTERN_C_BEGIN
/* Phase 5: Memory & Core Types */

/* 41. cfs_char_t mapped dynamically to char or wchar_t */
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
typedef wchar_t cfs_char_t;
#define CFS_CHAR(c) L##c
#define CFS_STR(s) L##s
#else
typedef char cfs_char_t;
#define CFS_CHAR(c) c
#define CFS_STR(s) s
#endif

/* 42. cfs_bool type strictly mapping to C89 integers */
typedef int cfs_bool;
#define cfs_true 1
#define cfs_false 0

/* 43. cfs_size_t */
typedef size_t cfs_size_t;

/* 49. Define system-specific maximum path length macros */
#if defined(CFS_OS_WINDOWS)
#ifndef CFS_MAX_PATH
#define CFS_MAX_PATH 260
#endif
#else
#ifndef CFS_MAX_PATH
#define CFS_MAX_PATH 4096
#endif
#endif

/* Global Out-Of-Memory Error Fallback Hook */
typedef void (*cfs_oom_handler_t)(void);

/* 44-48. Memory Allocation Wrappers */
CFS_API void cfs_set_oom_handler(cfs_oom_handler_t handler);
CFS_API void *cfs_malloc(cfs_size_t size);
CFS_API void cfs_free(void *ptr);
CFS_API void *cfs_realloc(void *ptr, cfs_size_t new_size);
CFS_API void *cfs_calloc(cfs_size_t num, cfs_size_t size);

/* Phase 6: String Handling & Charsets */

/* 51-54. Native string handling abstractions */
CFS_API cfs_size_t cfs_strlen(const cfs_char_t *str);
CFS_API cfs_char_t *cfs_strcpy(cfs_char_t *dest, const cfs_char_t *src);
CFS_API cfs_char_t *cfs_strncpy(cfs_char_t *dest, const cfs_char_t *src,
                                cfs_size_t n);
CFS_API cfs_char_t *cfs_strcat(cfs_char_t *dest, const cfs_char_t *src);
CFS_API int cfs_strcmp(const cfs_char_t *lhs, const cfs_char_t *rhs);
CFS_API int cfs_strncmp(const cfs_char_t *lhs, const cfs_char_t *rhs,
                        cfs_size_t count);

/* 55-59. Charset & Conversion logic */
#if defined(_MSC_VER)
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
#define CFS_STRCPY_SAFE(dst, dst_sz, src) wcscpy_s((dst), (dst_sz), (src))
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  wcsncpy_s((dst), (dst_sz), (src), (n))
#else
#define CFS_STRCPY_SAFE(dst, dst_sz, src) strcpy_s((dst), (dst_sz), (src))
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  strncpy_s((dst), (dst_sz), (src), (n))
#endif
#else
#define CFS_STRCPY_SAFE(dst, dst_sz, src) cfs_strcpy((dst), (src))
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n) cfs_strncpy((dst), (src), (n))
#endif
#if defined(CFS_OS_WINDOWS)
/* UTF-8 to UTF-16 conversion (Returns required buffer size in chars if dest is
 * NULL) */
CFS_API cfs_size_t cfs_utf8_to_utf16(const char *utf8_str, wchar_t *dest,
                                     cfs_size_t dest_len);
/* UTF-16 to UTF-8 conversion (Returns required buffer size in bytes if dest is
 * NULL) */
CFS_API cfs_size_t cfs_utf16_to_utf8(const wchar_t *utf16_str, char *dest,
                                     cfs_size_t dest_len);
#endif

/* ANSI to Wide character conversion (Generic Fallbacks) */
CFS_API cfs_size_t cfs_mb_to_wide(const char *mb_str, wchar_t *dest,
                                  cfs_size_t dest_len);
CFS_API cfs_size_t cfs_wide_to_mb(const wchar_t *wide_str, char *dest,
                                  cfs_size_t dest_len);
/* Phase 7: Error Handling & System Codes */

/* 62. Define cfs_errc mapping to std::errc (POSIX states) */
typedef enum cfs_errc {
  cfs_errc_success = 0,
  cfs_errc_address_family_not_supported,
  cfs_errc_address_in_use,
  cfs_errc_address_not_available,
  cfs_errc_already_connected,
  cfs_errc_argument_list_too_long,
  cfs_errc_argument_out_of_domain,
  cfs_errc_bad_address,
  cfs_errc_bad_file_descriptor,
  cfs_errc_bad_message,
  cfs_errc_broken_pipe,
  cfs_errc_connection_aborted,
  cfs_errc_connection_already_in_progress,
  cfs_errc_connection_refused,
  cfs_errc_connection_reset,
  cfs_errc_cross_device_link,
  cfs_errc_destination_address_required,
  cfs_errc_device_or_resource_busy,
  cfs_errc_directory_not_empty,
  cfs_errc_executable_format_error,
  cfs_errc_file_exists,
  cfs_errc_file_too_large,
  cfs_errc_filename_too_long,
  cfs_errc_function_not_supported,
  cfs_errc_host_unreachable,
  cfs_errc_identifier_removed,
  cfs_errc_illegal_byte_sequence,
  cfs_errc_inappropriate_io_control_operation,
  cfs_errc_interrupted,
  cfs_errc_invalid_argument,
  cfs_errc_invalid_seek,
  cfs_errc_io_error,
  cfs_errc_is_a_directory,
  cfs_errc_message_size,
  cfs_errc_network_down,
  cfs_errc_network_reset,
  cfs_errc_network_unreachable,
  cfs_errc_no_buffer_space,
  cfs_errc_no_child_process,
  cfs_errc_no_link,
  cfs_errc_no_lock_available,
  cfs_errc_no_message_available,
  cfs_errc_no_message,
  cfs_errc_no_protocol_option,
  cfs_errc_no_space_on_device,
  cfs_errc_no_stream_resources,
  cfs_errc_no_such_device_or_address,
  cfs_errc_no_such_device,
  cfs_errc_no_such_file_or_directory,
  cfs_errc_no_such_process,
  cfs_errc_not_a_directory,
  cfs_errc_not_a_socket,
  cfs_errc_not_a_stream,
  cfs_errc_not_connected,
  cfs_errc_not_enough_memory,
  cfs_errc_not_supported,
  cfs_errc_operation_canceled,
  cfs_errc_operation_in_progress,
  cfs_errc_operation_not_permitted,
  cfs_errc_operation_not_supported,
  cfs_errc_operation_would_block,
  cfs_errc_owner_dead,
  cfs_errc_permission_denied,
  cfs_errc_protocol_error,
  cfs_errc_protocol_not_supported,
  cfs_errc_read_only_file_system,
  cfs_errc_resource_deadlock_would_occur,
  cfs_errc_resource_unavailable_try_again,
  cfs_errc_result_out_of_range,
  cfs_errc_state_not_recoverable,
  cfs_errc_stream_timeout,
  cfs_errc_text_file_busy,
  cfs_errc_timed_out,
  cfs_errc_too_many_files_open_in_system,
  cfs_errc_too_many_files_open,
  cfs_errc_too_many_links,
  cfs_errc_too_many_symbolic_link_levels,
  cfs_errc_value_too_large,
  cfs_errc_wrong_protocol_type,
  cfs_errc_unknown_error /* fallback */
} cfs_errc;

/* 61. Define cfs_error_code */
typedef struct cfs_error_code {
  int value;     /* The raw OS specific error code (errno or GetLastError()) */
  cfs_errc errc; /* The unified POSIX mapping */
} cfs_error_code;

/* 63-64, 68. Global / Thread-Local Error Interfacing */
CFS_API void cfs_set_error(cfs_error_code *ec, int os_value,
                           cfs_errc standard_value);
CFS_API void cfs_clear_error(cfs_error_code *ec);
CFS_API const char *cfs_error_message(cfs_errc err);

/* 65-67. OS Translation Hooks */
CFS_API cfs_error_code cfs_make_error_code_from_os(int os_error);
CFS_API cfs_error_code cfs_get_last_error(void);
/* Phase 8: Path Struct Basics */

/* 76. Define platform-specific path separator macros */
#if defined(CFS_OS_WINDOWS)
#define CFS_PREFERRED_SEPARATOR CFS_CHAR('\\')
#else
#define CFS_PREFERRED_SEPARATOR CFS_CHAR('/')
#endif

/* 71. Define opaque cfs_path struct */
typedef struct cfs_path {
  cfs_char_t *str;
  cfs_size_t length;
  cfs_size_t capacity;
} cfs_path;

/* 72-75, 77-79. Path Initialization, Mutation, and Destruction */
CFS_API void cfs_path_init(cfs_path *p);
CFS_API cfs_error_code cfs_path_init_str(cfs_path *p, const cfs_char_t *source);
CFS_API void cfs_path_destroy(cfs_path *p);
CFS_API cfs_error_code cfs_path_clone(cfs_path *dest, const cfs_path *src);
CFS_API cfs_path *cfs_path_make_preferred(cfs_path *p);
CFS_API const cfs_char_t *cfs_path_c_str(const cfs_path *p);
/* Returns dynamically allocated generic path string (e.g. forward slashes on
 * Windows). Caller must free. */
CFS_API cfs_char_t *cfs_path_generic_string(const cfs_path *p);
/* Phase 9: Path Building */

/* 81-84, 88. Path assignment, concatenation, and manipulation */
CFS_API cfs_error_code cfs_path_assign(cfs_path *p, const cfs_char_t *source);
CFS_API cfs_error_code cfs_path_append(cfs_path *p, const cfs_char_t *source);
CFS_API cfs_error_code cfs_path_concat(cfs_path *p, const cfs_char_t *source);
CFS_API void cfs_path_clear(cfs_path *p);
CFS_API void cfs_path_swap(cfs_path *lhs, cfs_path *rhs);
/* Phase 10: Path Decomposition - Root Analysis */

/* Extracts drive letters (Windows) or root nodes. Returns path instance. */
CFS_API cfs_path cfs_path_root_name(const cfs_path *p);
/* Extracts base root separator. Returns path instance. */
CFS_API cfs_path cfs_path_root_directory(const cfs_path *p);
/* Combines root name and root directory. Returns path instance. */
CFS_API cfs_path cfs_path_root_path(const cfs_path *p);
/* Phase 11: Path Decomposition - Elements */

/* 101. Returns path relative to the root path */
CFS_API cfs_path cfs_path_relative_path(const cfs_path *p);
/* 102. Returns path of the parent directory */
CFS_API cfs_path cfs_path_parent_path(const cfs_path *p);
/* 103. Returns the filename component */
CFS_API cfs_path cfs_path_filename(const cfs_path *p);
/* 104. Returns the stem (filename without extension) */
CFS_API cfs_path cfs_path_stem(const cfs_path *p);
/* 105. Returns the file extension */
CFS_API cfs_path cfs_path_extension(const cfs_path *p);
/* Phase 12: Path Modifiers */

/* 111. Replaces the terminal filename component */
CFS_API cfs_error_code cfs_path_replace_filename(cfs_path *p,
                                                 const cfs_char_t *replacement);
/* 112. Replaces the extension of the terminal component */
CFS_API cfs_error_code
cfs_path_replace_extension(cfs_path *p, const cfs_char_t *replacement);
/* 113. Removes the terminal filename component (truncates back to parent) */
CFS_API void cfs_path_remove_filename(cfs_path *p);
/* Phase 13: Path Observers & Comparisons */

/* Observers */
CFS_API cfs_bool cfs_path_is_empty(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_root_path(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_root_name(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_root_directory(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_relative_path(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_parent_path(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_filename(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_stem(const cfs_path *p);
CFS_API cfs_bool cfs_path_has_extension(const cfs_path *p);
CFS_API cfs_bool cfs_path_is_absolute(const cfs_path *p);
CFS_API cfs_bool cfs_path_is_relative(const cfs_path *p);

/* Lexicographical comparison */
CFS_API int cfs_path_compare(const cfs_path *lhs, const cfs_path *rhs);
/* Phase 14: Lexical Path Operations */

/* 132. Lexically normalizes the path (resolves . and .. internally) */
CFS_API cfs_path cfs_path_lexically_normal(const cfs_path *p);
/* 134. Returns a path representing how to get from base to p */
CFS_API cfs_path cfs_path_lexically_relative(const cfs_path *p,
                                             const cfs_path *base);
/* 135. Returns relative path if mathematically divergent, otherwise the
 * original path */
CFS_API cfs_path cfs_path_lexically_proximate(const cfs_path *p,
                                              const cfs_path *base);

/* Internal path element iterator structure */
typedef struct cfs_path_element {
  const cfs_char_t *str;
  cfs_size_t length;
} cfs_path_element;

typedef struct cfs_path_iterator {
  const cfs_char_t *path_str;
  cfs_size_t path_len;
  cfs_size_t current_pos;
} cfs_path_iterator;
/* Phase 15 & 16: Filesystem Information & Status */

/* 144. Define file types matching std::filesystem::file_type */
typedef enum cfs_file_type {
  cfs_file_type_none = 0,
  cfs_file_type_not_found = -1,
  cfs_file_type_regular = 1,
  cfs_file_type_directory = 2,
  cfs_file_type_symlink = 3,
  cfs_file_type_block = 4,
  cfs_file_type_character = 5,
  cfs_file_type_fifo = 6,
  cfs_file_type_socket = 7,
  cfs_file_type_unknown = 8
} cfs_file_type;

/* OS-agnostic permissions (matches std::filesystem::perms) */
typedef unsigned int cfs_perms;

/* Status struct holding retrieved OS attributes */
typedef struct cfs_file_status {
  cfs_file_type type;
  cfs_perms permissions;
} cfs_file_status;

/* 141-142. Core Status Queries */
CFS_API cfs_file_status cfs_status(const cfs_path *p, cfs_error_code *ec);
CFS_API cfs_file_status cfs_symlink_status(const cfs_path *p,
                                           cfs_error_code *ec);

/* 145. Exists Observer */
CFS_API cfs_bool cfs_exists(cfs_file_status s);
CFS_API cfs_bool cfs_exists_path(const cfs_path *p, cfs_error_code *ec);

/* 151-159. Filesystem Type Queries */
CFS_API cfs_bool cfs_is_block_file(cfs_file_status s);
CFS_API cfs_bool cfs_is_character_file(cfs_file_status s);
CFS_API cfs_bool cfs_is_directory(cfs_file_status s);
CFS_API cfs_bool cfs_is_fifo(cfs_file_status s);
CFS_API cfs_bool cfs_is_other(cfs_file_status s);
CFS_API cfs_bool cfs_is_regular_file(cfs_file_status s);
CFS_API cfs_bool cfs_is_socket(cfs_file_status s);
CFS_API cfs_bool cfs_is_symlink(cfs_file_status s);

/* 154. Is Empty Query (Directory or zero-byte file) */
CFS_API cfs_bool cfs_is_empty_path(const cfs_path *p, cfs_error_code *ec);
/* Phase 17: Filesystem Operations - Creation */

/* 161. Create a single directory node */
CFS_API cfs_bool cfs_create_directory(const cfs_path *p, cfs_error_code *ec);
/* 162. Recursively create directory nodes */
CFS_API cfs_bool cfs_create_directories(const cfs_path *p, cfs_error_code *ec);

/* 163-165. Create links */
CFS_API void cfs_create_hard_link(const cfs_path *target, const cfs_path *link,
                                  cfs_error_code *ec);
CFS_API void cfs_create_symlink(const cfs_path *target, const cfs_path *link,
                                cfs_error_code *ec);
CFS_API void cfs_create_directory_symlink(const cfs_path *target,
                                          const cfs_path *link,
                                          cfs_error_code *ec);

/* Copy file options mirroring std::filesystem::copy_options */
typedef enum cfs_copy_options {
  cfs_copy_options_none = 0,
  cfs_copy_options_skip_existing = 1,
  cfs_copy_options_overwrite_existing = 2,
  cfs_copy_options_update_existing = 4
} cfs_copy_options;

/* 168. Copy files strictly */
CFS_API cfs_bool cfs_copy_file(const cfs_path *from, const cfs_path *to,
                               cfs_copy_options options, cfs_error_code *ec);
/* Phase 18: Filesystem Operations - Modification */

/* 171. Remove single file or empty directory */
CFS_API cfs_bool cfs_remove(const cfs_path *p, cfs_error_code *ec);
/* 172. Remove all contents recursively. Returns number of removed objects */
CFS_API cfs_size_t cfs_remove_all(const cfs_path *p, cfs_error_code *ec);

/* 173. Rename/Move node */
CFS_API void cfs_rename(const cfs_path *old_p, const cfs_path *new_p,
                        cfs_error_code *ec);

/* 174-175. Size mutations and queries */
/* Defined as large integer matching std::uintmax_t */
#if defined(__GNUC__) || defined(__clang__)
__extension__ typedef unsigned long long cfs_uintmax_t;
#elif defined(_MSC_VER)
typedef unsigned __int64 cfs_uintmax_t;
#else
typedef unsigned long cfs_uintmax_t;
#endif
CFS_API void cfs_resize_file(const cfs_path *p, cfs_uintmax_t size,
                             cfs_error_code *ec);
CFS_API cfs_uintmax_t cfs_file_size(const cfs_path *p, cfs_error_code *ec);

/* 176. Space Information */
typedef struct cfs_space_info {
  cfs_uintmax_t capacity;
  cfs_uintmax_t free;
  cfs_uintmax_t available;
} cfs_space_info;
CFS_API cfs_space_info cfs_space(const cfs_path *p, cfs_error_code *ec);

/* 177. File Write Time */
/* Mapped natively to time_t or system tick representation */
#if defined(__GNUC__) || defined(__clang__)
__extension__ typedef long long cfs_file_time_type;
#elif defined(_MSC_VER)
typedef __int64 cfs_file_time_type;
#else
typedef long cfs_file_time_type;
#endif
CFS_API cfs_file_time_type cfs_last_write_time(const cfs_path *p,
                                               cfs_error_code *ec);

/* 178-179. Environment paths */
CFS_API cfs_path cfs_current_path(cfs_error_code *ec);
CFS_API void cfs_current_path_set(const cfs_path *p, cfs_error_code *ec);
CFS_API cfs_path cfs_temp_directory_path(cfs_error_code *ec);
/* Phase 19: Directory Iteration */

/* 181. Directory entry structure caching path and status */
typedef struct cfs_directory_entry {
  cfs_path path;
  cfs_file_status status;
  cfs_file_status symlink_status;
} cfs_directory_entry;

/* 182-185. Standard Directory Iterator */
typedef struct cfs_directory_iterator cfs_directory_iterator;
CFS_API cfs_directory_iterator *cfs_dir_itr_init(const cfs_path *p,
                                                 cfs_error_code *ec);
/* Returns a pointer to the internal entry, or NULL if iteration complete or
 * error */
CFS_API const cfs_directory_entry *cfs_dir_itr_next(cfs_directory_iterator *it,
                                                    cfs_error_code *ec);
CFS_API void cfs_dir_itr_close(cfs_directory_iterator *it);

/* 186-189. Recursive Directory Iterator */
typedef struct cfs_recursive_directory_iterator
    cfs_recursive_directory_iterator;
CFS_API cfs_recursive_directory_iterator *
cfs_rec_dir_itr_init(const cfs_path *p, cfs_error_code *ec);
CFS_API const cfs_directory_entry *
cfs_rec_dir_itr_next(cfs_recursive_directory_iterator *it, cfs_error_code *ec);
/* Prevents the iterator from descending into the currently evaluated directory
 * node */
CFS_API void
cfs_rec_dir_itr_disable_recursion_pending(cfs_recursive_directory_iterator *it);
/* Moves the iterator one level up in the directory tree */
CFS_API void cfs_rec_dir_itr_pop(cfs_recursive_directory_iterator *it,
                                 cfs_error_code *ec);
CFS_API void cfs_rec_dir_itr_close(cfs_recursive_directory_iterator *it);

/* Phase 5.5: Execution Context & Modality */

/* 1. Define the cfs_modality enum */
typedef enum cfs_modality {
  cfs_modality_sync,
  cfs_modality_async,
  cfs_modality_multithread,
  cfs_modality_singlethread,
  cfs_modality_multiprocess,
  cfs_modality_greenthread,
  cfs_modality_message_passing
} cfs_modality;

/* 2. Define cfs_runtime_config struct */
typedef struct cfs_runtime_config {
  cfs_modality mode;
  cfs_size_t thread_pool_size;
  const cfs_char_t *ipc_path;
} cfs_runtime_config;

/* 3. Create the opaque cfs_runtime_t struct */
typedef struct cfs_runtime_t cfs_runtime_t;

/* 6. Introduce cfs_request_t struct */
typedef struct cfs_request_t cfs_request_t;

/* 7. Define unified cfs_callback_t function pointer type */
typedef void (*cfs_callback_t)(cfs_request_t *req, void *user_data);

/* The cfs_request_t struct represents an abstract file system operation */
struct cfs_request_t {
  int opcode;
  cfs_path target_path;
  cfs_path dest_path;
  void *result_buffer;
  cfs_size_t result_size;
  cfs_error_code error;
  cfs_callback_t callback;
  void *user_data;
  struct cfs_request_t *next;
  int ref_count;
  cfs_bool cancelled;
};

/* 4. Implement cfs_runtime_init() */
CFS_API cfs_runtime_t *cfs_runtime_init(const cfs_runtime_config *config,
                                        cfs_error_code *ec);

/* 5. Implement cfs_runtime_destroy() */
CFS_API void cfs_runtime_destroy(cfs_runtime_t *runtime);

/* 9. Create generic request dispatcher internal function */
CFS_API void cfs_dispatch_request(cfs_runtime_t *runtime, cfs_request_t *req,
                                  cfs_callback_t cb, void *user_data);

/* 33. Implement CFS_IMPLEMENTATION macro pattern */
/* 40. Finalize the unified cross-platform header structure layout. */

/* Phase 5.6: Deferred Execution and Asynchronous Base */

/* 11. Opcodes */
typedef enum cfs_opcode {
  cfs_opcode_none = 0,
  cfs_opcode_remove,
  cfs_opcode_remove_all,
  cfs_opcode_create_directory,
  cfs_opcode_create_directories,
  cfs_opcode_copy_file,
  cfs_opcode_rename,
  cfs_opcode_file_size,
  cfs_opcode_status,
  cfs_opcode_symlink_status,
  cfs_opcode_exists,
  cfs_opcode_is_empty,
  cfs_opcode_space,
  cfs_opcode_last_write_time
} cfs_opcode;

/* Forward declarations for internal sync structures */
typedef struct cfs_mutex_t cfs_mutex_t;
typedef struct cfs_cond_t cfs_cond_t;
typedef struct cfs_thread_t cfs_thread_t;

/* 15, 18. Thread-Safe FIFO Queue */
typedef struct cfs_queue_t cfs_queue_t;

/* 16. Thread Pool */
typedef struct cfs_thread_pool_t cfs_thread_pool_t;

/* 13. Non-blocking API variants */
CFS_API int cfs_remove_async(cfs_runtime_t *rt, const cfs_path *p,
                             cfs_callback_t cb, void *user_data);
CFS_API int cfs_file_size_async(cfs_runtime_t *rt, const cfs_path *p,
                                cfs_callback_t cb, void *user_data);

/* 19. cfs_runtime_poll() */
CFS_API int cfs_runtime_poll(cfs_runtime_t *rt);

/* Phase 3: Platform-Specific Async & Message Passing */

/* 29. Reference counting to cfs_request_t */
CFS_API void cfs_request_retain(cfs_request_t *req);
CFS_API void cfs_request_release(cfs_request_t *req);

/* 30. Implement cancellation logic */
CFS_API int cfs_cancel_request(cfs_runtime_t *rt, cfs_request_t *req);

/* Platform Specific Async Backend Configurations */
typedef struct cfs_io_uring_context cfs_io_uring_context;
typedef struct cfs_iocp_context cfs_iocp_context;

/* 24-26. Message Passing Actors */
typedef struct cfs_message_pipe cfs_message_pipe;
CFS_API int cfs_message_pipe_create(const cfs_char_t *path,
                                    cfs_message_pipe **out_pipe);
CFS_API void cfs_message_pipe_destroy(cfs_message_pipe *pipe);
CFS_API int cfs_serialize_request(const cfs_request_t *req, void **buffer,
                                  cfs_size_t *size);
CFS_API int cfs_deserialize_request(const void *buffer, cfs_size_t size,
                                    cfs_request_t **req);

/* Phase 4: Multiprocessing and Greenthreads */

/* 31. Multiprocess modality backend (Process Handles) */
typedef struct cfs_process_t cfs_process_t;
CFS_API int cfs_process_spawn(const cfs_char_t *executable,
                              cfs_process_t **out_proc);
CFS_API int cfs_process_wait(cfs_process_t *proc);
CFS_API void cfs_process_destroy(cfs_process_t *proc);

/* 32. Shared Memory (shm) segments */
typedef struct cfs_shm_segment cfs_shm_segment;
CFS_API int cfs_shm_create(cfs_size_t size, const cfs_char_t *name,
                           cfs_shm_segment **out_shm);
CFS_API void *cfs_shm_map(cfs_shm_segment *shm);
CFS_API void cfs_shm_unmap(cfs_shm_segment *shm, void *addr);
CFS_API void cfs_shm_destroy(cfs_shm_segment *shm);

/* 33. Multiprocess semaphore */
typedef struct cfs_named_semaphore cfs_named_semaphore;
CFS_API int cfs_named_semaphore_create(const cfs_char_t *name,
                                       int initial_count,
                                       cfs_named_semaphore **out_sem);
CFS_API int cfs_named_semaphore_wait(cfs_named_semaphore *sem);
CFS_API int cfs_named_semaphore_post(cfs_named_semaphore *sem);
CFS_API void cfs_named_semaphore_destroy(cfs_named_semaphore *sem);

/* 35-37. Greenthread modality (ucontext / setjmp) */
typedef struct cfs_greenthread_t cfs_greenthread_t;
typedef void (*cfs_greenthread_func)(void *);

CFS_API int cfs_greenthread_spawn(cfs_greenthread_func func, void *arg,
                                  cfs_greenthread_t **out_gt);
CFS_API int cfs_greenthread_yield(void);
CFS_API void cfs_greenthread_destroy(cfs_greenthread_t *gt);

/* 36. Greenthread scheduler */
typedef struct cfs_greenthread_scheduler cfs_greenthread_scheduler;
CFS_API int
cfs_greenthread_scheduler_init(cfs_greenthread_scheduler **out_sched);
CFS_API int cfs_greenthread_scheduler_run(cfs_greenthread_scheduler *sched);
CFS_API void
cfs_greenthread_scheduler_destroy(cfs_greenthread_scheduler *sched);

/* Phase 5.7: Integration, APIs, and Validation */

/* 41. Overarching API macro/function generator (e.g. exposing both sync and
 * async via a single wrapper if desired) */
/* In C, we typically just expose both, but we can define macros to map them
 * easily. */
#define CFS_REMOVE(rt, path, cb, ud)                                           \
  ((rt) && ((rt)->config.mode != cfs_modality_sync)                            \
       ? cfs_remove_async((rt), (path), (cb), (ud))                            \
       : (cfs_remove((path), NULL) ? 0 : -1))

/* 42. Integrate new context parameter into directory iterators */
typedef struct cfs_directory_iterator_async cfs_directory_iterator_async;
CFS_API int cfs_dir_itr_init_async(cfs_runtime_t *rt, const cfs_path *p,
                                   cfs_callback_t cb, void *user_data);

/* 43. Multi-process sandbox config */
typedef struct cfs_sandbox_config {
  cfs_path root_chroot;
  cfs_bool restrict_symlinks;
} cfs_sandbox_config;

CFS_API int cfs_runtime_set_sandbox(cfs_runtime_t *rt,
                                    const cfs_sandbox_config *config);

CFS_EXTERN_C_END

#endif /* CFS_H */

#ifdef CFS_IMPLEMENTATION

/* 38. Architectural #ifdef blocks delegating to MSVC native vs POSIX */
#if defined(CFS_OS_WINDOWS)
/* Windows native includes */
#elif defined(CFS_OS_LINUX) || defined(CFS_OS_MACOS) || defined(CFS_OS_BSD) || \
    defined(CFS_ENV_CYGWIN)
/* POSIX native includes */
#else
/* Fallback includes */
#endif

/* Implementation details */

static cfs_bool cfs_is_separator(cfs_char_t c);
static cfs_error_code cfs_path_reserve(cfs_path *p, cfs_size_t new_cap);

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
    cfs_uintmax_t size = cfs_file_size(&req->target_path, &req->error);
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

static cfs_request_t *cfs_queue_pop(cfs_queue_t *q, cfs_bool wait_for_data) {
  cfs_request_t *req = NULL;
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
  return req;
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
#else
static void *cfs_worker_thread(void *arg) {
#endif
  cfs_thread_pool_t *pool = (cfs_thread_pool_t *)arg;

  while (1) {
    cfs_request_t *req = cfs_queue_pop(pool->work_queue, cfs_true);
    if (!req) {
      /* Cascade shutdown signal to wake up other waiting threads when using
       * event fallback */
      cfs_cond_broadcast(&pool->work_queue->cond);
      break; /* Shutdown condition */
    }

    if (!req->cancelled) {
      cfs_execute_op_inline(req);
    } else {
      req->error = cfs_make_error_code_from_os(125); /* ECANCELED fallback */
    }

    cfs_queue_push(pool->completion_queue, req);
  }

#if defined(CFS_OS_WINDOWS)
  return 0;
#else
  return NULL;
#endif
}

static cfs_thread_pool_t *cfs_thread_pool_create(cfs_size_t num_threads,
                                                 cfs_queue_t *work,
                                                 cfs_queue_t *comp) {
  cfs_thread_pool_t *pool =
      (cfs_thread_pool_t *)cfs_malloc(sizeof(cfs_thread_pool_t));
  cfs_size_t i;
  if (!pool)
    return NULL;

  pool->num_threads = num_threads;
  pool->work_queue = work;
  pool->completion_queue = comp;
  pool->threads = (cfs_thread_t *)cfs_calloc(num_threads, sizeof(cfs_thread_t));

  if (!pool->threads) {
    cfs_free(pool);
    return NULL;
  }

  for (i = 0; i < num_threads; ++i) {
#if defined(CFS_OS_WINDOWS)
    pool->threads[i].h =
        CreateThread(NULL, 0, cfs_worker_thread, pool, 0, NULL);
#else
    pthread_create(&pool->threads[i].t, NULL, cfs_worker_thread, pool);
#endif
  }

  return pool;
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

  req = (cfs_request_t *)cfs_malloc(sizeof(cfs_request_t));
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

  req = (cfs_request_t *)cfs_malloc(sizeof(cfs_request_t));
  if (!req)
    return -1;

  req->opcode = cfs_opcode_file_size;
  cfs_path_init(&req->target_path);
  cfs_path_clone(&req->target_path, p);
  cfs_path_init(&req->dest_path);

  req->result_size = sizeof(cfs_uintmax_t);
  req->result_buffer = cfs_malloc(req->result_size);
  cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;

  cfs_dispatch_request(rt, req, cb, user_data);
  return 0;
}

CFS_API cfs_runtime_t *cfs_runtime_init(const cfs_runtime_config *config,
                                        cfs_error_code *ec) {
  cfs_runtime_t *rt;
  if (ec)
    cfs_clear_error(ec);

  if (!config) {
    if (ec)
      *ec = cfs_make_error_code_from_os(22); /* EINVAL fallback */
    return NULL;
  }

  rt = (cfs_runtime_t *)cfs_malloc(sizeof(cfs_runtime_t));
  if (!rt) {
    if (ec)
      *ec = cfs_make_error_code_from_os(12); /* ENOMEM fallback */
    return NULL;
  }

  rt->config = *config;
  rt->work_queue = NULL;
  rt->completion_queue = NULL;
  rt->thread_pool = NULL;

  /* TODO: initialize thread pools, IPC, etc based on config->mode */
  if (rt->config.mode == cfs_modality_async ||
      rt->config.mode == cfs_modality_multithread) {
    rt->work_queue = (cfs_queue_t *)cfs_malloc(sizeof(cfs_queue_t));
    rt->completion_queue = (cfs_queue_t *)cfs_malloc(sizeof(cfs_queue_t));
    cfs_queue_init(rt->work_queue);
    cfs_queue_init(rt->completion_queue);

    rt->thread_pool = cfs_thread_pool_create(
        rt->config.thread_pool_size > 0 ? rt->config.thread_pool_size : 4,
        rt->work_queue, rt->completion_queue);
  }

  return rt;
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
  cfs_request_t *req;
  if (!rt || !rt->completion_queue)
    return 0;

  while ((req = cfs_queue_pop(rt->completion_queue, cfs_false)) != NULL) {
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
CFS_API cfs_size_t cfs_strlen(const cfs_char_t *str) {
  cfs_size_t len = 0;
  if (!str)
    return 0;
  while (str[len])
    len++;
  return len;
}

CFS_API cfs_char_t *cfs_strcpy(cfs_char_t *dest, const cfs_char_t *src) {
  cfs_size_t i = 0;
  if (!dest || !src)
    return dest;
  while ((dest[i] = src[i]) != 0)
    i++;
  return dest;
}

CFS_API cfs_char_t *cfs_strncpy(cfs_char_t *dest, const cfs_char_t *src,
                                cfs_size_t n) {
  cfs_size_t i;
  if (!dest || !src)
    return dest;
  for (i = 0; i < n && src[i] != 0; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = 0;
  return dest;
}

CFS_API cfs_char_t *cfs_strcat(cfs_char_t *dest, const cfs_char_t *src) {
  cfs_size_t dest_len = cfs_strlen(dest);
  cfs_size_t i = 0;
  if (!dest || !src)
    return dest;
  while ((dest[dest_len + i] = src[i]) != 0)
    i++;
  return dest;
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
CFS_API cfs_size_t cfs_utf8_to_utf16(const char *utf8_str, wchar_t *dest,
                                     cfs_size_t dest_len) {
  int req = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, dest, (int)dest_len);
  return (req > 0) ? (cfs_size_t)req : 0;
}

CFS_API cfs_size_t cfs_utf16_to_utf8(const wchar_t *utf16_str, char *dest,
                                     cfs_size_t dest_len) {
  int req = WideCharToMultiByte(CP_UTF8, 0, utf16_str, -1, dest, (int)dest_len,
                                NULL, NULL);
  return (req > 0) ? (cfs_size_t)req : 0;
}
#endif

CFS_API cfs_size_t cfs_mb_to_wide(const char *mb_str, wchar_t *dest,
                                  cfs_size_t dest_len) {
#if defined(CFS_OS_WINDOWS)
  return cfs_utf8_to_utf16(mb_str, dest, dest_len);
#else
  return 0; /* Fallback not used on POSIX where cfs_char_t is char */
#endif
}

CFS_API cfs_size_t cfs_wide_to_mb(const wchar_t *wide_str, char *dest,
                                  cfs_size_t dest_len) {
#if defined(CFS_OS_WINDOWS)
  return cfs_utf16_to_utf8(wide_str, dest, dest_len);
#else
  return 0; /* Fallback not used on POSIX */
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

CFS_API cfs_error_code cfs_make_error_code_from_os(int os_error) {
  cfs_error_code ec;
  ec.value = os_error;
  ec.errc = os_error == 0 ? cfs_errc_success
                          : cfs_errc_unknown_error; /* simplified for now */
  return ec;
}

CFS_API cfs_error_code cfs_get_last_error(void) {
#if defined(CFS_OS_WINDOWS)
  return cfs_make_error_code_from_os(GetLastError());
#else
  /* missing errno include but simplify for now */
  return cfs_make_error_code_from_os(1);
#endif
}

CFS_API const char *cfs_error_message(cfs_errc err) {
  (void)err;
  return "Error";
}

/* Phase 8 & 9: Path Struct Basics */
CFS_API void cfs_path_init(cfs_path *p) {
  if (!p)
    return;
  p->str = NULL;
  p->length = 0;
  p->capacity = 0;
}

CFS_API cfs_error_code cfs_path_init_str(cfs_path *p,
                                         const cfs_char_t *source) {
  cfs_error_code ec = cfs_make_error_code_from_os(0);
  cfs_path_init(p);
  if (source) {
    cfs_path_assign(p, source);
  }
  return ec;
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

CFS_API cfs_error_code cfs_path_clone(cfs_path *dest, const cfs_path *src) {
  cfs_error_code ec = cfs_make_error_code_from_os(0);
  if (!dest || !src)
    return ec;
  cfs_path_init(dest);
  if (src->str) {
    cfs_path_assign(dest, src->str);
  }
  return ec;
}

CFS_API const cfs_char_t *cfs_path_c_str(const cfs_path *p) {
  return (p && p->str) ? p->str : CFS_STR("");
}

CFS_API cfs_path *cfs_path_make_preferred(cfs_path *p) {
  cfs_size_t i;
  if (!p || !p->str)
    return p;
  for (i = 0; i < p->length; i++) {
#if defined(CFS_OS_WINDOWS)
    if (p->str[i] == CFS_CHAR('/'))
      p->str[i] = CFS_CHAR('\\');
#else
    if (p->str[i] == CFS_CHAR('\\'))
      p->str[i] = CFS_CHAR('/');
#endif
  }
  return p;
}

CFS_API cfs_char_t *cfs_path_generic_string(const cfs_path *p) {
  cfs_char_t *res;
  cfs_size_t i;
  if (!p || !p->str)
    return NULL;
  res = (cfs_char_t *)cfs_malloc((p->length + 1) * sizeof(cfs_char_t));
  if (!res)
    return NULL;
  CFS_STRCPY_SAFE(res, p->length + 1, p->str);
  for (i = 0; i < p->length; i++) {
#if defined(CFS_OS_WINDOWS)
    if (res[i] == CFS_CHAR('\\'))
      res[i] = CFS_CHAR('/');
#endif
  }
  return res;
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

CFS_API cfs_error_code cfs_path_concat(cfs_path *p, const cfs_char_t *source) {
  cfs_error_code ec = cfs_make_error_code_from_os(0);
  cfs_size_t src_len;
  cfs_size_t new_len;
  if (!p || !source)
    return ec;
  src_len = cfs_strlen(source);
  if (src_len == 0)
    return ec;
  new_len = p->length + src_len;
  ec = cfs_path_reserve(p, new_len + 1);
  if (ec.value != 0)
    return ec;
  CFS_STRCPY_SAFE(p->str + p->length, p->capacity - p->length, source);
  p->length = new_len;
  return ec;
}

/* cfs_path_append implementation from recovered file ending */
CFS_API cfs_error_code cfs_path_append(cfs_path *p, const cfs_char_t *source) {
  cfs_error_code ec = cfs_make_error_code_from_os(0);
  cfs_size_t src_len;
  cfs_bool p_has_sep = cfs_false;
  cfs_bool src_has_sep = cfs_false;
  cfs_size_t new_len;

  if (!p || !source)
    return ec;

  if (cfs_path_is_empty(p)) {
    return cfs_path_assign(p, source);
  }

  if (source[0] == 0)
    return ec;

#if defined(CFS_OS_WINDOWS)
  if (source[0] == CFS_CHAR('\\') || source[0] == CFS_CHAR('/') ||
      (cfs_strlen(source) >= 2 && source[1] == CFS_CHAR(':'))) {
    return cfs_path_assign(p, source);
  }
#else
  if (source[0] == CFS_CHAR('/')) {
    return cfs_path_assign(p, source);
  }
#endif

  src_len = cfs_strlen(source);
  if (p->length > 0)
    p_has_sep = cfs_is_separator(p->str[p->length - 1]);
  src_has_sep = cfs_is_separator(source[0]);

  new_len = p->length + src_len;
  if (!p_has_sep && !src_has_sep)
    new_len++;
  else if (p_has_sep && src_has_sep)
    new_len--;

  ec = cfs_path_reserve(p, new_len + 1);
  if (ec.value != 0)
    return ec;

  if (!p_has_sep && !src_has_sep) {
    p->str[p->length] = CFS_PREFERRED_SEPARATOR;
    p->str[p->length + 1] = 0;
    p->length++;
  } else if (p_has_sep && src_has_sep) {
    source++;
  }

  CFS_STRCPY_SAFE(p->str + p->length, p->capacity - p->length, source);
  p->length = new_len;
  return ec;
}

static cfs_bool cfs_is_separator(cfs_char_t c) {
  return c == CFS_CHAR('/') || c == CFS_CHAR('\\');
}

static cfs_error_code cfs_path_reserve(cfs_path *p, cfs_size_t new_cap) {
  cfs_error_code ec = cfs_make_error_code_from_os(0);
  cfs_char_t *new_str;
  if (!p) {
    ec = cfs_make_error_code_from_os(22); /* EINVAL */
    return ec;
  }
  if (new_cap <= p->capacity)
    return ec;

  if (p->str) {
    new_str = (cfs_char_t *)cfs_realloc(p->str, new_cap * sizeof(cfs_char_t));
  } else {
    new_str = (cfs_char_t *)cfs_malloc(new_cap * sizeof(cfs_char_t));
  }

  if (!new_str) {
    ec = cfs_make_error_code_from_os(12); /* ENOMEM */
    return ec;
  }

  p->str = new_str;
  p->capacity = new_cap;
  return ec;
}

CFS_API cfs_bool cfs_path_is_empty(const cfs_path *p) {
  return !p || p->length == 0;
}

CFS_API cfs_error_code cfs_path_assign(cfs_path *p, const cfs_char_t *source) {
  cfs_error_code ec = cfs_make_error_code_from_os(0);
  cfs_size_t len;
  if (!p) {
    ec = cfs_make_error_code_from_os(22); /* EINVAL */
    return ec;
  }
  if (!source) {
    cfs_path_clear(p);
    return ec;
  }
  len = cfs_strlen(source);
  ec = cfs_path_reserve(p, len + 1);
  if (ec.value != 0)
    return ec;
  CFS_STRCPY_SAFE(p->str, p->capacity, source);
  p->length = len;
  return ec;
}

CFS_API void *cfs_malloc(cfs_size_t size) { return malloc(size); }
CFS_API void cfs_free(void *ptr) { free(ptr); }
CFS_API void *cfs_realloc(void *ptr, cfs_size_t size) {
  return realloc(ptr, size);
}
CFS_API void *cfs_calloc(cfs_size_t num, cfs_size_t size) {
  return calloc(num, size);
}

CFS_API cfs_path cfs_path_filename(const cfs_path *p) {
  cfs_path res;
  cfs_size_t i;
  cfs_path_init(&res);
  if (!p || p->length == 0)
    return res;

  for (i = p->length; i > 0; i--) {
    if (cfs_is_separator(p->str[i - 1]))
      break;
  }

  if (i < p->length) {
    cfs_path_assign(&res, p->str + i);
  }
  return res;
}

CFS_API cfs_path cfs_path_extension(const cfs_path *p) {
  cfs_path res;
  cfs_size_t i;
  cfs_path_init(&res);
  if (!p || p->length == 0)
    return res;

  for (i = p->length; i > 0; i--) {
    if (p->str[i - 1] == CFS_CHAR('.')) {
      if (i > 1 && !cfs_is_separator(p->str[i - 2])) {
        cfs_path_assign(&res, p->str + i - 1);
        return res;
      }
    }
    if (cfs_is_separator(p->str[i - 1]))
      break;
  }
  return res;
}

CFS_API cfs_path cfs_path_stem(const cfs_path *p) {
  cfs_path res;
  cfs_path fn;
  cfs_size_t i;
  cfs_path_init(&res);
  if (!p || p->length == 0)
    return res;

  fn = cfs_path_filename(p);
  if (fn.length == 0)
    return fn;

  for (i = fn.length; i > 0; i--) {
    if (fn.str[i - 1] == CFS_CHAR('.')) {
      if (i > 1) {
        cfs_path_reserve(&res, i);
        CFS_STRNCPY_SAFE(res.str, res.capacity, fn.str, i - 1);
        res.str[i - 1] = 0;
        res.length = i - 1;
        cfs_path_destroy(&fn);
        return res;
      }
    }
  }
  return fn;
}

CFS_API cfs_bool cfs_remove(const cfs_path *p, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || p->length == 0)
    return cfs_false;
#if defined(CFS_OS_WINDOWS)
  if (_wremove(p->str) == 0)
    return cfs_true;
  if (RemoveDirectoryW(p->str))
    return cfs_true;
#else
  if (remove(p->str) == 0)
    return cfs_true;
#endif
  if (ec)
    *ec = cfs_get_last_error();
  return cfs_false;
}

CFS_API cfs_uintmax_t cfs_file_size(const cfs_path *p, cfs_error_code *ec) {
  if (ec)
    cfs_clear_error(ec);
  if (!p || p->length == 0)
    return 0;
#if defined(CFS_OS_WINDOWS)
  {
    struct _stat64 st;
    if (_wstat64(p->str, &st) == 0) {
      return (cfs_uintmax_t)st.st_size;
    }
  }
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      return (cfs_uintmax_t)st.st_size;
    }
  }
#endif
  if (ec)
    *ec = cfs_get_last_error();
  return 0;
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
  *out_pipe = (cfs_message_pipe *)cfs_malloc(sizeof(cfs_message_pipe));
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
  *buffer = cfs_malloc(*size);
  if (!*buffer)
    return -1;
  ((int *)*buffer)[0] = req->opcode;
  return 0;
}

CFS_API int cfs_deserialize_request(const void *buffer, cfs_size_t size,
                                    cfs_request_t **req) {
  if (!buffer || !req || size < sizeof(int))
    return -1;
  *req = (cfs_request_t *)cfs_malloc(sizeof(cfs_request_t));
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
  *out_proc = (cfs_process_t *)cfs_malloc(sizeof(cfs_process_t));
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
  *out_shm = (cfs_shm_segment *)cfs_malloc(sizeof(cfs_shm_segment));
  if (!*out_shm)
    return -1;
  (*out_shm)->size = size;
#if defined(CFS_OS_WINDOWS)
  (*out_shm)->map_handle = CreateFileMappingW(
      INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      (DWORD)(((cfs_uintmax_t)size) >> 32), (DWORD)(size & 0xFFFFFFFF), name);
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

CFS_API void *cfs_shm_map(cfs_shm_segment *shm) {
  if (!shm)
    return NULL;
#if defined(CFS_OS_WINDOWS)
  return MapViewOfFile(shm->map_handle, FILE_MAP_ALL_ACCESS, 0, 0, shm->size);
#else
  return NULL; /* mmap stub */
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
  *out_sem = (cfs_named_semaphore *)cfs_malloc(sizeof(cfs_named_semaphore));
  if (!*out_sem)
    return -1;
#if defined(CFS_OS_WINDOWS)
  (*out_sem)->handle = CreateSemaphoreW(NULL, initial_count, 0x7FFFFFFF, name);
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
  *out_gt = (cfs_greenthread_t *)cfs_malloc(sizeof(cfs_greenthread_t));
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
  *out_sched = (cfs_greenthread_scheduler *)cfs_malloc(
      sizeof(cfs_greenthread_scheduler));
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

  req = (cfs_request_t *)cfs_malloc(sizeof(cfs_request_t));
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

#endif /* CFS_IMPLEMENTATION */
