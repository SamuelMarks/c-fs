#ifndef CFS_H
#define CFS_H

/* clang-format off */
#include <stddef.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
#elif defined(__WATCOMC__) && defined(__DOS__) || defined(__MSDOS__)
#define CFS_OS_DOS
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

/* Format Specifiers Abstractions */
#if defined(_MSC_VER)
#define CFS_NUM_FORMAT "%I64d"
#define CFS_UNUM_FORMAT "%I64u"
#else
#if defined(__APPLE__) || defined(__LP64__)
#define CFS_NUM_FORMAT "%lld"
#define CFS_UNUM_FORMAT "%llu"
#else
#define CFS_NUM_FORMAT "%ld"
#define CFS_UNUM_FORMAT "%lu"
#endif
#endif

/* Global Out-Of-Memory Error Fallback Hook */
typedef void (*cfs_oom_handler_t)(void);

/** \brief 44-48. Memory Allocation Wrappers */
CFS_API void cfs_set_oom_handler(cfs_oom_handler_t handler);
/** \brief cfs_malloc */
CFS_API int cfs_malloc(cfs_size_t size, void **out);
/** \brief cfs_free */
CFS_API void cfs_free(void *ptr);
/** \brief cfs_realloc */
CFS_API int cfs_realloc(void *ptr, cfs_size_t new_size, void **out);
/** \brief cfs_calloc */
CFS_API int cfs_calloc(cfs_size_t num, cfs_size_t size, void **out);

/* Phase 6: String Handling & Charsets */

/** \brief 51-54. Native string handling abstractions */
CFS_API int cfs_strlen(const cfs_char_t *str, cfs_size_t *out);
/** \brief cfs_strcpy */
CFS_API int cfs_strcpy(cfs_char_t *dest, const cfs_char_t *src,
                       cfs_char_t **out);
/** \brief cfs_strncpy */
CFS_API int cfs_strncpy(cfs_char_t *dest, const cfs_char_t *src, cfs_size_t n,
                        cfs_char_t **out);
/** \brief cfs_strcat */
CFS_API int cfs_strcat(cfs_char_t *dest, const cfs_char_t *src,
                       cfs_char_t **out);
/** \brief cfs_strcmp */
CFS_API int cfs_strcmp(const cfs_char_t *lhs, const cfs_char_t *rhs);
/** \brief cfs_strncmp */
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
#define CFS_STRCPY_SAFE(dst, dst_sz, src) cfs_strcpy((dst), (src), NULL)
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  cfs_strncpy((dst), (src), (n), NULL)
#endif
#if defined(CFS_OS_WINDOWS)
/* UTF-8 to UTF-16 conversion (Returns required buffer size in chars if dest is
 * NULL) */
CFS_API int cfs_utf8_to_utf16(const char *utf8_str, wchar_t *dest,
                              cfs_size_t dest_len, cfs_size_t *out_req);
/* UTF-16 to UTF-8 conversion (Returns required buffer size in bytes if dest is
 * NULL) */
CFS_API int cfs_utf16_to_utf8(const wchar_t *utf16_str, char *dest,
                              cfs_size_t dest_len, cfs_size_t *out_req);
#endif

/** \brief ANSI to Wide character conversion (Generic Fallbacks) */
CFS_API int cfs_mb_to_wide(const char *mb_str, wchar_t *dest,
                           cfs_size_t dest_len, cfs_size_t *out_req);
/** \brief cfs_wide_to_mb */
CFS_API int cfs_wide_to_mb(const wchar_t *wide_str, char *dest,
                           cfs_size_t dest_len, cfs_size_t *out_req);
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

/** \brief 63-64, 68. Global / Thread-Local Error Interfacing */
CFS_API void cfs_set_error(cfs_error_code *ec, int os_value,
                           cfs_errc standard_value);
/** \brief cfs_clear_error */
CFS_API void cfs_clear_error(cfs_error_code *ec);
/** \brief cfs_error_message */
CFS_API int cfs_error_message(cfs_errc err, const char **out);

/** \brief 65-67. OS Translation Hooks */
CFS_API int cfs_make_error_code_from_os(int os_error, cfs_error_code *out);
/** \brief cfs_get_last_error */
CFS_API int cfs_get_last_error(cfs_error_code *out);
/* Phase 8: Path Struct Basics */

/* 76. Define platform-specific path separator macros */
#if defined(CFS_OS_WINDOWS)
#define CFS_PREFERRED_SEPARATOR CFS_CHAR('\\')
#define PATH_SEP_CHAR CFS_CHAR('\\')
#define PATH_SEP_STR CFS_STR("\\")
#else
#define CFS_PREFERRED_SEPARATOR CFS_CHAR('/')
#define PATH_SEP_CHAR CFS_CHAR('/')
#define PATH_SEP_STR CFS_STR("/")
#endif

/* 71. Define opaque cfs_path struct */
typedef struct cfs_path {
  cfs_char_t *str;
  cfs_size_t length;
  cfs_size_t capacity;
} cfs_path;

/** \brief 72-75, 77-79. Path Initialization, Mutation, and Destruction */
CFS_API void cfs_path_init(cfs_path *p);
/** \brief cfs_path_init_str */
CFS_API int cfs_path_init_str(cfs_path *p, const cfs_char_t *source);
/** \brief cfs_path_destroy */
CFS_API void cfs_path_destroy(cfs_path *p);
/** \brief cfs_path_clone */
CFS_API int cfs_path_clone(cfs_path *dest, const cfs_path *src);
/** \brief cfs_path_make_preferred */
CFS_API int cfs_path_make_preferred(cfs_path *p);
/** \brief cfs_path_c_str */
CFS_API int cfs_path_c_str(const cfs_path *p, const cfs_char_t **out);
/* Returns dynamically allocated generic path string (e.g. forward slashes on
 * Windows). Caller must free. */
CFS_API int cfs_path_generic_string(const cfs_path *p, cfs_char_t **out);
/* Phase 9: Path Building */

/** \brief 81-84, 88. Path assignment, concatenation, and manipulation */
CFS_API int cfs_path_assign(cfs_path *p, const cfs_char_t *source);
/** \brief cfs_path_append */
CFS_API int cfs_path_append(cfs_path *p, const cfs_char_t *source);
/** \brief cfs_path_concat */
CFS_API int cfs_path_concat(cfs_path *p, const cfs_char_t *source);
/** \brief cfs_path_clear */
CFS_API void cfs_path_clear(cfs_path *p);
/** \brief cfs_path_swap */
CFS_API void cfs_path_swap(cfs_path *lhs, cfs_path *rhs);
/* Phase 10: Path Decomposition - Root Analysis */

/** \brief Extracts drive letters (Windows) or root nodes. Returns path
 * instance. */
CFS_API int cfs_path_root_name(const cfs_path *p, cfs_path *out);
/** \brief Extracts base root separator. Returns path instance. */
CFS_API int cfs_path_root_directory(const cfs_path *p, cfs_path *out);
/** \brief Combines root name and root directory. Returns path instance. */
CFS_API int cfs_path_root_path(const cfs_path *p, cfs_path *out);
/* Phase 11: Path Decomposition - Elements */

/** \brief 101. Returns path relative to the root path */
CFS_API int cfs_path_relative_path(const cfs_path *p, cfs_path *out);
/** \brief 102. Returns path of the parent directory */
CFS_API int cfs_path_parent_path(const cfs_path *p, cfs_path *out);
/** \brief 103. Returns the filename component */
CFS_API int cfs_path_filename(const cfs_path *p, cfs_path *out);
/** \brief 104. Returns the stem (filename without extension) */
CFS_API int cfs_path_stem(const cfs_path *p, cfs_path *out);
/** \brief 105. Returns the file extension */
CFS_API int cfs_path_extension(const cfs_path *p, cfs_path *out);
/* Phase 12: Path Modifiers */

/** \brief 111. Replaces the terminal filename component */
CFS_API int cfs_path_replace_filename(cfs_path *p,
                                      const cfs_char_t *replacement);
/** \brief 112. Replaces the extension of the terminal component */
CFS_API int cfs_path_replace_extension(cfs_path *p,
                                       const cfs_char_t *replacement);
/** \brief 113. Removes the terminal filename component (truncates back to
 * parent) */
CFS_API void cfs_path_remove_filename(cfs_path *p);
/** \brief 114. Returns absolute path */
CFS_API int cfs_absolute(const cfs_path *p, cfs_path *out, cfs_error_code *ec);

/* Copy file options mirroring std::filesystem::copy_options */
typedef enum cfs_copy_options {
  cfs_copy_options_none = 0,
  cfs_copy_options_skip_existing = 1,
  cfs_copy_options_overwrite_existing = 2,
  cfs_copy_options_update_existing = 4
} cfs_copy_options;

/** \brief Phase 20: Missing std::filesystem functions */
CFS_API int cfs_canonical(const cfs_path *p, cfs_path *out, cfs_error_code *ec);
/** \brief cfs_weakly_canonical */
CFS_API int cfs_weakly_canonical(const cfs_path *p, cfs_path *out,
                                 cfs_error_code *ec);
/** \brief cfs_read_symlink */
CFS_API int cfs_read_symlink(const cfs_path *p, cfs_path *out,
                             cfs_error_code *ec);
/** \brief cfs_relative */
CFS_API int cfs_relative(const cfs_path *p, const cfs_path *base, cfs_path *out,
                         cfs_error_code *ec);
/** \brief cfs_proximate */
CFS_API int cfs_proximate(const cfs_path *p, const cfs_path *base,
                          cfs_path *out, cfs_error_code *ec);
/** \brief cfs_copy */
CFS_API int cfs_copy(const cfs_path *from, const cfs_path *to,
                     cfs_copy_options options, cfs_error_code *ec);
/** \brief cfs_copy_symlink */
CFS_API int cfs_copy_symlink(const cfs_path *existing_symlink,
                             const cfs_path *new_symlink, cfs_error_code *ec);

/* Phase 13: Path Observers & Comparisons */

/** \brief Observers */
CFS_API int cfs_path_is_empty(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_root_path */
CFS_API int cfs_path_has_root_path(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_root_name */
CFS_API int cfs_path_has_root_name(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_root_directory */
CFS_API int cfs_path_has_root_directory(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_relative_path */
CFS_API int cfs_path_has_relative_path(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_parent_path */
CFS_API int cfs_path_has_parent_path(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_filename */
CFS_API int cfs_path_has_filename(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_stem */
CFS_API int cfs_path_has_stem(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_has_extension */
CFS_API int cfs_path_has_extension(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_is_absolute */
CFS_API int cfs_path_is_absolute(const cfs_path *p, cfs_bool *out);
/** \brief cfs_path_is_relative */
CFS_API int cfs_path_is_relative(const cfs_path *p, cfs_bool *out);

/** \brief Lexicographical comparison */
CFS_API int cfs_path_compare(const cfs_path *lhs, const cfs_path *rhs);
/* Phase 14: Lexical Path Operations */

/** \brief 132. Lexically normalizes the path (resolves . and .. internally) */
CFS_API int cfs_path_lexically_normal(const cfs_path *p, cfs_path *out);
/** \brief 134. Returns a path representing how to get from base to p */
CFS_API int cfs_path_lexically_relative(const cfs_path *p, const cfs_path *base,
                                        cfs_path *out);
/* 135. Returns relative path if mathematically divergent, otherwise the
 * original path */
CFS_API int cfs_path_lexically_proximate(const cfs_path *p,
                                         const cfs_path *base, cfs_path *out);

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

typedef enum cfs_perm_options {
  cfs_perm_options_replace = 1,
  cfs_perm_options_add = 2,
  cfs_perm_options_remove = 4,
  cfs_perm_options_nofollow = 8
} cfs_perm_options;

typedef enum cfs_directory_options {
  cfs_directory_options_none = 0,
  cfs_directory_options_follow_directory_symlink = 1,
  cfs_directory_options_skip_permission_denied = 2
} cfs_directory_options;

/* Status struct holding retrieved OS attributes */
typedef struct cfs_file_status {
  cfs_file_type type;
  cfs_perms permissions;
} cfs_file_status;

/** \brief 141-142. Core Status Queries */
CFS_API int cfs_status(const cfs_path *p, cfs_file_status *out,
                       cfs_error_code *ec);
/** \brief cfs_symlink_status */
CFS_API int cfs_symlink_status(const cfs_path *p, cfs_file_status *out,
                               cfs_error_code *ec);
/** \brief cfs_status_known */
CFS_API int cfs_status_known(cfs_file_status s, cfs_bool *out);

/** \brief 145. Exists Observer */
CFS_API int cfs_exists(cfs_file_status s, cfs_bool *out);
/** \brief cfs_exists_path */
CFS_API int cfs_exists_path(const cfs_path *p, cfs_bool *out,
                            cfs_error_code *ec);

/** \brief 151-159. Filesystem Type Queries */
CFS_API int cfs_is_block_file(cfs_file_status s, cfs_bool *out);
/** \brief cfs_is_character_file */
CFS_API int cfs_is_character_file(cfs_file_status s, cfs_bool *out);
/** \brief cfs_is_directory */
CFS_API int cfs_is_directory(cfs_file_status s, cfs_bool *out);
/** \brief cfs_is_fifo */
CFS_API int cfs_is_fifo(cfs_file_status s, cfs_bool *out);
/** \brief cfs_is_other */
CFS_API int cfs_is_other(cfs_file_status s, cfs_bool *out);
/** \brief cfs_is_regular_file */
CFS_API int cfs_is_regular_file(cfs_file_status s, cfs_bool *out);
/** \brief cfs_is_socket */
CFS_API int cfs_is_socket(cfs_file_status s, cfs_bool *out);
/** \brief cfs_is_symlink */
CFS_API int cfs_is_symlink(cfs_file_status s, cfs_bool *out);

/** \brief 154. Is Empty Query (Directory or zero-byte file) */
CFS_API int cfs_is_empty_path(const cfs_path *p, cfs_bool *out,
                              cfs_error_code *ec);
/* Phase 17: Filesystem Operations - Creation */

/** \brief 161. Create a single directory node */
CFS_API int cfs_create_directory(const cfs_path *p, cfs_error_code *ec);
/** \brief 162. Recursively create directory nodes */
CFS_API int cfs_create_directories(const cfs_path *p, cfs_error_code *ec);

/** \brief 163-165. Create links */
CFS_API void cfs_create_hard_link(const cfs_path *target, const cfs_path *link,
                                  cfs_error_code *ec);
/** \brief cfs_create_symlink */
CFS_API void cfs_create_symlink(const cfs_path *target, const cfs_path *link,
                                cfs_error_code *ec);
/** \brief cfs_create_directory_symlink */
CFS_API void cfs_create_directory_symlink(const cfs_path *target,
                                          const cfs_path *link,
                                          cfs_error_code *ec);

/* Copy file options mirroring std::filesystem::copy_options */

/** \brief 168. Copy files strictly */
CFS_API int cfs_copy_file(const cfs_path *from, const cfs_path *to,
                          cfs_copy_options options, cfs_error_code *ec);
/* Phase 18: Filesystem Operations - Modification */

/** \brief 171. Remove single file or empty directory */
CFS_API int cfs_remove(const cfs_path *p, cfs_error_code *ec);
/** \brief 172. Remove all contents recursively. Returns number of removed
 * objects */
CFS_API int cfs_remove_all(const cfs_path *p, cfs_size_t *out,
                           cfs_error_code *ec);

/** \brief 173. Rename/Move node */
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
/** \brief cfs_resize_file */
CFS_API void cfs_resize_file(const cfs_path *p, cfs_uintmax_t size,
                             cfs_error_code *ec);
/** \brief cfs_file_size */
CFS_API int cfs_file_size(const cfs_path *p, cfs_uintmax_t *out,
                          cfs_error_code *ec);

/* 176. Space Information */
typedef struct cfs_space_info {
  cfs_uintmax_t capacity;
  cfs_uintmax_t free;
  cfs_uintmax_t available;
} cfs_space_info;
/** \brief cfs_space */
CFS_API int cfs_space(const cfs_path *p, cfs_space_info *out,
                      cfs_error_code *ec);

/* 177. File Write Time */
/* Mapped natively to time_t or system tick representation */
#if defined(__GNUC__) || defined(__clang__)
__extension__ typedef long long cfs_file_time_type;
#elif defined(_MSC_VER)
typedef __int64 cfs_file_time_type;
#else
typedef long cfs_file_time_type;
#endif
/** \brief cfs_last_write_time */
CFS_API int cfs_last_write_time(const cfs_path *p, cfs_file_time_type *out,
                                cfs_error_code *ec);

/** \brief 17X. Permissions and Links */
CFS_API int cfs_permissions(const cfs_path *p, cfs_perms prms,
                            cfs_perm_options opts, cfs_error_code *ec);
/** \brief cfs_hard_link_count */
CFS_API int cfs_hard_link_count(const cfs_path *p, cfs_uintmax_t *out,
                                cfs_error_code *ec);
/** \brief cfs_equivalent */
CFS_API int cfs_equivalent(const cfs_path *p1, const cfs_path *p2,
                           cfs_bool *out, cfs_error_code *ec);

/** \brief 178-179. Environment paths */
CFS_API int cfs_current_path(cfs_path *out, cfs_error_code *ec);
/** \brief cfs_current_path_set */
CFS_API void cfs_current_path_set(const cfs_path *p, cfs_error_code *ec);
/** \brief cfs_temp_directory_path */
CFS_API int cfs_temp_directory_path(cfs_path *out, cfs_error_code *ec);
/* Phase 19: Directory Iteration */

/* 181. Directory entry structure caching path and status */
typedef struct cfs_directory_entry {
  cfs_path path;
  cfs_file_status status;
  cfs_file_status symlink_status;
} cfs_directory_entry;

/* 182-185. Standard Directory Iterator */
typedef struct cfs_directory_iterator cfs_directory_iterator;
/** \brief cfs_dir_itr_init */
CFS_API int cfs_dir_itr_init(const cfs_path *p, cfs_directory_iterator **out_it,
                             cfs_error_code *ec);
/* Returns 0 on success, with a pointer to the internal entry, or 1 if iteration
 * complete, or -1 on error */
CFS_API int cfs_dir_itr_next(cfs_directory_iterator *it,
                             const cfs_directory_entry **out_entry,
                             cfs_error_code *ec);
/** \brief cfs_dir_itr_close */
CFS_API void cfs_dir_itr_close(cfs_directory_iterator *it);

/* 186-189. Recursive Directory Iterator */
typedef struct cfs_recursive_directory_iterator
    cfs_recursive_directory_iterator;
/** \brief cfs_rec_dir_itr_init */
CFS_API int cfs_rec_dir_itr_init(const cfs_path *p,
                                 cfs_recursive_directory_iterator **out_it,
                                 cfs_error_code *ec);
/** \brief cfs_rec_dir_itr_next */
CFS_API int cfs_rec_dir_itr_next(cfs_recursive_directory_iterator *it,
                                 const cfs_directory_entry **out_entry,
                                 cfs_error_code *ec);
/* Prevents the iterator from descending into the currently evaluated directory
 * node */
CFS_API void
cfs_rec_dir_itr_disable_recursion_pending(cfs_recursive_directory_iterator *it);
/** \brief Moves the iterator one level up in the directory tree */
CFS_API void cfs_rec_dir_itr_pop(cfs_recursive_directory_iterator *it,
                                 cfs_error_code *ec);
/** \brief cfs_rec_dir_itr_close */
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

/** \brief 4. Implement cfs_runtime_init() */
CFS_API int cfs_runtime_init(const cfs_runtime_config *config,
                             cfs_runtime_t **out_rt, cfs_error_code *ec);

/** \brief 5. Implement cfs_runtime_destroy() */
CFS_API void cfs_runtime_destroy(cfs_runtime_t *runtime);

/** \brief 9. Create generic request dispatcher internal function */
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

/** \brief 13. Non-blocking API variants */
CFS_API int cfs_remove_async(cfs_runtime_t *rt, const cfs_path *p,
                             cfs_callback_t cb, void *user_data);
/** \brief cfs_file_size_async */
CFS_API int cfs_file_size_async(cfs_runtime_t *rt, const cfs_path *p,
                                cfs_callback_t cb, void *user_data);

/** \brief 19. cfs_runtime_poll() */
CFS_API int cfs_runtime_poll(cfs_runtime_t *rt);

/* Phase 3: Platform-Specific Async & Message Passing */

/** \brief 29. Reference counting to cfs_request_t */
CFS_API void cfs_request_retain(cfs_request_t *req);
/** \brief cfs_request_release */
CFS_API void cfs_request_release(cfs_request_t *req);

/** \brief 30. Implement cancellation logic */
CFS_API int cfs_cancel_request(cfs_runtime_t *rt, cfs_request_t *req);

/* Platform Specific Async Backend Configurations */
typedef struct cfs_io_uring_context cfs_io_uring_context;
typedef struct cfs_iocp_context cfs_iocp_context;

/* 24-26. Message Passing Actors */
typedef struct cfs_message_pipe cfs_message_pipe;
/** \brief cfs_message_pipe_create */
CFS_API int cfs_message_pipe_create(const cfs_char_t *path,
                                    cfs_message_pipe **out_pipe);
/** \brief cfs_message_pipe_destroy */
CFS_API void cfs_message_pipe_destroy(cfs_message_pipe *pipe);
/** \brief cfs_serialize_request */
CFS_API int cfs_serialize_request(const cfs_request_t *req, void **buffer,
                                  cfs_size_t *size);
/** \brief cfs_deserialize_request */
CFS_API int cfs_deserialize_request(const void *buffer, cfs_size_t size,
                                    cfs_request_t **req);

/* Phase 4: Multiprocessing and Greenthreads */

/* 31. Multiprocess modality backend (Process Handles) */
typedef struct cfs_process_t cfs_process_t;
/** \brief cfs_process_spawn */
CFS_API int cfs_process_spawn(const cfs_char_t *executable,
                              cfs_process_t **out_proc);
/** \brief cfs_process_wait */
CFS_API int cfs_process_wait(cfs_process_t *proc);
/** \brief cfs_process_destroy */
CFS_API void cfs_process_destroy(cfs_process_t *proc);

/* 32. Shared Memory (shm) segments */
typedef struct cfs_shm_segment cfs_shm_segment;
/** \brief cfs_shm_create */
CFS_API int cfs_shm_create(cfs_size_t size, const cfs_char_t *name,
                           cfs_shm_segment **out_shm);
/** \brief cfs_shm_map */
CFS_API int cfs_shm_map(cfs_shm_segment *shm, void **out);
/** \brief cfs_shm_unmap */
CFS_API void cfs_shm_unmap(cfs_shm_segment *shm, void *addr);
/** \brief cfs_shm_destroy */
CFS_API void cfs_shm_destroy(cfs_shm_segment *shm);

/* 33. Multiprocess semaphore */
typedef struct cfs_named_semaphore cfs_named_semaphore;
/** \brief cfs_named_semaphore_create */
CFS_API int cfs_named_semaphore_create(const cfs_char_t *name,
                                       int initial_count,
                                       cfs_named_semaphore **out_sem);
/** \brief cfs_named_semaphore_wait */
CFS_API int cfs_named_semaphore_wait(cfs_named_semaphore *sem);
/** \brief cfs_named_semaphore_post */
CFS_API int cfs_named_semaphore_post(cfs_named_semaphore *sem);
/** \brief cfs_named_semaphore_destroy */
CFS_API void cfs_named_semaphore_destroy(cfs_named_semaphore *sem);

/* 35-37. Greenthread modality (ucontext / setjmp) */
typedef struct cfs_greenthread_t cfs_greenthread_t;
typedef void (*cfs_greenthread_func)(void *);

/** \brief cfs_greenthread_spawn */
CFS_API int cfs_greenthread_spawn(cfs_greenthread_func func, void *arg,
                                  cfs_greenthread_t **out_gt);
/** \brief cfs_greenthread_yield */
CFS_API int cfs_greenthread_yield(void);
/** \brief cfs_greenthread_destroy */
CFS_API void cfs_greenthread_destroy(cfs_greenthread_t *gt);

/* 36. Greenthread scheduler */
typedef struct cfs_greenthread_scheduler cfs_greenthread_scheduler;
/** \brief stub */
CFS_API int
cfs_greenthread_scheduler_init(cfs_greenthread_scheduler **out_sched);
/** \brief cfs_greenthread_scheduler_run */
CFS_API int cfs_greenthread_scheduler_run(cfs_greenthread_scheduler *sched);
/** \brief stub */
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
/** \brief cfs_dir_itr_init_async */
CFS_API int cfs_dir_itr_init_async(cfs_runtime_t *rt, const cfs_path *p,
                                   cfs_callback_t cb, void *user_data);

/* 43. Multi-process sandbox config */
typedef struct cfs_sandbox_config {
  cfs_path root_chroot;
  cfs_bool restrict_symlinks;
} cfs_sandbox_config;

/** \brief cfs_runtime_set_sandbox */
CFS_API int cfs_runtime_set_sandbox(cfs_runtime_t *rt,
                                    const cfs_sandbox_config *config);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#if defined(CFS_HEADER_ONLY_MODE)
#include "cfs.c"
#endif

#endif /* CFS_H */
