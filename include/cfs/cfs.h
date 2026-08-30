/**
 * \file cfs.h
 * \brief Unified cross-platform filesystem API definitions and data structures.
 */
#ifndef CFS_H
#define CFS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(CFS_HEADER_ONLY_MODE) && !defined(CFS_IMPLEMENTATION)
#define CFS_IMPLEMENTATION
#endif

/* clang-format off */
#include <stddef.h>
#include "cfs/log.h"
#include "cfs/no_discard.h"

#ifdef CFS_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>
#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <io.h>
#elif !defined(__WATCOMC__) && !defined(__MSDOS__) && !defined(CFS_OS_DOS)
#include <dirent.h>
#include <pthread.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#endif
#endif
/* clang-format on */

/* Phase 3: Platform & Compiler Detection Macros */

/* 22-25. OS Detection */
#if defined(_WIN32) || defined(_WIN64)
#define CFS_OS_WINDOWS
#elif defined(__EMSCRIPTEN__)
#define CFS_OS_EMSCRIPTEN
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

/** \brief cfs_char_t mapped dynamically to char or wchar_t depending on UNICODE
 * settings. */
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
typedef wchar_t cfs_char_t;
#define CFS_CHAR(c) L##c
#define CFS_STR(s) L##s
#else
typedef char cfs_char_t;
#define CFS_CHAR(c) c
#define CFS_STR(s) s
#endif

/** \brief cfs_bool type strictly mapping to C89 integers. */
typedef int cfs_bool;
#define cfs_true 1
#define cfs_false 0

/** \brief cfs_size_t type mapping to standard size_t. */
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

/** \brief Global Out-Of-Memory Error Fallback Hook type. */
typedef enum cfs_errc {
  /** \brief Represents the cfs_errc_success enumerator or field data. */
  cfs_errc_success = 0,
  /** \brief Represents the cfs_errc_address_family_not_supported enumerator or
     field data. */
  cfs_errc_address_family_not_supported,
  /** \brief Represents the cfs_errc_address_in_use enumerator or field data. */
  cfs_errc_address_in_use,
  /** \brief Represents the cfs_errc_address_not_available enumerator or field
     data. */
  cfs_errc_address_not_available,
  /** \brief Represents the cfs_errc_already_connected enumerator or field data.
   */
  cfs_errc_already_connected,
  /** \brief Represents the cfs_errc_argument_list_too_long enumerator or field
     data. */
  cfs_errc_argument_list_too_long,
  /** \brief Represents the cfs_errc_argument_out_of_domain enumerator or field
     data. */
  cfs_errc_argument_out_of_domain,
  /** \brief Represents the cfs_errc_bad_address enumerator or field data. */
  cfs_errc_bad_address,
  /** \brief Represents the cfs_errc_bad_file_descriptor enumerator or field
     data. */
  cfs_errc_bad_file_descriptor,
  /** \brief Represents the cfs_errc_bad_message enumerator or field data. */
  cfs_errc_bad_message,
  /** \brief Represents the cfs_errc_broken_pipe enumerator or field data. */
  cfs_errc_broken_pipe,
  /** \brief Represents the cfs_errc_connection_aborted enumerator or field
     data. */
  cfs_errc_connection_aborted,
  /** \brief Represents the cfs_errc_connection_already_in_progress enumerator
     or field data. */
  cfs_errc_connection_already_in_progress,
  /** \brief Represents the cfs_errc_connection_refused enumerator or field
     data. */
  cfs_errc_connection_refused,
  /** \brief Represents the cfs_errc_connection_reset enumerator or field data.
   */
  cfs_errc_connection_reset,
  /** \brief Represents the cfs_errc_cross_device_link enumerator or field data.
   */
  cfs_errc_cross_device_link,
  /** \brief Represents the cfs_errc_destination_address_required enumerator or
     field data. */
  cfs_errc_destination_address_required,
  /** \brief Represents the cfs_errc_device_or_resource_busy enumerator or field
     data. */
  cfs_errc_device_or_resource_busy,
  /** \brief Represents the cfs_errc_directory_not_empty enumerator or field
     data. */
  cfs_errc_directory_not_empty,
  /** \brief Represents the cfs_errc_executable_format_error enumerator or field
     data. */
  cfs_errc_executable_format_error,
  /** \brief Represents the cfs_errc_file_exists enumerator or field data. */
  cfs_errc_file_exists,
  /** \brief Represents the cfs_errc_file_too_large enumerator or field data. */
  cfs_errc_file_too_large,
  /** \brief Represents the cfs_errc_filename_too_long enumerator or field data.
   */
  cfs_errc_filename_too_long,
  /** \brief Represents the cfs_errc_function_not_supported enumerator or field
     data. */
  cfs_errc_function_not_supported,
  /** \brief Represents the cfs_errc_host_unreachable enumerator or field data.
   */
  cfs_errc_host_unreachable,
  /** \brief Represents the cfs_errc_identifier_removed enumerator or field
     data. */
  cfs_errc_identifier_removed,
  /** \brief Represents the cfs_errc_illegal_byte_sequence enumerator or field
     data. */
  cfs_errc_illegal_byte_sequence,
  /** \brief Represents the cfs_errc_inappropriate_io_control_operation
     enumerator or field data. */
  cfs_errc_inappropriate_io_control_operation,
  /** \brief Represents the cfs_errc_interrupted enumerator or field data. */
  cfs_errc_interrupted,
  /** \brief Represents the cfs_errc_invalid_argument enumerator or field data.
   */
  cfs_errc_invalid_argument,
  /** \brief Represents the cfs_errc_invalid_seek enumerator or field data. */
  cfs_errc_invalid_seek,
  /** \brief Represents the cfs_errc_io_error enumerator or field data. */
  cfs_errc_io_error,
  /** \brief Represents the cfs_errc_is_a_directory enumerator or field data. */
  cfs_errc_is_a_directory,
  /** \brief Represents the cfs_errc_message_size enumerator or field data. */
  cfs_errc_message_size,
  /** \brief Represents the cfs_errc_network_down enumerator or field data. */
  cfs_errc_network_down,
  /** \brief Represents the cfs_errc_network_reset enumerator or field data. */
  cfs_errc_network_reset,
  /** \brief Represents the cfs_errc_network_unreachable enumerator or field
     data. */
  cfs_errc_network_unreachable,
  /** \brief Represents the cfs_errc_no_buffer_space enumerator or field data.
   */
  cfs_errc_no_buffer_space,
  /** \brief Represents the cfs_errc_no_child_process enumerator or field data.
   */
  cfs_errc_no_child_process,
  /** \brief Represents the cfs_errc_no_link enumerator or field data. */
  cfs_errc_no_link,
  /** \brief Represents the cfs_errc_no_lock_available enumerator or field data.
   */
  cfs_errc_no_lock_available,
  /** \brief Represents the cfs_errc_no_message_available enumerator or field
     data. */
  cfs_errc_no_message_available,
  /** \brief Represents the cfs_errc_no_message enumerator or field data. */
  cfs_errc_no_message,
  /** \brief Represents the cfs_errc_no_protocol_option enumerator or field
     data. */
  cfs_errc_no_protocol_option,
  /** \brief Represents the cfs_errc_no_space_on_device enumerator or field
     data. */
  cfs_errc_no_space_on_device,
  /** \brief Represents the cfs_errc_no_stream_resources enumerator or field
     data. */
  cfs_errc_no_stream_resources,
  /** \brief Represents the cfs_errc_no_such_device_or_address enumerator or
     field data. */
  cfs_errc_no_such_device_or_address,
  /** \brief Represents the cfs_errc_no_such_device enumerator or field data. */
  cfs_errc_no_such_device,
  /** \brief Represents the cfs_errc_no_such_file_or_directory enumerator or
     field data. */
  cfs_errc_no_such_file_or_directory,
  /** \brief Represents the cfs_errc_no_such_process enumerator or field data.
   */
  cfs_errc_no_such_process,
  /** \brief Represents the cfs_errc_not_a_directory enumerator or field data.
   */
  cfs_errc_not_a_directory,
  /** \brief Represents the cfs_errc_not_a_socket enumerator or field data. */
  cfs_errc_not_a_socket,
  /** \brief Represents the cfs_errc_not_a_stream enumerator or field data. */
  cfs_errc_not_a_stream,
  /** \brief Represents the cfs_errc_not_connected enumerator or field data. */
  cfs_errc_not_connected,
  /** \brief Represents the cfs_errc_not_enough_memory enumerator or field data.
   */
  cfs_errc_not_enough_memory,
  /** \brief Represents the cfs_errc_not_supported enumerator or field data. */
  cfs_errc_not_supported,
  /** \brief Represents the cfs_errc_operation_canceled enumerator or field
     data. */
  cfs_errc_operation_canceled,
  /** \brief Represents the cfs_errc_operation_in_progress enumerator or field
     data. */
  cfs_errc_operation_in_progress,
  /** \brief Represents the cfs_errc_operation_not_permitted enumerator or field
     data. */
  cfs_errc_operation_not_permitted,
  /** \brief Represents the cfs_errc_operation_not_supported enumerator or field
     data. */
  cfs_errc_operation_not_supported,
  /** \brief Represents the cfs_errc_operation_would_block enumerator or field
     data. */
  cfs_errc_operation_would_block,
  /** \brief Represents the cfs_errc_owner_dead enumerator or field data. */
  cfs_errc_owner_dead,
  /** \brief Represents the cfs_errc_permission_denied enumerator or field data.
   */
  cfs_errc_permission_denied,
  /** \brief Represents the cfs_errc_protocol_error enumerator or field data. */
  cfs_errc_protocol_error,
  /** \brief Represents the cfs_errc_protocol_not_supported enumerator or field
     data. */
  cfs_errc_protocol_not_supported,
  /** \brief Represents the cfs_errc_read_only_file_system enumerator or field
     data. */
  cfs_errc_read_only_file_system,
  /** \brief Represents the cfs_errc_resource_deadlock_would_occur enumerator or
     field data. */
  cfs_errc_resource_deadlock_would_occur,
  /** \brief Represents the cfs_errc_resource_unavailable_try_again enumerator
     or field data. */
  cfs_errc_resource_unavailable_try_again,
  /** \brief Represents the cfs_errc_result_out_of_range enumerator or field
     data. */
  cfs_errc_result_out_of_range,
  /** \brief Represents the cfs_errc_state_not_recoverable enumerator or field
     data. */
  cfs_errc_state_not_recoverable,
  /** \brief Represents the cfs_errc_stream_timeout enumerator or field data. */
  cfs_errc_stream_timeout,
  /** \brief Represents the cfs_errc_text_file_busy enumerator or field data. */
  cfs_errc_text_file_busy,
  /** \brief Represents the cfs_errc_timed_out enumerator or field data. */
  cfs_errc_timed_out,
  /** \brief Represents the cfs_errc_too_many_files_open_in_system enumerator or
     field data. */
  cfs_errc_too_many_files_open_in_system,
  /** \brief Represents the cfs_errc_too_many_files_open enumerator or field
     data. */
  cfs_errc_too_many_files_open,
  /** \brief Represents the cfs_errc_too_many_links enumerator or field data. */
  cfs_errc_too_many_links,
  /** \brief Represents the cfs_errc_too_many_symbolic_link_levels enumerator or
     field data. */
  cfs_errc_too_many_symbolic_link_levels,
  /** \brief Represents the cfs_errc_value_too_large enumerator or field data.
   */
  cfs_errc_value_too_large,
  /** \brief Represents the cfs_errc_wrong_protocol_type enumerator or field
     data. */
  cfs_errc_wrong_protocol_type,
  /** \brief Unknown error. */ cfs_errc_unknown_error
} cfs_errc;

typedef void (*cfs_oom_handler_t)(void);

/**
 * \brief Registers a global callback hook to trigger when dynamic memory
 * allocation fails.
 *
 * \param handler Pointer to the handler function, or NULL to clear.
 */
CFS_API void cfs_set_oom_handler(cfs_oom_handler_t handler);
/**
 * \brief Allocates memory using the internal allocator or fallback OS
 * mechanism.
 *
 * \param size Number of bytes to allocate.
 * \param out Pointer to store the result of the malloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_malloc(cfs_size_t size, void **out);
/**
 * \brief Frees previously allocated memory.
 *
 * \param ptr Pointer to the memory block to deallocate.
 */
CFS_API void cfs_free(void *ptr);
/**
 * \brief Reallocates an existing memory block to a new size.
 *
 * \param ptr Argument representing the target resource.
 * \param new_size The new requested size in bytes.
 * \param out Pointer to store the result of the realloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_realloc(void *ptr, cfs_size_t new_size,
                                        void **out);
/**
 * \brief Allocates zero-initialized memory for an array of elements.
 *
 * \param num Number of elements to allocate.
 * \param size Argument representing the target resource.
 * \param out Pointer to store the result of the calloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_calloc(cfs_size_t num, cfs_size_t size,
                                       void **out);

/* Phase 6: String Handling & Charsets */

/** \brief 51-54. Native string handling abstractions */
/**
 * \brief Computes the length of a string safely.
 *
 * \param str The null-terminated string to evaluate.
 * \param out Pointer to store the result of the strlen operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strlen(const cfs_char_t *str, cfs_size_t *out);
/**
 * \brief Copies a string into a destination buffer safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcpy operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strcpy(cfs_char_t *dest, const cfs_char_t *src,
                                       cfs_char_t **out);
/**
 * \brief Copies up to n characters of a string into a destination buffer.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param n The maximum number of characters to copy.
 * \param out Pointer to store the result of the strncpy operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strncpy(cfs_char_t *dest, const cfs_char_t *src,
                                        cfs_size_t n, cfs_char_t **out);
/**
 * \brief Concatenates two strings safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcat operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strcat(cfs_char_t *dest, const cfs_char_t *src,
                                       cfs_char_t **out);
/**
 * \brief Compares two strings lexicographically.
 *
 * \param lhs The left-hand side string for comparison.
 * \param rhs The right-hand side string for comparison.
 * \param out Pointer to store the result of the strcmp operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strcmp(const cfs_char_t *lhs,
                                       const cfs_char_t *rhs, int *out);
/**
 * \brief Compares up to a specified count of characters of two strings
 * lexicographically.
 *
 * \param lhs Argument representing the target resource.
 * \param rhs Argument representing the target resource.
 * \param count The maximum number of characters to compare.
 * \param out Pointer to store the result of the strncmp operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strncmp(const cfs_char_t *lhs,
                                        const cfs_char_t *rhs, cfs_size_t count,
                                        int *out);

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
#define CFS_STRCPY_SAFE(dst, dst_sz, src) (void)cfs_strcpy((dst), (src), NULL)
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  (void)cfs_strncpy((dst), (src), (n), NULL)
#endif
#if defined(CFS_OS_WINDOWS)
/* UTF-8 to UTF-16 conversion (Returns required buffer size in chars if dest is
 * NULL) */
/**
 * \brief Converts a UTF-8 string to a UTF-16 wide string.
 *
 * \param utf8_str Null-terminated UTF-8 source string.
 * \param dest Pointer to the wide character destination buffer.
 * \param dest_len Capacity of the destination buffer in wide characters.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_utf8_to_utf16(const char *utf8_str,
                                              wchar_t *dest,
                                              cfs_size_t dest_len,
                                              cfs_size_t *out_req);
/* UTF-16 to UTF-8 conversion (Returns required buffer size in bytes if dest is
 * NULL) */
/**
 * \brief Converts a UTF-16 wide string to a UTF-8 string.
 *
 * \param utf16_str Null-terminated UTF-16 source wide string.
 * \param dest Pointer to the UTF-8 destination buffer.
 * \param dest_len Capacity of the destination buffer in bytes.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_utf16_to_utf8(const wchar_t *utf16_str,
                                              char *dest, cfs_size_t dest_len,
                                              cfs_size_t *out_req);
#endif

/** \brief ANSI to Wide character conversion (Generic Fallbacks) */
/**
 * \brief Converts a multi-byte string to a wide character string.
 *
 * \param mb_str Null-terminated multi-byte source string.
 * \param dest Pointer to the destination buffer or path.
 * \param dest_len Capacity of the destination buffer.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_mb_to_wide(const char *mb_str, wchar_t *dest,
                                           cfs_size_t dest_len,
                                           cfs_size_t *out_req);
/**
 * \brief Converts a wide character string to a multi-byte string.
 *
 * \param wide_str Null-terminated wide source string.
 * \param dest Pointer to the destination buffer or path.
 * \param dest_len Capacity of the destination buffer.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_wide_to_mb(const wchar_t *wide_str, char *dest,
                                           cfs_size_t dest_len,
                                           cfs_size_t *out_req);
/* Phase 7: Error Handling & System Codes */

/* 62. Define cfs_errc mapping to std::errc (POSIX states) */
/**
 * \brief Standard POSIX error codes mirroring std::errc definitions.
 */

/* 61. Define cfs_error_code */
/**
 * \brief Represents a unified OS-agnostic error state with both raw and POSIX
 * mappings.
 */
typedef struct cfs_error_code {
  /** \brief The raw OS specific error code (errno or (int)GetLastError()). */
  int value;
  /** \brief The unified POSIX mapping. */
  cfs_errc errc;
} cfs_error_code;

/** \brief 63-64, 68. Global / Thread-Local Error Interfacing */
/**
 * \brief Manually populates a `cfs_error_code` structure with an OS and POSIX
 * error code.
 *
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \param os_value The raw system error code (errno, GetLastError).
 * \param standard_value The normalized POSIX cfs_errc mapping.
 */
CFS_API void cfs_set_error(cfs_error_code *ec, int os_value,
                           cfs_errc standard_value);
/**
 * \brief Resets a `cfs_error_code` structure to a success state.
 *
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
CFS_API void cfs_clear_error(cfs_error_code *ec);
/**
 * \brief Retrieves a human-readable string representation of a POSIX error
 * code.
 *
 * \param err The POSIX standard error mapping to evaluate.
 * \param out Pointer to store the result of the error_message operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_error_message(cfs_errc err, const char **out);

/** \brief 65-67. OS Translation Hooks */
/**
 * \brief Populates an error structure based on a raw OS error code,
 * automatically mapping to the POSIX enum.
 *
 * \param os_error The raw system error code.
 * \param out Pointer to store the result of the make_error_code_from_os
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_make_error_code_from_os(int os_error,
                                                        cfs_error_code *out);
/**
 * \brief Retrieves the last thread-local system error (errno or GetLastError)
 * and wraps it into a `cfs_error_code`.
 *
 * \param out Pointer to store the result of the get_last_error operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_get_last_error(cfs_error_code *out);
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
/**
 * \brief Opaque representation of a filesystem path with dynamic allocation
 * scaling.
 */
typedef struct cfs_path {
  /** \brief Pointer to the dynamically allocated string buffer. */
  cfs_char_t *str;
  /** \brief Number of meaningful characters stored in the buffer (excluding
   * null-terminator). */
  cfs_size_t length;
  /** \brief Total allocated byte capacity of the string buffer. */
  cfs_size_t capacity;
} cfs_path;

/** \brief 72-75, 77-79. Path Initialization, Mutation, and Destruction */
/**
 * \brief Initializes an empty `cfs_path` structure.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_init(cfs_path *p);
/**
 * \brief Initializes a `cfs_path` structure using a provided string source.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The null-terminated string to initialize the path with.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_init_str(cfs_path *p,
                                              const cfs_char_t *source);
/**
 * \brief Destroys a `cfs_path` structure, releasing its allocated memory.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_destroy(cfs_path *p);
/**
 * \brief Creates a deep copy of a `cfs_path` structure.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_clone(cfs_path *dest, const cfs_path *src);
/**
 * \brief Mutates the path in-place, converting all directory separators to the
 * native OS preferred separator.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_make_preferred(cfs_path *p);
/**
 * \brief Retrieves the underlying null-terminated C string from a path.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_c_str operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_c_str(const cfs_path *p,
                                           const cfs_char_t **out);
/* Returns dynamically allocated generic path string (e.g. forward slashes on
 * Windows). Caller must free. */
/**
 * \brief Returns a dynamically allocated string using generic (forward slash)
 * directory separators.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_generic_string operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_generic_string(const cfs_path *p,
                                                    cfs_char_t **out);
/* Phase 9: Path Building */

/** \brief 81-84, 88. Path assignment, concatenation, and manipulation */
/**
 * \brief Assigns a new string source to an existing `cfs_path` structure,
 * clearing the previous contents.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The new string to assign to the path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_assign(cfs_path *p,
                                            const cfs_char_t *source);
/**
 * \brief Appends a source string to the path, resolving directory separators
 * intelligently.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to append to the path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_append(cfs_path *p,
                                            const cfs_char_t *source);
/**
 * \brief Concatenates a string directly to the end of the path without
 * inserting separators.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to concatenate.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_concat(cfs_path *p,
                                            const cfs_char_t *source);
/**
 * \brief Clears the contents of the path, rendering it empty without freeing
 * its capacity.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_clear(cfs_path *p);
/**
 * \brief Swaps the contents and capacities of two `cfs_path` structures.
 *
 * \param lhs The left-hand side path to swap.
 * \param rhs The right-hand side path to swap.
 */
CFS_API void cfs_path_swap(cfs_path *lhs, cfs_path *rhs);
/* Phase 10: Path Decomposition - Root Analysis */

/** \brief Extracts drive letters (Windows) or root nodes. Returns path
 * instance. */
/**
 * \brief Performs the cfs_path_root_name filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_name operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_root_name(const cfs_path *p,
                                               cfs_path *out);
/** \brief Extracts base root separator. Returns path instance. */
/**
 * \brief Performs the cfs_path_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_root_directory(const cfs_path *p,
                                                    cfs_path *out);
/** \brief Combines root name and root directory. Returns path instance. */
/**
 * \brief Performs the cfs_path_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_root_path(const cfs_path *p,
                                               cfs_path *out);
/* Phase 11: Path Decomposition - Elements */

/** \brief 101. Returns path relative to the root path */
/**
 * \brief Performs the cfs_path_relative_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_relative_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_relative_path(const cfs_path *p,
                                                   cfs_path *out);
/** \brief 102. Returns path of the parent directory */
/**
 * \brief Performs the cfs_path_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_parent_path(const cfs_path *p,
                                                 cfs_path *out);
/** \brief 103. Returns the filename component */
/**
 * \brief Performs the cfs_path_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_filename(const cfs_path *p, cfs_path *out);
/** \brief 104. Returns the stem (filename without extension) */
/**
 * \brief Performs the cfs_path_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_stem(const cfs_path *p, cfs_path *out);
/** \brief 105. Returns the file extension */
/**
 * \brief Performs the cfs_path_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_extension(const cfs_path *p,
                                               cfs_path *out);
/* Phase 12: Path Modifiers */

/** \brief 111. Replaces the terminal filename component */
/**
 * \brief Performs the cfs_path_replace_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param replacement Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_path_replace_filename(cfs_path *p, const cfs_char_t *replacement);
/** \brief 112. Replaces the extension of the terminal component */
/**
 * \brief Performs the cfs_path_replace_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param replacement Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_path_replace_extension(cfs_path *p, const cfs_char_t *replacement);
/** \brief 113. Removes the terminal filename component (truncates back to
 * parent) */
/**
 * \brief Performs the cfs_path_remove_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_remove_filename(cfs_path *p);
/** \brief 114. Returns absolute path */
/**
 * \brief Performs the cfs_absolute filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the absolute operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_absolute(const cfs_path *p, cfs_path *out,
                                         cfs_error_code *ec);

/* Copy file options mirroring std::filesystem::copy_options */
/**
 * \brief Data structure for cfs_copy_options.
 */
typedef enum cfs_copy_options {
  /** \brief Represents the cfs_copy_options_none enumerator or field data. */
  cfs_copy_options_none = 0,
  /** \brief Represents the cfs_copy_options_skip_existing enumerator or field
     data. */
  cfs_copy_options_skip_existing = 1,
  /** \brief Represents the cfs_copy_options_overwrite_existing enumerator or
     field data. */
  cfs_copy_options_overwrite_existing = 2,
  /** \brief Update existing files. */ cfs_copy_options_update_existing = 4
} cfs_copy_options;

/** \brief Phase 20: Missing std::filesystem functions */
/**
 * \brief Performs the cfs_canonical filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the canonical operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_canonical(const cfs_path *p, cfs_path *out,
                                          cfs_error_code *ec);
/**
 * \brief Performs the cfs_weakly_canonical filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the weakly_canonical operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_weakly_canonical(const cfs_path *p,
                                                 cfs_path *out,
                                                 cfs_error_code *ec);
/**
 * \brief Performs the cfs_read_symlink filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the read_symlink operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_read_symlink(const cfs_path *p, cfs_path *out,
                                             cfs_error_code *ec);
/**
 * \brief Performs the cfs_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the relative operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_relative(const cfs_path *p,
                                         const cfs_path *base, cfs_path *out,
                                         cfs_error_code *ec);
/**
 * \brief Performs the cfs_proximate filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the proximate operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_proximate(const cfs_path *p,
                                          const cfs_path *base, cfs_path *out,
                                          cfs_error_code *ec);
/**
 * \brief Performs the cfs_copy filesystem operation.
 *
 * \param from Argument representing the target resource.
 * \param to Argument representing the target resource.
 * \param options Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_copy(const cfs_path *from, const cfs_path *to,
                                     cfs_copy_options options,
                                     cfs_error_code *ec);
/**
 * \brief Performs the cfs_copy_symlink filesystem operation.
 *
 * \param existing_symlink Argument representing the target resource.
 * \param new_symlink Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_copy_symlink(const cfs_path *existing_symlink,
                                             const cfs_path *new_symlink,
                                             cfs_error_code *ec);

/* Phase 13: Path Observers & Comparisons */

/** \brief Observers */
/**
 * \brief Checks if the path is entirely empty.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_empty operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_is_empty(const cfs_path *p, cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_root_path(const cfs_path *p,
                                                   cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_root_name filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_name operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_root_name(const cfs_path *p,
                                                   cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_directory
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_root_directory(const cfs_path *p,
                                                        cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_relative_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_relative_path
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_relative_path(const cfs_path *p,
                                                       cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_parent_path(const cfs_path *p,
                                                     cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_filename(const cfs_path *p,
                                                  cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_stem(const cfs_path *p, cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_extension(const cfs_path *p,
                                                   cfs_bool *out);
/**
 * \brief Performs the cfs_path_is_absolute filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_absolute operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_is_absolute(const cfs_path *p,
                                                 cfs_bool *out);
/**
 * \brief Performs the cfs_path_is_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_relative operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_is_relative(const cfs_path *p,
                                                 cfs_bool *out);

/** \brief Lexicographical comparison */
/**
 * \brief Performs the cfs_path_compare filesystem operation.
 *
 * \param lhs Argument representing the target resource.
 * \param rhs Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_compare(const cfs_path *lhs,
                                             const cfs_path *rhs);
/* Phase 14: Lexical Path Operations */

/** \brief 132. Lexically normalizes the path (resolves . and .. internally) */
/**
 * \brief Performs the cfs_path_lexically_normal filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_lexically_normal
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_lexically_normal(const cfs_path *p,
                                                      cfs_path *out);
/** \brief 134. Returns a path representing how to get from base to p */
/**
 * \brief Performs the cfs_path_lexically_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the path_lexically_relative
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_lexically_relative(const cfs_path *p,
                                                        const cfs_path *base,
                                                        cfs_path *out);
/* 135. Returns relative path if mathematically divergent, otherwise the
 * original path */
/**
 * \brief Performs the cfs_path_lexically_proximate filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the path_lexically_proximate
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_lexically_proximate(const cfs_path *p,
                                                         const cfs_path *base,
                                                         cfs_path *out);

/* Internal path element iterator structure */
/**
 * \brief Data structure for cfs_path_element.
 */
typedef struct cfs_path_element {
  /** \brief Pointer to the dynamically allocated string buffer. */
  const cfs_char_t *str;
  /** \brief Number of meaningful characters stored in the buffer (excluding
   * null-terminator). */
  cfs_size_t length;
} cfs_path_element;

/**
 * \brief Data structure for cfs_path_iterator.
 */
typedef struct cfs_path_iterator {
  /** \brief Represents the path_str enumerator or field data. */
  const cfs_char_t *path_str;
  /** \brief Represents the path_len enumerator or field data. */
  cfs_size_t path_len;
  /** \brief Represents the current_pos enumerator or field data. */
  cfs_size_t current_pos;
} cfs_path_iterator;
/* Phase 15 & 16: Filesystem Information & Status */

/* 144. Define file types matching std::filesystem::file_type */
/**
 * \brief Represents the specific node type of a file (e.g. regular, directory,
 * socket).
 */
typedef enum cfs_file_type {
  /** \brief Represents the cfs_file_type_none enumerator or field data. */
  cfs_file_type_none = 0,
  /** \brief Represents the cfs_file_type_not_found enumerator or field data. */
  cfs_file_type_not_found = -1,
  /** \brief Represents the cfs_file_type_regular enumerator or field data. */
  cfs_file_type_regular = 1,
  /** \brief Represents the cfs_file_type_directory enumerator or field data. */
  cfs_file_type_directory = 2,
  /** \brief Represents the cfs_file_type_symlink enumerator or field data. */
  cfs_file_type_symlink = 3,
  /** \brief Represents the cfs_file_type_block enumerator or field data. */
  cfs_file_type_block = 4,
  /** \brief Represents the cfs_file_type_character enumerator or field data. */
  cfs_file_type_character = 5,
  /** \brief Represents the cfs_file_type_fifo enumerator or field data. */
  cfs_file_type_fifo = 6,
  /** \brief Represents the cfs_file_type_socket enumerator or field data. */
  cfs_file_type_socket = 7,
  /** \brief Unknown file type. */ cfs_file_type_unknown = 8
} cfs_file_type;

/** \brief OS-agnostic permissions (matches std::filesystem::perms) */
typedef unsigned int cfs_perms;

/**
 * \brief Data structure for cfs_perm_options.
 */
typedef enum cfs_perm_options {
  /** \brief Represents the cfs_perm_options_replace enumerator or field data.
   */
  cfs_perm_options_replace = 1,
  /** \brief Represents the cfs_perm_options_add enumerator or field data. */
  cfs_perm_options_add = 2,
  /** \brief Represents the cfs_perm_options_remove enumerator or field data. */
  cfs_perm_options_remove = 4,
  /** \brief Do not follow symlinks. */ cfs_perm_options_nofollow = 8
} cfs_perm_options;

/**
 * \brief Data structure for cfs_directory_options.
 */
typedef enum cfs_directory_options {
  /** \brief Represents the cfs_directory_options_none enumerator or field data.
   */
  cfs_directory_options_none = 0,
  /** \brief Represents the cfs_directory_options_follow_directory_symlink
     enumerator or field data. */
  cfs_directory_options_follow_directory_symlink = 1,
  /** \brief Skip directories if permission is denied. */
  cfs_directory_options_skip_permission_denied = 2
} cfs_directory_options;

/* Status struct holding retrieved OS attributes */
/**
 * \brief Data structure for cfs_file_status.
 */
typedef struct cfs_file_status {
  /** \brief Represents the type enumerator or field data. */
  cfs_file_type type;
  /** \brief Represents the permissions enumerator or field data. */
  cfs_perms permissions;
} cfs_file_status;

/** \brief 141-142. Core Status Queries */
/**
 * \brief Evaluates the file status and type of the given path, traversing
 * symlinks.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the status operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_status(const cfs_path *p, cfs_file_status *out,
                                       cfs_error_code *ec);
/**
 * \brief Evaluates the file status and type of the given path without
 * traversing symlinks.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the symlink_status operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_symlink_status(const cfs_path *p,
                                               cfs_file_status *out,
                                               cfs_error_code *ec);
/**
 * \brief Performs the cfs_status_known filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the status_known operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_status_known(cfs_file_status s, cfs_bool *out);

/** \brief 145. Exists Observer */
/**
 * \brief Evaluates if a given file status indicates an existing filesystem
 * node.
 *
 * \param s The file status struct to evaluate.
 * \param out Pointer to store the result of the exists operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_exists(cfs_file_status s, cfs_bool *out);
/**
 * \brief Checks if a given path corresponds to an existing filesystem node.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the exists_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_exists_path(const cfs_path *p, cfs_bool *out,
                                            cfs_error_code *ec);

/** \brief 151-159. Filesystem Type Queries */
/**
 * \brief Performs the cfs_is_block_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_block_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_block_file(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_character_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_character_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_character_file(cfs_file_status s,
                                                  cfs_bool *out);
/**
 * \brief Performs the cfs_is_directory filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_directory(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_fifo filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_fifo operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_fifo(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_other filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_other operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_other(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_regular_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_regular_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_regular_file(cfs_file_status s,
                                                cfs_bool *out);
/**
 * \brief Performs the cfs_is_socket filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_socket operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_socket(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_symlink filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_symlink operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_symlink(cfs_file_status s, cfs_bool *out);

/** \brief 154. Is Empty Query (Directory or zero-byte file) */
/**
 * \brief Performs the cfs_is_empty_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the is_empty_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_empty_path(const cfs_path *p, cfs_bool *out,
                                              cfs_error_code *ec);
/* Phase 17: Filesystem Operations - Creation */

/** \brief 161. Create a single directory node */
/**
 * \brief Creates a single new directory node at the given path.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_directory(const cfs_path *p,
                                                 cfs_error_code *ec);
/** \brief 162. Recursively create directory nodes */
/**
 * \brief Recursively creates a directory and any missing parent directories.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_directories(const cfs_path *p,
                                                   cfs_error_code *ec);

/** \brief 163-165. Create links */
/**
 * \brief Performs the cfs_create_hard_link filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_hard_link(const cfs_path *target,
                                                 const cfs_path *link,
                                                 cfs_error_code *ec);
/**
 * \brief Performs the cfs_create_symlink filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_symlink(const cfs_path *target,
                                               const cfs_path *link,
                                               cfs_error_code *ec);
/**
 * \brief Performs the cfs_create_directory_symlink filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_directory_symlink(const cfs_path *target,
                                                         const cfs_path *link,
                                                         cfs_error_code *ec);

/* Copy file options mirroring std::filesystem::copy_options */

/** \brief 168. Copy files strictly */
/**
 * \brief Copies a single file from a source path to a destination path.
 *
 * \param from The source path pointing to the file to copy.
 * \param to The destination path for the copied file.
 * \param options Copy behavior flags mapping to `cfs_copy_options`.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_copy_file(const cfs_path *from,
                                          const cfs_path *to,
                                          cfs_copy_options options,
                                          cfs_error_code *ec);
/* Phase 18: Filesystem Operations - Modification */

/** \brief 171. Remove single file or empty directory */
/**
 * \brief Deletes a specific file or empty directory node.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_remove(const cfs_path *p, cfs_error_code *ec);
/** \brief 172. Remove all contents recursively. Returns number of removed
 * objects */
/**
 * \brief Recursively deletes a directory node and all of its nested contents.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the remove_all operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_remove_all(const cfs_path *p, cfs_size_t *out,
                                           cfs_error_code *ec);

/** \brief 173. Rename/Move node */
/**
 * \brief Performs the cfs_rename filesystem operation.
 *
 * \param old_p Argument representing the target resource.
 * \param new_p Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_rename(const cfs_path *old_p,
                                       const cfs_path *new_p,
                                       cfs_error_code *ec);

/** \brief Defined as large integer matching std::uintmax_t */
#if defined(__GNUC__) || defined(__clang__)
__extension__ typedef unsigned long long cfs_uintmax_t;
#elif defined(_MSC_VER)
typedef unsigned __int64 cfs_uintmax_t;
#else
typedef unsigned long cfs_uintmax_t;
#endif
/**
 * \brief Performs the cfs_resize_file filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param size Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_resize_file(const cfs_path *p,
                                            cfs_uintmax_t size,
                                            cfs_error_code *ec);
/**
 * \brief Retrieves the size of a given file in bytes.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the file_size operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_file_size(const cfs_path *p, cfs_uintmax_t *out,
                                          cfs_error_code *ec);

/* 176. Space Information */
/**
 * \brief Data structure for cfs_space_info.
 */
typedef struct cfs_space_info {
  /** \brief Total allocated byte capacity of the string buffer. */
  cfs_uintmax_t capacity;
  /** \brief Represents the free enumerator or field data. */
  cfs_uintmax_t free;
  /** \brief Represents the available enumerator or field data. */
  cfs_uintmax_t available;
} cfs_space_info;
/**
 * \brief Performs the cfs_space filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the space operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_space(const cfs_path *p, cfs_space_info *out,
                                      cfs_error_code *ec);

/** \brief Mapped natively to time_t or system tick representation. */
#if defined(__GNUC__) || defined(__clang__)
__extension__ typedef long long cfs_file_time_type;
#elif defined(_MSC_VER)
typedef __int64 cfs_file_time_type;
#else
typedef long cfs_file_time_type;
#endif
/**
 * \brief Performs the cfs_last_write_time filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the last_write_time operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_last_write_time(const cfs_path *p,
                                                cfs_file_time_type *out,
                                                cfs_error_code *ec);

/** \brief 17X. Permissions and Links */
/**
 * \brief Performs the cfs_permissions filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param prms Argument representing the target resource.
 * \param opts Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_permissions(const cfs_path *p, cfs_perms prms,
                                            cfs_perm_options opts,
                                            cfs_error_code *ec);
/**
 * \brief Performs the cfs_hard_link_count filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the hard_link_count operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_hard_link_count(const cfs_path *p,
                                                cfs_uintmax_t *out,
                                                cfs_error_code *ec);
/**
 * \brief Performs the cfs_equivalent filesystem operation.
 *
 * \param p1 Specific input argument required for the operation.
 * \param p2 Specific input argument required for the operation.
 * \param out Pointer to store the result of the equivalent operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_equivalent(const cfs_path *p1,
                                           const cfs_path *p2, cfs_bool *out,
                                           cfs_error_code *ec);

/** \brief 178-179. Environment paths */
/**
 * \brief Performs the cfs_current_path filesystem operation.
 *
 * \param out Pointer to store the result of the current_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_current_path(cfs_path *out, cfs_error_code *ec);
/**
 * \brief Performs the cfs_current_path_set filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_current_path_set(const cfs_path *p,
                                                 cfs_error_code *ec);
/**
 * \brief Performs the cfs_temp_directory_path filesystem operation.
 *
 * \param out Pointer to store the result of the temp_directory_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_temp_directory_path(cfs_path *out,
                                                    cfs_error_code *ec);
/* Phase 19: Directory Iteration */

/* 181. Directory entry structure caching path and status */
/**
 * \brief Data structure for cfs_directory_entry.
 */
typedef struct cfs_directory_entry {
  /** \brief Represents the path enumerator or field data. */
  cfs_path path;
  /** \brief Represents the status enumerator or field data. */
  cfs_file_status status;
  /** \brief Represents the symlink_status enumerator or field data. */
  cfs_file_status symlink_status;
} cfs_directory_entry;

/* 182-185. Standard Directory Iterator */
typedef struct cfs_directory_iterator cfs_directory_iterator;

/**
 * \brief Performs the cfs_dir_itr_init filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out_it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init(const cfs_path *p,
                                             cfs_directory_iterator **out_it,
                                             cfs_error_code *ec);
/* Returns 0 on success, with a pointer to the internal entry, or 1 if iteration
 * complete, or -1 on error */
/**
 * \brief Performs the cfs_dir_itr_next filesystem operation.
 *
 * \param it Argument representing the target resource.
 * \param out_entry Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_dir_itr_next(cfs_directory_iterator *it,
                 const cfs_directory_entry **out_entry, cfs_error_code *ec);
/**
 * \brief Performs the cfs_dir_itr_close filesystem operation.
 *
 * \param it Argument representing the target resource.
 */
CFS_API void cfs_dir_itr_close(cfs_directory_iterator *it);

/* 186-189. Recursive Directory Iterator */
typedef struct cfs_recursive_directory_iterator
    cfs_recursive_directory_iterator;

/**
 * \brief Performs the cfs_rec_dir_itr_init filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out_it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_init(
    const cfs_path *p, cfs_recursive_directory_iterator **out_it,
    cfs_error_code *ec);
/**
 * \brief Performs the cfs_rec_dir_itr_next filesystem operation.
 *
 * \param it Argument representing the target resource.
 * \param out_entry Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_rec_dir_itr_next(cfs_recursive_directory_iterator *it,
                     const cfs_directory_entry **out_entry, cfs_error_code *ec);
/**
 * \brief Prevents the iterator from descending into the currently evaluated
 * directory node.
 *
 * \param it Pointer to the recursive directory iterator to modify.
 */
CFS_API void
cfs_rec_dir_itr_disable_recursion_pending(cfs_recursive_directory_iterator *it);
/**
 * \brief Moves the iterator one level up in the directory tree.
 *
 * \param it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc
cfs_rec_dir_itr_pop(cfs_recursive_directory_iterator *it, cfs_error_code *ec);
/**
 * \brief Performs the cfs_rec_dir_itr_close filesystem operation.
 *
 * \param it Argument representing the target resource.
 */
CFS_API void cfs_rec_dir_itr_close(cfs_recursive_directory_iterator *it);

/* Phase 5.5: Execution Context & Modality */

/* 1. Define the cfs_modality enum */
/**
 * \brief Defines the execution strategy for asynchronous requests (e.g. sync,
 * threadpool, multiprocess).
 */
typedef enum cfs_modality {
  /** \brief Represents the cfs_modality_sync enumerator or field data. */
  cfs_modality_sync,
  /** \brief Represents the cfs_modality_async enumerator or field data. */
  cfs_modality_async,
  /** \brief Represents the cfs_modality_multithread enumerator or field data.
   */
  cfs_modality_multithread,
  /** \brief Represents the cfs_modality_singlethread enumerator or field data.
   */
  cfs_modality_singlethread,
  /** \brief Represents the cfs_modality_multiprocess enumerator or field data.
   */
  cfs_modality_multiprocess,
  /** \brief Represents the cfs_modality_greenthread enumerator or field data.
   */
  cfs_modality_greenthread,
  /** \brief Message passing modality. */ cfs_modality_message_passing
} cfs_modality;

/* 2. Define cfs_runtime_config struct */
/**
 * \brief Configuration payload defining the modality and thread constraints of
 * the runtime.
 */
typedef struct cfs_runtime_config {
  /** \brief Represents the mode enumerator or field data. */
  cfs_modality mode;
  /** \brief Represents the thread_pool_size enumerator or field data. */
  cfs_size_t thread_pool_size;
  /** \brief Represents the ipc_path enumerator or field data. */
  const cfs_char_t *ipc_path;
} cfs_runtime_config;

/* 3. Create the opaque cfs_runtime_t struct */
typedef struct cfs_runtime_t cfs_runtime_t;

/* 6. Introduce cfs_request_t struct */
typedef struct cfs_request_t cfs_request_t;

/** \brief Unified callback function pointer type for async operations. */
typedef void (*cfs_callback_t)(cfs_request_t *req, void *user_data);

/* The cfs_request_t struct represents an abstract file system operation */
/**
 * \brief Abstract payload representing a deferred filesystem operation.
 */
struct cfs_request_t {
  /** \brief Identifier indicating the specific file operation to perform. */
  int opcode;
  /** \brief Primary path parameter for the operation. */
  cfs_path target_path;
  /** \brief Secondary path parameter for copy or rename operations. */
  cfs_path dest_path;
  /** \brief Dynamically allocated buffer to hold operation results. */
  void *result_buffer;
  /** \brief Byte size of the allocated result buffer. */
  cfs_size_t result_size;
  /** \brief Structure to capture any error generated during the operation. */
  cfs_error_code error;
  /** \brief User-defined function pointer triggered upon operation completion.
   */
  cfs_callback_t callback;
  /** \brief Opaque context pointer passed to the callback function. */
  void *user_data;
  /** \brief Pointer to the next request object in a linked list or queue. */
  struct cfs_request_t *next;
  /** \brief Thread-safe counter tracking active references to this request. */
  int ref_count;
  /** \brief Boolean flag marking the request for early termination. */
  cfs_bool cancelled;
};

/** \brief 4. Implement cfs_runtime_init() */
/**
 * \brief Performs the cfs_runtime_init filesystem operation.
 *
 * \param config Argument representing the target resource.
 * \param out_rt Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_runtime_init(const cfs_runtime_config *config,
                                             cfs_runtime_t **out_rt,
                                             cfs_error_code *ec);

/** \brief 5. Implement cfs_runtime_destroy() */
/**
 * \brief Performs the cfs_runtime_destroy filesystem operation.
 *
 * \param runtime Argument representing the target resource.
 */
CFS_API void cfs_runtime_destroy(cfs_runtime_t *runtime);

/** \brief 9. Create generic request dispatcher internal function */
/**
 * \brief Performs the cfs_dispatch_request filesystem operation.
 *
 * \param runtime Argument representing the target resource.
 * \param req Argument representing the target resource.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback.
 */
CFS_API void cfs_dispatch_request(cfs_runtime_t *runtime, cfs_request_t *req,
                                  cfs_callback_t cb, void *user_data);

/* 33. Implement CFS_IMPLEMENTATION macro pattern */
/* 40. Finalize the unified cross-platform header structure layout. */

/* Phase 5.6: Deferred Execution and Asynchronous Base */

/* 11. Opcodes */
/**
 * \brief Internal identifiers dictating the specific asynchronous operation to
 * execute.
 */
typedef enum cfs_opcode {
  /** \brief Represents the cfs_opcode_none enumerator or field data. */
  cfs_opcode_none = 0,
  /** \brief Represents the cfs_opcode_remove enumerator or field data. */
  cfs_opcode_remove,
  /** \brief Represents the cfs_opcode_remove_all enumerator or field data. */
  cfs_opcode_remove_all,
  /** \brief Represents the cfs_opcode_create_directory enumerator or field
     data. */
  cfs_opcode_create_directory,
  /** \brief Represents the cfs_opcode_create_directories enumerator or field
     data. */
  cfs_opcode_create_directories,
  /** \brief Represents the cfs_opcode_copy_file enumerator or field data. */
  cfs_opcode_copy_file,
  /** \brief Represents the cfs_opcode_rename enumerator or field data. */
  cfs_opcode_rename,
  /** \brief Represents the cfs_opcode_file_size enumerator or field data. */
  cfs_opcode_file_size,
  /** \brief Represents the cfs_opcode_status enumerator or field data. */
  cfs_opcode_status,
  /** \brief Represents the cfs_opcode_symlink_status enumerator or field data.
   */
  cfs_opcode_symlink_status,
  /** \brief Represents the cfs_opcode_exists enumerator or field data. */
  cfs_opcode_exists,
  /** \brief Represents the cfs_opcode_is_empty enumerator or field data. */
  cfs_opcode_is_empty,
  /** \brief Represents the cfs_opcode_space enumerator or field data. */
  cfs_opcode_space,
  /** \brief Last write time operation. */ cfs_opcode_last_write_time
} cfs_opcode;

/** \brief Forward declaration for internal mutex structure. */
typedef struct cfs_mutex_t cfs_mutex_t;
/** \brief Forward declaration for internal condition variable structure. */
typedef struct cfs_cond_t cfs_cond_t;
/** \brief Forward declaration for internal thread structure. */
typedef struct cfs_thread_t cfs_thread_t;

/** \brief Thread-Safe FIFO Queue. */
typedef struct cfs_queue_t cfs_queue_t;

/** \brief Thread Pool. */
typedef struct cfs_thread_pool_t cfs_thread_pool_t;

/** \brief 13. Non-blocking API variants */
/**
 * \brief Schedules an asynchronous operation to remove a file or empty
 * directory.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_remove_async(cfs_runtime_t *rt,
                                             const cfs_path *p,
                                             cfs_callback_t cb,
                                             void *user_data);
/**
 * \brief Schedules an asynchronous operation to retrieve a file\'s size.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_file_size_async(cfs_runtime_t *rt,
                                                const cfs_path *p,
                                                cfs_callback_t cb,
                                                void *user_data);

/** \brief 19. cfs_runtime_poll() */
/**
 * \brief Processes completed asynchronous requests and executes their
 * callbacks.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_runtime_poll(cfs_runtime_t *rt);

/* Phase 3: Platform-Specific Async & Message Passing */

/** \brief 29. Reference counting to cfs_request_t */
/**
 * \brief Performs the cfs_request_retain filesystem operation.
 *
 * \param req Argument representing the target resource.
 */
CFS_API void cfs_request_retain(cfs_request_t *req);
/**
 * \brief Performs the cfs_request_release filesystem operation.
 *
 * \param req Argument representing the target resource.
 */
CFS_API void cfs_request_release(cfs_request_t *req);

/** \brief 30. Implement cancellation logic */
/**
 * \brief Performs the cfs_cancel_request filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param req Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_cancel_request(cfs_runtime_t *rt,
                                               cfs_request_t *req);

/** \brief Forward declaration for io_uring context configuration. */
typedef struct cfs_io_uring_context cfs_io_uring_context;
/** \brief Forward declaration for IOCP context configuration. */
typedef struct cfs_iocp_context cfs_iocp_context;

/** \brief Forward declaration for message pipe. */
typedef struct cfs_message_pipe cfs_message_pipe;
/**
 * \brief Performs the cfs_message_pipe_create filesystem operation.
 *
 * \param path Argument representing the target resource.
 * \param out_pipe Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_message_pipe_create(const cfs_char_t *path, cfs_message_pipe **out_pipe);
/**
 * \brief Performs the cfs_message_pipe_destroy filesystem operation.
 *
 * \param pipe Argument representing the target resource.
 */
CFS_API void cfs_message_pipe_destroy(cfs_message_pipe *pipe);
/**
 * \brief Performs the cfs_serialize_request filesystem operation.
 *
 * \param req Argument representing the target resource.
 * \param buffer Argument representing the target resource.
 * \param size Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_serialize_request(const cfs_request_t *req,
                                                  void **buffer,
                                                  cfs_size_t *size);
/**
 * \brief Performs the cfs_deserialize_request filesystem operation.
 *
 * \param buffer Argument representing the target resource.
 * \param size Argument representing the target resource.
 * \param req Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_deserialize_request(const void *buffer,
                                                    cfs_size_t size,
                                                    cfs_request_t **req);

/* Phase 4: Multiprocessing and Greenthreads */

/* 31. Multiprocess modality backend (Process Handles) */
typedef struct cfs_process_t cfs_process_t;
/**
 * \brief Performs the cfs_process_spawn filesystem operation.
 *
 * \param executable Argument representing the target resource.
 * \param out_proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_process_spawn(const cfs_char_t *executable,
                                              cfs_process_t **out_proc);
/**
 * \brief Performs the cfs_process_wait filesystem operation.
 *
 * \param proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_process_wait(cfs_process_t *proc);
/**
 * \brief Performs the cfs_process_destroy filesystem operation.
 *
 * \param proc Argument representing the target resource.
 */
CFS_API void cfs_process_destroy(cfs_process_t *proc);

/* 32. Shared Memory (shm) segments */
typedef struct cfs_shm_segment cfs_shm_segment;
/**
 * \brief Performs the cfs_shm_create filesystem operation.
 *
 * \param size Argument representing the target resource.
 * \param name Argument representing the target resource.
 * \param out_shm Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_shm_create(cfs_size_t size,
                                           const cfs_char_t *name,
                                           cfs_shm_segment **out_shm);
/**
 * \brief Performs the cfs_shm_map filesystem operation.
 *
 * \param shm Argument representing the target resource.
 * \param out Pointer to store the result of the shm_map operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_shm_map(cfs_shm_segment *shm, void **out);
/**
 * \brief Performs the cfs_shm_unmap filesystem operation.
 *
 * \param shm Argument representing the target resource.
 * \param addr Argument representing the target resource.
 */
CFS_API void cfs_shm_unmap(cfs_shm_segment *shm, void *addr);
/**
 * \brief Performs the cfs_shm_destroy filesystem operation.
 *
 * \param shm Argument representing the target resource.
 */
CFS_API void cfs_shm_destroy(cfs_shm_segment *shm);

/* 33. Multiprocess semaphore */
typedef struct cfs_named_semaphore cfs_named_semaphore;
/**
 * \brief Performs the cfs_named_semaphore_create filesystem operation.
 *
 * \param name Argument representing the target resource.
 * \param initial_count Argument representing the target resource.
 * \param out_sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_create(
    const cfs_char_t *name, int initial_count, cfs_named_semaphore **out_sem);
/**
 * \brief Performs the cfs_named_semaphore_wait filesystem operation.
 *
 * \param sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_wait(cfs_named_semaphore *sem);
/**
 * \brief Performs the cfs_named_semaphore_post filesystem operation.
 *
 * \param sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_post(cfs_named_semaphore *sem);
/**
 * \brief Performs the cfs_named_semaphore_destroy filesystem operation.
 *
 * \param sem Argument representing the target resource.
 */
CFS_API void cfs_named_semaphore_destroy(cfs_named_semaphore *sem);

/** \brief Forward declaration for greenthread structure. */
typedef struct cfs_greenthread_t cfs_greenthread_t;
/** \brief Function pointer type for greenthread entry points. */
typedef void (*cfs_greenthread_func)(void *);

/**
 * \brief Performs the cfs_greenthread_spawn filesystem operation.
 *
 * \param func Argument representing the target resource.
 * \param arg Argument representing the target resource.
 * \param out_gt Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_greenthread_spawn(cfs_greenthread_func func,
                                                  void *arg,
                                                  cfs_greenthread_t **out_gt);
/**
 * \brief Performs the cfs_greenthread_yield filesystem operation.
 *
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_greenthread_yield(void);
/**
 * \brief Performs the cfs_greenthread_destroy filesystem operation.
 *
 * \param gt Argument representing the target resource.
 */
CFS_API void cfs_greenthread_destroy(cfs_greenthread_t *gt);

/* 36. Greenthread scheduler */
typedef struct cfs_greenthread_scheduler cfs_greenthread_scheduler;
/**
 * \brief Initializes a greenthread scheduler.
 *
 * \param out_sched Pointer to store the initialized scheduler.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_init(cfs_greenthread_scheduler **out_sched);
/**
 * \brief Performs the cfs_greenthread_scheduler_run filesystem operation.
 *
 * \param sched Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_run(cfs_greenthread_scheduler *sched);
/**
 * \brief Destroys a greenthread scheduler.
 *
 * \param sched Argument representing the target resource to destroy.
 */
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
       : (cfs_remove((path), NULL) ? cfs_errc_success                          \
                                   : cfs_errc_not_enough_memory))

/* 42. Integrate new context parameter into directory iterators */
typedef struct cfs_directory_iterator_async cfs_directory_iterator_async;
/**
 * \brief Performs the cfs_dir_itr_init_async filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init_async(cfs_runtime_t *rt,
                                                   const cfs_path *p,
                                                   cfs_callback_t cb,
                                                   void *user_data);

/* 43. Multi-process sandbox config */
/**
 * \brief Data structure for cfs_sandbox_config.
 */
typedef struct cfs_sandbox_config {
  /** \brief Represents the root_chroot enumerator or field data. */
  cfs_path root_chroot;
  /** \brief Represents the restrict_symlinks enumerator or field data. */
  cfs_bool restrict_symlinks;
} cfs_sandbox_config;

/**
 * \brief Performs the cfs_runtime_set_sandbox filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param config Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_runtime_set_sandbox(cfs_runtime_t *rt, const cfs_sandbox_config *config);

#ifdef CFS_IMPLEMENTATION

/* 38. Architectural #ifdef blocks delegating to MSVC native vs POSIX */
#if defined(CFS_OS_WINDOWS)
/* Windows native includes */
#elif defined(CFS_OS_LINUX) || defined(CFS_OS_MACOS) || defined(CFS_OS_BSD) || \
    defined(CFS_ENV_CYGWIN) || defined(CFS_OS_EMSCRIPTEN)
/* POSIX native includes */
#elif defined(CFS_OS_DOS)
/* DOS stub includes */
#else
/* Fallback includes */
#endif

/* Implementation details */

struct cfs_directory_iterator {
#if defined(CFS_OS_WINDOWS)
  int dummy;
#else
  DIR *dirp;
#endif
  cfs_directory_entry current;
  cfs_bool is_end;
};

struct cfs_recursive_directory_iterator {
  cfs_directory_iterator base;
};

/**
 * \brief Performs the cfs_is_separator filesystem operation.
 *
 * \param c Argument representing the target resource.
 * \param out Pointer to store the result of the is_separator operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
static cfs_errc cfs_is_separator(cfs_char_t c, cfs_bool *out);
/**
 * \brief Performs the cfs_path_reserve filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param new_cap Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
static cfs_errc cfs_path_reserve(cfs_path *p, cfs_size_t new_cap);

/* Phase 5.5: Execution Context & Modality Implementations */

/**
 * \brief The primary execution context handling thread pools, message passing,
 * and work queues.
 */
struct cfs_runtime_t {
  /** \brief Represents the config enumerator or field data. */
  cfs_runtime_config config;
  /** \brief Represents the work_queue enumerator or field data. */
  cfs_queue_t *work_queue;
  /** \brief Represents the completion_queue enumerator or field data. */
  cfs_queue_t *completion_queue;
  /** \brief Represents the thread_pool enumerator or field data. */
  cfs_thread_pool_t *thread_pool;
};

/* Phase 5.6: Deferred Execution and Asynchronous Base Implementations */

/* 12. Generic Opcode Execution */
/**
 * \brief Performs the cfs_execute_op_inline filesystem operation.
 *
 * \param req Argument representing the target resource.
 */
static void cfs_execute_op_inline(cfs_request_t *req) {

  switch (req->opcode) {
  case cfs_opcode_remove:
    (void)cfs_remove(&req->target_path, &req->error);
    break;
  case cfs_opcode_file_size: {
    cfs_uintmax_t size = 0;
    (void)cfs_file_size(&req->target_path, &size, &req->error);
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
/**
 * \brief Data structure for cfs_mutex_t.
 */
struct cfs_mutex_t {
  /** \brief Represents the cs enumerator or field data. */
  CRITICAL_SECTION cs;
};
/**
 * \brief Data structure for cfs_thread_t.
 */
struct cfs_thread_t {
  /** \brief Represents the h enumerator or field data. */
  HANDLE h;
};

#if defined(_MSC_VER) && _MSC_VER < 1500
/* MSVC 2005 fallback */
/**
 * \brief Data structure for cfs_cond_t.
 */
struct cfs_cond_t {
  /** \brief Represents the event enumerator or field data. */
  HANDLE event;
};
/**
 * \brief Performs the cfs_cond_init filesystem operation.
 *
 * \param c Argument representing the target resource.
 */
static void cfs_cond_init(cfs_cond_t *c) {
  c->event = CreateEvent(NULL, FALSE, FALSE, NULL);
}
/**
 * \brief Performs the cfs_cond_destroy filesystem operation.
 *
 * \param event Argument representing the target resource.
 */
static void cfs_cond_destroy(cfs_cond_t *c) { CloseHandle(c->event); }
/**
 * \brief Performs the cfs_cond_wait filesystem operation.
 *
 * \param c Argument representing the target resource.
 * \param m Argument representing the target resource.
 */
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  LeaveCriticalSection(&m->cs);
  WaitForSingleObject(c->event, INFINITE);
  EnterCriticalSection(&m->cs);
}
/**
 * \brief Performs the cfs_cond_signal filesystem operation.
 *
 * \param event Argument representing the target resource.
 */
static void cfs_cond_signal(cfs_cond_t *c) { SetEvent(c->event); }
/**
 * \brief Performs the cfs_cond_broadcast filesystem operation.
 *
 * \param event Argument representing the target resource.
 */
static void cfs_cond_broadcast(cfs_cond_t *c) { SetEvent(c->event); }
#else
/**
 * \brief Data structure for cfs_cond_t.
 */
struct cfs_cond_t {
  /** \brief Represents the cv enumerator or field data. */
  CONDITION_VARIABLE cv;
};
/**
 * \brief Performs the cfs_cond_init filesystem operation.
 *
 * \param c Argument representing the target resource.
 */
static void cfs_cond_init(cfs_cond_t *c) {
  InitializeConditionVariable(&c->cv);
}
/**
 * \brief Performs the cfs_cond_destroy filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_cond_destroy(cfs_cond_t *c) { (void)c; } /* No-op on Windows */
/**
 * \brief Performs the cfs_cond_wait filesystem operation.
 *
 * \param c Argument representing the target resource.
 * \param m Argument representing the target resource.
 */
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  SleepConditionVariableCS(&c->cv, &m->cs, INFINITE);
}
/**
 * \brief Performs the cfs_cond_signal filesystem operation.
 *
 * \param cv Argument representing the target resource.
 */
static void cfs_cond_signal(cfs_cond_t *c) { WakeConditionVariable(&c->cv); }
/**
 * \brief Performs the cfs_cond_broadcast filesystem operation.
 *
 * \param c Argument representing the target resource.
 */
static void cfs_cond_broadcast(cfs_cond_t *c) {
  WakeAllConditionVariable(&c->cv);
}
#endif

/**
 * \brief Performs the cfs_mutex_init filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_init(cfs_mutex_t *m) {
  InitializeCriticalSection(&m->cs);
}
/**
 * \brief Performs the cfs_mutex_destroy filesystem operation.
 *
 * \param cs Argument representing the target resource.
 */
static void cfs_mutex_destroy(cfs_mutex_t *m) { DeleteCriticalSection(&m->cs); }
/**
 * \brief Performs the cfs_mutex_lock filesystem operation.
 *
 * \param cs Argument representing the target resource.
 */
static void cfs_mutex_lock(cfs_mutex_t *m) { EnterCriticalSection(&m->cs); }
/**
 * \brief Performs the cfs_mutex_unlock filesystem operation.
 *
 * \param cs Argument representing the target resource.
 */
static void cfs_mutex_unlock(cfs_mutex_t *m) { LeaveCriticalSection(&m->cs); }
#elif defined(CFS_OS_DOS)
/**
 * \brief Data structure for cfs_mutex_t.
 */
struct cfs_mutex_t {
  /** \brief Represents the dummy enumerator or field data. */
  int dummy;
};
/**
 * \brief Data structure for cfs_cond_t.
 */
struct cfs_cond_t {
  /** \brief Represents the dummy enumerator or field data. */
  int dummy;
};
/**
 * \brief Data structure for cfs_thread_t.
 */
struct cfs_thread_t {
  /** \brief Represents the dummy enumerator or field data. */
  int dummy;
};

/**
 * \brief Performs the cfs_mutex_init filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_mutex_init(cfs_mutex_t *m) { (void)m; }
/**
 * \brief Performs the cfs_mutex_destroy filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_mutex_destroy(cfs_mutex_t *m) { (void)m; }
/**
 * \brief Performs the cfs_mutex_lock filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_mutex_lock(cfs_mutex_t *m) { (void)m; }
/**
 * \brief Performs the cfs_mutex_unlock filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_mutex_unlock(cfs_mutex_t *m) { (void)m; }

/**
 * \brief Performs the cfs_cond_init filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_cond_init(cfs_cond_t *c) { (void)c; }
/**
 * \brief Performs the cfs_cond_destroy filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_cond_destroy(cfs_cond_t *c) { (void)c; }
/**
 * \brief Performs the cfs_cond_wait filesystem operation.
 *
 * \param c Argument representing the target resource.
 * \param m Argument representing the target resource.
 */
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  (void)c;
  (void)m;
}
/**
 * \brief Performs the cfs_cond_signal filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_cond_signal(cfs_cond_t *c) { (void)c; }
/**
 * \brief Performs the cfs_cond_broadcast filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
static void cfs_cond_broadcast(cfs_cond_t *c) { (void)c; }
#else
/**
 * \brief Data structure for cfs_mutex_t.
 */
struct cfs_mutex_t {
  /** \brief Represents the m enumerator or field data. */
  pthread_mutex_t m;
};
/**
 * \brief Data structure for cfs_cond_t.
 */
struct cfs_cond_t {
  /** \brief Represents the c enumerator or field data. */
  pthread_cond_t c;
};
/**
 * \brief Data structure for cfs_thread_t.
 */
struct cfs_thread_t {
  /** \brief Represents the t enumerator or field data. */
  pthread_t t;
};

/**
 * \brief Performs the cfs_mutex_init filesystem operation.
 *
 * \param m Argument representing the target resource.
 * \param NULL Argument representing the target resource.
 */
static void cfs_mutex_init(cfs_mutex_t *m) { pthread_mutex_init(&m->m, NULL); }
/**
 * \brief Performs the cfs_mutex_destroy filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_destroy(cfs_mutex_t *m) { pthread_mutex_destroy(&m->m); }
/**
 * \brief Performs the cfs_mutex_lock filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_lock(cfs_mutex_t *m) { pthread_mutex_lock(&m->m); }
/**
 * \brief Performs the cfs_mutex_unlock filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_unlock(cfs_mutex_t *m) { pthread_mutex_unlock(&m->m); }

/**
 * \brief Performs the cfs_cond_init filesystem operation.
 *
 * \param c Argument representing the target resource.
 * \param NULL Argument representing the target resource.
 */
static void cfs_cond_init(cfs_cond_t *c) { pthread_cond_init(&c->c, NULL); }
/**
 * \brief Performs the cfs_cond_destroy filesystem operation.
 *
 * \param c Argument representing the target resource.
 */
static void cfs_cond_destroy(cfs_cond_t *c) { pthread_cond_destroy(&c->c); }
/**
 * \brief Performs the cfs_cond_wait filesystem operation.
 *
 * \param c Argument representing the target resource.
 * \param m Argument representing the target resource.
 */
static void cfs_cond_wait(cfs_cond_t *c, cfs_mutex_t *m) {
  pthread_cond_wait(&c->c, &m->m);
}
/**
 * \brief Performs the cfs_cond_signal filesystem operation.
 *
 * \param c Argument representing the target resource.
 */
static void cfs_cond_signal(cfs_cond_t *c) { pthread_cond_signal(&c->c); }
/**
 * \brief Performs the cfs_cond_broadcast filesystem operation.
 *
 * \param c Argument representing the target resource.
 */
static void cfs_cond_broadcast(cfs_cond_t *c) { pthread_cond_broadcast(&c->c); }
#endif

/* 15, 18. Thread-Safe FIFO Queue */
/**
 * \brief Data structure for cfs_queue_t.
 */
struct cfs_queue_t {
  /** \brief Represents the head enumerator or field data. */
  cfs_request_t *head;
  /** \brief Represents the tail enumerator or field data. */
  cfs_request_t *tail;
  /** \brief Represents the lock enumerator or field data. */
  cfs_mutex_t lock;
  /** \brief Represents the cond enumerator or field data. */
  cfs_cond_t cond;
  /** \brief Represents the shutdown enumerator or field data. */
  cfs_bool shutdown;
};

/**
 * \brief Performs the cfs_queue_init filesystem operation.
 *
 * \param q Argument representing the target resource.
 */
static void cfs_queue_init(cfs_queue_t *q) {
  q->head = NULL;
  q->tail = NULL;
  q->shutdown = cfs_false;
  (void)cfs_mutex_init(&q->lock);
  (void)cfs_cond_init(&q->cond);
}

/**
 * \brief Performs the cfs_queue_destroy filesystem operation.
 *
 * \param q Argument representing the target resource.
 */
static void cfs_queue_destroy(cfs_queue_t *q) {
  cfs_request_t *req;
  while (q->head) {
    req = q->head;
    q->head = req->next;
    (void)cfs_request_release(req);
  }
  (void)cfs_mutex_destroy(&q->lock);
  (void)cfs_cond_destroy(&q->cond);
}

/**
 * \brief Performs the cfs_queue_push filesystem operation.
 *
 * \param q Argument representing the target resource.
 * \param req Argument representing the target resource.
 */
static void cfs_queue_push(cfs_queue_t *q, cfs_request_t *req) {
  (void)cfs_mutex_lock(&q->lock);
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;
  if (q->tail) {
    q->tail->next = req;
  } else {
    q->head = req;
  }
  q->tail = req;
  (void)cfs_cond_signal(&q->cond);
  (void)cfs_mutex_unlock(&q->lock);
}

/**
 * \brief Performs the cfs_queue_pop filesystem operation.
 *
 * \param q Argument representing the target resource.
 * \param wait_for_data Argument representing the target resource.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
static cfs_errc cfs_queue_pop(cfs_queue_t *q, cfs_bool wait_for_data,
                              cfs_request_t **out_req) {
  cfs_request_t *req = NULL;
  *out_req = NULL;
  (void)cfs_mutex_lock(&q->lock);

  while (q->head == NULL && !q->shutdown && wait_for_data) {
    (void)cfs_cond_wait(&q->cond, &q->lock);
  }

  if (q->head != NULL) {
    req = q->head;
    q->head = req->next;
    if (q->head == NULL) {
      q->tail = NULL;
    }
  }

  (void)cfs_mutex_unlock(&q->lock);
  *out_req = req;
  return req ? cfs_errc_success : cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_queue_shutdown filesystem operation.
 *
 * \param q Argument representing the target resource.
 */
static void cfs_queue_shutdown(cfs_queue_t *q) {
  (void)cfs_mutex_lock(&q->lock);
  q->shutdown = cfs_true;
  (void)cfs_cond_broadcast(&q->cond);
  (void)cfs_mutex_unlock(&q->lock);
}

/* 16. Thread Pool */
/**
 * \brief Data structure for cfs_thread_pool_t.
 */
struct cfs_thread_pool_t {
  /** \brief Represents the threads enumerator or field data. */
  cfs_thread_t *threads;
  /** \brief Represents the num_threads enumerator or field data. */
  cfs_size_t num_threads;
  /** \brief Represents the work_queue enumerator or field data. */
  cfs_queue_t *work_queue;
  /** \brief Represents the completion_queue enumerator or field data. */
  cfs_queue_t *completion_queue;
};

/* 17. Worker Thread Loop */
#if defined(CFS_OS_WINDOWS)
/**
 * \brief Performs the cfs_worker_thread filesystem operation.
 *
 * \param arg Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
static DWORD WINAPI cfs_worker_thread(LPVOID arg) {
#elif defined(CFS_OS_DOS)
/**
 * \brief Performs the cfs_worker_thread filesystem operation.
 *
 * \param arg Argument representing the target resource.
 */
static void *cfs_worker_thread(void *arg) {
#else
/**
 * \brief Performs the cfs_worker_thread filesystem operation.
 *
 * \param arg Argument representing the target resource.
 */
static void *cfs_worker_thread(void *arg) {
#endif
  cfs_thread_pool_t *pool = (cfs_thread_pool_t *)arg;
  cfs_request_t *req = NULL;

  for (;;) {
    if (cfs_queue_pop(pool->work_queue, cfs_true, &req) != 0 || !req) {
      /* Cascade shutdown signal to wake up other waiting threads when using
       * event fallback */
      (void)cfs_cond_broadcast(&pool->work_queue->cond);
      break; /* Shutdown condition */
    }

    if (!req->cancelled) {
      (void)cfs_execute_op_inline(req);
    } else {
      (void)cfs_make_error_code_from_os(125,
                                        &req->error); /* ECANCELED fallback */
    }

    (void)cfs_queue_push(pool->completion_queue, req);
  }

#if defined(CFS_OS_WINDOWS)
  return cfs_errc_success;
#else
  return NULL;
#endif
}

/**
 * \brief Performs the cfs_thread_pool_create filesystem operation.
 *
 * \param num_threads Argument representing the target resource.
 * \param work Argument representing the target resource.
 * \param comp Argument representing the target resource.
 * \param out_pool Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
static cfs_errc cfs_thread_pool_create(cfs_size_t num_threads,
                                       cfs_queue_t *work, cfs_queue_t *comp,
                                       cfs_thread_pool_t **out_pool) {
  cfs_size_t i;
  cfs_thread_pool_t *pool = NULL;
  /* if (!out_pool) */
  /* return cfs_errc_not_enough_memory; */
  *out_pool = NULL;

  (void)cfs_calloc(1, sizeof(cfs_thread_pool_t), (void **)&pool);
  if (!pool)
    return cfs_errc_not_enough_memory;

  pool->num_threads = num_threads;
  pool->work_queue = work;
  pool->completion_queue = comp;
  (void)cfs_calloc(num_threads, sizeof(cfs_thread_t),
                   (void **)&(pool->threads));

  if (!pool->threads) {
    (void)cfs_free(pool);
    return cfs_errc_not_enough_memory;
  }
  memset(pool->threads, 0, num_threads * sizeof(cfs_thread_t));

  for (i = 0; i < num_threads; ++i) {
#if defined(CFS_OS_WINDOWS)
    pool->threads[i].h = NULL;
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
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_thread_pool_destroy filesystem operation.
 *
 * \param pool Argument representing the target resource.
 */
static void cfs_thread_pool_destroy(
#if defined(_MSC_VER) && _MSC_VER >= 1900
    _In_
#endif
    cfs_thread_pool_t *pool) {
  cfs_size_t i;

  if (!pool)
    return;

  (void)cfs_queue_shutdown(pool->work_queue);

  if (pool->threads && pool->num_threads > 0) {
    cfs_thread_t *threads = pool->threads;
    for (i = 0; i < pool->num_threads; ++i) {
#if defined(CFS_OS_WINDOWS)
      if (threads[i].h != NULL) {
        WaitForSingleObject(threads[i].h, INFINITE);
        CloseHandle(threads[i].h);
      }
#elif defined(CFS_OS_DOS)
      (void)pool;
      (void)i;
#else
      pthread_join(threads[i].t, NULL);
#endif
    }
    (void)cfs_free(pool->threads);
  }

  (void)cfs_free(pool);
}

/* 13. Non-blocking API variants */
/**
 * \brief Schedules an asynchronous operation to remove a file or empty
 * directory.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_remove_async(cfs_runtime_t *rt,
                                             const cfs_path *p,
                                             cfs_callback_t cb,
                                             void *user_data) {
  cfs_request_t *req;

  if (!rt || !p)
    return cfs_errc_not_enough_memory;

  (void)cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (!req)
    return cfs_errc_not_enough_memory;

  req->opcode = cfs_opcode_remove;
  (void)cfs_path_init(&req->target_path);
  (void)cfs_path_clone(&req->target_path, p);
  (void)cfs_path_init(&req->dest_path);
  req->result_buffer = NULL;
  req->result_size = 0;
  (void)cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;

  (void)cfs_dispatch_request(rt, req, cb, user_data);
  return cfs_errc_success;
}

/**
 * \brief Schedules an asynchronous operation to retrieve a file\'s size.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_file_size_async(cfs_runtime_t *rt,
                                                const cfs_path *p,
                                                cfs_callback_t cb,
                                                void *user_data) {
  cfs_request_t *req;

  if (!rt || !p)
    return cfs_errc_not_enough_memory;

  (void)cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (!req)
    return cfs_errc_not_enough_memory;

  req->opcode = cfs_opcode_file_size;
  (void)cfs_path_init(&req->target_path);
  (void)cfs_path_clone(&req->target_path, p);
  (void)cfs_path_init(&req->dest_path);

  req->result_size = sizeof(cfs_uintmax_t);
  (void)cfs_malloc(req->result_size, (void **)&req->result_buffer);
  (void)cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;

  (void)cfs_dispatch_request(rt, req, cb, user_data);
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_runtime_init filesystem operation.
 *
 * \param config Argument representing the target resource.
 * \param out_rt Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_runtime_init(const cfs_runtime_config *config,
                                             cfs_runtime_t **out_rt,
                                             cfs_error_code *ec) {
  cfs_runtime_t *rt;
  if (ec)
    (void)cfs_clear_error(ec);
  if (!out_rt) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_not_enough_memory;
  }
  *out_rt = NULL;

  if (!config) {
    if (ec)
      (void)cfs_make_error_code_from_os(22, ec); /* EINVAL fallback */
    return cfs_errc_not_enough_memory;
  }

  (void)cfs_malloc(sizeof(cfs_runtime_t), (void **)&rt);
  if (!rt) {
    if (ec)
      (void)cfs_make_error_code_from_os(12, ec); /* ENOMEM fallback */
    return cfs_errc_not_enough_memory;
  }

  rt->config = *config;
  rt->work_queue = NULL;
  rt->completion_queue = NULL;
  rt->thread_pool = NULL;

  /* TODO: initialize thread pools, IPC, etc based on config->mode */
  if (rt->config.mode == cfs_modality_async ||
      rt->config.mode == cfs_modality_multithread) {
    (void)cfs_malloc(sizeof(cfs_queue_t), (void **)&(rt->work_queue));
    (void)cfs_malloc(sizeof(cfs_queue_t), (void **)&(rt->completion_queue));
    if (!rt->work_queue || !rt->completion_queue) {
      if (rt->work_queue)
        (void)cfs_free(rt->work_queue);
      if (rt->completion_queue)
        (void)cfs_free(rt->completion_queue);
      (void)cfs_free(rt);
      return cfs_errc_not_enough_memory;
    }
    (void)cfs_queue_init(rt->work_queue);
    (void)cfs_queue_init(rt->completion_queue);

    (void)cfs_thread_pool_create(
        rt->config.thread_pool_size > 0 ? rt->config.thread_pool_size : 4,
        rt->work_queue, rt->completion_queue, &rt->thread_pool);
  }

  *out_rt = rt;
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_runtime_destroy filesystem operation.
 *
 * \param runtime Argument representing the target resource.
 */
CFS_API void cfs_runtime_destroy(cfs_runtime_t *runtime) {
  if (!runtime)
    return;

  if (runtime->thread_pool) {
    (void)cfs_thread_pool_destroy(runtime->thread_pool);
  }
  if (runtime->work_queue) {
    (void)cfs_queue_destroy(runtime->work_queue);
    (void)cfs_free(runtime->work_queue);
  }
  if (runtime->completion_queue) {
    (void)cfs_queue_destroy(runtime->completion_queue);
    (void)cfs_free(runtime->completion_queue);
  }

  (void)cfs_free(runtime);
}

/**
 * \brief Performs the cfs_dispatch_request filesystem operation.
 *
 * \param runtime Argument representing the target resource.
 * \param req Argument representing the target resource.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback.
 */
CFS_API void cfs_dispatch_request(cfs_runtime_t *runtime, cfs_request_t *req,
                                  cfs_callback_t cb, void *user_data) {
  if (!runtime || !req)
    return;

  req->callback = cb;
  req->user_data = user_data;

  if (runtime->config.mode == cfs_modality_sync) {
    if (!req->cancelled)
      (void)cfs_execute_op_inline(req);
    if (req->callback) {
      req->callback(req, req->user_data);
    }
    (void)cfs_request_release(req);
  } else {
    if (runtime->work_queue) {
      (void)cfs_queue_push(runtime->work_queue, req);
    } else {
      (void)cfs_request_release(req);
    }
  }
}

/* 19. cfs_runtime_poll() */
/**
 * \brief Processes completed asynchronous requests and executes their
 * callbacks.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_runtime_poll(cfs_runtime_t *rt) {
  int processed = 0;
  cfs_request_t *req = NULL;
  if (!rt || !rt->completion_queue)
    return cfs_errc_success;

  while (cfs_queue_pop(rt->completion_queue, cfs_false, &req) == 0 &&
         req != NULL) {
    if (req->callback) {
      req->callback(req, req->user_data);
    }
    (void)cfs_request_release(req);
    processed++;
  }
  (void)processed;
  return cfs_errc_success;
}

/* Re-implementing lost Phase 6, 7, 8, 9 functions */

/* Phase 6: String Handling */
/**
 * \brief Computes the length of a string safely.
 *
 * \param str The null-terminated string to evaluate.
 * \param out Pointer to store the result of the strlen operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strlen(const cfs_char_t *str, cfs_size_t *out) {
  cfs_size_t len = 0;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = 0;
  if (!str)
    return cfs_errc_not_enough_memory;
  while (str[len])
    len++;
  *out = len;
  return cfs_errc_success;
}

/**
 * \brief Copies a string into a destination buffer safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcpy operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strcpy(cfs_char_t *dest, const cfs_char_t *src,
                                       cfs_char_t **out) {
  cfs_size_t i = 0;
  if (out)
    *out = dest;
  if (!dest || !src)
    return cfs_errc_not_enough_memory;
  while ((dest[i] = src[i]) != 0)
    i++;
  return cfs_errc_success;
}

/**
 * \brief Copies up to n characters of a string into a destination buffer.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param n The maximum number of characters to copy.
 * \param out Pointer to store the result of the strncpy operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strncpy(cfs_char_t *dest, const cfs_char_t *src,
                                        cfs_size_t n, cfs_char_t **out) {
  cfs_size_t i;
  if (out)
    *out = dest;
  if (!dest || !src)
    return cfs_errc_not_enough_memory;
  for (i = 0; i < n && src[i] != 0; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = 0;
  return cfs_errc_success;
}

/**
 * \brief Concatenates two strings safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcat operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strcat(cfs_char_t *dest, const cfs_char_t *src,
                                       cfs_char_t **out) {
  cfs_size_t dest_len = 0;
  cfs_size_t i = 0;
  if (out)
    *out = dest;
  (void)cfs_strlen(dest, &dest_len);
  if (!dest || !src)
    return cfs_errc_not_enough_memory;
  while ((dest[dest_len + i] = src[i]) != 0)
    i++;
  return cfs_errc_success;
}

/**
 * \brief Compares two strings lexicographically.
 *
 * \param lhs The left-hand side string for comparison.
 * \param rhs The right-hand side string for comparison.
 * \param out Pointer to store the result of the strcmp operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strcmp(const cfs_char_t *lhs,
                                       const cfs_char_t *rhs, int *out) {
  if (!out)
    return cfs_errc_not_enough_memory;
  if (!lhs && !rhs) {
    *out = 0;
    return cfs_errc_success;
  }
  if (!lhs) {
    *out = -1;
    return cfs_errc_success;
  }
  if (!rhs) {
    *out = 1;
    return cfs_errc_success;
  }
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
  *out = wcscmp(lhs, rhs);
#else
  *out = strcmp(lhs, rhs);
#endif
  return cfs_errc_success;
}

/**
 * \brief Compares up to a specified count of characters of two strings
 * lexicographically.
 *
 * \param lhs Argument representing the target resource.
 * \param rhs Argument representing the target resource.
 * \param count The maximum number of characters to compare.
 * \param out Pointer to store the result of the strncmp operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_strncmp(const cfs_char_t *lhs,
                                        const cfs_char_t *rhs, cfs_size_t count,
                                        int *out) {
  if (!out)
    return cfs_errc_not_enough_memory;
  if (!lhs && !rhs) {
    *out = 0;
    return cfs_errc_success;
  }
  if (!lhs) {
    *out = -1;
    return cfs_errc_success;
  }
  if (!rhs) {
    *out = 1;
    return cfs_errc_success;
  }
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
  *out = wcsncmp(lhs, rhs, count);
#else
  *out = strncmp(lhs, rhs, count);
#endif
  return cfs_errc_success;
}

#if defined(CFS_OS_WINDOWS)
/**
 * \brief Converts a UTF-8 string to a UTF-16 wide string.
 *
 * \param utf8_str Null-terminated UTF-8 source string.
 * \param dest Pointer to the wide character destination buffer.
 * \param dest_len Capacity of the destination buffer in wide characters.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_utf8_to_utf16(const char *utf8_str,
                                              wchar_t *dest,
                                              cfs_size_t dest_len,
                                              cfs_size_t *out_req) {
  int req = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, dest, (int)dest_len);
  if (out_req) {
    *out_req = (req > 0) ? (cfs_size_t)req : 0;
  }
  return (req > 0) ? cfs_errc_success : cfs_errc_not_enough_memory;
}

/**
 * \brief Converts a UTF-16 wide string to a UTF-8 string.
 *
 * \param utf16_str Null-terminated UTF-16 source wide string.
 * \param dest Pointer to the UTF-8 destination buffer.
 * \param dest_len Capacity of the destination buffer in bytes.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_utf16_to_utf8(const wchar_t *utf16_str,
                                              char *dest, cfs_size_t dest_len,
                                              cfs_size_t *out_req) {
  int req = WideCharToMultiByte(CP_UTF8, 0, utf16_str, -1, dest, (int)dest_len,
                                NULL, NULL);
  if (out_req) {
    *out_req = (req > 0) ? (cfs_size_t)req : 0;
  }
  return (req > 0) ? cfs_errc_success : cfs_errc_not_enough_memory;
}
#endif

/**
 * \brief Converts a multi-byte string to a wide character string.
 *
 * \param mb_str Null-terminated multi-byte source string.
 * \param dest Pointer to the destination buffer or path.
 * \param dest_len Capacity of the destination buffer.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_mb_to_wide(const char *mb_str, wchar_t *dest,
                                           cfs_size_t dest_len,
                                           cfs_size_t *out_req) {
#if defined(CFS_OS_WINDOWS)
  return cfs_utf8_to_utf16(mb_str, dest, dest_len, out_req);
#else
  (void)mb_str;
  (void)dest;
  (void)dest_len;
  if (out_req) {
    *out_req = 0; /* Fallback not used on POSIX where cfs_char_t is char */
  }
  return cfs_errc_not_enough_memory;
#endif
}

/**
 * \brief Converts a wide character string to a multi-byte string.
 *
 * \param wide_str Null-terminated wide source string.
 * \param dest Pointer to the destination buffer or path.
 * \param dest_len Capacity of the destination buffer.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_wide_to_mb(const wchar_t *wide_str, char *dest,
                                           cfs_size_t dest_len,
                                           cfs_size_t *out_req) {
#if defined(CFS_OS_WINDOWS)
  return cfs_utf16_to_utf8(wide_str, dest, dest_len, out_req);
#else
  (void)wide_str;
  (void)dest;
  (void)dest_len;
  if (out_req) {
    *out_req = 0; /* Fallback not used on POSIX */
  }
  return cfs_errc_not_enough_memory;
#endif
}

/* Phase 7: Error Handling */
/**
 * \brief Manually populates a `cfs_error_code` structure with an OS and POSIX
 * error code.
 *
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \param os_value The raw system error code (errno, GetLastError).
 * \param standard_value The normalized POSIX cfs_errc mapping.
 */
CFS_API void cfs_set_error(cfs_error_code *ec, int os_value,
                           cfs_errc standard_value) {
  if (ec) {
    ec->value = os_value;
    ec->errc = standard_value;
  }
}

/**
 * \brief Resets a `cfs_error_code` structure to a success state.
 *
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
CFS_API void cfs_clear_error(cfs_error_code *ec) {
  (void)cfs_set_error(ec, 0, cfs_errc_success);
}

/**
 * \brief Populates an error structure based on a raw OS error code,
 * automatically mapping to the POSIX enum.
 *
 * \param os_error The raw system error code.
 * \param out Pointer to store the result of the make_error_code_from_os
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_make_error_code_from_os(int os_error,
                                                        cfs_error_code *out) {
  if (out) {
    out->value = os_error;
    out->errc = os_error == 0 ? cfs_errc_success : cfs_errc_unknown_error;
  }
  return cfs_errc_success;
}

/**
 * \brief Retrieves the last thread-local system error (errno or GetLastError)
 * and wraps it into a `cfs_error_code`.
 *
 * \param out Pointer to store the result of the get_last_error operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_get_last_error(cfs_error_code *out) {
#if defined(CFS_OS_WINDOWS)
  return cfs_make_error_code_from_os((int)GetLastError(), out);
#else
  /* missing errno include but simplify for now */
  return cfs_make_error_code_from_os(1, out);
#endif
}

/**
 * \brief Retrieves a human-readable string representation of a POSIX error
 * code.
 *
 * \param err The POSIX standard error mapping to evaluate.
 * \param out Pointer to store the result of the error_message operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_error_message(cfs_errc err, const char **out) {
  (void)err;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = "Error";
  return cfs_errc_success;
}

/* Phase 8 & 9: Path Struct Basics */
/**
 * \brief Initializes an empty `cfs_path` structure.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_init(cfs_path *p) {
  if (!p)
    return;
  p->str = NULL;
  p->length = 0;
  p->capacity = 0;
}

/**
 * \brief Initializes a `cfs_path` structure using a provided string source.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The null-terminated string to initialize the path with.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_init_str(cfs_path *p,
                                              const cfs_char_t *source) {
  (void)cfs_path_init(p);
  if (source) {
    return cfs_path_assign(p, source);
  }
  return cfs_errc_success;
}

/**
 * \brief Destroys a `cfs_path` structure, releasing its allocated memory.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_destroy(cfs_path *p) {
  if (!p)
    return;
  if (p->str)
    (void)cfs_free(p->str);
  p->str = NULL;
  p->length = 0;
  p->capacity = 0;
}

/**
 * \brief Creates a deep copy of a `cfs_path` structure.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_clone(cfs_path *dest,
                                           const cfs_path *src) {
  if (!dest || !src)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(dest);
  if (src->str) {
    return cfs_path_assign(dest, src->str);
  }
  return cfs_errc_success;
}

/**
 * \brief Retrieves the underlying null-terminated C string from a path.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_c_str operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_c_str(const cfs_path *p,
                                           const cfs_char_t **out) {
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = (p && p->str) ? p->str : CFS_STR("");
  return cfs_errc_success;
}

/**
 * \brief Mutates the path in-place, converting all directory separators to the
 * native OS preferred separator.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_make_preferred(cfs_path *p) {
  cfs_size_t i;
  if (!p || !p->str)
    return cfs_errc_not_enough_memory;
  for (i = 0; i < p->length; i++) {
    if (p->str[i] == CFS_CHAR('/') || p->str[i] == CFS_CHAR('\\'))
      p->str[i] = PATH_SEP_CHAR;
  }
  return cfs_errc_success;
}

/**
 * \brief Returns a dynamically allocated string using generic (forward slash)
 * directory separators.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_generic_string operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_generic_string(const cfs_path *p,
                                                    cfs_char_t **out) {
  cfs_char_t *res;
  cfs_size_t i;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = NULL;
  if (!p || !p->str)
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc((p->length + 1) * sizeof(cfs_char_t), (void **)&res);
  if (!res)
    return cfs_errc_not_enough_memory;
  CFS_STRCPY_SAFE(res, p->length + 1, p->str);
  for (i = 0; i < p->length; i++) {
#if defined(CFS_OS_WINDOWS)
    if (res[i] == CFS_CHAR('\\'))
      res[i] = CFS_CHAR('/');
#endif
  }
  *out = res;
  return cfs_errc_success;
}

/**
 * \brief Clears the contents of the path, rendering it empty without freeing
 * its capacity.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_clear(cfs_path *p) {
  if (!p)
    return;
  if (p->str)
    p->str[0] = 0;
  p->length = 0;
}

/**
 * \brief Swaps the contents and capacities of two `cfs_path` structures.
 *
 * \param lhs The left-hand side path to swap.
 * \param rhs The right-hand side path to swap.
 */
CFS_API void cfs_path_swap(cfs_path *lhs, cfs_path *rhs) {
  cfs_path temp;
  if (!lhs || !rhs)
    return;
  temp = *lhs;
  *lhs = *rhs;
  *rhs = temp;
}

/**
 * \brief Concatenates a string directly to the end of the path without
 * inserting separators.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to concatenate.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_concat(cfs_path *p,
                                            const cfs_char_t *source) {
  cfs_size_t src_len;
  cfs_size_t new_len;
  if (!p || !source)
    return cfs_errc_not_enough_memory;
  (void)cfs_strlen(source, &src_len);
  if (src_len == 0)
    return cfs_errc_success;
  new_len = p->length + src_len;
  if (cfs_path_reserve(p, new_len + 1) != 0)
    return cfs_errc_not_enough_memory;
  CFS_STRCPY_SAFE(p->str + p->length, p->capacity - p->length, source);
  p->length = new_len;
  return cfs_errc_success;
}

/* cfs_path_append implementation from recovered file ending */
/**
 * \brief Appends a source string to the path, resolving directory separators
 * intelligently.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to append to the path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_append(cfs_path *p,
                                            const cfs_char_t *source) {
  cfs_size_t src_len;
  cfs_bool p_has_sep = cfs_false;
  cfs_bool src_has_sep = cfs_false;
  cfs_size_t new_len;
#if defined(CFS_OS_WINDOWS)
  cfs_size_t len = 0;
#endif

  if (!p || !source)
    return cfs_errc_not_enough_memory;

  {
    cfs_bool empty;
    (void)cfs_path_is_empty(p, &empty);
    if (empty) {
      return cfs_path_assign(p, source);
    }
  }

  if (source[0] == 0)
    return cfs_errc_success;

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

  (void)cfs_strlen(source, &src_len);
  if (p->length > 0)
    (void)cfs_is_separator(p->str[p->length - 1], &p_has_sep);
  (void)cfs_is_separator(source[0], &src_has_sep);

  new_len = p->length + src_len;
  if (!p_has_sep && !src_has_sep)
    new_len++;
  else if (p_has_sep && src_has_sep)
    new_len--;

  if (cfs_path_reserve(p, new_len + 1) != 0)
    return cfs_errc_not_enough_memory;

  if (!p_has_sep && !src_has_sep) {
    p->str[p->length] = PATH_SEP_CHAR;
    p->str[p->length + 1] = 0;
    p->length++;
  } else if (p_has_sep && src_has_sep) {
    source++;
  }

  CFS_STRCPY_SAFE(p->str + p->length, p->capacity - p->length, source);
  p->length = new_len;
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_is_separator filesystem operation.
 *
 * \param c Argument representing the target resource.
 * \param out Pointer to store the result of the is_separator operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */

/* --- Internal Path Parsing Helpers --- */
static cfs_size_t cfs_get_root_name_len(const cfs_path *p);
static cfs_size_t cfs_get_root_dir_len(const cfs_path *p,
                                       cfs_size_t root_name_len);

static cfs_size_t cfs_get_root_name_len(const cfs_path *p) {
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  {
    const cfs_char_t *s = p->str;
    cfs_size_t len = p->length;
    cfs_size_t i;

    /* 1. Drive letter: "C:" */
    if (len >= 2 && s[1] == CFS_CHAR(':')) {
      if ((s[0] >= CFS_CHAR('a') && s[0] <= CFS_CHAR('z')) ||
          (s[0] >= CFS_CHAR('A') && s[0] <= CFS_CHAR('Z'))) {
        return 2;
      }
    }

    /* 2. Device or Extended-Length or UNC */
    if (len >= 4 && (s[0] == CFS_CHAR('\\') || s[0] == CFS_CHAR('/')) &&
        (s[1] == CFS_CHAR('\\') || s[1] == CFS_CHAR('/')) &&
        (s[2] == CFS_CHAR('?') || s[2] == CFS_CHAR('.')) &&
        (s[3] == CFS_CHAR('\\') || s[3] == CFS_CHAR('/'))) {

      if (s[2] == CFS_CHAR('?') && len >= 8 &&
          (s[4] == CFS_CHAR('U') || s[4] == CFS_CHAR('u')) &&
          (s[5] == CFS_CHAR('N') || s[5] == CFS_CHAR('n')) &&
          (s[6] == CFS_CHAR('C') || s[6] == CFS_CHAR('c')) &&
          (s[7] == CFS_CHAR('\\') || s[7] == CFS_CHAR('/'))) {
        cfs_size_t sep_count = 0;
        for (i = 8; i < len; i++) {
          if (s[i] == CFS_CHAR('\\') || s[i] == CFS_CHAR('/')) {
            sep_count++;
            if (sep_count == 2)
              return i;
          }
        }
        return len;
      }

      if (len >= 6 && s[5] == CFS_CHAR(':') &&
          ((s[4] >= CFS_CHAR('a') && s[4] <= CFS_CHAR('z')) ||
           (s[4] >= CFS_CHAR('A') && s[4] <= CFS_CHAR('Z')))) {
        return 6;
      }

      for (i = 4; i < len; i++) {
        if (s[i] == CFS_CHAR('\\') || s[i] == CFS_CHAR('/')) {
          return i;
        }
      }
      return len;
    }

    /* 3. Normal UNC: \\server\share */
    if (len >= 2 && (s[0] == CFS_CHAR('\\') || s[0] == CFS_CHAR('/')) &&
        (s[1] == CFS_CHAR('\\') || s[1] == CFS_CHAR('/'))) {
      cfs_size_t sep_count = 0;
      for (i = 2; i < len; i++) {
        if (s[i] == CFS_CHAR('\\') || s[i] == CFS_CHAR('/')) {
          sep_count++;
          if (sep_count == 2)
            return i;
        }
      }
      return len;
    }
  }
#endif
  return cfs_errc_success;
}

static cfs_size_t cfs_get_root_dir_len(const cfs_path *p,
                                       cfs_size_t root_name_len) {
  if (!p || p->length <= root_name_len || !p->str)
    return cfs_errc_success;
  if (p->str[root_name_len] == CFS_CHAR('\\') ||
      p->str[root_name_len] == CFS_CHAR('/')) {
    return 1;
  }
  return cfs_errc_success;
}

static cfs_errc cfs_is_separator(cfs_char_t c, cfs_bool *out) {
  *out = (c == CFS_CHAR('/') || c == CFS_CHAR('\\')) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_reserve filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param new_cap Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
static cfs_errc cfs_path_reserve(cfs_path *p, cfs_size_t new_cap) {
  cfs_char_t *new_str;
  if (new_cap <= p->capacity)
    return cfs_errc_success;

  if (p->str) {
    (void)cfs_realloc(p->str, new_cap * sizeof(cfs_char_t), (void **)&new_str);
  } else {
    (void)cfs_malloc(new_cap * sizeof(cfs_char_t), (void **)&new_str);
  }

  if (!new_str)
    return cfs_errc_not_enough_memory;

  p->str = new_str;
  p->capacity = new_cap;
  return cfs_errc_success;
}

/**
 * \brief Checks if the path is entirely empty.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_empty operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_is_empty(const cfs_path *p,
                                              cfs_bool *out) {
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = (!p || p->length == 0);
  return cfs_errc_success;
}

/**
 * \brief Assigns a new string source to an existing `cfs_path` structure,
 * clearing the previous contents.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The new string to assign to the path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_assign(cfs_path *p,
                                            const cfs_char_t *source) {
  cfs_size_t len;
  cfs_errc rc;
  if (!p) {
    LOG_DEBUG("cfs_path_assign: p is NULL");
    return cfs_errc_not_enough_memory;
  }
  if (!source) {
    (void)cfs_path_clear(p);
    return cfs_errc_success;
  }
  (void)cfs_strlen(source, &len);
  rc = cfs_path_reserve(p, len + 1);
  if (rc != 0) {
    LOG_DEBUG("cfs_path_assign: cfs_path_reserve failed with %d", rc);
    return rc;
  }
  CFS_STRCPY_SAFE(p->str, p->capacity, source);
  p->length = len;
  return cfs_errc_success;
}

/**
 * \brief Allocates memory using the internal allocator or fallback OS
 * mechanism.
 *
 * \param size Number of bytes to allocate.
 * \param out Pointer to store the result of the malloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_malloc(cfs_size_t size, void **out) {
  if (out)
    *out = malloc(size);
  return (out && *out) ? cfs_errc_success : cfs_errc_not_enough_memory;
}
/**
 * \brief Frees previously allocated memory.
 *
 * \param ptr Pointer to the memory block to deallocate.
 */
CFS_API void cfs_free(void *ptr) { free(ptr); }
/**
 * \brief Reallocates an existing memory block to a new size.
 *
 * \param ptr Argument representing the target resource.
 * \param size Argument representing the target resource.
 * \param out Pointer to store the result of the realloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_realloc(void *ptr, cfs_size_t size,
                                        void **out) {
  if (out)
    *out = realloc(ptr, size);
  return (out && *out) ? cfs_errc_success : cfs_errc_not_enough_memory;
}
/**
 * \brief Allocates zero-initialized memory for an array of elements.
 *
 * \param num Number of elements to allocate.
 * \param size Argument representing the target resource.
 * \param out Pointer to store the result of the calloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_calloc(cfs_size_t num, cfs_size_t size,
                                       void **out) {
  if (out)
    *out = calloc(num, size);
  return (out && *out) ? cfs_errc_success : cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_path_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_filename(const cfs_path *p,
                                              cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t i, start_idx;
  cfs_bool is_sep = cfs_false;
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;
  if (p->length == root_len)
    return cfs_errc_success;
  (void)cfs_is_separator(p->str[p->length - 1], &is_sep);
  if (is_sep)
    return cfs_errc_success;
  start_idx = root_len;
  for (i = p->length; i > root_len; i--) {
    (void)cfs_is_separator(p->str[i - 1], &is_sep);
    if (is_sep) {
      start_idx = i;
      break;
    }
  }
  {
    cfs_size_t fn_len = p->length - start_idx;
    cfs_char_t *buf;
    if (fn_len > 0) {
      if (cfs_calloc(fn_len + 1, sizeof(cfs_char_t), (void **)&buf) != 0)
        return cfs_errc_not_enough_memory;
      CFS_STRNCPY_SAFE(buf, fn_len + 1, p->str + start_idx, fn_len);
      (void)cfs_path_assign(out, buf);
      (void)cfs_free(buf);
    }
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_extension(const cfs_path *p,
                                               cfs_path *out) {
  cfs_path fn;
  cfs_size_t i;
  cfs_size_t dot_idx = (cfs_size_t)-1;
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p)
    return cfs_errc_not_enough_memory;
  if (cfs_path_filename(p, &fn) != 0)
    return cfs_errc_not_enough_memory;
  if (fn.length == 0 || (fn.length == 1 && fn.str[0] == CFS_CHAR('.')) ||
      (fn.length == 2 && fn.str[0] == CFS_CHAR('.') &&
       fn.str[1] == CFS_CHAR('.'))) {
    (void)cfs_path_destroy(&fn);
    return cfs_errc_success;
  }
  for (i = fn.length; i > 0; i--) {
    if (fn.str[i - 1] == CFS_CHAR('.')) {
      dot_idx = i - 1;
      break;
    }
  }
  if (dot_idx != (cfs_size_t)-1 && dot_idx > 0) {
    cfs_size_t ext_len = fn.length - dot_idx;
    cfs_char_t *buf;
    if (cfs_calloc(ext_len + 1, sizeof(cfs_char_t), (void **)&buf) != 0) {
      (void)cfs_path_destroy(&fn);
      return cfs_errc_not_enough_memory;
    }
    CFS_STRNCPY_SAFE(buf, ext_len + 1, fn.str + dot_idx, ext_len);
    (void)cfs_path_assign(out, buf);
    (void)cfs_free(buf);
  }
  (void)cfs_path_destroy(&fn);
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_stem(const cfs_path *p, cfs_path *out) {
  cfs_path fn;
  cfs_size_t i;
  cfs_size_t dot_idx = (cfs_size_t)-1;
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p)
    return cfs_errc_not_enough_memory;
  if (cfs_path_filename(p, &fn) != 0)
    return cfs_errc_not_enough_memory;
  if (fn.length == 0 || (fn.length == 1 && fn.str[0] == CFS_CHAR('.')) ||
      (fn.length == 2 && fn.str[0] == CFS_CHAR('.') &&
       fn.str[1] == CFS_CHAR('.'))) {
    (void)cfs_path_assign(out, fn.str);
    (void)cfs_path_destroy(&fn);
    return cfs_errc_success;
  }
  for (i = fn.length; i > 0; i--) {
    if (fn.str[i - 1] == CFS_CHAR('.')) {
      dot_idx = i - 1;
      break;
    }
  }
  if (dot_idx != (cfs_size_t)-1 && dot_idx > 0) {
    cfs_char_t *buf;
    if (cfs_calloc(dot_idx + 1, sizeof(cfs_char_t), (void **)&buf) != 0) {
      (void)cfs_path_destroy(&fn);
      return cfs_errc_not_enough_memory;
    }
    CFS_STRNCPY_SAFE(buf, dot_idx + 1, fn.str, dot_idx);
    (void)cfs_path_assign(out, buf);
    (void)cfs_free(buf);
  } else {
    (void)cfs_path_assign(out, fn.str);
  }
  (void)cfs_path_destroy(&fn);
  return cfs_errc_success;
}

/**
 * \brief Deletes a specific file or empty directory node.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_remove(const cfs_path *p, cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || p->length == 0)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (_wremove(p->str) == 0)
    return cfs_errc_success;
  if (RemoveDirectoryW(p->str))
    return cfs_errc_success;
#else
  if (remove(p->str) == 0)
    return cfs_errc_success;
  if (RemoveDirectoryA(p->str))
    return cfs_errc_success;
#endif
#else
  if (remove(p->str) == 0)
    return cfs_errc_success;
#endif
  if (ec)
    (void)cfs_get_last_error(ec);
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Retrieves the size of a given file in bytes.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the file_size operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_file_size(const cfs_path *p, cfs_uintmax_t *out,
                                          cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || p->length == 0)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
  {
    struct _stat64 st;
#if defined(CFS_UNICODE)
    if (_wstat64(p->str, &st) == 0) {
      if (out)
        *out = (cfs_uintmax_t)st.st_size;
      return cfs_errc_success;
    }
#else
    if (_stat64(p->str, &st) == 0) {
      if (out)
        *out = (cfs_uintmax_t)st.st_size;
      return cfs_errc_success;
    }
#endif
  }
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      if (out)
        *out = (cfs_uintmax_t)st.st_size;
      return cfs_errc_success;
    }
  }
#endif
  if (ec)
    (void)cfs_get_last_error(ec);
  return cfs_errc_not_enough_memory;
}

/* Phase 3: Platform-Specific Async & Message Passing Implementations */

/**
 * \brief Performs the cfs_request_retain filesystem operation.
 *
 * \param req Argument representing the target resource.
 */
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

/**
 * \brief Performs the cfs_request_destroy_internal filesystem operation.
 *
 * \param req Argument representing the target resource.
 */
static void cfs_request_destroy_internal(cfs_request_t *req) {
  (void)cfs_path_destroy(&req->target_path);
  (void)cfs_path_destroy(&req->dest_path);
  if (req->result_buffer)
    (void)cfs_free(req->result_buffer);
  (void)cfs_free(req);
}

/**
 * \brief Performs the cfs_request_release filesystem operation.
 *
 * \param req Argument representing the target resource.
 */
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
    (void)cfs_request_destroy_internal(req);
  }
}

/**
 * \brief Performs the cfs_cancel_request filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param req Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_cancel_request(cfs_runtime_t *rt,
                                               cfs_request_t *req) {
  if (!rt || !req)
    return cfs_errc_not_enough_memory;
  /* Basic cancellation just marks it. The worker thread will skip execution if
   * it sees this. */
  /* Thread-safe boolean assignment (assuming aligned int is atomic enough for
   * this) */
  req->cancelled = cfs_true;
  return cfs_errc_success;
}

/* Phase 3: Message Passing and Async Backend Stubs */

/**
 * \brief Data structure for cfs_message_pipe.
 */
struct cfs_message_pipe {
  /** \brief Represents the handle enumerator or field data. */
  void *handle;
};

/**
 * \brief Performs the cfs_message_pipe_create filesystem operation.
 *
 * \param path Argument representing the target resource.
 * \param out_pipe Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_message_pipe_create(const cfs_char_t *path, cfs_message_pipe **out_pipe) {
  if (!path || !out_pipe)
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc(sizeof(cfs_message_pipe), (void **)out_pipe);
  if (*out_pipe)
    (*out_pipe)->handle = NULL;
  return (*out_pipe) ? cfs_errc_success : cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_message_pipe_destroy filesystem operation.
 *
 * \param pipe Argument representing the target resource.
 */
CFS_API void cfs_message_pipe_destroy(cfs_message_pipe *pipe) {
  if (pipe)
    (void)cfs_free(pipe);
}

/**
 * \brief Performs the cfs_serialize_request filesystem operation.
 *
 * \param req Argument representing the target resource.
 * \param buffer Argument representing the target resource.
 * \param size Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_serialize_request(const cfs_request_t *req,
                                                  void **buffer,
                                                  cfs_size_t *size) {
  if (!req || !buffer || !size)
    return cfs_errc_not_enough_memory;
  /* Simplistic serialization simulation */
  *size = sizeof(int) + sizeof(cfs_size_t) * 2; /* Opcode + path lengths */
  (void)cfs_malloc(*size, (void **)buffer);
  if (!*buffer)
    return cfs_errc_not_enough_memory;
  ((int *)*buffer)[0] = req->opcode;
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_deserialize_request filesystem operation.
 *
 * \param buffer Argument representing the target resource.
 * \param size Argument representing the target resource.
 * \param req Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_deserialize_request(const void *buffer,
                                                    cfs_size_t size,
                                                    cfs_request_t **req) {
  if (!buffer || !req || size < sizeof(int))
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc(sizeof(cfs_request_t), (void **)req);
  if (!*req)
    return cfs_errc_not_enough_memory;
  (*req)->opcode = ((const int *)buffer)[0];
  (*req)->ref_count = 1;
  (*req)->cancelled = cfs_false;
  (*req)->result_buffer = NULL;
  (*req)->result_size = 0;
  (void)cfs_path_init(&(*req)->target_path);
  (void)cfs_path_init(&(*req)->dest_path);
  (*req)->callback = NULL;
  (*req)->user_data = NULL;
  return cfs_errc_success;
}

/* Phase 4: Multiprocessing and Greenthreads Implementations */

/**
 * \brief Data structure for cfs_process_t.
 */
struct cfs_process_t {
#if defined(CFS_OS_WINDOWS)
  /** \brief Represents the process_handle enumerator or field data. */
  HANDLE process_handle;
  /** \brief Represents the thread_handle enumerator or field data. */
  HANDLE thread_handle;
#else
  /** \brief Represents the pid enumerator or field data. */
  pid_t pid;
#endif
};

/**
 * \brief Performs the cfs_process_spawn filesystem operation.
 *
 * \param executable Argument representing the target resource.
 * \param out_proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_process_spawn(const cfs_char_t *executable,
                                              cfs_process_t **out_proc) {
  if (!executable || !out_proc)
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc(sizeof(cfs_process_t), (void **)out_proc);
  if (!*out_proc)
    return cfs_errc_not_enough_memory;

#if defined(CFS_OS_WINDOWS)
  {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    cfs_char_t *exec_copy;
    cfs_size_t len;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    (void)cfs_strlen(executable, &len);
    (void)cfs_malloc((len + 1) * sizeof(cfs_char_t), (void **)&exec_copy);
    if (!exec_copy) {
      (void)cfs_free(*out_proc);
      *out_proc = NULL;
      return cfs_errc_not_enough_memory;
    }
    CFS_STRCPY_SAFE(exec_copy, len + 1, executable);

    if (!CreateProcessW(NULL, (LPWSTR)exec_copy, NULL, NULL, FALSE, 0, NULL,
                        NULL, &si, &pi)) {
      (void)cfs_free(exec_copy);
      (void)cfs_free(*out_proc);
      *out_proc = NULL;
      return cfs_errc_not_enough_memory;
    }
    (void)cfs_free(exec_copy);
    (*out_proc)->process_handle = pi.hProcess;
    (*out_proc)->thread_handle = pi.hThread;
  }
#else
  /* Fork/Exec stub for POSIX */
  (*out_proc)->pid = -1; /* Implement full fork/exec */
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_process_wait filesystem operation.
 *
 * \param proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_process_wait(cfs_process_t *proc) {
  if (!proc)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
  WaitForSingleObject(proc->process_handle, INFINITE);
  return cfs_errc_success;
#else
  /* waitpid stub */
  return cfs_errc_success;
#endif
}

/**
 * \brief Performs the cfs_process_destroy filesystem operation.
 *
 * \param proc Argument representing the target resource.
 */
CFS_API void cfs_process_destroy(cfs_process_t *proc) {
  if (!proc)
    return;
#if defined(CFS_OS_WINDOWS)
  CloseHandle(proc->process_handle);
  CloseHandle(proc->thread_handle);
#endif
  (void)cfs_free(proc);
}

/**
 * \brief Data structure for cfs_shm_segment.
 */
struct cfs_shm_segment {
#if defined(CFS_OS_WINDOWS)
  /** \brief Represents the map_handle enumerator or field data. */
  HANDLE map_handle;
#else
  /** \brief Represents the fd enumerator or field data. */
  int fd;
#endif
  /** \brief Represents the size enumerator or field data. */
  cfs_size_t size;
};

/**
 * \brief Performs the cfs_shm_create filesystem operation.
 *
 * \param size Argument representing the target resource.
 * \param name Argument representing the target resource.
 * \param out_shm Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_shm_create(cfs_size_t size,
                                           const cfs_char_t *name,
                                           cfs_shm_segment **out_shm) {
  if (!name || !out_shm || size == 0)
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc(sizeof(cfs_shm_segment), (void **)out_shm);
  if (!*out_shm)
    return cfs_errc_not_enough_memory;
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
    (void)cfs_free(*out_shm);
    *out_shm = NULL;
    return cfs_errc_not_enough_memory;
  }
#else
  /* shm_open stub */
  (*out_shm)->fd = -1;
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_shm_map filesystem operation.
 *
 * \param shm Argument representing the target resource.
 * \param out Pointer to store the result of the shm_map operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_shm_map(cfs_shm_segment *shm, void **out) {
  if (!shm || !out)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
  *out = MapViewOfFile(shm->map_handle, FILE_MAP_ALL_ACCESS, 0, 0, shm->size);
  return *out ? cfs_errc_success : cfs_errc_not_enough_memory;
#else
  *out = NULL; /* mmap stub */
  return cfs_errc_not_enough_memory;
#endif
}

/**
 * \brief Performs the cfs_shm_unmap filesystem operation.
 *
 * \param shm Argument representing the target resource.
 * \param addr Argument representing the target resource.
 */
CFS_API void cfs_shm_unmap(cfs_shm_segment *shm, void *addr) {
  if (!shm || !addr)
    return;
#if defined(CFS_OS_WINDOWS)
  UnmapViewOfFile(addr);
#else
  /* munmap stub */
#endif
}

/**
 * \brief Performs the cfs_shm_destroy filesystem operation.
 *
 * \param shm Argument representing the target resource.
 */
CFS_API void cfs_shm_destroy(cfs_shm_segment *shm) {
  if (!shm)
    return;
#if defined(CFS_OS_WINDOWS)
  CloseHandle(shm->map_handle);
#else
  /* close stub */
#endif
  (void)cfs_free(shm);
}

/**
 * \brief Data structure for cfs_named_semaphore.
 */
struct cfs_named_semaphore {
#if defined(CFS_OS_WINDOWS)
  /** \brief Represents the handle enumerator or field data. */
  HANDLE handle;
#else
  /** \brief POSIX semaphore pointer. */
  void *sem;
#endif
};

/**
 * \brief Performs the cfs_named_semaphore_create filesystem operation.
 *
 * \param name Argument representing the target resource.
 * \param initial_count Argument representing the target resource.
 * \param out_sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_create(
    const cfs_char_t *name, int initial_count, cfs_named_semaphore **out_sem) {
  if (!name || !out_sem)
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc(sizeof(cfs_named_semaphore), (void **)out_sem);
  if (!*out_sem)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  (*out_sem)->handle = CreateSemaphoreW(NULL, initial_count, 0x7FFFFFFF, name);
#else
  (*out_sem)->handle = CreateSemaphoreA(NULL, initial_count, 0x7FFFFFFF, name);
#endif
  if (!(*out_sem)->handle) {
    (void)cfs_free(*out_sem);
    *out_sem = NULL;
    return cfs_errc_not_enough_memory;
  }
#else
  (void)initial_count;
  /* sem_open stub */
  (*out_sem)->sem = NULL;
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_named_semaphore_wait filesystem operation.
 *
 * \param sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_wait(cfs_named_semaphore *sem) {
  if (!sem)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
  return WaitForSingleObject(sem->handle, INFINITE) == WAIT_OBJECT_0
             ? cfs_errc_success
             : cfs_errc_not_enough_memory;
#else
  return cfs_errc_not_enough_memory; /* sem_wait stub */
#endif
}

/**
 * \brief Performs the cfs_named_semaphore_post filesystem operation.
 *
 * \param sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_post(cfs_named_semaphore *sem) {
  if (!sem)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
  return ReleaseSemaphore(sem->handle, 1, NULL) ? cfs_errc_success
                                                : cfs_errc_not_enough_memory;
#else
  return cfs_errc_not_enough_memory; /* sem_post stub */
#endif
}

/**
 * \brief Performs the cfs_named_semaphore_destroy filesystem operation.
 *
 * \param sem Argument representing the target resource.
 */
CFS_API void cfs_named_semaphore_destroy(cfs_named_semaphore *sem) {
  if (!sem)
    return;
#if defined(CFS_OS_WINDOWS)
  CloseHandle(sem->handle);
#else
  /* sem_close stub */
#endif
  (void)cfs_free(sem);
}

/* Greenthread basic stubs (Platform implementation requires assembly/ucontext
 * or setjmp logic) */
/**
 * \brief Data structure for cfs_greenthread_t.
 */
struct cfs_greenthread_t {
  /** \brief Represents the context enumerator or field data. */
  void *context;
};

/**
 * \brief Data structure for cfs_greenthread_scheduler.
 */
struct cfs_greenthread_scheduler {
  /** \brief Represents the current enumerator or field data. */
  cfs_greenthread_t *current;
};

/**
 * \brief Performs the cfs_greenthread_spawn filesystem operation.
 *
 * \param func Argument representing the target resource.
 * \param arg Argument representing the target resource.
 * \param out_gt Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_greenthread_spawn(cfs_greenthread_func func,
                                                  void *arg,
                                                  cfs_greenthread_t **out_gt) {
  (void)func;
  (void)arg;
  if (!out_gt)
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc(sizeof(cfs_greenthread_t), (void **)out_gt);
  if (*out_gt)
    (*out_gt)->context = NULL;
  return (*out_gt) ? cfs_errc_success : cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_greenthread_yield filesystem operation.
 *
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_greenthread_yield(void) {
  return cfs_errc_success; /* stub */
}

/**
 * \brief Performs the cfs_greenthread_destroy filesystem operation.
 *
 * \param gt Argument representing the target resource.
 */
CFS_API void cfs_greenthread_destroy(cfs_greenthread_t *gt) {
  if (gt)
    (void)cfs_free(gt);
}

/**
 * \brief Initializes a greenthread scheduler.
 *
 * \param out_sched Pointer to store the initialized scheduler.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_init(cfs_greenthread_scheduler **out_sched) {
  if (!out_sched)
    return cfs_errc_not_enough_memory;
  (void)cfs_malloc(sizeof(cfs_greenthread_scheduler), (void **)out_sched);
  if (*out_sched)
    (*out_sched)->current = NULL;
  return (*out_sched) ? cfs_errc_success : cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_greenthread_scheduler_run filesystem operation.
 *
 * \param sched Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_run(cfs_greenthread_scheduler *sched) {
  return sched ? cfs_errc_success : cfs_errc_not_enough_memory;
}

/**
 * \brief Destroys a greenthread scheduler.
 *
 * \param sched Argument representing the target resource to destroy.
 */
CFS_API void
cfs_greenthread_scheduler_destroy(cfs_greenthread_scheduler *sched) {
  if (sched)
    (void)cfs_free(sched);
}

/* Phase 5.7: Integration Implementations */

/**
 * \brief Performs the cfs_dir_itr_init_async filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init_async(cfs_runtime_t *rt,
                                                   const cfs_path *p,
                                                   cfs_callback_t cb,
                                                   void *user_data) {
  cfs_request_t *req;
  if (!rt || !p)
    return cfs_errc_not_enough_memory;

  (void)cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (!req)
    return cfs_errc_not_enough_memory;

  /* Reusing a non-existent opcode for iterator init, but in reality we'd add
   * cfs_opcode_dir_itr_init */
  req->opcode = 999;
  (void)cfs_path_init(&req->target_path);
  (void)cfs_path_clone(&req->target_path, p);
  (void)cfs_path_init(&req->dest_path);
  req->result_buffer = NULL;
  req->result_size = 0;
  (void)cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->ref_count = 1;
  req->cancelled = cfs_false;
  req->next = NULL;

  (void)cfs_dispatch_request(rt, req, cb, user_data);
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_runtime_set_sandbox filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param config Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_runtime_set_sandbox(cfs_runtime_t *rt, const cfs_sandbox_config *config) {
  /* 43. Set internal sandbox bounds. This just stubs out the validation
   * structure. */
  if (!rt || !config)
    return cfs_errc_not_enough_memory;
  return cfs_errc_success;
}
/**
 * \brief Performs the cfs_status_known filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the status_known operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_status_known(cfs_file_status s, cfs_bool *out) {
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = (s.type != cfs_file_type_none);
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_hard_link_count filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the hard_link_count operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_hard_link_count(const cfs_path *p,
                                                cfs_uintmax_t *out,
                                                cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || p->length == 0 || !out) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_not_enough_memory;
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
      return cfs_errc_success;
    }
  }
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      *out = (cfs_uintmax_t)st.st_nlink;
      return cfs_errc_success;
    }
  }
#endif
  if (ec)
    (void)cfs_get_last_error(ec);
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_permissions filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param prms Argument representing the target resource.
 * \param opts Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_permissions(const cfs_path *p, cfs_perms prms,
                                            cfs_perm_options opts,
                                            cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || p->length == 0 || opts != cfs_perm_options_replace) {
    if (ec)
      (void)cfs_set_error(
          ec, 0, cfs_errc_invalid_argument); /* Only support replace for now */
    return cfs_errc_not_enough_memory;
  }
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (_wchmod(p->str, (int)prms) == 0) {
#else
  if (_chmod(p->str, (int)prms) == 0) {
#endif
    return cfs_errc_success;
  }
#else
  if (chmod(p->str, (mode_t)prms) == 0) {
    return cfs_errc_success;
  }
#endif
  if (ec)
    (void)cfs_get_last_error(ec);
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_equivalent filesystem operation.
 *
 * \param p1 Specific input argument required for the operation.
 * \param p2 Specific input argument required for the operation.
 * \param out Pointer to store the result of the equivalent operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_equivalent(const cfs_path *p1,
                                           const cfs_path *p2, cfs_bool *out,
                                           cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p1 || !p2 || p1->length == 0 || p2->length == 0 || !out) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_not_enough_memory;
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
        return cfs_errc_success;
      }
    }
    if (ec)
      (void)cfs_get_last_error(ec);
    if (h1 != INVALID_HANDLE_VALUE)
      CloseHandle(h1);
    if (h2 != INVALID_HANDLE_VALUE)
      CloseHandle(h2);
    return cfs_errc_not_enough_memory;
  }
#else
  {
    struct stat s1, s2;
    if (stat(p1->str, &s1) != 0 || stat(p2->str, &s2) != 0) {
      if (ec)
        (void)cfs_get_last_error(ec); /* Or make error from errno */
      return cfs_errc_not_enough_memory;
    }
    *out = (s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino) ? cfs_true
                                                              : cfs_false;
    return cfs_errc_success;
  }
#endif
}

/**
 * \brief Performs the cfs_read_symlink filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the read_symlink operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_read_symlink(const cfs_path *p, cfs_path *out,
                                             cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || !out || p->length == 0) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_not_enough_memory;
  }
#if defined(CFS_OS_WINDOWS)
  {
    /* Minimal stub for Windows C89, symlinks require reparse points parsing */
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return cfs_errc_not_enough_memory;
  }
#elif defined(CFS_OS_DOS)
  {
    /* Minimal stub for DOS C89, symlinks require reparse points parsing */
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return cfs_errc_not_enough_memory;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    ssize_t len;
    len = readlink(p->str, buf, sizeof(buf) - 1);
    if (len != -1) {
      buf[len] = '\0';
      (void)cfs_path_assign(out, buf);
      return cfs_errc_success;
    }
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_not_enough_memory;
  }
#endif
}

/**
 * \brief Performs the cfs_absolute filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the absolute operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_absolute(const cfs_path *p, cfs_path *out,
                                         cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || !out) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_not_enough_memory;
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
      (void)cfs_path_assign(out, buf);
      return cfs_errc_success;
    }
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_not_enough_memory;
  }
#elif defined(CFS_OS_DOS)
  {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return cfs_errc_not_enough_memory;
  }
#else
  {
    cfs_bool is_abs = cfs_false;
    cfs_path cp;
    if (cfs_path_is_absolute(p, &is_abs) == 0 && is_abs) {
      (void)cfs_path_assign(out, p->str);
      return cfs_errc_success;
    }
    (void)cfs_path_init(&cp);
    if (cfs_current_path(&cp, ec) == 0) {
      (void)cfs_path_assign(out, cp.str);
      (void)cfs_path_append(out, p->str);
      (void)cfs_path_destroy(&cp);
      return cfs_errc_success;
    }
    (void)cfs_path_destroy(&cp);
    return cfs_errc_not_enough_memory;
  }
#endif
}

/**
 * \brief Performs the cfs_canonical filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the canonical operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_canonical(const cfs_path *p, cfs_path *out,
                                          cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || !out) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_not_enough_memory;
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
      (void)cfs_path_assign(out, buf);
      return cfs_errc_success;
    }
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_not_enough_memory;
  }
#elif defined(CFS_OS_DOS)
  {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return cfs_errc_not_enough_memory;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    if (realpath(p->str, buf) != NULL) {
      (void)cfs_path_assign(out, buf);
      return cfs_errc_success;
    }
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_not_enough_memory;
  }
#endif
}

/**
 * \brief Performs the cfs_weakly_canonical filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the weakly_canonical operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_weakly_canonical(const cfs_path *p,
                                                 cfs_path *out,
                                                 cfs_error_code *ec) {
  return cfs_canonical(p, out, ec); /* Simplified stub */
}

/**
 * \brief Performs the cfs_copy filesystem operation.
 *
 * \param from Argument representing the target resource.
 * \param to Argument representing the target resource.
 * \param options Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_copy(const cfs_path *from, const cfs_path *to,
                                     cfs_copy_options options,
                                     cfs_error_code *ec) {
  /* Basic wrapper mapping to copy_file for now */
  if (cfs_copy_file(from, to, options, ec) == 0)
    return cfs_errc_success;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_copy_symlink filesystem operation.
 *
 * \param existing_symlink Argument representing the target resource.
 * \param new_symlink Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_copy_symlink(const cfs_path *existing_symlink,
                                             const cfs_path *new_symlink,
                                             cfs_error_code *ec) {
  cfs_path out;
  cfs_errc res;
  (void)cfs_path_init(&out);
  res = cfs_read_symlink(existing_symlink, &out, ec);
  if (res == cfs_errc_success) {
    (void)cfs_create_symlink(&out, new_symlink, ec);
    (void)cfs_path_destroy(&out);
    return cfs_errc_success;
  }
  return (cfs_errc)res;
}

/**
 * \brief Performs the cfs_proximate filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the proximate operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_proximate(const cfs_path *p,
                                          const cfs_path *base, cfs_path *out,
                                          cfs_error_code *ec) {
  cfs_path tmp;
  (void)cfs_path_lexically_proximate(p, base, &tmp);
  (void)ec;
  (void)cfs_path_clone(out, &tmp);
  (void)cfs_path_destroy(&tmp);
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the relative operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_relative(const cfs_path *p,
                                         const cfs_path *base, cfs_path *out,
                                         cfs_error_code *ec) {
  cfs_path tmp;
  (void)cfs_path_lexically_relative(p, base, &tmp);
  (void)ec;
  (void)cfs_path_clone(out, &tmp);
  (void)cfs_path_destroy(&tmp);
  return cfs_errc_success;
}

/**
 * \brief Copies a single file from a source path to a destination path.
 *
 * \param from The source path pointing to the file to copy.
 * \param to The destination path for the copied file.
 * \param options Copy behavior flags mapping to `cfs_copy_options`.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_copy_file(const cfs_path *from,
                                          const cfs_path *to,
                                          cfs_copy_options options,
                                          cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!from || !to)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (CopyFileW(from->str, to->str,
                !(options & cfs_copy_options_overwrite_existing)))
#else
  if (CopyFileA(from->str, to->str,
                !(options & cfs_copy_options_overwrite_existing)))
#endif
    return cfs_errc_success;
#else
  (void)options;
  /* stub */
  return cfs_errc_success;
#endif
  if (ec)
    (void)cfs_get_last_error(ec);
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_create_symlink filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_symlink(const cfs_path *target,
                                               const cfs_path *link,
                                               cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
#if defined(CFS_OS_WINDOWS)
  (void)target;
  (void)link;
  /* Needs dynamic loading or Vista+ */
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
#elif defined(CFS_OS_DOS)
  (void)target;
  (void)link;
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
#else
  if (symlink(target->str, link->str) != 0) {
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_unknown_error;
  }
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_lexically_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the path_lexically_relative
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_lexically_relative(const cfs_path *p,
                                                        const cfs_path *base,
                                                        cfs_path *out) {
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  (void)base;
  if (p)
    (void)cfs_path_assign(out, p->str);
  return cfs_errc_success; /* simplified stub */
}

/**
 * \brief Performs the cfs_path_lexically_proximate filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the path_lexically_proximate
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_lexically_proximate(const cfs_path *p,
                                                         const cfs_path *base,
                                                         cfs_path *out) {
  return cfs_path_lexically_relative(p, base, out); /* simplified stub */
}

/**
 * \brief Performs the cfs_path_is_absolute filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_absolute operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_is_absolute(const cfs_path *p,
                                                 cfs_bool *out) {
  cfs_size_t root_name_len;
  cfs_size_t root_dir_len;

  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;

  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);

#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (root_name_len > 0 && root_dir_len > 0) {
    *out = cfs_true;
  } else {
    *out = cfs_false;
  }
#else
  if (root_dir_len > 0)
    *out = cfs_true;
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_current_path filesystem operation.
 *
 * \param out Pointer to store the result of the current_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_current_path(cfs_path *out,
                                             cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!out) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_not_enough_memory;
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
      (void)cfs_path_assign(out, buf);
      return cfs_errc_success;
    }
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_not_enough_memory;
  }
#elif defined(CFS_OS_DOS)
  {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return cfs_errc_not_enough_memory;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    if (getcwd(buf, sizeof(buf)) != NULL) {
      (void)cfs_path_assign(out, buf);
      return cfs_errc_success;
    }
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_not_enough_memory;
  }
#endif
}

/**
 * \brief Performs the cfs_current_path_set filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_current_path_set(const cfs_path *p,
                                                 cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || !p->str) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (SetCurrentDirectoryW(p->str) == 0) {
#else
  if (SetCurrentDirectoryA(p->str) == 0) {
#endif
    if (ec)
      (void)cfs_get_last_error(ec);
  }
#elif defined(CFS_OS_DOS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
#else
  if (chdir(p->str) != 0) {
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_unknown_error;
  }
#endif
  return cfs_errc_success;
}

/* --- Auto-generated stubs for missing functions --- */
/**
 * \brief Registers a global callback hook to trigger when dynamic memory
 * allocation fails.
 *
 * \param handler Argument representing the target resource.
 */
CFS_API void cfs_set_oom_handler(cfs_oom_handler_t handler) { (void)handler; }

/**
 * \brief Performs the cfs_path_root_name filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_name operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_root_name(const cfs_path *p,
                                               cfs_path *out) {
  cfs_size_t len;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  cfs_char_t *buf;
#endif
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p)
    return cfs_errc_not_enough_memory;
  len = cfs_get_root_name_len(p);
  (void)len;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (len == 0)
    return cfs_errc_success;
  if (cfs_calloc(len + 1, sizeof(cfs_char_t), (void **)&buf) != 0)
    return cfs_errc_not_enough_memory;
  CFS_STRNCPY_SAFE(buf, len + 1, p->str, len);
  (void)cfs_path_assign(out, buf);
  (void)cfs_free(buf);
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_root_directory(const cfs_path *p,
                                                    cfs_path *out) {
  cfs_size_t name_len;
  cfs_size_t dir_len;
  cfs_char_t buf[2];
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p)
    return cfs_errc_not_enough_memory;
  name_len = cfs_get_root_name_len(p);
  dir_len = cfs_get_root_dir_len(p, name_len);
  if (dir_len > 0) {
    buf[0] = p->str[name_len];
    buf[1] = CFS_CHAR('\0');
    (void)cfs_path_assign(out, buf);
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_root_path(const cfs_path *p,
                                               cfs_path *out) {
  cfs_size_t name_len;
  cfs_size_t dir_len;
  cfs_size_t total_len;
  cfs_char_t *buf;
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p)
    return cfs_errc_not_enough_memory;
  name_len = cfs_get_root_name_len(p);
  dir_len = cfs_get_root_dir_len(p, name_len);
  total_len = name_len + dir_len;
  if (total_len == 0)
    return cfs_errc_success;
  if (cfs_calloc(total_len + 1, sizeof(cfs_char_t), (void **)&buf) != 0)
    return cfs_errc_not_enough_memory;
  CFS_STRNCPY_SAFE(buf, total_len + 1, p->str, total_len);
  (void)cfs_path_assign(out, buf);
  (void)cfs_free(buf);
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_relative_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_relative_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_relative_path(const cfs_path *p,
                                                   cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t rel_len;
  cfs_char_t *buf;
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;
  if (p->length <= root_len)
    return cfs_errc_success;
  rel_len = p->length - root_len;
  if (cfs_calloc(rel_len + 1, sizeof(cfs_char_t), (void **)&buf) != 0)
    return cfs_errc_not_enough_memory;
  CFS_STRNCPY_SAFE(buf, rel_len + 1, p->str + root_len, rel_len);
  (void)cfs_path_assign(out, buf);
  (void)cfs_free(buf);
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_parent_path(const cfs_path *p,
                                                 cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t out_len;
  cfs_bool is_sep = cfs_false;
  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  if (cfs_path_clone(out, p) != 0)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_remove_filename(out);
  root_name_len = cfs_get_root_name_len(out);
  root_dir_len = cfs_get_root_dir_len(out, root_name_len);
  root_len = root_name_len + root_dir_len;
  out_len = out->length;
  while (out_len > root_len) {
    (void)cfs_is_separator(out->str[out_len - 1], &is_sep);
    if (!is_sep)
      break;
    out_len--;
  }
  if (out_len < out->length) {
    out->length = out_len;
    out->str[out_len] = CFS_CHAR('\0');
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_replace_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param replacement Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_path_replace_filename(cfs_path *p, const cfs_char_t *replacement) {
  if (!p)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_remove_filename(p);
  if (replacement) {
    if (cfs_path_append(p, replacement) != 0)
      return cfs_errc_not_enough_memory;
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_replace_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param replacement Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_path_replace_extension(cfs_path *p, const cfs_char_t *replacement) {
  cfs_path ext;
  if (!p)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(&ext);
  if (cfs_path_extension(p, &ext) == 0) {
    p->length -= ext.length;
    if (p->str) {
      p->str[p->length] = CFS_CHAR('\0');
    }
    (void)cfs_path_destroy(&ext);
  }
  if (replacement && replacement[0] != CFS_CHAR('\0')) {
    if (replacement[0] != CFS_CHAR('.')) {
      if (cfs_path_concat(p, CFS_STR(".")) != 0)
        return cfs_errc_not_enough_memory;
    }
    if (cfs_path_concat(p, replacement) != 0)
      return cfs_errc_not_enough_memory;
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_remove_filename filesystem operation.
 *
 * \param p Argument representing the target resource.
 */
CFS_API void cfs_path_remove_filename(cfs_path *p) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t i, start_idx;
  cfs_bool is_sep = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;
  if (p->length == root_len)
    return;
  (void)cfs_is_separator(p->str[p->length - 1], &is_sep);
  if (is_sep)
    return;
  start_idx = root_len;
  for (i = p->length; i > root_len; i--) {
    (void)cfs_is_separator(p->str[i - 1], &is_sep);
    if (is_sep) {
      start_idx = i;
      break;
    }
  }
  p->length = start_idx;
  p->str[p->length] = CFS_CHAR('\0');
}

/**
 * \brief Performs the cfs_path_has_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_root_path(const cfs_path *p,
                                                   cfs_bool *out) {
  cfs_size_t root_name_len;
  cfs_size_t root_dir_len;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  if ((root_name_len + root_dir_len) > 0)
    *out = cfs_true;
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_has_root_name filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_name operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_root_name(const cfs_path *p,
                                                   cfs_bool *out) {
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (cfs_get_root_name_len(p) > 0)
    *out = cfs_true;
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_has_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_directory
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_root_directory(const cfs_path *p,
                                                        cfs_bool *out) {
  cfs_size_t root_name_len;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  if (cfs_get_root_dir_len(p, root_name_len) > 0)
    *out = cfs_true;
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_has_relative_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_relative_path
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_relative_path(const cfs_path *p,
                                                       cfs_bool *out) {
  cfs_size_t root_name_len, root_dir_len;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  if (p->length > (root_name_len + root_dir_len))
    *out = cfs_true;
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_has_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_parent_path(const cfs_path *p,
                                                     cfs_bool *out) {
  cfs_path parent;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  if (cfs_path_parent_path(p, &parent) == 0) {
    if (parent.length > 0)
      *out = cfs_true;
    (void)cfs_path_destroy(&parent);
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_has_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_filename(const cfs_path *p,
                                                  cfs_bool *out) {
  cfs_path fn;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  if (cfs_path_filename(p, &fn) == 0) {
    if (fn.length > 0)
      *out = cfs_true;
    (void)cfs_path_destroy(&fn);
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_has_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_stem(const cfs_path *p,
                                              cfs_bool *out) {
  cfs_path stem;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  if (cfs_path_stem(p, &stem) == 0) {
    if (stem.length > 0)
      *out = cfs_true;
    (void)cfs_path_destroy(&stem);
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_has_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_has_extension(const cfs_path *p,
                                                   cfs_bool *out) {
  cfs_path ext;
  if (!out)
    return cfs_errc_not_enough_memory;
  *out = cfs_false;
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;
  if (cfs_path_extension(p, &ext) == 0) {
    if (ext.length > 0)
      *out = cfs_true;
    (void)cfs_path_destroy(&ext);
  }
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_path_is_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_relative operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_is_relative(const cfs_path *p,
                                                 cfs_bool *out) {
  (void)p;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_path_compare filesystem operation.
 *
 * \param lhs Argument representing the target resource.
 * \param rhs Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_compare(const cfs_path *lhs,
                                             const cfs_path *rhs) {
  (void)lhs;
  (void)rhs;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_path_lexically_normal filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_lexically_normal
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_path_lexically_normal(const cfs_path *p,
                                                      cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t i, start, end;
  cfs_bool is_sep = cfs_false;
  cfs_size_t comps_len = 0;
  struct cfs_path_component {
    cfs_size_t s, e;
  } comps[128];

  if (!out)
    return cfs_errc_not_enough_memory;
  (void)cfs_path_init(out);
  if (!p || p->length == 0 || !p->str)
    return cfs_errc_success;

  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;

#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (root_name_len > 0) {
    cfs_char_t *rn_buf;
    if (cfs_calloc(root_name_len + 1, sizeof(cfs_char_t), (void **)&rn_buf) !=
        0)
      return cfs_errc_not_enough_memory;
    CFS_STRNCPY_SAFE(rn_buf, root_name_len + 1, p->str, root_name_len);
    (void)cfs_path_assign(out, rn_buf);
    (void)cfs_free(rn_buf);
  }
#endif
  if (root_dir_len > 0) {
    (void)cfs_path_concat(out, PATH_SEP_STR);
  }

  start = root_len;
  while (start < p->length) {
    (void)cfs_is_separator(p->str[start], &is_sep);
    if (is_sep) {
      start++;
      continue;
    }
    end = start;
    while (end < p->length) {
      (void)cfs_is_separator(p->str[end], &is_sep);
      if (is_sep)
        break;
      end++;
    }

    if (end - start == 1 && p->str[start] == CFS_CHAR('.')) {
      /* ignore . */
    } else if (end - start == 2 && p->str[start] == CFS_CHAR('.') &&
               p->str[start + 1] == CFS_CHAR('.')) {
      if (comps_len > 0) {
        cfs_size_t prev_s = comps[comps_len - 1].s;
        cfs_size_t prev_e = comps[comps_len - 1].e;
        if (!(prev_e - prev_s == 2 && p->str[prev_s] == CFS_CHAR('.') &&
              p->str[prev_s + 1] == CFS_CHAR('.'))) {
          comps_len--;
        } else {
          if (comps_len < 128) {
            comps[comps_len].s = start;
            comps[comps_len].e = end;
            comps_len++;
          }
        }
      } else if (root_dir_len == 0) {
        if (comps_len < 128) {
          comps[comps_len].s = start;
          comps[comps_len].e = end;
          comps_len++;
        }
      }
    } else {
      if (comps_len < 128) {
        comps[comps_len].s = start;
        comps[comps_len].e = end;
        comps_len++;
      }
    }
    start = end;
  }

  for (i = 0; i < comps_len; i++) {
    cfs_size_t len = comps[i].e - comps[i].s;
    cfs_char_t *buf;
    if (i > 0 || (root_name_len > 0 && root_dir_len == 0)) {
      (void)cfs_path_concat(out, PATH_SEP_STR);
    }
    if (cfs_calloc(len + 1, sizeof(cfs_char_t), (void **)&buf) != 0)
      return cfs_errc_not_enough_memory;
    CFS_STRNCPY_SAFE(buf, len + 1, p->str + comps[i].s, len);
    (void)cfs_path_concat(out, buf);
    (void)cfs_free(buf);
  }

  if (out->length == 0) {
    (void)cfs_path_assign(out, CFS_STR("."));
  }

  return cfs_errc_success;
}

/**
 * \brief Evaluates the file status and type of the given path, traversing
 * symlinks.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the status operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_status(const cfs_path *p, cfs_file_status *out,
                                       cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Evaluates the file status and type of the given path without
 * traversing symlinks.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the symlink_status operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_symlink_status(const cfs_path *p,
                                               cfs_file_status *out,
                                               cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Evaluates if a given file status indicates an existing filesystem
 * node.
 *
 * \param s The file status struct to evaluate.
 * \param out Pointer to store the result of the exists operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_exists(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Checks if a given path corresponds to an existing filesystem node.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the exists_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_exists_path(const cfs_path *p, cfs_bool *out,
                                            cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_block_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_block_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_block_file(cfs_file_status s,
                                              cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_character_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_character_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_character_file(cfs_file_status s,
                                                  cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_directory filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_directory(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_fifo filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_fifo operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_fifo(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_other filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_other operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_other(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_regular_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_regular_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_regular_file(cfs_file_status s,
                                                cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_socket filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_socket operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_socket(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_symlink filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_symlink operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_symlink(cfs_file_status s, cfs_bool *out) {
  (void)s;
  (void)out;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_is_empty_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the is_empty_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_is_empty_path(const cfs_path *p, cfs_bool *out,
                                              cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Creates a single new directory node at the given path.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_directory(const cfs_path *p,
                                                 cfs_error_code *ec) {
  (void)p;
  (void)ec;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Recursively creates a directory and any missing parent directories.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_directories(const cfs_path *p,
                                                   cfs_error_code *ec) {
  (void)p;
  (void)ec;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_create_hard_link filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_hard_link(const cfs_path *target,
                                                 const cfs_path *link,
                                                 cfs_error_code *ec) {
  (void)target;
  (void)link;
  (void)ec;
  return cfs_errc_not_supported;
}

/**
 * \brief Performs the cfs_create_directory_symlink filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_create_directory_symlink(const cfs_path *target,
                                                         const cfs_path *link,
                                                         cfs_error_code *ec) {
  (void)target;
  (void)link;
  (void)ec;
  return cfs_errc_not_supported;
}

/**
 * \brief Recursively deletes a directory node and all of its nested contents.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the remove_all operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_remove_all(const cfs_path *p, cfs_size_t *out,
                                           cfs_error_code *ec) {
  (void)p;
  (void)out;
  (void)ec;
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_rename filesystem operation.
 *
 * \param old_p Argument representing the target resource.
 * \param new_p Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_rename(const cfs_path *old_p,
                                       const cfs_path *new_p,
                                       cfs_error_code *ec) {
  (void)old_p;
  (void)new_p;
  (void)ec;
  return cfs_errc_not_supported;
}

/**
 * \brief Performs the cfs_resize_file filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param size Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc cfs_resize_file(const cfs_path *p,
                                            cfs_uintmax_t size,
                                            cfs_error_code *ec) {
  (void)p;
  (void)size;
  (void)ec;
  return cfs_errc_not_supported;
}

/**
 * \brief Performs the cfs_space filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the space operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_space(const cfs_path *p, cfs_space_info *out,
                                      cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  (void)p;
  (void)out;
  return cfs_errc_not_enough_memory;
#else
  struct statvfs sv;
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || !out)
    return cfs_errc_not_enough_memory;
  if (statvfs(p->str, &sv) == 0) {
    out->capacity = sv.f_blocks * sv.f_frsize;
    out->free = sv.f_bfree * sv.f_frsize;
    out->available = sv.f_bavail * sv.f_frsize;
    return cfs_errc_success;
  }
  if (ec)
    (void)cfs_get_last_error(ec);
  return cfs_errc_not_enough_memory;
#endif
}

/**
 * \brief Performs the cfs_last_write_time filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the last_write_time operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_last_write_time(const cfs_path *p,
                                                cfs_file_time_type *out,
                                                cfs_error_code *ec) {
  if (ec)
    (void)cfs_clear_error(ec);
  if (!p || !out)
    return cfs_errc_not_enough_memory;
#if defined(CFS_OS_WINDOWS)
  {
    struct _stat64 st;
#if defined(CFS_UNICODE)
    if (_wstat64(p->str, &st) == 0) {
      *out = (cfs_file_time_type)st.st_mtime;
      return cfs_errc_success;
    }
#else
    if (_stat64(p->str, &st) == 0) {
      *out = (cfs_file_time_type)st.st_mtime;
      return cfs_errc_success;
    }
#endif
  }
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      *out = (cfs_file_time_type)st.st_mtime;
      return cfs_errc_success;
    }
  }
#endif
  if (ec)
    (void)cfs_get_last_error(ec);
  return cfs_errc_not_enough_memory;
}

/**
 * \brief Performs the cfs_temp_directory_path filesystem operation.
 *
 * \param out Pointer to store the result of the temp_directory_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_temp_directory_path(cfs_path *out,
                                                    cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  (void)out;
  return cfs_errc_not_enough_memory;
#else
  const char *tmp;
  if (ec)
    (void)cfs_clear_error(ec);
  if (!out)
    return cfs_errc_not_enough_memory;
  tmp = getenv("TMPDIR");
  if (!tmp)
    tmp = "/tmp";
  (void)cfs_path_init_str(out, tmp);
  return cfs_errc_success;
#endif
}

/**
 * \brief Performs the cfs_dir_itr_init filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out_it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init(const cfs_path *p,
                                             cfs_directory_iterator **out_it,
                                             cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  (void)p;
  if (out_it)
    *out_it = NULL;
  return cfs_errc_not_enough_memory;
#else
  cfs_directory_iterator *it;
  if (ec)
    (void)cfs_clear_error(ec);
  if (out_it)
    *out_it = NULL;
  if (!p || !out_it)
    return cfs_errc_not_enough_memory;
  if (cfs_malloc(sizeof(cfs_directory_iterator), (void **)&it) != 0) {
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return cfs_errc_not_enough_memory;
  }
  it->is_end = cfs_false;
  (void)cfs_path_init(&it->current.path);
  it->dirp = opendir(p->str);
  if (!it->dirp) {
    (void)cfs_free(it);
    if (ec)
      (void)cfs_get_last_error(ec);
    return cfs_errc_not_enough_memory;
  }
  *out_it = it;
  return cfs_errc_success;
#endif
}

/**
 * \brief Performs the cfs_dir_itr_next filesystem operation.
 *
 * \param it Argument representing the target resource.
 * \param out_entry Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc
cfs_dir_itr_next(cfs_directory_iterator *it,
                 const cfs_directory_entry **out_entry, cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  (void)it;
  (void)out_entry;
  return cfs_errc_not_enough_memory;
#else
  struct dirent *dp;
  if (ec)
    (void)cfs_clear_error(ec);
  if (!it || !out_entry)
    return cfs_errc_not_enough_memory;
  if (it->is_end)
    return 1;
  dp = readdir(it->dirp);
  if (!dp) {
    it->is_end = cfs_true;
    return 1;
  }
  (void)cfs_path_init_str(&it->current.path, dp->d_name);
  *out_entry = &it->current;
  return cfs_errc_success;
#endif
}

/**
 * \brief Performs the cfs_dir_itr_close filesystem operation.
 *
 * \param void Argument representing the target resource.
 */
CFS_API void cfs_dir_itr_close(cfs_directory_iterator *it) {
#if defined(CFS_OS_WINDOWS)
  (void)it;
#else
  if (!it)
    return;
  if (it->dirp)
    closedir(it->dirp);
  (void)cfs_path_destroy(&it->current.path);
  (void)cfs_free(it);
#endif
}

/**
 * \brief Performs the cfs_rec_dir_itr_init filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out_it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_init(
    const cfs_path *p, cfs_recursive_directory_iterator **out_it,
    cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  (void)p;
  if (out_it)
    *out_it = NULL;
  return cfs_errc_not_enough_memory;
#else
  cfs_recursive_directory_iterator *it;
  cfs_directory_iterator *base_it;
  if (ec)
    (void)cfs_clear_error(ec);
  if (out_it)
    *out_it = NULL;
  if (!p || !out_it)
    return cfs_errc_not_enough_memory;
  if (cfs_dir_itr_init(p, &base_it, ec) != 0)
    return cfs_errc_not_enough_memory;
  if (cfs_malloc(sizeof(cfs_recursive_directory_iterator), (void **)&it) != 0) {
    (void)cfs_dir_itr_close(base_it);
    if (ec)
      (void)cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return cfs_errc_not_enough_memory;
  }
  it->base = *base_it;
  (void)cfs_free(base_it);
  *out_it = it;
  return cfs_errc_success;
#endif
}

/**
 * \brief Performs the cfs_rec_dir_itr_next filesystem operation.
 *
 * \param it Argument representing the target resource.
 * \param out_entry Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_next(
    cfs_recursive_directory_iterator *it, const cfs_directory_entry **out_entry,
    cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  (void)it;
  (void)out_entry;
  return cfs_errc_not_enough_memory;
#else
  if (!it)
    return cfs_errc_not_enough_memory;
  /* Not a true recursive implementation yet, just falls back to base dir
   * iteration for POSIX stub */
  return cfs_dir_itr_next(&it->base, out_entry, ec);
#endif
}

/**
 * \brief Performs the cfs_rec_dir_itr_disable_recursion_pending filesystem
 * operation.
 *
 * \param it Argument representing the target resource.
 */
CFS_API void cfs_rec_dir_itr_disable_recursion_pending(
    cfs_recursive_directory_iterator *it) {
#if defined(CFS_OS_WINDOWS)
  (void)it;
#else
  (void)it; /* Recursion not fully tracked in basic stub yet */
#endif
}

/**
 * \brief Performs the cfs_rec_dir_itr_pop filesystem operation.
 *
 * \param it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
NO_DISCARD CFS_API cfs_errc
cfs_rec_dir_itr_pop(cfs_recursive_directory_iterator *it, cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  if (ec)
    (void)cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  (void)it;
#else
  if (ec)
    (void)cfs_clear_error(ec);
  if (it)
    it->base.is_end = cfs_true;
#endif
  return cfs_errc_success;
}

/**
 * \brief Performs the cfs_rec_dir_itr_close filesystem operation.
 *
 * \param it Argument representing the target resource.
 */
CFS_API void cfs_rec_dir_itr_close(cfs_recursive_directory_iterator *it) {
#if defined(CFS_OS_WINDOWS)
  (void)it;
#else
  if (!it)
    return;
  if (it->base.dirp)
    closedir(it->base.dirp);
  (void)cfs_path_destroy(&it->base.current.path);
  (void)cfs_free(it);
#endif
}

#endif /* CFS_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CFS_H */
