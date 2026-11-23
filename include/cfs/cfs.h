/**
 * \file cfs.h
 * \brief Unified cross-platform filesystem API definitions and data structures.
 */
#ifndef CFS_H
#define CFS_H

#if defined(CFS_HEADER_ONLY_MODE) && !defined(CFS_IMPLEMENTATION)
#define CFS_IMPLEMENTATION
#endif

/* clang-format off */
#include <stddef.h>
#include "cfs/log.h"
#include "cfs/no_discard.h"

#ifdef CFS_IMPLEMENTATION
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>
#if defined(_WIN32) || defined(_WIN64)
/** @brief Excludes rarely-used APIs from Win32 headers. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <io.h>
#include <direct.h>
#elif (defined(__WATCOMC__) && defined(__DOS__)) || defined(__MSDOS__) ||      \
    defined(_MSDOS) || defined(__DOS__) || defined(CFS_OS_DOS)
#include <direct.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/types.h>
#include <sys/utime.h>
#else
#include <dirent.h>
#if defined(CFS_MULTITHREADED)
#include <pthread.h>
#endif
#if !defined(__EMSCRIPTEN__)
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#endif
#endif
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Phase 3: Platform & Compiler Detection Macros */

/* 22-25. OS Detection */
#if defined(_WIN32) || defined(_WIN64)
/** @brief Indicates a Microsoft Windows target platform. */
#define CFS_OS_WINDOWS
#elif defined(__EMSCRIPTEN__)
/** @brief Indicates an Emscripten WebAssembly target platform. */
#define CFS_OS_EMSCRIPTEN
#elif defined(__linux__)
/** @brief Indicates a Linux target platform. */
#define CFS_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
/** @brief Indicates an Apple macOS/Darwin target platform. */
#define CFS_OS_MACOS
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__bsdi__) || defined(__DragonFly__)
/** @brief Indicates a BSD target platform. */
#define CFS_OS_BSD
#elif (defined(__WATCOMC__) && defined(__DOS__)) || defined(__MSDOS__) ||      \
    defined(_MSDOS) || defined(__DOS__)
/** @brief Indicates an MS-DOS / Watcom target platform. */
#define CFS_OS_DOS
#else
/* Unknown OS fallback (assume POSIX by default if needed) */
#endif

/* 29-30. Environment Detection */
#if defined(__MINGW32__) || defined(__MINGW64__)
/** @brief Indicates a MinGW toolchain environment. */
#define CFS_ENV_MINGW
#elif defined(__CYGWIN__)
/** @brief Indicates a Cygwin toolchain environment. */
#define CFS_ENV_CYGWIN
#endif

/* 26-28. Compiler Detection */
#if defined(_MSC_VER)
/** @brief Indicates the Microsoft Visual C++ compiler version. */
#define CFS_COMPILER_MSVC _MSC_VER
#elif defined(__clang__)
/** @brief Indicates the Clang major compiler version. */
#define CFS_COMPILER_CLANG __clang_major__
#elif defined(__GNUC__) || defined(__GNUG__)
/** @brief Indicates the GCC major compiler version. */
#define CFS_COMPILER_GCC __GNUC__
#else
/* Unknown Compiler */
#endif
/* Phase 4: Distribution Modes Scaffolding */

/* 31. Implement CFS_API macro */
#if defined(CFS_OS_WINDOWS) || defined(__CYGWIN__)
#if defined(CFS_BUILD_SHARED)
#if defined(CFS_EXPORTS)
/** @brief Symbol export visibility annotation for shared builds. */
#define CFS_API __declspec(dllexport)
#else
/** @brief Symbol import visibility annotation for shared builds. */
#define CFS_API __declspec(dllimport)
#endif
#else
/** @brief Symbol visibility annotation for static builds. */
#define CFS_API
#endif
#else
#if defined(CFS_BUILD_SHARED) && defined(__GNUC__) && __GNUC__ >= 4
/** @brief Symbol export visibility annotation for ELF/Mach-O shared builds. */
#define CFS_API __attribute__((visibility("default")))
#else
/** @brief Default symbol visibility annotation. */
#define CFS_API
#endif
#endif

/* 32. Implement CFS_INLINE macro */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
/** @brief Cross-platform inline function specifier. */
#define CFS_INLINE inline
#elif defined(__GNUC__)
/** @brief Cross-platform inline function specifier. */
#define CFS_INLINE __inline__
#elif defined(_MSC_VER)
/** @brief Cross-platform inline function specifier. */
#define CFS_INLINE __inline
#else
/** @brief Cross-platform inline function specifier fallback for C89. */
#define CFS_INLINE static
#endif

/* 37. Thread-local storage macros */
#if defined(CFS_MULTITHREADED)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L &&                \
    !defined(__STDC_NO_THREADS__)
/** @brief Thread-local storage qualifier. */
#define CFS_THREAD_LOCAL _Thread_local
#elif defined(CFS_COMPILER_MSVC)
/** @brief Thread-local storage qualifier. */
#define CFS_THREAD_LOCAL __declspec(thread)
#elif defined(CFS_COMPILER_GCC) || defined(CFS_COMPILER_CLANG)
/** @brief Thread-local storage qualifier. */
#define CFS_THREAD_LOCAL __thread
#else
/** @brief Thread-local storage qualifier fallback. */
#define CFS_THREAD_LOCAL
#endif
#else
/** @brief Single-threaded storage qualifier fallback. */
#define CFS_THREAD_LOCAL
#endif

/* 39. LTO Attribute Macros */
#if defined(CFS_ENABLE_LTO) &&                                                 \
    (defined(CFS_COMPILER_GCC) || defined(CFS_COMPILER_CLANG))
/** @brief Marks symbols externally visible under link-time optimization. */
#define CFS_EXTERNALLY_VISIBLE __attribute__((externally_visible))
#else
/** @brief Fallback LTO external visibility macro. */
#define CFS_EXTERNALLY_VISIBLE
#endif

/* Phase 5: Memory & Core Types */

#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
/** @brief Native path character type (`wchar_t` on Windows Unicode). */
typedef wchar_t cfs_char_t;
/** @brief Character literal wrapper matching `cfs_char_t`. */
#define CFS_CHAR(c) L##c
/** @brief String literal wrapper matching `cfs_char_t`. */
#define CFS_STR(s) L##s
#else
/** @brief Native path character type (`char` on POSIX and ANSI builds). */
typedef char cfs_char_t;
/** @brief Character literal wrapper matching `cfs_char_t`. */
#define CFS_CHAR(c) c
/** @brief String literal wrapper matching `cfs_char_t`. */
#define CFS_STR(s) s
#endif

/** @brief Boolean type strictly mapping to C89 integers. */
typedef int cfs_bool;
/** @brief Boolean true constant. */
#define cfs_true 1
/** @brief Boolean false constant. */
#define cfs_false 0

/** @brief Size type mapping to standard `size_t`. */
typedef size_t cfs_size_t;

/* 49. Define system-specific maximum path length macros */
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
#ifndef CFS_MAX_PATH
/** @brief Platform default maximum path buffer length. */
#define CFS_MAX_PATH 260
#endif
#else
#ifndef CFS_MAX_PATH
/** @brief Platform default maximum path buffer length. */
#define CFS_MAX_PATH 4096
#endif
#endif

/* Format Specifiers Abstractions */
#if defined(_MSC_VER)
/** @brief Signed 64-bit integer printf format specifier. */
#define CFS_NUM_FORMAT "%I64d"
/** @brief Unsigned 64-bit integer printf format specifier. */
#define CFS_UNUM_FORMAT "%I64u"
#elif defined(__LP64__) || defined(_LP64)
/** @brief Signed 64-bit integer printf format specifier on LP64 C89. */
#define CFS_NUM_FORMAT "%ld"
/** @brief Unsigned 64-bit integer printf format specifier on LP64 C89. */
#define CFS_UNUM_FORMAT "%lu"
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Signed 64-bit integer printf format specifier on LLP64/ILP32. */
#define CFS_NUM_FORMAT "%lld"
/** @brief Unsigned 64-bit integer printf format specifier on LLP64/ILP32. */
#define CFS_UNUM_FORMAT "%llu"
#else
/** @brief Signed integer printf format specifier on C89 targets. */
#define CFS_NUM_FORMAT "%ld"
/** @brief Unsigned integer printf format specifier on C89 targets. */
#define CFS_UNUM_FORMAT "%lu"
#endif

/** \brief POSIX-compatible error code enumeration for c-fs operations. */
typedef enum CFS_NO_DISCARD_TYPE cfs_errc {
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
} cfs_errc CFS_NO_DISCARD_TYPEDEF;

/** \brief Global Out-Of-Memory Error Fallback Hook type. */
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_malloc(cfs_size_t size, void **out);
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_realloc(void *ptr, cfs_size_t new_size,
                                            void **out);
/**
 * \brief Allocates zero-initialized memory for an array of elements.
 *
 * \param num Number of elements to allocate.
 * \param size Argument representing the target resource.
 * \param out Pointer to store the result of the calloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_calloc(cfs_size_t num, cfs_size_t size,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_strlen(const cfs_char_t *str,
                                           cfs_size_t *out);
/**
 * \brief Copies a string into a destination buffer safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcpy operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strcpy(cfs_char_t *dest,
                                           const cfs_char_t *src,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_strncpy(cfs_char_t *dest,
                                            const cfs_char_t *src, cfs_size_t n,
                                            cfs_char_t **out);
/**
 * \brief Concatenates two strings safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcat operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strcat(cfs_char_t *dest,
                                           const cfs_char_t *src,
                                           cfs_char_t **out);
/**
 * \brief Compares two strings lexicographically.
 *
 * \param lhs The left-hand side string for comparison.
 * \param rhs The right-hand side string for comparison.
 * \param out Pointer to store the result of the strcmp operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strcmp(const cfs_char_t *lhs,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_strncmp(const cfs_char_t *lhs,
                                            const cfs_char_t *rhs,
                                            cfs_size_t count, int *out);

/* 55-59. Charset & Conversion logic */
#if defined(_MSC_VER)
#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
/** \brief Safe string copy macro using MSVC Safe CRT or C89 fallback. */
#define CFS_STRCPY_SAFE(dst, dst_sz, src) wcscpy_s((dst), (dst_sz), (src))
/** \brief Safe bounded string copy macro using MSVC Safe CRT or C89 fallback.
 */
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  wcsncpy_s((dst), (dst_sz), (src), (n))
#else
/** \brief Safe string copy macro using MSVC Safe CRT or C89 fallback. */
#define CFS_STRCPY_SAFE(dst, dst_sz, src) strcpy_s((dst), (dst_sz), (src))
/** \brief Safe bounded string copy macro using MSVC Safe CRT or C89 fallback.
 */
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  strncpy_s((dst), (dst_sz), (src), (n))
#endif
#elif defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
/** \brief Safe string copy macro using MSVC Safe CRT or C89 fallback. */
#define CFS_STRCPY_SAFE(dst, dst_sz, src) ((void)(dst_sz), wcscpy((dst), (src)))
/** \brief Safe bounded string copy macro using MSVC Safe CRT or C89 fallback.
 */
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  ((void)(dst_sz), wcsncpy((dst), (src), (n)))
#else
/** \brief Safe string copy macro using MSVC Safe CRT or C89 fallback. */
#define CFS_STRCPY_SAFE(dst, dst_sz, src) ((void)(dst_sz), strcpy((dst), (src)))
/** \brief Safe bounded string copy macro using MSVC Safe CRT or C89 fallback.
 */
#define CFS_STRNCPY_SAFE(dst, dst_sz, src, n)                                  \
  ((void)(dst_sz), strncpy((dst), (src), (n)))
#endif

#if defined(_MSC_VER)
/** \brief Safe memory copy macro using MSVC Safe CRT memcpy_s. */
#define CFS_MEMCPY_SAFE(dst, dst_sz, src, count)                               \
  memcpy_s((dst), (dst_sz), (src), (count))
/** \brief Safe single-argument formatting macro using MSVC Safe CRT sprintf_s.
 */
#define CFS_SPRINTF_1_SAFE(dst, dst_sz, fmt, arg)                              \
  sprintf_s((dst), (dst_sz), (fmt), (arg))
#elif defined(__GNUC__) || defined(__clang__)
/** \brief Safe memory copy macro falling back to C89 memcpy. */
#define CFS_MEMCPY_SAFE(dst, dst_sz, src, count)                               \
  ((void)(dst_sz), memcpy((dst), (src), (count)))
/** \brief Safe single-argument formatting macro falling back to C89 sprintf. */
#define CFS_SPRINTF_1_SAFE(dst, dst_sz, fmt, arg)                              \
  ((void)(dst_sz), __extension__ sprintf((dst), (fmt), (arg)))
#else
/** \brief Safe memory copy macro falling back to C89 memcpy. */
#define CFS_MEMCPY_SAFE(dst, dst_sz, src, count)                               \
  ((void)(dst_sz), memcpy((dst), (src), (count)))
/** \brief Safe single-argument formatting macro falling back to C89 sprintf. */
#define CFS_SPRINTF_1_SAFE(dst, dst_sz, fmt, arg)                              \
  ((void)(dst_sz), sprintf((dst), (fmt), (arg)))
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_utf8_to_utf16(const char *utf8_str,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_utf16_to_utf8(const wchar_t *utf16_str,
                                                  char *dest,
                                                  cfs_size_t dest_len,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_mb_to_wide(const char *mb_str,
                                               wchar_t *dest,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_wide_to_mb(const wchar_t *wide_str,
                                               char *dest, cfs_size_t dest_len,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_error_message(cfs_errc err,
                                                  const char **out);

/** \brief 65-67. OS Translation Hooks */
/**
 * \brief Populates an error structure based on a raw OS error code,
 * automatically mapping to the POSIX enum.
 *
 * \param os_error The raw system error code.
 * \param out Pointer to store the result of the make_error_code_from_os
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_make_error_code_from_os(int os_error, cfs_error_code *out);
/**
 * \brief Retrieves the last thread-local system error (errno or GetLastError)
 * and wraps it into a `cfs_error_code`.
 *
 * \param out Pointer to store the result of the get_last_error operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_get_last_error(cfs_error_code *out);
/* Phase 8: Path Struct Basics */

/* 76. Define platform-specific path separator macros */
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
/** @brief Preferred native path separator character. */
#define CFS_PREFERRED_SEPARATOR CFS_CHAR('\\')
/** @brief Native path separator character alias. */
#define PATH_SEP_CHAR CFS_CHAR('\\')
/** @brief Native path separator string literal. */
#define PATH_SEP_STR CFS_STR("\\")
#else
/** @brief Preferred native path separator character. */
#define CFS_PREFERRED_SEPARATOR CFS_CHAR('/')
/** @brief Native path separator character alias. */
#define PATH_SEP_CHAR CFS_CHAR('/')
/** @brief Native path separator string literal. */
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_init_str(cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_clone(cfs_path *dest,
                                               const cfs_path *src);
/**
 * \brief Mutates the path in-place, converting all directory separators to the
 * native OS preferred separator.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_make_preferred(cfs_path *p);
/**
 * \brief Retrieves the underlying null-terminated C string from a path.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_c_str operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_c_str(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_generic_string(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_assign(cfs_path *p,
                                                const cfs_char_t *source);
/**
 * \brief Appends a source string to the path, resolving directory separators
 * intelligently.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to append to the path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_append(cfs_path *p,
                                                const cfs_char_t *source);
/**
 * \brief Concatenates a string directly to the end of the path without
 * inserting separators.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to concatenate.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_concat(cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_root_name(const cfs_path *p,
                                                   cfs_path *out);
/** \brief Extracts base root separator. Returns path instance. */
/**
 * \brief Performs the cfs_path_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_root_directory(const cfs_path *p,
                                                        cfs_path *out);
/** \brief Combines root name and root directory. Returns path instance. */
/**
 * \brief Performs the cfs_path_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_root_path(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_relative_path(const cfs_path *p,
                                                       cfs_path *out);
/** \brief 102. Returns path of the parent directory */
/**
 * \brief Performs the cfs_path_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_parent_path(const cfs_path *p,
                                                     cfs_path *out);
/** \brief 103. Returns the filename component */
/**
 * \brief Performs the cfs_path_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_filename(const cfs_path *p,
                                                  cfs_path *out);
/** \brief 104. Returns the stem (filename without extension) */
/**
 * \brief Performs the cfs_path_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_stem(const cfs_path *p, cfs_path *out);
/** \brief 105. Returns the file extension */
/**
 * \brief Performs the cfs_path_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_extension(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc
cfs_path_replace_filename(cfs_path *p, const cfs_char_t *replacement);
/** \brief 112. Replaces the extension of the terminal component */
/**
 * \brief Performs the cfs_path_replace_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param replacement Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_absolute(const cfs_path *p, cfs_path *out,
                                             cfs_error_code *ec);

/* Copy file options mirroring std::filesystem::copy_options */
/**
 * \brief Data structure for cfs_copy_options.
 */
typedef enum cfs_copy_options {
  /** \brief Default copy behavior; error if destination exists. */
  cfs_copy_options_none = 0,
  /** \brief Skip existing destination files without error. */
  cfs_copy_options_skip_existing = 1,
  /** \brief Replace existing destination files unconditionally. */
  cfs_copy_options_overwrite_existing = 2,
  /** \brief Replace existing destination files only if source is newer. */
  cfs_copy_options_update_existing = 4,
  /** \brief Recursively copy subdirectories and their contents. */
  cfs_copy_options_recursive = 8,
  /** \brief Copy symbolic links as symbolic links rather than targets. */
  cfs_copy_options_copy_symlinks = 16,
  /** \brief Ignore symbolic links encountered during copy. */
  cfs_copy_options_skip_symlinks = 32,
  /** \brief Copy directory structure without copying non-directory files. */
  cfs_copy_options_directories_only = 64,
  /** \brief Create symbolic links instead of copying file contents. */
  cfs_copy_options_create_symlinks = 128,
  /** \brief Create hard links instead of copying file contents. */
  cfs_copy_options_create_hard_links = 256
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_canonical(const cfs_path *p, cfs_path *out,
                                              cfs_error_code *ec);
/**
 * \brief Performs the cfs_weakly_canonical filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the weakly_canonical operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_weakly_canonical(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_read_symlink(const cfs_path *p,
                                                 cfs_path *out,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_relative(const cfs_path *p,
                                             const cfs_path *base,
                                             cfs_path *out, cfs_error_code *ec);
/**
 * \brief Performs the cfs_proximate filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the proximate operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_proximate(const cfs_path *p,
                                              const cfs_path *base,
                                              cfs_path *out,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_copy(const cfs_path *from,
                                         const cfs_path *to,
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
CFS_NO_DISCARD CFS_API cfs_errc
cfs_copy_symlink(const cfs_path *existing_symlink, const cfs_path *new_symlink,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_is_empty(const cfs_path *p,
                                                  cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_root_path(const cfs_path *p,
                                                       cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_root_name filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_name operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_root_name(const cfs_path *p,
                                                       cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_directory
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_root_directory(const cfs_path *p,
                                                            cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_relative_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_relative_path
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_relative_path(const cfs_path *p,
                                                           cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_parent_path(const cfs_path *p,
                                                         cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_filename(const cfs_path *p,
                                                      cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_stem(const cfs_path *p,
                                                  cfs_bool *out);
/**
 * \brief Performs the cfs_path_has_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_extension(const cfs_path *p,
                                                       cfs_bool *out);
/**
 * \brief Performs the cfs_path_is_absolute filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_absolute operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_is_absolute(const cfs_path *p,
                                                     cfs_bool *out);
/**
 * \brief Performs the cfs_path_is_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_relative operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_is_relative(const cfs_path *p,
                                                     cfs_bool *out);

/** \brief Lexicographical comparison */
/**
 * \brief Performs the cfs_path_compare filesystem operation.
 *
 * \param lhs Argument representing the target resource.
 * \param rhs Argument representing the target resource.
 * \param out Pointer to store the lexicographical comparison result.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_compare(const cfs_path *lhs,
                                                 const cfs_path *rhs, int *out);
/* Phase 14: Lexical Path Operations */

/** \brief 132. Lexically normalizes the path (resolves . and .. internally) */
/**
 * \brief Performs the cfs_path_lexically_normal filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_lexically_normal
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_lexically_normal(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_lexically_relative(
    const cfs_path *p, const cfs_path *base, cfs_path *out);
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_lexically_proximate(
    const cfs_path *p, const cfs_path *base, cfs_path *out);

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

/**
 * \brief Initializes a component iterator over a `cfs_path`.
 *
 * \param p Pointer to the `cfs_path` to iterate over.
 * \param out_it Pointer to the `cfs_path_iterator` to initialize.
 * \return 0 on success, or `cfs_errc_invalid_argument` if `out_it` is NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_itr_init(const cfs_path *p,
                                                  cfs_path_iterator *out_it);

/**
 * \brief Advances a `cfs_path_iterator` and retrieves the next path element.
 *
 * \param it Pointer to an initialized `cfs_path_iterator`.
 * \param out_elem Pointer to receive the slice of the next element.
 * \param out_has_elem Set to `cfs_true` if an element was produced, or
 * `cfs_false` at end of path.
 * \return 0 on success, or `cfs_errc_invalid_argument` if any pointer is NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_itr_next(cfs_path_iterator *it,
                                                  cfs_path_element *out_elem,
                                                  cfs_bool *out_has_elem);
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
 * \brief Named POSIX-style permission bitmask constants matching
 * `std::filesystem::perms`.
 */
typedef enum cfs_perms_bits {
  /** \brief No permission bits set. */
  cfs_perms_none = 0,
  /** \brief Others have execute permission (`01`). */
  cfs_perms_others_exec = 01,
  /** \brief Others have write permission (`02`). */
  cfs_perms_others_write = 02,
  /** \brief Others have read permission (`04`). */
  cfs_perms_others_read = 04,
  /** \brief Others have read, write, and execute permissions (`07`). */
  cfs_perms_others_all = 07,
  /** \brief Group has execute permission (`010`). */
  cfs_perms_group_exec = 010,
  /** \brief Group has write permission (`020`). */
  cfs_perms_group_write = 020,
  /** \brief Group has read permission (`040`). */
  cfs_perms_group_read = 040,
  /** \brief Group has read, write, and execute permissions (`070`). */
  cfs_perms_group_all = 070,
  /** \brief Owner has execute permission (`0100`). */
  cfs_perms_owner_exec = 0100,
  /** \brief Owner has write permission (`0200`). */
  cfs_perms_owner_write = 0200,
  /** \brief Owner has read permission (`0400`). */
  cfs_perms_owner_read = 0400,
  /** \brief Owner has read, write, and execute permissions (`0700`). */
  cfs_perms_owner_all = 0700,
  /** \brief All users have read, write, and execute permissions (`0777`). */
  cfs_perms_all = 0777,
  /** \brief Sticky bit (`01000`). */
  cfs_perms_sticky_bit = 01000,
  /** \brief Set-group-ID on execution (`02000`). */
  cfs_perms_set_gid = 02000,
  /** \brief Set-user-ID on execution (`04000`). */
  cfs_perms_set_uid = 04000,
  /** \brief Mask covering all permission and special mode bits (`07777`). */
  cfs_perms_mask = 07777,
  /** \brief Unknown permissions sentinel (`0xFFFF`). */
  cfs_perms_unknown = 0xFFFF
} cfs_perms_bits;

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
CFS_NO_DISCARD CFS_API cfs_errc cfs_status(const cfs_path *p,
                                           cfs_file_status *out,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_symlink_status(const cfs_path *p,
                                                   cfs_file_status *out,
                                                   cfs_error_code *ec);
/**
 * \brief Performs the cfs_status_known filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the status_known operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_status_known(cfs_file_status s,
                                                 cfs_bool *out);
/**
 * \brief Converts raw OS `st_mode` bits into a normalized `cfs_file_status`.
 *
 * \param raw_mode Raw `st_mode` bitmask from `stat`/`lstat`/`_stat64`.
 * \param out Pointer to store the decoded `cfs_file_status`.
 * \return 0 on success, or `cfs_errc_invalid_argument` if `out` is NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_status_from_mode(unsigned int raw_mode,
                                                     cfs_file_status *out);

/** \brief 145. Exists Observer */
/**
 * \brief Evaluates if a given file status indicates an existing filesystem
 * node.
 *
 * \param s The file status struct to evaluate.
 * \param out Pointer to store the result of the exists operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_exists(cfs_file_status s, cfs_bool *out);
/**
 * \brief Checks if a given path corresponds to an existing filesystem node.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the exists_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_exists_path(const cfs_path *p,
                                                cfs_bool *out,
                                                cfs_error_code *ec);

/** \brief 151-159. Filesystem Type Queries */
/**
 * \brief Performs the cfs_is_block_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_block_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_block_file(cfs_file_status s,
                                                  cfs_bool *out);
/**
 * \brief Performs the cfs_is_character_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_character_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_character_file(cfs_file_status s,
                                                      cfs_bool *out);
/**
 * \brief Performs the cfs_is_directory filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_directory(cfs_file_status s,
                                                 cfs_bool *out);
/**
 * \brief Performs the cfs_is_fifo filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_fifo operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_fifo(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_other filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_other operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_other(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_regular_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_regular_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_regular_file(cfs_file_status s,
                                                    cfs_bool *out);
/**
 * \brief Performs the cfs_is_socket filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_socket operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_socket(cfs_file_status s, cfs_bool *out);
/**
 * \brief Performs the cfs_is_symlink filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_symlink operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_symlink(cfs_file_status s,
                                               cfs_bool *out);

/** \brief 154. Is Empty Query (Directory or zero-byte file) */
/**
 * \brief Performs the cfs_is_empty_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the is_empty_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_empty_path(const cfs_path *p,
                                                  cfs_bool *out,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory(const cfs_path *p,
                                                     cfs_error_code *ec);
/**
 * \brief Creates a single new directory copying attributes/permissions from an
 * existing template directory.
 *
 * \param p Target path for the new directory.
 * \param existing_p Existing template directory path whose attributes are
 * copied.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory_attr(
    const cfs_path *p, const cfs_path *existing_p, cfs_error_code *ec);
/** \brief 162. Recursively create directory nodes */
/**
 * \brief Recursively creates a directory and any missing parent directories.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directories(const cfs_path *p,
                                                       cfs_error_code *ec);

/**
 * @brief Performs the cfs_create_hard_link filesystem operation.
 *
 * @param target Argument representing the target resource.
 * @param new_link Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_hard_link(const cfs_path *target,
                                                     const cfs_path *new_link,
                                                     cfs_error_code *ec);
/**
 * @brief Performs the cfs_create_symlink filesystem operation.
 *
 * @param target Argument representing the target resource.
 * @param link Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_symlink(const cfs_path *target,
                                                   const cfs_path *link,
                                                   cfs_error_code *ec);
/**
 * @brief Performs the cfs_create_directory_symlink filesystem operation.
 *
 * @param target Argument representing the target resource.
 * @param new_link Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory_symlink(
    const cfs_path *target, const cfs_path *new_link, cfs_error_code *ec);

/* Copy file options mirroring std::filesystem::copy_options */

/**
 * @brief Copies a single file from a source path to a destination path.
 *
 * @param from The source path pointing to the file to copy.
 * @param to The destination path for the copied file.
 * @param options Copy behavior flags mapping to `cfs_copy_options`.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_copy_file(const cfs_path *from,
                                              const cfs_path *to,
                                              cfs_copy_options options,
                                              cfs_error_code *ec);
/* Phase 18: Filesystem Operations - Modification */

/**
 * @brief Deletes a specific file or empty directory node.
 *
 * @param p Pointer to the `cfs_path` object to evaluate or modify.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove(const cfs_path *p,
                                           cfs_error_code *ec);
/**
 * @brief Recursively deletes a directory node and all of its nested contents.
 *
 * @param p Pointer to the `cfs_path` object to evaluate or modify.
 * @param out Pointer to store the result of the remove_all operation.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove_all(const cfs_path *p,
                                               cfs_size_t *out,
                                               cfs_error_code *ec);

/**
 * @brief Performs the cfs_rename filesystem operation.
 *
 * @param old_p Argument representing the target resource.
 * @param new_p Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rename(const cfs_path *old_p,
                                           const cfs_path *new_p,
                                           cfs_error_code *ec);

#if defined(_MSC_VER)
/** @brief Unsigned maximum-width integer matching `std::uintmax_t`. */
typedef unsigned __int64 cfs_uintmax_t;
#elif defined(__LP64__) || defined(_LP64)
/** @brief Unsigned maximum-width integer matching `std::uintmax_t`. */
typedef unsigned long cfs_uintmax_t;
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Unsigned maximum-width integer matching `std::uintmax_t`. */
__extension__ typedef unsigned long long cfs_uintmax_t;
#else
/** @brief Unsigned maximum-width integer matching `std::uintmax_t`. */
typedef unsigned long cfs_uintmax_t;
#endif
/**
 * @brief Performs the cfs_resize_file filesystem operation.
 *
 * @param p Pointer to the `cfs_path` object to evaluate or modify.
 * @param size Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_resize_file(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_file_size(const cfs_path *p,
                                              cfs_uintmax_t *out,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_space(const cfs_path *p,
                                          cfs_space_info *out,
                                          cfs_error_code *ec);

/** \brief Mapped natively to time_t or system tick representation. */
#if defined(_MSC_VER)
typedef __int64 cfs_file_time_type;
#elif defined(__LP64__) || defined(_LP64)
typedef long cfs_file_time_type;
#elif defined(__GNUC__) || defined(__clang__)
__extension__ typedef long long cfs_file_time_type;
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_last_write_time(const cfs_path *p,
                                                    cfs_file_time_type *out,
                                                    cfs_error_code *ec);
/**
 * \brief Updates the last modification timestamp of a filesystem node.
 *
 * \param p Pointer to the target `cfs_path` object.
 * \param new_time New modification timestamp in system epoch seconds.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_last_write_time_set(
    const cfs_path *p, cfs_file_time_type new_time, cfs_error_code *ec);

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
CFS_NO_DISCARD CFS_API cfs_errc cfs_permissions(const cfs_path *p,
                                                cfs_perms prms,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_hard_link_count(const cfs_path *p,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_equivalent(const cfs_path *p1,
                                               const cfs_path *p2,
                                               cfs_bool *out,
                                               cfs_error_code *ec);

/** \brief 178-179. Environment paths */
/**
 * \brief Performs the cfs_current_path filesystem operation.
 *
 * \param out Pointer to store the result of the current_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_current_path(cfs_path *out,
                                                 cfs_error_code *ec);
/**
 * @brief Performs the cfs_current_path_set filesystem operation.
 *
 * @param p Pointer to the `cfs_path` object to evaluate or modify.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_current_path_set(const cfs_path *p,
                                                     cfs_error_code *ec);
/**
 * @brief Performs the cfs_temp_directory_path filesystem operation.
 *
 * @param out Pointer to store the result of the temp_directory_path operation.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_temp_directory_path(cfs_path *out,
                                                        cfs_error_code *ec);
/* Phase 19: Directory Iteration */

/* 181. Directory entry structure caching path and status */
/**
 * @brief Data structure for cfs_directory_entry.
 */
typedef struct cfs_directory_entry {
  /** @brief Represents the path enumerator or field data. */
  cfs_path path;
  /** @brief Represents the status enumerator or field data. */
  cfs_file_status status;
  /** @brief Represents the symlink_status enumerator or field data. */
  cfs_file_status symlink_status;
} cfs_directory_entry;

/**
 * \brief Initializes an empty `cfs_directory_entry` with unknown cached status.
 *
 * \param entry Pointer to the `cfs_directory_entry` to initialize.
 */
CFS_API void cfs_dir_entry_init(cfs_directory_entry *entry);
/**
 * \brief Releases resources owned by a `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to destroy.
 */
CFS_API void cfs_dir_entry_destroy(cfs_directory_entry *entry);
/**
 * \brief Updates cached status attributes of a `cfs_directory_entry` from disk.
 *
 * \param entry Pointer to the `cfs_directory_entry` to refresh.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_entry_refresh(cfs_directory_entry *entry, cfs_error_code *ec);
/**
 * \brief Assigns a new path to a `cfs_directory_entry` and refreshes its cached
 * status.
 *
 * \param entry Pointer to the `cfs_directory_entry` to modify.
 * \param p New path to assign.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_assign(cfs_directory_entry *entry,
                                                     const cfs_path *p,
                                                     cfs_error_code *ec);
/**
 * \brief Replaces the terminal filename of a `cfs_directory_entry` and
 * refreshes its cached status.
 *
 * \param entry Pointer to the `cfs_directory_entry` to modify.
 * \param replacement New terminal filename string.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_replace_filename(
    cfs_directory_entry *entry, const cfs_char_t *replacement,
    cfs_error_code *ec);
/**
 * \brief Retrieves a pointer to the `cfs_path` stored in a
 * `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out_path Output pointer receiving `&entry->path`.
 * \return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_entry_path(const cfs_directory_entry *entry, const cfs_path **out_path);
/**
 * \brief Retrieves the target status (following symlinks) of a
 * `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output pointer receiving the resolved `cfs_file_status`.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_status(
    const cfs_directory_entry *entry, cfs_file_status *out, cfs_error_code *ec);
/**
 * \brief Retrieves the link status (not following symlinks) of a
 * `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output pointer receiving the resolved `cfs_file_status`.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_symlink_status(
    const cfs_directory_entry *entry, cfs_file_status *out, cfs_error_code *ec);
/**
 * \brief Checks whether the node referenced by `entry` exists.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_exists(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` refers to a regular file.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_regular_file(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` refers to a directory.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_directory(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` itself is a symbolic link.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_symlink(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` refers to a block device.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_block_file(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` refers to a character device.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_character_file(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` refers to a named pipe (FIFO).
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_fifo(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` refers to a socket.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_socket(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Checks whether `entry` refers to a non-regular, non-directory,
 * non-symlink existing node.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_other(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec);
/**
 * \brief Queries the byte size of the regular file referenced by `entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output file size in bytes.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_file_size(
    const cfs_directory_entry *entry, cfs_uintmax_t *out, cfs_error_code *ec);
/**
 * \brief Queries the hard-link count of the node referenced by `entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output hard-link count.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_hard_link_count(
    const cfs_directory_entry *entry, cfs_uintmax_t *out, cfs_error_code *ec);
/**
 * \brief Queries the last modification timestamp of the node referenced by
 * `entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output timestamp.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_entry_last_write_time(const cfs_directory_entry *entry,
                              cfs_file_time_type *out, cfs_error_code *ec);

/* 182-185. Standard Directory Iterator */
/** @brief Opaque handle for non-recursive directory iteration. */
typedef struct cfs_directory_iterator cfs_directory_iterator;

/**
 * @brief Performs the cfs_dir_itr_init filesystem operation.
 *
 * @param p Pointer to the `cfs_path` object to evaluate or modify.
 * @param out_it Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init(
    const cfs_path *p, cfs_directory_iterator **out_it, cfs_error_code *ec);
/**
 * @brief Opens a non-recursive directory iterator with explicit
 * `cfs_directory_options`.
 *
 * @param p Directory path to iterate.
 * @param opts Iteration options (such as `skip_permission_denied`).
 * @param out_it Output iterator pointer.
 * @param ec Optional output error code.
 * @return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_itr_init_opts(const cfs_path *p, cfs_directory_options opts,
                      cfs_directory_iterator **out_it, cfs_error_code *ec);
/**
 * @brief Performs the cfs_dir_itr_next filesystem operation.
 *
 * @param it Argument representing the target resource.
 * @param out_entry Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_itr_next(cfs_directory_iterator *it,
                 const cfs_directory_entry **out_entry, cfs_error_code *ec);
/**
 * @brief Performs the cfs_dir_itr_close filesystem operation.
 *
 * @param it Argument representing the target resource.
 */
CFS_API void cfs_dir_itr_close(cfs_directory_iterator *it);

/* 186-189. Recursive Directory Iterator */
/** @brief Opaque handle for recursive directory iteration. */
typedef struct cfs_recursive_directory_iterator
    cfs_recursive_directory_iterator;

/**
 * @brief Performs the cfs_rec_dir_itr_init filesystem operation.
 *
 * @param p Pointer to the `cfs_path` object to evaluate or modify.
 * @param out_it Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_init(
    const cfs_path *p, cfs_recursive_directory_iterator **out_it,
    cfs_error_code *ec);
/**
 * @brief Opens a recursive directory iterator with explicit
 * `cfs_directory_options`.
 *
 * @param p Root directory path to traverse recursively.
 * @param opts Traversal options (`follow_directory_symlink`,
 * `skip_permission_denied`).
 * @param out_it Output recursive iterator pointer.
 * @param ec Optional output error code.
 * @return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_init_opts(
    const cfs_path *p, cfs_directory_options opts,
    cfs_recursive_directory_iterator **out_it, cfs_error_code *ec);
/**
 * @brief Performs the cfs_rec_dir_itr_next filesystem operation.
 *
 * @param it Argument representing the target resource.
 * @param out_entry Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_rec_dir_itr_next(cfs_recursive_directory_iterator *it,
                     const cfs_directory_entry **out_entry, cfs_error_code *ec);
/**
 * @brief Prevents the iterator from descending into the currently evaluated
 * directory node.
 *
 * @param it Pointer to the recursive directory iterator to modify.
 */
CFS_API void
cfs_rec_dir_itr_disable_recursion_pending(cfs_recursive_directory_iterator *it);
/**
 * @brief Moves the iterator one level up in the directory tree.
 *
 * @param it Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_rec_dir_itr_pop(cfs_recursive_directory_iterator *it, cfs_error_code *ec);
/**
 * @brief Queries the current recursion depth of a recursive directory iterator.
 *
 * @param it Pointer to the recursive directory iterator.
 * @param out_depth Output depth (`0` for root directory entries).
 * @return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_depth(
    const cfs_recursive_directory_iterator *it, int *out_depth);
/**
 * @brief Queries whether subdirectory recursion is currently enabled for the
 * active entry.
 *
 * @param it Pointer to the recursive directory iterator.
 * @param out_pending Output boolean flag.
 * @return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_recursion_pending(
    const cfs_recursive_directory_iterator *it, cfs_bool *out_pending);
/**
 * @brief Performs the cfs_rec_dir_itr_close filesystem operation.
 *
 * @param it Argument representing the target resource.
 */
CFS_API void cfs_rec_dir_itr_close(cfs_recursive_directory_iterator *it);

/* Phase 5.5: Execution Context & Modality */

/* 1. Define the cfs_modality enum */
/**
 * @brief Defines the execution strategy for asynchronous requests (e.g. sync,
 * threadpool, multiprocess).
 */
typedef enum cfs_modality {
  /** @brief Represents the cfs_modality_sync enumerator or field data. */
  cfs_modality_sync,
  /** @brief Represents the cfs_modality_async enumerator or field data. */
  cfs_modality_async,
  /** @brief Represents the cfs_modality_multithread enumerator or field data.
   */
  cfs_modality_multithread,
  /** @brief Represents the cfs_modality_singlethread enumerator or field data.
   */
  cfs_modality_singlethread,
  /** @brief Represents the cfs_modality_multiprocess enumerator or field data.
   */
  cfs_modality_multiprocess,
  /** @brief Represents the cfs_modality_greenthread enumerator or field data.
   */
  cfs_modality_greenthread,
  /** @brief Message passing modality. */ cfs_modality_message_passing
} cfs_modality;

/* 2. Define cfs_runtime_config struct */
/**
 * @brief Configuration payload defining the modality and thread constraints of
 * the runtime.
 */
typedef struct cfs_runtime_config {
  /** @brief Represents the mode enumerator or field data. */
  cfs_modality mode;
  /** @brief Represents the thread_pool_size enumerator or field data. */
  cfs_size_t thread_pool_size;
  /** @brief Represents the ipc_path enumerator or field data. */
  const cfs_char_t *ipc_path;
} cfs_runtime_config;

/* 3. Create the opaque cfs_runtime_t struct */
/** @brief Opaque execution runtime context handle. */
typedef struct cfs_runtime_t cfs_runtime_t;

/* 6. Introduce cfs_request_t struct */
/** @brief Forward declaration for deferred filesystem operation request. */
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
  /** \brief Copy options bitmask for `cfs_copy` and `cfs_copy_file`. */
  cfs_copy_options copy_opts;
  /** \brief Unsigned scalar argument (such as target size for `resize_file`).
   */
  cfs_uintmax_t uint_arg;
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
  /** \brief Owning runtime context used to enforce active sandbox policies. */
  cfs_runtime_t *owner_rt;
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
CFS_NO_DISCARD CFS_API cfs_errc
cfs_runtime_init(const cfs_runtime_config *config, cfs_runtime_t **out_rt,
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
  /** \brief Recursive or shallow directory/file copy operation. */
  cfs_opcode_copy,
  /** \brief Represents the cfs_opcode_copy_file enumerator or field data. */
  cfs_opcode_copy_file,
  /** \brief Represents the cfs_opcode_rename enumerator or field data. */
  cfs_opcode_rename,
  /** \brief File truncation or extension operation. */
  cfs_opcode_resize_file,
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
  /** \brief Last write time operation. */ cfs_opcode_last_write_time,
  /** \brief Directory iterator init operation. */ cfs_opcode_dir_itr_init
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove_async(cfs_runtime_t *rt,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_file_size_async(cfs_runtime_t *rt,
                                                    const cfs_path *p,
                                                    cfs_callback_t cb,
                                                    void *user_data);
/**
 * \brief Schedules an asynchronous recursive removal (`cfs_remove_all`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path to remove recursively.
 * \param cb Completion callback (`result_buffer` holds `cfs_size_t` count).
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove_all_async(cfs_runtime_t *rt,
                                                     const cfs_path *p,
                                                     cfs_callback_t cb,
                                                     void *user_data);
/**
 * \brief Schedules an asynchronous single-directory creation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target directory path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory_async(cfs_runtime_t *rt,
                                                           const cfs_path *p,
                                                           cfs_callback_t cb,
                                                           void *user_data);
/**
 * \brief Schedules an asynchronous recursive directory creation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target directory path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directories_async(cfs_runtime_t *rt,
                                                             const cfs_path *p,
                                                             cfs_callback_t cb,
                                                             void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_copy` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param from Source path.
 * \param to Destination path.
 * \param options Copy options bitmask.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_copy_async(cfs_runtime_t *rt, const cfs_path *from, const cfs_path *to,
               cfs_copy_options options, cfs_callback_t cb, void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_copy_file` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param from Source regular file path.
 * \param to Destination file path.
 * \param options Copy options bitmask.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_copy_file_async(
    cfs_runtime_t *rt, const cfs_path *from, const cfs_path *to,
    cfs_copy_options options, cfs_callback_t cb, void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_rename` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param old_p Existing path.
 * \param new_p Target new path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rename_async(cfs_runtime_t *rt,
                                                 const cfs_path *old_p,
                                                 const cfs_path *new_p,
                                                 cfs_callback_t cb,
                                                 void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_resize_file` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target file path.
 * \param size New file size in bytes.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_resize_file_async(cfs_runtime_t *rt,
                                                      const cfs_path *p,
                                                      cfs_uintmax_t size,
                                                      cfs_callback_t cb,
                                                      void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_status` query (`result_buffer` holds
 * `cfs_file_status`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_status_async(cfs_runtime_t *rt,
                                                 const cfs_path *p,
                                                 cfs_callback_t cb,
                                                 void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_symlink_status` query (`result_buffer`
 * holds `cfs_file_status`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_symlink_status_async(cfs_runtime_t *rt,
                                                         const cfs_path *p,
                                                         cfs_callback_t cb,
                                                         void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_exists_path` query (`result_buffer`
 * holds `cfs_bool`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_exists_async(cfs_runtime_t *rt,
                                                 const cfs_path *p,
                                                 cfs_callback_t cb,
                                                 void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_is_empty_path` query (`result_buffer`
 * holds `cfs_bool`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_empty_async(cfs_runtime_t *rt,
                                                   const cfs_path *p,
                                                   cfs_callback_t cb,
                                                   void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_space` query (`result_buffer` holds
 * `cfs_space_info`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_space_async(cfs_runtime_t *rt,
                                                const cfs_path *p,
                                                cfs_callback_t cb,
                                                void *user_data);
/**
 * \brief Schedules an asynchronous `cfs_last_write_time` query (`result_buffer`
 * holds `cfs_file_time_type`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_last_write_time_async(cfs_runtime_t *rt,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_runtime_poll(cfs_runtime_t *rt);

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
CFS_NO_DISCARD CFS_API cfs_errc cfs_cancel_request(cfs_runtime_t *rt,
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
CFS_NO_DISCARD CFS_API cfs_errc
cfs_message_pipe_create(const cfs_char_t *path, cfs_message_pipe **out_pipe);
/**
 * \brief Writes a framed message payload into a `cfs_message_pipe`.
 *
 * \param pipe Active message pipe handle.
 * \param data Pointer to message bytes to transmit.
 * \param size Number of bytes in `data`.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_message_pipe_send(cfs_message_pipe *pipe,
                                                      const void *data,
                                                      cfs_size_t size);
/**
 * \brief Reads the next framed message payload from a `cfs_message_pipe`.
 *
 * \param pipe Active message pipe handle.
 * \param out_buf Output pointer receiving heap-allocated message buffer (caller
 * frees via `cfs_free`).
 * \param out_size Output byte length of the received message.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_message_pipe_recv(cfs_message_pipe *pipe,
                                                      void **out_buf,
                                                      cfs_size_t *out_size);
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_serialize_request(const cfs_request_t *req,
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_deserialize_request(const void *buffer,
                                                        cfs_size_t size,
                                                        cfs_request_t **req);

/* Phase 4: Multiprocessing and Greenthreads */

/* 31. Multiprocess modality backend (Process Handles) */
/** @brief Opaque handle representing a spawned child process. */
typedef struct cfs_process_t cfs_process_t;
/**
 * \brief Performs the cfs_process_spawn filesystem operation.
 *
 * \param executable Argument representing the target resource.
 * \param out_proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_process_spawn(const cfs_char_t *executable,
                                                  cfs_process_t **out_proc);
/**
 * \brief Performs the cfs_process_wait filesystem operation.
 *
 * \param proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_process_wait(cfs_process_t *proc);
/**
 * \brief Performs the cfs_process_destroy filesystem operation.
 *
 * \param proc Argument representing the target resource.
 */
CFS_API void cfs_process_destroy(cfs_process_t *proc);

/* 32. Shared Memory (shm) segments */
/** @brief Opaque handle representing a shared memory segment. */
typedef struct cfs_shm_segment cfs_shm_segment;
/**
 * @brief Performs the cfs_shm_create filesystem operation.
 *
 * @param size Argument representing the target resource.
 * @param name Argument representing the target resource.
 * @param out_shm Argument representing the target resource.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_shm_create(cfs_size_t size,
                                               const cfs_char_t *name,
                                               cfs_shm_segment **out_shm);
/**
 * @brief Performs the cfs_shm_map filesystem operation.
 *
 * @param shm Argument representing the target resource.
 * @param out Pointer to store the result of the shm_map operation.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_shm_map(cfs_shm_segment *shm, void **out);
/**
 * @brief Performs the cfs_shm_unmap filesystem operation.
 *
 * @param shm Argument representing the target resource.
 * @param addr Argument representing the target resource.
 */
CFS_API void cfs_shm_unmap(cfs_shm_segment *shm, void *addr);
/**
 * @brief Performs the cfs_shm_destroy filesystem operation.
 *
 * @param shm Argument representing the target resource.
 */
CFS_API void cfs_shm_destroy(cfs_shm_segment *shm);

/* 33. Multiprocess semaphore */
/** @brief Opaque handle representing a named semaphore. */
typedef struct cfs_named_semaphore cfs_named_semaphore;
/**
 * @brief Performs the cfs_named_semaphore_create filesystem operation.
 *
 * @param name Argument representing the target resource.
 * @param initial_count Argument representing the target resource.
 * @param out_sem Argument representing the target resource.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_create(
    const cfs_char_t *name, int initial_count, cfs_named_semaphore **out_sem);
/**
 * @brief Performs the cfs_named_semaphore_wait filesystem operation.
 *
 * @param sem Argument representing the target resource.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_named_semaphore_wait(cfs_named_semaphore *sem);
/**
 * @brief Attempts to decrement a named semaphore without blocking.
 *
 * @param sem Active named semaphore handle.
 * @param out_acquired Set to `cfs_true` if decremented, or `cfs_false` if count
 * was zero.
 * @return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_named_semaphore_try_wait(cfs_named_semaphore *sem, cfs_bool *out_acquired);
/**
 * @brief Performs the cfs_named_semaphore_post filesystem operation.
 *
 * @param sem Argument representing the target resource.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_named_semaphore_post(cfs_named_semaphore *sem);
/**
 * @brief Performs the cfs_named_semaphore_destroy filesystem operation.
 *
 * @param sem Argument representing the target resource.
 */
CFS_API void cfs_named_semaphore_destroy(cfs_named_semaphore *sem);

/** @brief Forward declaration for greenthread structure. */
typedef struct cfs_greenthread_t cfs_greenthread_t;
/** @brief Function pointer type for greenthread entry points. */
typedef void (*cfs_greenthread_func)(void *);

/**
 * @brief Performs the cfs_greenthread_spawn filesystem operation.
 *
 * @param func Argument representing the target resource.
 * @param arg Argument representing the target resource.
 * @param out_gt Argument representing the target resource.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_greenthread_spawn(
    cfs_greenthread_func func, void *arg, cfs_greenthread_t **out_gt);
/**
 * @brief Performs the cfs_greenthread_yield filesystem operation.
 *
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_greenthread_yield(void);
/**
 * @brief Performs the cfs_greenthread_destroy filesystem operation.
 *
 * @param gt Argument representing the target resource.
 */
CFS_API void cfs_greenthread_destroy(cfs_greenthread_t *gt);

/* 36. Greenthread scheduler */
/** @brief Opaque handle representing a cooperative greenthread scheduler. */
typedef struct cfs_greenthread_scheduler cfs_greenthread_scheduler;
/**
 * @brief Initializes a greenthread scheduler.
 *
 * @param out_sched Pointer to store the initialized scheduler.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_init(cfs_greenthread_scheduler **out_sched);
/**
 * @brief Enqueues a spawned green thread onto a cooperative scheduler.
 *
 * @param sched Target scheduler.
 * @param gt Spawned green thread handle to enqueue.
 * @return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_greenthread_scheduler_schedule(
    cfs_greenthread_scheduler *sched, cfs_greenthread_t *gt);
/**
 * @brief Performs the cfs_greenthread_scheduler_run filesystem operation.
 *
 * @param sched Argument representing the target resource.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_run(cfs_greenthread_scheduler *sched);
/**
 * @brief Destroys a greenthread scheduler.
 *
 * @param sched Argument representing the target resource to destroy.
 */
CFS_API void
cfs_greenthread_scheduler_destroy(cfs_greenthread_scheduler *sched);

/* Phase 5.7: Integration, APIs, and Validation */

/** @brief Convenience macro dispatching remove synchronously or asynchronously.
 */
#define CFS_REMOVE(rt, path, cb, ud)                                           \
  (((const cfs_runtime_t *)(rt) != (const cfs_runtime_t *)0)                   \
       ? cfs_remove_async((cfs_runtime_t *)(rt), (path), (cb), (ud))           \
       : cfs_remove((path), (cfs_error_code *)0))

/* 42. Integrate new context parameter into directory iterators */
/** @brief Opaque handle for asynchronous directory iteration. */
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
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init_async(cfs_runtime_t *rt,
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
  /** \brief Non-zero to block all mutating filesystem operations. */
  cfs_bool read_only;
} cfs_sandbox_config;

/**
 * \brief Performs the cfs_runtime_set_sandbox filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param config Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_runtime_set_sandbox(cfs_runtime_t *rt, const cfs_sandbox_config *config);

#ifdef CFS_IMPLEMENTATION

/* Implementation details */

/**
 * @brief Explicitly consumes an intentionally ignored `cfs_errc` return value.
 * @param rc Return code from a best-effort helper call.
 */
static void cfs_ignore_errc(cfs_errc rc) { (void)rc; }

/** @brief Internal representation of a non-recursive directory iterator. */
struct cfs_directory_iterator {
#if defined(CFS_OS_WINDOWS)
  /** @brief Win32 search handle. */
  HANDLE find_handle;
#if defined(CFS_UNICODE)
  /** @brief Wide Win32 find data buffer. */
  WIN32_FIND_DATAW find_data;
#else
  /** @brief Narrow Win32 find data buffer. */
  WIN32_FIND_DATAA find_data;
#endif
  /** @brief Non-zero before advancing past the initial FindFirstFile entry. */
  cfs_bool first;
#else
  /** @brief POSIX `DIR` stream pointer. */
  DIR *dirp;
#endif
  /** @brief Base directory path prefix for yielded entries. */
  cfs_path base_dir;
  /** @brief Cached current directory entry. */
  cfs_directory_entry current;
  /** @brief Options controlling permission-denied handling. */
  cfs_directory_options options;
  /** @brief Non-zero once iteration reaches the end of the directory. */
  cfs_bool is_end;
};

/** @brief Internal representation of a recursive directory iterator. */
struct cfs_recursive_directory_iterator {
  /** @brief Dynamically allocated stack of open directory iterators. */
  cfs_directory_iterator **frames;
  /** @brief Number of open directory frames on `frames`. */
  cfs_size_t depth;
  /** @brief Allocated slot capacity of `frames`. */
  cfs_size_t capacity;
  /** @brief Traversal options (`follow_directory_symlink`,
   * `skip_permission_denied`). */
  cfs_directory_options options;
  /** @brief Non-zero when descent into the active directory entry is allowed.
   */
  cfs_bool recursion_pending;
  /** @brief Non-zero after yielding an entry from the top frame. */
  cfs_bool has_yielded;
};

/**
 * \brief Checks whether a character is a forward or backward path separator.
 *
 * \param c Character to test.
 * \param out Pointer to store the boolean result.
 */
static void cfs_is_separator(cfs_char_t c, cfs_bool *out);
/**
 * \brief Performs the cfs_path_reserve filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param new_cap Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
static cfs_errc cfs_path_reserve(cfs_path *p, cfs_size_t new_cap);

#if defined(CFS_OS_WINDOWS) && defined(CFS_UNICODE)
/** \brief Internal raw string length helper matching cfs_char_t. */
#define CFS_RAW_STRLEN wcslen
/** \brief Internal raw string comparison helper matching cfs_char_t. */
#define CFS_RAW_STRCMP wcscmp
/** \brief Internal bounded raw string comparison helper matching cfs_char_t. */
#define CFS_RAW_STRNCMP wcsncmp
#else
/** \brief Internal raw string length helper matching cfs_char_t. */
#define CFS_RAW_STRLEN strlen
/** \brief Internal raw string comparison helper matching cfs_char_t. */
#define CFS_RAW_STRCMP strcmp
/** \brief Internal bounded raw string comparison helper matching cfs_char_t. */
#define CFS_RAW_STRNCMP strncmp
#endif

/* Phase 5.5: Execution Context & Modality Implementations */

/** @brief Internal representation of an asynchronous directory iterator handle.
 */
struct cfs_directory_iterator_async {
  /** @brief Underlying synchronous directory iterator. */
  cfs_directory_iterator *itr;
};

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
  /** \brief Message pipe channel used in `cfs_modality_message_passing`. */
  cfs_message_pipe *ipc_pipe;
  /** \brief Cooperative green-thread scheduler for `cfs_modality_greenthread`.
   */
  cfs_greenthread_scheduler *gt_sched;
  /** \brief Non-zero when a sandbox policy is active on this runtime. */
  cfs_bool has_sandbox;
  /** \brief Canonicalized sandbox confinement root path. */
  cfs_path sandbox_root;
  /** \brief Non-zero to disallow operating on symbolic links. */
  cfs_bool sandbox_restrict_symlinks;
  /** \brief Non-zero to disallow mutating filesystem operations. */
  cfs_bool sandbox_read_only;
};

/* Phase 5.6: Deferred Execution and Asynchronous Base Implementations */

/**
 * \brief Copies an asynchronous result value into `req->result_buffer` if
 * sufficient capacity is available.
 *
 * \param req Target request object.
 * \param src Source result value pointer.
 * \param len Byte size of `src`.
 */
static void cfs_store_result(cfs_request_t *req, const void *src,
                             cfs_size_t len) {
  if (req->result_buffer && req->result_size >= len) {
    CFS_MEMCPY_SAFE(req->result_buffer, req->result_size, src, len);
  }
}

/**
 * \brief Validates a single path against an active runtime sandbox policy.
 *
 * \param rt Active runtime context.
 * \param p Candidate path to validate.
 * \param ec Output error code on violation.
 * \return 0 if permitted, or `cfs_errc_permission_denied` if blocked.
 */
static cfs_errc cfs_check_sandbox_path(const cfs_runtime_t *rt,
                                       const cfs_path *p, cfs_error_code *ec) {
  if (rt->sandbox_restrict_symlinks && p->str && p->length > 0) {
    cfs_file_status sym_st;
    if (cfs_symlink_status(p, &sym_st, NULL) == cfs_errc_success &&
        sym_st.type == cfs_file_type_symlink) {
      cfs_set_error(ec, 13, cfs_errc_permission_denied);
      return cfs_errc_permission_denied;
    }
  }
  if (rt->sandbox_root.length > 0 && p->str && p->length > 0) {
    cfs_path resolved;
    cfs_errc rc;
    cfs_bool root_trail_sep = cfs_false;
    cfs_bool next_is_sep = cfs_false;
    cfs_path_init(&resolved);
    rc = cfs_weakly_canonical(p, &resolved, NULL);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&resolved);
      cfs_set_error(ec, 0, rc);
      return rc;
    }
    cfs_is_separator(rt->sandbox_root.str[rt->sandbox_root.length - 1],
                     &root_trail_sep);
    if (resolved.length > rt->sandbox_root.length) {
      cfs_is_separator(resolved.str[rt->sandbox_root.length], &next_is_sep);
    }
    if (resolved.length < rt->sandbox_root.length ||
        CFS_RAW_STRNCMP(resolved.str, rt->sandbox_root.str,
                        rt->sandbox_root.length) != 0 ||
        (resolved.length > rt->sandbox_root.length && !root_trail_sep &&
         !next_is_sep)) {
      cfs_path_destroy(&resolved);
      cfs_set_error(ec, 13, cfs_errc_permission_denied);
      return cfs_errc_permission_denied;
    }
    cfs_path_destroy(&resolved);
  }
  return cfs_errc_success;
}

/**
 * \brief Enforces runtime sandbox constraints prior to executing `req`.
 *
 * \param req Request to validate.
 * \return 0 if allowed, or `cfs_errc_permission_denied` on policy violation.
 */
static cfs_errc cfs_check_sandbox(cfs_request_t *req) {
  const cfs_runtime_t *rt = req->owner_rt;
  cfs_errc rc;
  if (!rt->has_sandbox)
    return cfs_errc_success;
  if (rt->sandbox_read_only && req->opcode >= (int)cfs_opcode_remove &&
      req->opcode <= (int)cfs_opcode_resize_file) {
    cfs_set_error(&req->error, 13, cfs_errc_permission_denied);
    return cfs_errc_permission_denied;
  }
  rc = cfs_check_sandbox_path(rt, &req->target_path, &req->error);
  if (rc != cfs_errc_success)
    return rc;
  if (req->dest_path.length > 0) {
    rc = cfs_check_sandbox_path(rt, &req->dest_path, &req->error);
    if (rc != cfs_errc_success)
      return rc;
  }
  return cfs_errc_success;
}

/* 12. Generic Opcode Execution */
/**
 * \brief Executes a queued filesystem request and returns its status code.
 *
 * \param req Pointer to the request to execute.
 * \return 0 on success, or a non-zero error code on failure.
 */
static cfs_errc cfs_execute_op(cfs_request_t *req) {
  cfs_errc rc = cfs_check_sandbox(req);
  if (rc != cfs_errc_success)
    return rc;

  switch (req->opcode) {
  case cfs_opcode_remove:
    return cfs_remove(&req->target_path, &req->error);
  case cfs_opcode_remove_all: {
    cfs_size_t count = 0;
    rc = cfs_remove_all(&req->target_path, &count, &req->error);
    if (rc == cfs_errc_success)
      cfs_store_result(req, &count, sizeof(count));
    return rc;
  }
  case cfs_opcode_create_directory:
    return cfs_create_directory(&req->target_path, &req->error);
  case cfs_opcode_create_directories:
    return cfs_create_directories(&req->target_path, &req->error);
  case cfs_opcode_copy:
    return cfs_copy(&req->target_path, &req->dest_path, req->copy_opts,
                    &req->error);
  case cfs_opcode_copy_file:
    return cfs_copy_file(&req->target_path, &req->dest_path, req->copy_opts,
                         &req->error);
  case cfs_opcode_rename:
    return cfs_rename(&req->target_path, &req->dest_path, &req->error);
  case cfs_opcode_resize_file:
    return cfs_resize_file(&req->target_path, req->uint_arg, &req->error);
  case cfs_opcode_file_size: {
    cfs_uintmax_t size = 0;
    rc = cfs_file_size(&req->target_path, &size, &req->error);
    if (rc == cfs_errc_success)
      cfs_store_result(req, &size, sizeof(size));
    return rc;
  }
  case cfs_opcode_status: {
    cfs_file_status st;
    rc = cfs_status(&req->target_path, &st, &req->error);
    if (rc == cfs_errc_success)
      cfs_store_result(req, &st, sizeof(st));
    return rc;
  }
  case cfs_opcode_symlink_status: {
    cfs_file_status st;
    rc = cfs_symlink_status(&req->target_path, &st, &req->error);
    if (rc == cfs_errc_success)
      cfs_store_result(req, &st, sizeof(st));
    return rc;
  }
  case cfs_opcode_exists: {
    cfs_bool val = cfs_false;
    cfs_ignore_errc(cfs_exists_path(&req->target_path, &val, &req->error));
    cfs_store_result(req, &val, sizeof(val));
    return cfs_errc_success;
  }
  case cfs_opcode_is_empty: {
    cfs_bool val = cfs_false;
    rc = cfs_is_empty_path(&req->target_path, &val, &req->error);
    if (rc == cfs_errc_success)
      cfs_store_result(req, &val, sizeof(val));
    return rc;
  }
  case cfs_opcode_space: {
    cfs_space_info spc;
    rc = cfs_space(&req->target_path, &spc, &req->error);
    if (rc == cfs_errc_success)
      cfs_store_result(req, &spc, sizeof(spc));
    return rc;
  }
  case cfs_opcode_last_write_time: {
    cfs_file_time_type ft = 0;
    rc = cfs_last_write_time(&req->target_path, &ft, &req->error);
    if (rc == cfs_errc_success)
      cfs_store_result(req, &ft, sizeof(ft));
    return rc;
  }
  case cfs_opcode_dir_itr_init: {
    cfs_directory_iterator *it = NULL;
    rc = cfs_dir_itr_init(&req->target_path, &it, &req->error);
    if (rc == cfs_errc_success) {
      if (req->result_buffer &&
          req->result_size >= sizeof(cfs_directory_iterator *)) {
        cfs_store_result(req, &it, sizeof(it));
      } else {
        cfs_dir_itr_close(it);
      }
    }
    return rc;
  }
  default:
    return cfs_errc_operation_not_supported;
  }
}

/**
 * \brief Performs the cfs_execute_op_inline filesystem operation.
 *
 * \param req Argument representing the target resource.
 */
static void cfs_execute_op_inline(cfs_request_t *req) {
  cfs_errc rc;
  rc = cfs_execute_op(req);
  if (rc != cfs_errc_success && req->error.errc == cfs_errc_success) {
    req->error.errc = rc;
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
 * \param c Argument representing the target resource.
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
 * \param c Argument representing the target resource.
 */
static void cfs_cond_signal(cfs_cond_t *c) { SetEvent(c->event); }
/**
 * \brief Performs the cfs_cond_broadcast filesystem operation.
 *
 * \param c Argument representing the target resource.
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
 * \param c Argument representing the target resource.
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
 * \param c Argument representing the target resource.
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
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_destroy(cfs_mutex_t *m) { DeleteCriticalSection(&m->cs); }
/**
 * \brief Performs the cfs_mutex_lock filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_lock(cfs_mutex_t *m) { EnterCriticalSection(&m->cs); }
/**
 * \brief Performs the cfs_mutex_unlock filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_unlock(cfs_mutex_t *m) { LeaveCriticalSection(&m->cs); }
#elif defined(CFS_OS_DOS) || !defined(CFS_MULTITHREADED)
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
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_init(cfs_mutex_t *m) { (void)m; }
/**
 * \brief Performs the cfs_mutex_destroy filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_destroy(cfs_mutex_t *m) { (void)m; }
/**
 * \brief Performs the cfs_mutex_lock filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_lock(cfs_mutex_t *m) { (void)m; }
/**
 * \brief Performs the cfs_mutex_unlock filesystem operation.
 *
 * \param m Argument representing the target resource.
 */
static void cfs_mutex_unlock(cfs_mutex_t *m) { (void)m; }

/**
 * \brief Performs the cfs_cond_init filesystem operation.
 *
 * \param c Argument representing the target resource.
 */
static void cfs_cond_init(cfs_cond_t *c) { (void)c; }
/**
 * \brief Performs the cfs_cond_destroy filesystem operation.
 *
 * \param c Argument representing the target resource.
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
 * \param c Argument representing the target resource.
 */
static void cfs_cond_signal(cfs_cond_t *c) { (void)c; }
/**
 * \brief Performs the cfs_cond_broadcast filesystem operation.
 *
 * \param c Argument representing the target resource.
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
 * @brief Initializes a POSIX mutex wrapper.
 *
 * @param m Pointer to the mutex structure to initialize.
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
 * @brief Initializes a POSIX condition variable wrapper.
 *
 * @param c Pointer to the condition variable structure to initialize.
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
  cfs_mutex_init(&q->lock);
  cfs_cond_init(&q->cond);
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
    cfs_request_release(req);
  }
  cfs_mutex_destroy(&q->lock);
  cfs_cond_destroy(&q->cond);
}

/**
 * \brief Performs the cfs_queue_push filesystem operation.
 *
 * \param q Argument representing the target resource.
 * \param req Argument representing the target resource.
 */
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

/**
 * \brief Pops the next request from a thread-safe FIFO queue.
 *
 * \param q Pointer to the queue.
 * \param wait_for_data Non-zero to block until an item arrives or shutdown.
 * \param out_req Pointer to store the popped request (or NULL if empty).
 */
static void cfs_queue_pop(cfs_queue_t *q, cfs_bool wait_for_data,
                          cfs_request_t **out_req) {
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
  *out_req = req;
}

/**
 * \brief Performs the cfs_queue_shutdown filesystem operation.
 *
 * \param q Argument representing the target resource.
 */
static void cfs_queue_shutdown(cfs_queue_t *q) {
  cfs_mutex_lock(&q->lock);
  q->shutdown = cfs_true;
  cfs_cond_broadcast(&q->cond);
  cfs_mutex_unlock(&q->lock);
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
#if defined(CFS_OS_WINDOWS) ||                                                 \
    (!defined(CFS_OS_DOS) && defined(CFS_MULTITHREADED))
#if defined(CFS_OS_WINDOWS)
/**
 * \brief Performs the cfs_worker_thread filesystem operation.
 *
 * \param arg Argument representing the target resource.
 * \return 0 on completion.
 */
static DWORD WINAPI cfs_worker_thread(LPVOID arg) {
#else
/**
 * \brief Performs the cfs_worker_thread filesystem operation.
 *
 * \param arg Argument representing the target resource.
 * \return NULL on completion.
 */
static void *cfs_worker_thread(void *arg) {
#endif
  cfs_thread_pool_t *pool = (cfs_thread_pool_t *)arg;
  cfs_request_t *req = NULL;

  for (;;) {
    cfs_queue_pop(pool->work_queue, cfs_true, &req);
    if (!req) {
      /* Cascade shutdown signal to wake up other waiting threads when using
       * event fallback */
      cfs_cond_broadcast(&pool->work_queue->cond);
      break; /* Shutdown condition */
    }

    if (!req->cancelled) {
      cfs_execute_op_inline(req);
    } else {
      cfs_set_error(&req->error, 125, cfs_errc_operation_canceled);
    }

    cfs_queue_push(pool->completion_queue, req);
  }

#if defined(CFS_OS_WINDOWS)
  return 0;
#else
  return NULL;
#endif
}
#endif

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
  cfs_errc rc;
  *out_pool = NULL;

  rc = cfs_calloc(1, sizeof(cfs_thread_pool_t), (void **)&pool);
  if (rc != cfs_errc_success)
    return rc;

  pool->num_threads = 0;
  pool->work_queue = work;
  pool->completion_queue = comp;
  rc = cfs_calloc(num_threads, sizeof(cfs_thread_t), (void **)&(pool->threads));
  if (rc != cfs_errc_success) {
    cfs_free(pool);
    return rc;
  }
  memset(pool->threads, 0, num_threads * sizeof(cfs_thread_t));

  for (i = 0; i < num_threads; ++i) {
#if defined(CFS_OS_WINDOWS)
    pool->threads[pool->num_threads].h =
        CreateThread(NULL, 0, cfs_worker_thread, pool, 0, NULL);
    if (pool->threads[pool->num_threads].h)
      pool->num_threads++;
#elif defined(CFS_OS_DOS) || !defined(CFS_MULTITHREADED)
    (void)pool;
    (void)i;
#else
    {
      pthread_t *pt_slot = &pool->threads[pool->num_threads].t;
      int pt_rc = pthread_create(pt_slot, NULL, cfs_worker_thread, pool);
      if (pt_rc == 0)
        pool->num_threads++;
    }
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
  cfs_thread_t *threads = pool->threads;

  cfs_queue_shutdown(pool->work_queue);

  for (i = 0; i < pool->num_threads; ++i) {
#if defined(CFS_OS_WINDOWS)
    if (threads[i].h != NULL) {
      WaitForSingleObject(threads[i].h, INFINITE);
      CloseHandle(threads[i].h);
    }
#elif defined(CFS_OS_DOS) || !defined(CFS_MULTITHREADED)
    (void)threads;
    (void)i;
#else
    pthread_join(threads[i].t, NULL);
#endif
  }
  cfs_free(pool->threads);
  cfs_free(pool);
}

/* 13. Non-blocking API variants */
/**
 * \brief Cooperative green-thread worker task executing a single
 * `cfs_request_t`.
 *
 * \param arg Pointer to the `cfs_request_t` to execute.
 */
static void cfs_gt_request_runner(void *arg) {
  cfs_request_t *req = (cfs_request_t *)arg;
  cfs_ignore_errc(cfs_greenthread_yield());
  if (!req->cancelled) {
    cfs_execute_op_inline(req);
  } else {
    cfs_set_error(&req->error, 125, cfs_errc_operation_canceled);
  }
}

/**
 * \brief Allocates, populates, and dispatches a `cfs_request_t` to `rt`.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param opcode Operation identifier from `cfs_opcode`.
 * \param target Primary path parameter (required).
 * \param dest Optional secondary destination path parameter.
 * \param copy_opts Copy options bitmask.
 * \param uint_arg Scalar unsigned integer parameter.
 * \param res_size Byte size of `result_buffer` to allocate (0 if none).
 * \param cb Completion callback.
 * \param user_data Opaque callback context.
 * \return 0 on success, or non-zero error code on failure.
 */
static cfs_errc cfs_submit_async_op(cfs_runtime_t *rt, int opcode,
                                    const cfs_path *target,
                                    const cfs_path *dest,
                                    cfs_copy_options copy_opts,
                                    cfs_uintmax_t uint_arg, cfs_size_t res_size,
                                    cfs_callback_t cb, void *user_data) {
  cfs_request_t *req;
  cfs_errc rc;
  if (!rt || !target)
    return cfs_errc_invalid_argument;

  rc = cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (rc != cfs_errc_success)
    return rc;

  req->opcode = opcode;
  cfs_path_init(&req->target_path);
  rc = cfs_path_clone(&req->target_path, target);
  if (rc != cfs_errc_success) {
    cfs_free(req);
    return rc;
  }
  cfs_path_init(&req->dest_path);
  if (dest) {
    rc = cfs_path_clone(&req->dest_path, dest);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&req->target_path);
      cfs_free(req);
      return rc;
    }
  }
  req->copy_opts = copy_opts;
  req->uint_arg = uint_arg;
  req->result_buffer = NULL;
  req->result_size = res_size;
  if (res_size > 0) {
    rc = cfs_calloc(1, res_size, &req->result_buffer);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&req->dest_path);
      cfs_path_destroy(&req->target_path);
      cfs_free(req);
      return rc;
    }
  }
  cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->owner_rt = rt;
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;

  cfs_dispatch_request(rt, req, cb, user_data);
  return cfs_errc_success;
}

/*
 * \brief Schedules an asynchronous operation to remove a file or empty
 * directory.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove_async(cfs_runtime_t *rt,
                                                 const cfs_path *p,
                                                 cfs_callback_t cb,
                                                 void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_remove, p, NULL,
                             cfs_copy_options_none, 0, 0, cb, user_data);
}

/*
 * \brief Schedules an asynchronous operation to retrieve a file\'s size.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_file_size_async(cfs_runtime_t *rt,
                                                    const cfs_path *p,
                                                    cfs_callback_t cb,
                                                    void *user_data) {
  cfs_request_t *req;
  cfs_errc rc;

  if (!rt || !p)
    return cfs_errc_invalid_argument;

  rc = cfs_malloc(sizeof(cfs_request_t), (void **)&req);
  if (rc != cfs_errc_success)
    return rc;

  req->opcode = cfs_opcode_file_size;
  cfs_path_init(&req->target_path);
  rc = cfs_path_clone(&req->target_path, p);
  if (rc != cfs_errc_success) {
    cfs_free(req);
    return rc;
  }
  cfs_path_init(&req->dest_path);
  req->copy_opts = cfs_copy_options_none;
  req->uint_arg = 0;
  req->result_size = sizeof(cfs_uintmax_t);
  rc = cfs_malloc(req->result_size, (void **)&req->result_buffer);
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&req->target_path);
    cfs_free(req);
    return rc;
  }
  memset(req->result_buffer, 0, req->result_size);
  cfs_clear_error(&req->error);
  req->callback = cb;
  req->user_data = user_data;
  req->owner_rt = rt;
  req->next = NULL;
  req->ref_count = 1;
  req->cancelled = cfs_false;

  cfs_dispatch_request(rt, req, cb, user_data);
  return cfs_errc_success;
}

/*
 * \brief Schedules an asynchronous recursive removal (`cfs_remove_all`).
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path to remove recursively.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove_all_async(cfs_runtime_t *rt,
                                                     const cfs_path *p,
                                                     cfs_callback_t cb,
                                                     void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_remove_all, p, NULL,
                             cfs_copy_options_none, 0, sizeof(cfs_size_t), cb,
                             user_data);
}

/*
 * \brief Schedules an asynchronous single-directory creation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target directory path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory_async(cfs_runtime_t *rt,
                                                           const cfs_path *p,
                                                           cfs_callback_t cb,
                                                           void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_create_directory, p, NULL,
                             cfs_copy_options_none, 0, 0, cb, user_data);
}

/*
 * \brief Schedules an asynchronous recursive directory creation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target directory path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directories_async(cfs_runtime_t *rt,
                                                             const cfs_path *p,
                                                             cfs_callback_t cb,
                                                             void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_create_directories, p, NULL,
                             cfs_copy_options_none, 0, 0, cb, user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_copy` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param from Source path.
 * \param to Destination path.
 * \param options Copy options bitmask.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_copy_async(cfs_runtime_t *rt, const cfs_path *from, const cfs_path *to,
               cfs_copy_options options, cfs_callback_t cb, void *user_data) {
  if (!to)
    return cfs_errc_invalid_argument;
  return cfs_submit_async_op(rt, cfs_opcode_copy, from, to, options, 0, 0, cb,
                             user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_copy_file` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param from Source regular file path.
 * \param to Destination file path.
 * \param options Copy options bitmask.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_copy_file_async(
    cfs_runtime_t *rt, const cfs_path *from, const cfs_path *to,
    cfs_copy_options options, cfs_callback_t cb, void *user_data) {
  if (!to)
    return cfs_errc_invalid_argument;
  return cfs_submit_async_op(rt, cfs_opcode_copy_file, from, to, options, 0, 0,
                             cb, user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_rename` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param old_p Existing path.
 * \param new_p Target new path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rename_async(cfs_runtime_t *rt,
                                                 const cfs_path *old_p,
                                                 const cfs_path *new_p,
                                                 cfs_callback_t cb,
                                                 void *user_data) {
  if (!new_p)
    return cfs_errc_invalid_argument;
  return cfs_submit_async_op(rt, cfs_opcode_rename, old_p, new_p,
                             cfs_copy_options_none, 0, 0, cb, user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_resize_file` operation.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target file path.
 * \param size New file size in bytes.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_resize_file_async(cfs_runtime_t *rt,
                                                      const cfs_path *p,
                                                      cfs_uintmax_t size,
                                                      cfs_callback_t cb,
                                                      void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_resize_file, p, NULL,
                             cfs_copy_options_none, size, 0, cb, user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_status` query.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_status_async(cfs_runtime_t *rt,
                                                 const cfs_path *p,
                                                 cfs_callback_t cb,
                                                 void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_status, p, NULL,
                             cfs_copy_options_none, 0, sizeof(cfs_file_status),
                             cb, user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_symlink_status` query.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_symlink_status_async(cfs_runtime_t *rt,
                                                         const cfs_path *p,
                                                         cfs_callback_t cb,
                                                         void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_symlink_status, p, NULL,
                             cfs_copy_options_none, 0, sizeof(cfs_file_status),
                             cb, user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_exists_path` query.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_exists_async(cfs_runtime_t *rt,
                                                 const cfs_path *p,
                                                 cfs_callback_t cb,
                                                 void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_exists, p, NULL,
                             cfs_copy_options_none, 0, sizeof(cfs_bool), cb,
                             user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_is_empty_path` query.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_empty_async(cfs_runtime_t *rt,
                                                   const cfs_path *p,
                                                   cfs_callback_t cb,
                                                   void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_is_empty, p, NULL,
                             cfs_copy_options_none, 0, sizeof(cfs_bool), cb,
                             user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_space` query.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_space_async(cfs_runtime_t *rt,
                                                const cfs_path *p,
                                                cfs_callback_t cb,
                                                void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_space, p, NULL,
                             cfs_copy_options_none, 0, sizeof(cfs_space_info),
                             cb, user_data);
}

/*
 * \brief Schedules an asynchronous `cfs_last_write_time` query.
 *
 * \param rt Active `cfs_runtime_t` context.
 * \param p Target path.
 * \param cb Completion callback.
 * \param user_data Opaque user pointer.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_last_write_time_async(cfs_runtime_t *rt,
                                                          const cfs_path *p,
                                                          cfs_callback_t cb,
                                                          void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_last_write_time, p, NULL,
                             cfs_copy_options_none, 0,
                             sizeof(cfs_file_time_type), cb, user_data);
}

/*
 * \brief Performs the cfs_runtime_init filesystem operation.
 *
 * \param config Argument representing the target resource.
 * \param out_rt Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_runtime_init(const cfs_runtime_config *config, cfs_runtime_t **out_rt,
                 cfs_error_code *ec) {
  cfs_runtime_t *rt;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!out_rt) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  *out_rt = NULL;

  if (!config) {
    cfs_set_error(ec, 22, cfs_errc_unknown_error);
    return cfs_errc_invalid_argument;
  }

  rc = cfs_malloc(sizeof(cfs_runtime_t), (void **)&rt);
  if (rc != cfs_errc_success) {
    cfs_set_error(ec, 12, cfs_errc_unknown_error);
    return rc;
  }

  rt->config = *config;
  rt->work_queue = NULL;
  rt->completion_queue = NULL;
  rt->thread_pool = NULL;
  rt->ipc_pipe = NULL;
  rt->gt_sched = NULL;
  rt->has_sandbox = cfs_false;
  cfs_path_init(&rt->sandbox_root);
  rt->sandbox_restrict_symlinks = cfs_false;
  rt->sandbox_read_only = cfs_false;

  if (rt->config.mode != cfs_modality_sync) {
    rc = cfs_malloc(sizeof(cfs_queue_t), (void **)&(rt->work_queue));
    if (rc != cfs_errc_success) {
      cfs_free(rt);
      return rc;
    }
    rc = cfs_malloc(sizeof(cfs_queue_t), (void **)&(rt->completion_queue));
    if (rc != cfs_errc_success) {
      cfs_free(rt->work_queue);
      cfs_free(rt);
      return rc;
    }
    cfs_queue_init(rt->work_queue);
    cfs_queue_init(rt->completion_queue);

    if (rt->config.mode == cfs_modality_async ||
        rt->config.mode == cfs_modality_multithread) {
      rc = cfs_thread_pool_create(
          rt->config.thread_pool_size > 0 ? rt->config.thread_pool_size : 4,
          rt->work_queue, rt->completion_queue, &rt->thread_pool);
    } else if (rt->config.mode == cfs_modality_greenthread) {
      rc = cfs_greenthread_scheduler_init(&rt->gt_sched);
    } else if (rt->config.mode == cfs_modality_message_passing) {
      rc = cfs_message_pipe_create(rt->config.ipc_path ? rt->config.ipc_path
                                                       : CFS_STR("cfs_ipc"),
                                   &rt->ipc_pipe);
    }
    if (rc != cfs_errc_success) {
      cfs_queue_destroy(rt->work_queue);
      cfs_free(rt->work_queue);
      cfs_queue_destroy(rt->completion_queue);
      cfs_free(rt->completion_queue);
      cfs_free(rt);
      return rc;
    }
  }

  *out_rt = rt;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_runtime_destroy filesystem operation.
 *
 * \param runtime Argument representing the target resource.
 */
CFS_API void cfs_runtime_destroy(cfs_runtime_t *runtime) {
  if (!runtime)
    return;

  if (runtime->thread_pool) {
    cfs_thread_pool_destroy(runtime->thread_pool);
  }
  if (runtime->gt_sched) {
    cfs_greenthread_scheduler_destroy(runtime->gt_sched);
  }
  if (runtime->ipc_pipe) {
    cfs_message_pipe_destroy(runtime->ipc_pipe);
  }
  if (runtime->work_queue) {
    cfs_queue_destroy(runtime->work_queue);
    cfs_free(runtime->work_queue);
  }
  if (runtime->completion_queue) {
    cfs_queue_destroy(runtime->completion_queue);
    cfs_free(runtime->completion_queue);
  }
  cfs_path_destroy(&runtime->sandbox_root);

  cfs_free(runtime);
}

/*
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
  req->owner_rt = runtime;

  if (runtime->config.mode == cfs_modality_sync) {
    if (!req->cancelled)
      cfs_execute_op_inline(req);
    if (req->callback) {
      req->callback(req, req->user_data);
    }
    cfs_request_release(req);
  } else {
    cfs_queue_push(runtime->work_queue, req);
  }
}

/* 19. cfs_runtime_poll() */
/*
 * \brief Processes completed asynchronous requests and executes their
 * callbacks.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_runtime_poll(cfs_runtime_t *rt) {
  cfs_request_t *req = NULL;
  if (!rt || !rt->completion_queue)
    return cfs_errc_success;

  if (!rt->thread_pool) {
    for (;;) {
      cfs_queue_pop(rt->work_queue, cfs_false, &req);
      if (!req)
        break;
      if (rt->config.mode == cfs_modality_greenthread) {
        cfs_greenthread_t *gt = NULL;
        if (cfs_greenthread_spawn(cfs_gt_request_runner, req, &gt) ==
            cfs_errc_success) {
          cfs_ignore_errc(cfs_greenthread_scheduler_schedule(rt->gt_sched, gt));
          cfs_ignore_errc(cfs_greenthread_scheduler_run(rt->gt_sched));
          cfs_greenthread_destroy(gt);
        } else {
          cfs_set_error(&req->error, 12, cfs_errc_not_enough_memory);
        }
      } else if (rt->config.mode == cfs_modality_message_passing) {
        void *sbuf = NULL;
        void *rbuf = NULL;
        cfs_size_t ssz = 0;
        cfs_size_t rsz = 0;
        cfs_request_t *wreq = NULL;
        if (!req->cancelled &&
            cfs_serialize_request(req, &sbuf, &ssz) == cfs_errc_success) {
          if (cfs_message_pipe_send(rt->ipc_pipe, sbuf, ssz) ==
                  cfs_errc_success &&
              cfs_message_pipe_recv(rt->ipc_pipe, &rbuf, &rsz) ==
                  cfs_errc_success &&
              cfs_deserialize_request(rbuf, rsz, &wreq) == cfs_errc_success) {
            wreq->owner_rt = rt;
            wreq->result_buffer = req->result_buffer;
            wreq->result_size = req->result_size;
            cfs_execute_op_inline(wreq);
            req->error = wreq->error;
            wreq->result_buffer = NULL;
            cfs_request_release(wreq);
          } else {
            cfs_set_error(&req->error, 5, cfs_errc_io_error);
          }
          cfs_free(sbuf);
          if (rbuf)
            cfs_free(rbuf);
        } else if (req->cancelled) {
          cfs_set_error(&req->error, 125, cfs_errc_operation_canceled);
        } else {
          cfs_set_error(&req->error, 12, cfs_errc_not_enough_memory);
        }
      } else {
        if (!req->cancelled) {
          cfs_execute_op_inline(req);
        } else {
          cfs_set_error(&req->error, 125, cfs_errc_operation_canceled);
        }
      }
      cfs_queue_push(rt->completion_queue, req);
    }
  }
#if !defined(CFS_OS_WINDOWS) &&                                                \
    (defined(CFS_OS_DOS) || !defined(CFS_MULTITHREADED))
  else if (rt->work_queue) {
    for (;;) {
      cfs_queue_pop(rt->work_queue, cfs_false, &req);
      if (!req) {
        break;
      }
      if (!req->cancelled) {
        cfs_execute_op_inline(req);
      } else {
        cfs_set_error(&req->error, 125, cfs_errc_operation_canceled);
      }
      cfs_queue_push(rt->completion_queue, req);
    }
  }
#endif

  for (;;) {
    cfs_queue_pop(rt->completion_queue, cfs_false, &req);
    if (!req) {
      break;
    }
    if (req->callback) {
      req->callback(req, req->user_data);
    }
    cfs_request_release(req);
  }
  return cfs_errc_success;
}

/* Phase 6: String Handling */

/*
 * \brief Computes the length of a string safely.
 *
 * \param str The null-terminated string to evaluate.
 * \param out Pointer to store the result of the strlen operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strlen(const cfs_char_t *str,
                                           cfs_size_t *out) {
  cfs_size_t len = 0;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = 0;
  if (!str)
    return cfs_errc_invalid_argument;
  while (str[len])
    len++;
  *out = len;
  return cfs_errc_success;
}

/*
 * \brief Copies a string into a destination buffer safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcpy operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strcpy(cfs_char_t *dest,
                                           const cfs_char_t *src,
                                           cfs_char_t **out) {
  cfs_size_t i = 0;
  if (out)
    *out = dest;
  if (!dest || !src)
    return cfs_errc_invalid_argument;
  while ((dest[i] = src[i]) != 0)
    i++;
  return cfs_errc_success;
}

/*
 * \brief Copies up to n characters of a string into a destination buffer.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param n The maximum number of characters to copy.
 * \param out Pointer to store the result of the strncpy operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strncpy(cfs_char_t *dest,
                                            const cfs_char_t *src, cfs_size_t n,
                                            cfs_char_t **out) {
  cfs_size_t i;
  if (out)
    *out = dest;
  if (!dest || !src)
    return cfs_errc_invalid_argument;
  for (i = 0; i < n && src[i] != 0; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = 0;
  return cfs_errc_success;
}

/*
 * \brief Concatenates two strings safely.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \param out Pointer to store the result of the strcat operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strcat(cfs_char_t *dest,
                                           const cfs_char_t *src,
                                           cfs_char_t **out) {
  cfs_size_t dest_len = 0;
  cfs_size_t i = 0;
  cfs_errc rc;
  if (out)
    *out = dest;
  if (!src)
    return cfs_errc_invalid_argument;
  rc = cfs_strlen(dest, &dest_len);
  if (rc != cfs_errc_success)
    return rc;
  while ((dest[dest_len + i] = src[i]) != 0)
    i++;
  return cfs_errc_success;
}

/*
 * \brief Compares two strings lexicographically.
 *
 * \param lhs The left-hand side string for comparison.
 * \param rhs The right-hand side string for comparison.
 * \param out Pointer to store the result of the strcmp operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strcmp(const cfs_char_t *lhs,
                                           const cfs_char_t *rhs, int *out) {
  if (!out)
    return cfs_errc_invalid_argument;
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

/*
 * \brief Compares up to a specified count of characters of two strings
 * lexicographically.
 *
 * \param lhs Argument representing the target resource.
 * \param rhs Argument representing the target resource.
 * \param count The maximum number of characters to compare.
 * \param out Pointer to store the result of the strncmp operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_strncmp(const cfs_char_t *lhs,
                                            const cfs_char_t *rhs,
                                            cfs_size_t count, int *out) {
  if (!out)
    return cfs_errc_invalid_argument;
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
/*
 * \brief Converts a UTF-8 string to a UTF-16 wide string.
 *
 * \param utf8_str Null-terminated UTF-8 source string.
 * \param dest Pointer to the wide character destination buffer.
 * \param dest_len Capacity of the destination buffer in wide characters.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_utf8_to_utf16(const char *utf8_str,
                                                  wchar_t *dest,
                                                  cfs_size_t dest_len,
                                                  cfs_size_t *out_req) {
  int req = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, dest, (int)dest_len);
  if (out_req) {
    *out_req = (req > 0) ? (cfs_size_t)req : 0;
  }
  return (req > 0) ? cfs_errc_success : cfs_errc_invalid_argument;
}

/*
 * \brief Converts a UTF-16 wide string to a UTF-8 string.
 *
 * \param utf16_str Null-terminated UTF-16 source wide string.
 * \param dest Pointer to the UTF-8 destination buffer.
 * \param dest_len Capacity of the destination buffer in bytes.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_utf16_to_utf8(const wchar_t *utf16_str,
                                                  char *dest,
                                                  cfs_size_t dest_len,
                                                  cfs_size_t *out_req) {
  int req = WideCharToMultiByte(CP_UTF8, 0, utf16_str, -1, dest, (int)dest_len,
                                NULL, NULL);
  if (out_req) {
    *out_req = (req > 0) ? (cfs_size_t)req : 0;
  }
  return (req > 0) ? cfs_errc_success : cfs_errc_invalid_argument;
}
#endif

/*
 * \brief Converts a multi-byte string to a wide character string.
 *
 * \param mb_str Null-terminated multi-byte source string.
 * \param dest Pointer to the destination buffer or path.
 * \param dest_len Capacity of the destination buffer.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_mb_to_wide(const char *mb_str,
                                               wchar_t *dest,
                                               cfs_size_t dest_len,
                                               cfs_size_t *out_req) {
  if (!mb_str) {
    if (out_req) {
      *out_req = 0;
    }
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    int req =
        MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, mb_str, -1, NULL, 0);
    if (req <= 0) {
      if (out_req) {
        *out_req = 0;
      }
      return cfs_errc_illegal_byte_sequence;
    }
    if (out_req) {
      *out_req = (cfs_size_t)req;
    }
    if (!dest || dest_len == 0) {
      return cfs_errc_success;
    }
    if (dest_len < (cfs_size_t)req) {
      return cfs_errc_result_out_of_range;
    }
    MultiByteToWideChar(CP_ACP, 0, mb_str, -1, dest, (int)dest_len);
    return cfs_errc_success;
  }
#else
  {
    size_t len = mbstowcs(NULL, mb_str, 0);
    if (len == (size_t)-1) {
      if (out_req) {
        *out_req = 0;
      }
      return cfs_errc_illegal_byte_sequence;
    }
    if (out_req) {
      *out_req = (cfs_size_t)(len + 1);
    }
    if (!dest || dest_len == 0) {
      return cfs_errc_success;
    }
    if (dest_len < (cfs_size_t)(len + 1)) {
      return cfs_errc_result_out_of_range;
    }
    (void)mbstowcs(dest, mb_str, dest_len);
    dest[len] = (wchar_t)0;
    return cfs_errc_success;
  }
#endif
}

/*
 * \brief Converts a wide character string to a multi-byte string.
 *
 * \param wide_str Null-terminated wide source string.
 * \param dest Pointer to the destination buffer or path.
 * \param dest_len Capacity of the destination buffer.
 * \param out_req Pointer to store the required buffer size.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_wide_to_mb(const wchar_t *wide_str,
                                               char *dest, cfs_size_t dest_len,
                                               cfs_size_t *out_req) {
  if (!wide_str) {
    if (out_req) {
      *out_req = 0;
    }
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    int req = WideCharToMultiByte(CP_ACP, 0, wide_str, -1, NULL, 0, NULL, NULL);
    if (req <= 0) {
      if (out_req) {
        *out_req = 0;
      }
      return cfs_errc_illegal_byte_sequence;
    }
    if (out_req) {
      *out_req = (cfs_size_t)req;
    }
    if (!dest || dest_len == 0) {
      return cfs_errc_success;
    }
    if (dest_len < (cfs_size_t)req) {
      return cfs_errc_result_out_of_range;
    }
    WideCharToMultiByte(CP_ACP, 0, wide_str, -1, dest, (int)dest_len, NULL,
                        NULL);
    return cfs_errc_success;
  }
#else
  {
    size_t len = wcstombs(NULL, wide_str, 0);
    if (len == (size_t)-1) {
      if (out_req) {
        *out_req = 0;
      }
      return cfs_errc_illegal_byte_sequence;
    }
    if (out_req) {
      *out_req = (cfs_size_t)(len + 1);
    }
    if (!dest || dest_len == 0) {
      return cfs_errc_success;
    }
    if (dest_len < (cfs_size_t)(len + 1)) {
      return cfs_errc_result_out_of_range;
    }
    (void)wcstombs(dest, wide_str, dest_len);
    dest[len] = '\0';
    return cfs_errc_success;
  }
#endif
}

/* Phase 7: Error Handling */
/*
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

/*
 * \brief Resets a `cfs_error_code` structure to a success state.
 *
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 */
CFS_API void cfs_clear_error(cfs_error_code *ec) {
  cfs_set_error(ec, 0, cfs_errc_success);
}

/**
 * \brief Maps a raw platform OS error code to its normalized `cfs_errc` value.
 *
 * \param os_error Raw OS error value (`errno` on POSIX, `GetLastError()` on
 * Windows).
 * \return Normalized `cfs_errc` value.
 */
static cfs_errc cfs_map_os_error(int os_error) {
  static const struct {
    int code;
    cfs_errc errc;
  } k_os_map[] = {
#if defined(CFS_OS_WINDOWS)
      {ERROR_FILE_NOT_FOUND, cfs_errc_no_such_file_or_directory},
      {ERROR_PATH_NOT_FOUND, cfs_errc_no_such_file_or_directory},
      {ERROR_INVALID_DRIVE, cfs_errc_no_such_device},
      {ERROR_BAD_NETPATH, cfs_errc_no_such_file_or_directory},
      {ERROR_ACCESS_DENIED, cfs_errc_permission_denied},
      {ERROR_SHARING_VIOLATION, cfs_errc_permission_denied},
      {ERROR_LOCK_VIOLATION, cfs_errc_permission_denied},
      {ERROR_PRIVILEGE_NOT_HELD, cfs_errc_operation_not_permitted},
      {ERROR_WRITE_PROTECT, cfs_errc_read_only_file_system},
      {ERROR_ALREADY_EXISTS, cfs_errc_file_exists},
      {ERROR_FILE_EXISTS, cfs_errc_file_exists},
      {ERROR_DIR_NOT_EMPTY, cfs_errc_directory_not_empty},
      {ERROR_DIRECTORY, cfs_errc_not_a_directory},
      {ERROR_INVALID_PARAMETER, cfs_errc_invalid_argument},
      {ERROR_INVALID_NAME, cfs_errc_invalid_argument},
      {ERROR_INVALID_HANDLE, cfs_errc_bad_file_descriptor},
      {ERROR_NOT_ENOUGH_MEMORY, cfs_errc_not_enough_memory},
      {ERROR_OUTOFMEMORY, cfs_errc_not_enough_memory},
      {ERROR_DISK_FULL, cfs_errc_no_space_on_device},
      {ERROR_HANDLE_DISK_FULL, cfs_errc_no_space_on_device},
      {ERROR_NOT_SAME_DEVICE, cfs_errc_cross_device_link},
      {ERROR_FILENAME_EXCED_RANGE, cfs_errc_filename_too_long},
      {ERROR_BUFFER_OVERFLOW, cfs_errc_filename_too_long},
      {ERROR_CALL_NOT_IMPLEMENTED, cfs_errc_function_not_supported},
      {ERROR_NOT_SUPPORTED, cfs_errc_operation_not_supported},
      {ERROR_OPERATION_ABORTED, cfs_errc_operation_canceled},
      {ERROR_SEM_TIMEOUT, cfs_errc_timed_out},
      {ERROR_BROKEN_PIPE, cfs_errc_broken_pipe},
      {ERROR_TOO_MANY_OPEN_FILES, cfs_errc_too_many_files_open},
      {ERROR_BUSY, cfs_errc_device_or_resource_busy}
#else
      {EACCES, cfs_errc_permission_denied},
      {EPERM, cfs_errc_operation_not_permitted},
      {ENOENT, cfs_errc_no_such_file_or_directory},
      {EEXIST, cfs_errc_file_exists},
      {EISDIR, cfs_errc_is_a_directory},
      {ENOTDIR, cfs_errc_not_a_directory},
      {ENOTEMPTY, cfs_errc_directory_not_empty},
      {EINVAL, cfs_errc_invalid_argument},
      {ENOMEM, cfs_errc_not_enough_memory},
      {ENOSPC, cfs_errc_no_space_on_device},
      {EROFS, cfs_errc_read_only_file_system},
      {EXDEV, cfs_errc_cross_device_link},
      {ENAMETOOLONG, cfs_errc_filename_too_long},
      {EMLINK, cfs_errc_too_many_links},
      {EMFILE, cfs_errc_too_many_files_open},
      {ENFILE, cfs_errc_too_many_files_open_in_system},
      {EBUSY, cfs_errc_device_or_resource_busy},
      {EIO, cfs_errc_io_error},
      {EBADF, cfs_errc_bad_file_descriptor},
      {ENOSYS, cfs_errc_function_not_supported},
      {EAGAIN, cfs_errc_resource_unavailable_try_again},
      {EINTR, cfs_errc_interrupted},
      {EFBIG, cfs_errc_file_too_large},
      {ESPIPE, cfs_errc_invalid_seek},
      {EDOM, cfs_errc_argument_out_of_domain},
      {ERANGE, cfs_errc_result_out_of_range},
      {EPIPE, cfs_errc_broken_pipe},
      {ENODEV, cfs_errc_no_such_device},
      {ENXIO, cfs_errc_no_such_device_or_address},
      {ECHILD, cfs_errc_no_child_process},
      {ESRCH, cfs_errc_no_such_process},
      {EFAULT, cfs_errc_bad_address},
      {E2BIG, cfs_errc_argument_list_too_long},
      {EDEADLK, cfs_errc_resource_deadlock_would_occur},
      {ENOLCK, cfs_errc_no_lock_available},
      {ENOEXEC, cfs_errc_executable_format_error},
      {ENOTTY, cfs_errc_inappropriate_io_control_operation}
#ifdef EILSEQ
      ,
      {EILSEQ, cfs_errc_illegal_byte_sequence}
#endif
#ifdef ELOOP
      ,
      {ELOOP, cfs_errc_too_many_symbolic_link_levels}
#endif
#ifdef ENOTSUP
      ,
      {ENOTSUP, cfs_errc_not_supported}
#endif
#ifdef EOPNOTSUPP
      ,
      {EOPNOTSUPP, cfs_errc_operation_not_supported}
#endif
#ifdef ECANCELED
      ,
      {ECANCELED, cfs_errc_operation_canceled}
#endif
#ifdef ETIMEDOUT
      ,
      {ETIMEDOUT, cfs_errc_timed_out}
#endif
#ifdef ETXTBSY
      ,
      {ETXTBSY, cfs_errc_text_file_busy}
#endif
#ifdef EOVERFLOW
      ,
      {EOVERFLOW, cfs_errc_value_too_large}
#endif
#ifdef EWOULDBLOCK
      ,
      {EWOULDBLOCK, cfs_errc_operation_would_block}
#endif
#ifdef EAFNOSUPPORT
      ,
      {EAFNOSUPPORT, cfs_errc_address_family_not_supported}
#endif
#ifdef EADDRINUSE
      ,
      {EADDRINUSE, cfs_errc_address_in_use}
#endif
#ifdef EADDRNOTAVAIL
      ,
      {EADDRNOTAVAIL, cfs_errc_address_not_available}
#endif
#ifdef EISCONN
      ,
      {EISCONN, cfs_errc_already_connected}
#endif
#ifdef EBADMSG
      ,
      {EBADMSG, cfs_errc_bad_message}
#endif
#ifdef ECONNABORTED
      ,
      {ECONNABORTED, cfs_errc_connection_aborted}
#endif
#ifdef EALREADY
      ,
      {EALREADY, cfs_errc_connection_already_in_progress}
#endif
#ifdef ECONNREFUSED
      ,
      {ECONNREFUSED, cfs_errc_connection_refused}
#endif
#ifdef ECONNRESET
      ,
      {ECONNRESET, cfs_errc_connection_reset}
#endif
#ifdef EDESTADDRREQ
      ,
      {EDESTADDRREQ, cfs_errc_destination_address_required}
#endif
#ifdef EHOSTUNREACH
      ,
      {EHOSTUNREACH, cfs_errc_host_unreachable}
#endif
#ifdef EIDRM
      ,
      {EIDRM, cfs_errc_identifier_removed}
#endif
#ifdef EMSGSIZE
      ,
      {EMSGSIZE, cfs_errc_message_size}
#endif
#ifdef ENETDOWN
      ,
      {ENETDOWN, cfs_errc_network_down}
#endif
#ifdef ENETRESET
      ,
      {ENETRESET, cfs_errc_network_reset}
#endif
#ifdef ENETUNREACH
      ,
      {ENETUNREACH, cfs_errc_network_unreachable}
#endif
#ifdef ENOBUFS
      ,
      {ENOBUFS, cfs_errc_no_buffer_space}
#endif
#ifdef ENOLINK
      ,
      {ENOLINK, cfs_errc_no_link}
#endif
#ifdef ENODATA
      ,
      {ENODATA, cfs_errc_no_message_available}
#endif
#ifdef ENOMSG
      ,
      {ENOMSG, cfs_errc_no_message}
#endif
#ifdef ENOPROTOOPT
      ,
      {ENOPROTOOPT, cfs_errc_no_protocol_option}
#endif
#ifdef ENOSR
      ,
      {ENOSR, cfs_errc_no_stream_resources}
#endif
#ifdef ENOSTR
      ,
      {ENOSTR, cfs_errc_not_a_stream}
#endif
#ifdef ENOTSOCK
      ,
      {ENOTSOCK, cfs_errc_not_a_socket}
#endif
#ifdef ENOTCONN
      ,
      {ENOTCONN, cfs_errc_not_connected}
#endif
#ifdef EINPROGRESS
      ,
      {EINPROGRESS, cfs_errc_operation_in_progress}
#endif
#ifdef EOWNERDEAD
      ,
      {EOWNERDEAD, cfs_errc_owner_dead}
#endif
#ifdef EPROTO
      ,
      {EPROTO, cfs_errc_protocol_error}
#endif
#ifdef EPROTONOSUPPORT
      ,
      {EPROTONOSUPPORT, cfs_errc_protocol_not_supported}
#endif
#ifdef ENOTRECOVERABLE
      ,
      {ENOTRECOVERABLE, cfs_errc_state_not_recoverable}
#endif
#ifdef ETIME
      ,
      {ETIME, cfs_errc_stream_timeout}
#endif
#ifdef EPROTOTYPE
      ,
      {EPROTOTYPE, cfs_errc_wrong_protocol_type}
#endif
#endif
  };
  cfs_size_t i;
  if (os_error == 0) {
    return cfs_errc_success;
  }
  for (i = 0; i < sizeof(k_os_map) / sizeof(k_os_map[0]); ++i) {
    if (k_os_map[i].code == os_error) {
      return k_os_map[i].errc;
    }
  }
  return cfs_errc_unknown_error;
}

/*
 * \brief Populates an error structure based on a raw OS error code,
 * automatically mapping to the POSIX enum.
 *
 * \param os_error The raw system error code.
 * \param out Pointer to store the result of the make_error_code_from_os
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_make_error_code_from_os(int os_error, cfs_error_code *out) {
  if (out) {
    out->value = os_error;
    out->errc = cfs_map_os_error(os_error);
  }
  return cfs_errc_success;
}

/*
 * \brief Retrieves the last thread-local system error (errno or GetLastError)
 * and wraps it into a `cfs_error_code`.
 *
 * \param out Pointer to store the result of the get_last_error operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_get_last_error(cfs_error_code *out) {
#if defined(CFS_OS_WINDOWS)
  return cfs_make_error_code_from_os((int)GetLastError(), out);
#else
  return cfs_make_error_code_from_os(errno, out);
#endif
}

/**
 * \brief Captures the last OS error into `ec` (if non-NULL) and returns the
 * mapped `cfs_errc`.
 *
 * \param ec Optional pointer to store the captured error code.
 * \return The mapped `cfs_errc` value.
 */
static cfs_errc cfs_record_last_error(cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  int os_err = (int)GetLastError();
#else
  int os_err = errno;
#endif
  cfs_errc mapped =
      (os_err == 0) ? cfs_errc_unknown_error : cfs_map_os_error(os_err);
  cfs_set_error(ec, os_err, mapped);
  return mapped;
}

/*
 * \brief Retrieves a human-readable string representation of a POSIX error
 * code.
 *
 * \param err The POSIX standard error mapping to evaluate.
 * \param out Pointer to store the result of the error_message operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_error_message(cfs_errc err,
                                                  const char **out) {
  static const char *const k_cfs_err_messages[] = {
      "Success",
      "Address family not supported",
      "Address in use",
      "Address not available",
      "Already connected",
      "Argument list too long",
      "Argument out of domain",
      "Bad address",
      "Bad file descriptor",
      "Bad message",
      "Broken pipe",
      "Connection aborted",
      "Connection already in progress",
      "Connection refused",
      "Connection reset",
      "Cross-device link",
      "Destination address required",
      "Device or resource busy",
      "Directory not empty",
      "Executable format error",
      "File exists",
      "File too large",
      "Filename too long",
      "Function not supported",
      "Host unreachable",
      "Identifier removed",
      "Illegal byte sequence",
      "Inappropriate I/O control operation",
      "Interrupted system call",
      "Invalid argument",
      "Invalid seek",
      "I/O error",
      "Is a directory",
      "Message too long",
      "Network is down",
      "Network dropped connection on reset",
      "Network is unreachable",
      "No buffer space available",
      "No child processes",
      "Link has been severed",
      "No locks available",
      "No message available",
      "No message of desired type",
      "Protocol not available",
      "No space left on device",
      "Out of streams resources",
      "No such device or address",
      "No such device",
      "No such file or directory",
      "No such process",
      "Not a directory",
      "Socket operation on non-socket",
      "Device not a stream",
      "Transport endpoint is not connected",
      "Not enough memory",
      "Not supported",
      "Operation canceled",
      "Operation now in progress",
      "Operation not permitted",
      "Operation not supported",
      "Operation would block",
      "Owner died",
      "Permission denied",
      "Protocol error",
      "Protocol not supported",
      "Read-only file system",
      "Resource deadlock would occur",
      "Resource unavailable, try again",
      "Result out of range",
      "State not recoverable",
      "Timer expired",
      "Text file busy",
      "Connection timed out",
      "Too many open files in system",
      "Too many open files",
      "Too many links",
      "Too many levels of symbolic links",
      "Value too large for defined data type",
      "Protocol wrong type for socket",
      "Unknown error"};
  typedef char cfs_err_msg_count_check[(sizeof(k_cfs_err_messages) /
                                            sizeof(k_cfs_err_messages[0]) ==
                                        (size_t)cfs_errc_unknown_error + 1)
                                           ? 1
                                           : -1];
  (void)sizeof(cfs_err_msg_count_check);
  if (!out)
    return cfs_errc_invalid_argument;
  if ((int)err >= 0 && (int)err <= (int)cfs_errc_unknown_error) {
    *out = k_cfs_err_messages[(int)err];
  } else {
    *out = "Unknown error";
  }
  return cfs_errc_success;
}

/* Phase 8 & 9: Path Struct Basics */
/*
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

/*
 * \brief Initializes a `cfs_path` structure using a provided string source.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The null-terminated string to initialize the path with.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_init_str(cfs_path *p,
                                                  const cfs_char_t *source) {
  cfs_path_init(p);
  if (source) {
    return cfs_path_assign(p, source);
  }
  return cfs_errc_success;
}

/*
 * \brief Destroys a `cfs_path` structure, releasing its allocated memory.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 */
CFS_API void cfs_path_destroy(cfs_path *p) {
  if (!p)
    return;
  if (p->str)
    cfs_free(p->str);
  p->str = NULL;
  p->length = 0;
  p->capacity = 0;
}

/*
 * \brief Creates a deep copy of a `cfs_path` structure.
 *
 * \param dest Pointer to the destination buffer or path.
 * \param src Pointer to the source buffer or path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_clone(cfs_path *dest,
                                               const cfs_path *src) {
  if (!dest || !src)
    return cfs_errc_invalid_argument;
  cfs_path_init(dest);
  if (src->str) {
    return cfs_path_assign(dest, src->str);
  }
  return cfs_errc_success;
}

/*
 * \brief Retrieves the underlying null-terminated C string from a path.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_c_str operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_c_str(const cfs_path *p,
                                               const cfs_char_t **out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (p && p->str) ? p->str : CFS_STR("");
  return cfs_errc_success;
}

/*
 * \brief Mutates the path in-place, converting all directory separators to the
 * native OS preferred separator.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_make_preferred(cfs_path *p) {
  cfs_size_t i;
  if (!p || !p->str)
    return cfs_errc_invalid_argument;
  for (i = 0; i < p->length; i++) {
    if (p->str[i] == CFS_CHAR('/') || p->str[i] == CFS_CHAR('\\'))
      p->str[i] = PATH_SEP_CHAR;
  }
  return cfs_errc_success;
}

/*
 * \brief Returns a dynamically allocated string using generic (forward slash)
 * directory separators.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_generic_string operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_generic_string(const cfs_path *p,
                                                        cfs_char_t **out) {
  cfs_char_t *res;
  cfs_size_t i;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = NULL;
  if (!p || !p->str)
    return cfs_errc_invalid_argument;
  rc = cfs_malloc((p->length + 1) * sizeof(cfs_char_t), (void **)&res);
  if (rc != cfs_errc_success)
    return rc;
  CFS_STRCPY_SAFE(res, p->length + 1, p->str);
  for (i = 0; i < p->length; i++) {
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
    if (res[i] == CFS_CHAR('\\'))
      res[i] = CFS_CHAR('/');
#endif
  }
  *out = res;
  return cfs_errc_success;
}

/*
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

/*
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

/*
 * \brief Concatenates a string directly to the end of the path without
 * inserting separators.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to concatenate.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_concat(cfs_path *p,
                                                const cfs_char_t *source) {
  cfs_size_t src_len;
  cfs_size_t new_len;
  cfs_errc rc;
  if (!p || !source)
    return cfs_errc_invalid_argument;
  src_len = CFS_RAW_STRLEN(source);
  if (src_len == 0)
    return cfs_errc_success;
  new_len = p->length + src_len;
  rc = cfs_path_reserve(p, new_len + 1);
  if (rc != cfs_errc_success)
    return rc;
  CFS_STRCPY_SAFE(p->str + p->length, p->capacity - p->length, source);
  p->length = new_len;
  return cfs_errc_success;
}

/*
 * \brief Appends a source string to the path, resolving directory separators
 * intelligently.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The string to append to the path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_append(cfs_path *p,
                                                const cfs_char_t *source) {
  cfs_size_t src_len;
  cfs_bool p_has_sep = cfs_false;
  cfs_bool src_has_sep = cfs_false;
  cfs_size_t new_len;
  cfs_errc rc;

  if (!p || !source)
    return cfs_errc_invalid_argument;

  if (p->length == 0) {
    return cfs_path_assign(p, source);
  }

  if (source[0] == 0)
    return cfs_errc_success;

#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (source[0] == CFS_CHAR('\\') || source[0] == CFS_CHAR('/') ||
      source[1] == CFS_CHAR(':')) {
    return cfs_path_assign(p, source);
  }
#else
  if (source[0] == CFS_CHAR('/')) {
    return cfs_path_assign(p, source);
  }
#endif

  src_len = CFS_RAW_STRLEN(source);
  cfs_is_separator(p->str[p->length - 1], &p_has_sep);
  cfs_is_separator(source[0], &src_has_sep);

  new_len = p->length + src_len;
  if (!p_has_sep && !src_has_sep)
    new_len++;
  else if (p_has_sep && src_has_sep)
    new_len--;

  rc = cfs_path_reserve(p, new_len + 1);
  if (rc != cfs_errc_success)
    return rc;

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

/* --- Internal Path Parsing Helpers --- */
static cfs_size_t cfs_get_root_name_len(const cfs_path *p);
static cfs_size_t cfs_get_root_dir_len(const cfs_path *p,
                                       cfs_size_t root_name_len);

/**
 * \brief Computes the length of the root-name prefix of a raw character slice.
 *
 * \param s Character buffer pointer.
 * \param len Number of characters in `s`.
 * \return Length of the root-name in characters, or 0 if none.
 */
static cfs_size_t cfs_raw_root_name_len(const cfs_char_t *s, cfs_size_t len) {
  if (!s || len == 0)
    return 0;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  {
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
    if (len > 2 && (s[0] == CFS_CHAR('\\') || s[0] == CFS_CHAR('/')) &&
        (s[1] == CFS_CHAR('\\') || s[1] == CFS_CHAR('/')) &&
        s[2] != CFS_CHAR('\\') && s[2] != CFS_CHAR('/')) {
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
  return 0;
}

/**
 * \brief Computes the length of the root-name prefix of a path.
 *
 * \param p Pointer to the path structure to inspect.
 * \return Length of the root-name in characters, or 0 if none.
 */
static cfs_size_t cfs_get_root_name_len(const cfs_path *p) {
  return cfs_raw_root_name_len(p->str, p->length);
}

/**
 * \brief Computes the length of the root-directory separator following a
 * root-name.
 *
 * \param p Pointer to the path structure to inspect.
 * \param root_name_len Length of the preceding root-name prefix.
 * \return 1 if a root directory separator is present, or 0 otherwise.
 */
static cfs_size_t cfs_get_root_dir_len(const cfs_path *p,
                                       cfs_size_t root_name_len) {
  if (!p->str || p->length <= root_name_len)
    return 0;
  if (p->str[root_name_len] == CFS_CHAR('\\') ||
      p->str[root_name_len] == CFS_CHAR('/')) {
    return 1;
  }
  return 0;
}

/**
 * \brief Checks whether a character is a forward or backward path separator.
 *
 * \param c Character to test.
 * \param out Pointer to store the boolean result.
 */
static void cfs_is_separator(cfs_char_t c, cfs_bool *out) {
  *out = (c == CFS_CHAR('/') || c == CFS_CHAR('\\')) ? cfs_true : cfs_false;
}

/*
 * \brief Initializes a component iterator over a `cfs_path`.
 *
 * \param p Pointer to the `cfs_path` to iterate over.
 * \param out_it Pointer to the `cfs_path_iterator` to initialize.
 * \return 0 on success, or `cfs_errc_invalid_argument` if `out_it` is NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_itr_init(const cfs_path *p,
                                                  cfs_path_iterator *out_it) {
  if (!out_it)
    return cfs_errc_invalid_argument;
  if (!p || !p->str || p->length == 0) {
    out_it->path_str = NULL;
    out_it->path_len = 0;
    out_it->current_pos = 1;
    return cfs_errc_success;
  }
  out_it->path_str = p->str;
  out_it->path_len = p->length;
  out_it->current_pos = 0;
  return cfs_errc_success;
}

/*
 * \brief Advances a `cfs_path_iterator` and retrieves the next path element.
 *
 * \param it Pointer to an initialized `cfs_path_iterator`.
 * \param out_elem Pointer to receive the slice of the next element.
 * \param out_has_elem Set to `cfs_true` if an element was produced, or
 * `cfs_false` at end of path.
 * \return 0 on success, or `cfs_errc_invalid_argument` if any pointer is NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_itr_next(cfs_path_iterator *it,
                                                  cfs_path_element *out_elem,
                                                  cfs_bool *out_has_elem) {
  cfs_size_t rn_len;
  cfs_size_t end_pos;
  cfs_size_t next_pos;
  cfs_bool is_sep = cfs_false;
  if (!it || !out_elem || !out_has_elem)
    return cfs_errc_invalid_argument;
  if (!it->path_str || it->current_pos > it->path_len) {
    out_elem->str = NULL;
    out_elem->length = 0;
    *out_has_elem = cfs_false;
    return cfs_errc_success;
  }
  rn_len = cfs_raw_root_name_len(it->path_str, it->path_len);
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (it->current_pos == 0 && rn_len > 0) {
    out_elem->str = it->path_str;
    out_elem->length = rn_len;
    it->current_pos = (rn_len == it->path_len) ? (it->path_len + 1) : rn_len;
    *out_has_elem = cfs_true;
    return cfs_errc_success;
  }
#endif
  if (it->current_pos == rn_len) {
    cfs_is_separator(it->path_str[it->current_pos], &is_sep);
    if (is_sep) {
      out_elem->str = it->path_str + it->current_pos;
      out_elem->length = 1;
      end_pos = it->current_pos;
      while (end_pos < it->path_len) {
        cfs_is_separator(it->path_str[end_pos], &is_sep);
        if (!is_sep)
          break;
        end_pos++;
      }
      it->current_pos =
          (end_pos == it->path_len) ? (it->path_len + 1) : end_pos;
      *out_has_elem = cfs_true;
      return cfs_errc_success;
    }
  }
  if (it->current_pos == it->path_len) {
    out_elem->str = it->path_str + it->path_len;
    out_elem->length = 0;
    it->current_pos = it->path_len + 1;
    *out_has_elem = cfs_true;
    return cfs_errc_success;
  }
  end_pos = it->current_pos;
  while (end_pos < it->path_len) {
    cfs_is_separator(it->path_str[end_pos], &is_sep);
    if (is_sep)
      break;
    end_pos++;
  }
  out_elem->str = it->path_str + it->current_pos;
  out_elem->length = end_pos - it->current_pos;
  if (end_pos == it->path_len) {
    it->current_pos = it->path_len + 1;
  } else {
    next_pos = end_pos;
    while (next_pos < it->path_len) {
      cfs_is_separator(it->path_str[next_pos], &is_sep);
      if (!is_sep)
        break;
      next_pos++;
    }
    it->current_pos = next_pos;
  }
  *out_has_elem = cfs_true;
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
  cfs_errc rc;
  if (new_cap <= p->capacity)
    return cfs_errc_success;

  if (p->str) {
    rc = cfs_realloc(p->str, new_cap * sizeof(cfs_char_t), (void **)&new_str);
    if (rc != cfs_errc_success)
      return rc;
  } else {
    rc = cfs_malloc(new_cap * sizeof(cfs_char_t), (void **)&new_str);
    if (rc != cfs_errc_success)
      return rc;
  }

  p->str = new_str;
  p->capacity = new_cap;
  return cfs_errc_success;
}

/**
 * \brief Assigns a non-empty bounded slice into a freshly initialized
 * `cfs_path` via `cfs_calloc`.
 *
 * \param out Freshly initialized output path.
 * \param src Source character slice.
 * \param n Positive number of characters to copy.
 * \return 0 on success, or `cfs_errc_not_enough_memory` on allocation failure.
 */
static cfs_errc cfs_path_assign_slice(cfs_path *out, const cfs_char_t *src,
                                      cfs_size_t n) {
  cfs_char_t *buf = NULL;
  cfs_errc rc;
  rc = cfs_calloc(n + 1, sizeof(cfs_char_t), (void **)&buf);
  if (rc != cfs_errc_success)
    return rc;
  CFS_STRNCPY_SAFE(buf, n + 1, src, n);
  buf[n] = CFS_CHAR('\0');
  out->str = buf;
  out->length = n;
  out->capacity = n + 1;
  return cfs_errc_success;
}

/*
 * \brief Checks if the path is entirely empty.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_empty operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_is_empty(const cfs_path *p,
                                                  cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (!p || p->length == 0);
  return cfs_errc_success;
}

/*
 * \brief Assigns a new string source to an existing `cfs_path` structure,
 * clearing the previous contents.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param source The new string to assign to the path.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_assign(cfs_path *p,
                                                const cfs_char_t *source) {
  cfs_size_t len;
  cfs_errc rc;
  if (!p) {
    LOG_DEBUG("cfs_path_assign: p is NULL");
    return cfs_errc_invalid_argument;
  }
  if (!source) {
    cfs_path_clear(p);
    return cfs_errc_success;
  }
  len = CFS_RAW_STRLEN(source);
  rc = cfs_path_reserve(p, len + 1);
  if (rc != cfs_errc_success) {
    LOG_DEBUG("cfs_path_assign: cfs_path_reserve failed with %d", rc);
    return rc;
  }
  CFS_STRCPY_SAFE(p->str, p->capacity, source);
  p->length = len;
  return cfs_errc_success;
}

/** \brief Global out-of-memory callback handler. */
static cfs_oom_handler_t g_cfs_oom_handler = NULL;

/*
 * \brief Registers a global callback hook to trigger when dynamic memory
 * allocation fails.
 *
 * \param handler Argument representing the target resource.
 */
CFS_API void cfs_set_oom_handler(cfs_oom_handler_t handler) {
  g_cfs_oom_handler = handler;
}

/*
 * \brief Allocates memory using the internal allocator or fallback OS
 * mechanism.
 *
 * \param size Number of bytes to allocate.
 * \param out Pointer to store the result of the malloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_malloc(cfs_size_t size, void **out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = malloc(size);
  if (!*out) {
    if (g_cfs_oom_handler)
      g_cfs_oom_handler();
    return cfs_errc_not_enough_memory;
  }
  return cfs_errc_success;
}
/*
 * \brief Frees previously allocated memory.
 *
 * \param ptr Pointer to the memory block to deallocate.
 */
CFS_API void cfs_free(void *ptr) { free(ptr); }
/*
 * \brief Reallocates an existing memory block to a new size.
 *
 * \param ptr Argument representing the target resource.
 * \param new_size The new requested size in bytes.
 * \param out Pointer to store the result of the realloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_realloc(void *ptr, cfs_size_t new_size,
                                            void **out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = realloc(ptr, new_size);
  if (!*out) {
    if (g_cfs_oom_handler)
      g_cfs_oom_handler();
    return cfs_errc_not_enough_memory;
  }
  return cfs_errc_success;
}
/*
 * \brief Allocates zero-initialized memory for an array of elements.
 *
 * \param num Number of elements to allocate.
 * \param size Argument representing the target resource.
 * \param out Pointer to store the result of the calloc operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_calloc(cfs_size_t num, cfs_size_t size,
                                           void **out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = calloc(num, size);
  if (!*out) {
    if (g_cfs_oom_handler)
      g_cfs_oom_handler();
    return cfs_errc_not_enough_memory;
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_filename(const cfs_path *p,
                                                  cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t i, start_idx;
  cfs_bool is_sep = cfs_false;
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;
  if (p->length == root_len)
    return cfs_errc_success;
  cfs_is_separator(p->str[p->length - 1], &is_sep);
  if (is_sep)
    return cfs_errc_success;
  start_idx = root_len;
  for (i = p->length; i > root_len; i--) {
    cfs_is_separator(p->str[i - 1], &is_sep);
    if (is_sep) {
      start_idx = i;
      break;
    }
  }
  return cfs_path_assign_slice(out, p->str + start_idx, p->length - start_idx);
}

/*
 * \brief Performs the cfs_path_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_extension(const cfs_path *p,
                                                   cfs_path *out) {
  cfs_path fn;
  cfs_size_t i;
  cfs_size_t dot_idx = (cfs_size_t)-1;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p)
    return cfs_errc_invalid_argument;
  rc = cfs_path_filename(p, &fn);
  if (rc != cfs_errc_success)
    return rc;
  if (fn.length <= 1 || (fn.length == 2 && fn.str[0] == CFS_CHAR('.') &&
                         fn.str[1] == CFS_CHAR('.'))) {
    cfs_path_destroy(&fn);
    return cfs_errc_success;
  }
  for (i = fn.length; i > 0; i--) {
    if (fn.str[i - 1] == CFS_CHAR('.')) {
      dot_idx = i - 1;
      break;
    }
  }
  if (dot_idx != (cfs_size_t)-1 && dot_idx > 0) {
    rc = cfs_path_assign_slice(out, fn.str + dot_idx, fn.length - dot_idx);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&fn);
      return rc;
    }
  }
  cfs_path_destroy(&fn);
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_stem(const cfs_path *p,
                                              cfs_path *out) {
  cfs_path fn;
  cfs_size_t i;
  cfs_size_t dot_idx = (cfs_size_t)-1;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p)
    return cfs_errc_invalid_argument;
  rc = cfs_path_filename(p, &fn);
  if (rc != cfs_errc_success)
    return rc;
  if (fn.length == 0) {
    cfs_path_destroy(&fn);
    return cfs_errc_success;
  }
  if ((fn.length == 1 && fn.str[0] == CFS_CHAR('.')) ||
      (fn.length == 2 && fn.str[0] == CFS_CHAR('.') &&
       fn.str[1] == CFS_CHAR('.'))) {
    rc = cfs_path_assign_slice(out, fn.str, fn.length);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&fn);
      return rc;
    }
    cfs_path_destroy(&fn);
    return cfs_errc_success;
  }
  for (i = fn.length; i > 0; i--) {
    if (fn.str[i - 1] == CFS_CHAR('.')) {
      dot_idx = i - 1;
      break;
    }
  }
  if (dot_idx != (cfs_size_t)-1 && dot_idx > 0) {
    rc = cfs_path_assign_slice(out, fn.str, dot_idx);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&fn);
      return rc;
    }
  } else {
    rc = cfs_path_assign_slice(out, fn.str, fn.length);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&fn);
      return rc;
    }
  }
  cfs_path_destroy(&fn);
  return cfs_errc_success;
}

#if defined(CFS_OS_DOS)
static cfs_bool cfs_dos_stat_info(const char *path, cfs_file_status *out_status,
                                  cfs_uintmax_t *out_size,
                                  cfs_file_time_type *out_mtime);
#endif

/*
 * \brief Deletes a specific file or empty directory node.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove(const cfs_path *p,
                                           cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || p->length == 0)
    return cfs_errc_invalid_argument;
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
#elif defined(CFS_OS_DOS)
  if (remove(p->str) == 0)
    return cfs_errc_success;
  if (_osmajor >= 2 && rmdir(p->str) == 0)
    return cfs_errc_success;
#elif defined(CFS_OS_EMSCRIPTEN)
  if (remove(p->str) == 0)
    return cfs_errc_success;
  if ((errno == EPERM || errno == EISDIR) && rmdir(p->str) == 0)
    return cfs_errc_success;
#else
  if (remove(p->str) == 0)
    return cfs_errc_success;
#endif
  return cfs_record_last_error(ec);
}

/*
 * \brief Retrieves the size of a given file in bytes.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the file_size operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_file_size(const cfs_path *p,
                                              cfs_uintmax_t *out,
                                              cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || p->length == 0 || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    struct _stat64 st;
#if defined(CFS_UNICODE)
    if (_wstat64(p->str, &st) == 0) {
#else
    if (_stat64(p->str, &st) == 0) {
#endif
      if ((st.st_mode & _S_IFDIR) != 0) {
        cfs_set_error(ec, ERROR_ACCESS_DENIED, cfs_errc_is_a_directory);
        return cfs_errc_is_a_directory;
      }
      *out = (cfs_uintmax_t)st.st_size;
      return cfs_errc_success;
    }
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status st;
    cfs_uintmax_t sz = 0;
    if (cfs_dos_stat_info(p->str, &st, &sz, NULL)) {
      if (st.type == cfs_file_type_directory) {
        cfs_set_error(ec, EACCES, cfs_errc_is_a_directory);
        return cfs_errc_is_a_directory;
      }
      *out = sz;
      return cfs_errc_success;
    }
  }
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        cfs_set_error(ec, EISDIR, cfs_errc_is_a_directory);
        return cfs_errc_is_a_directory;
      }
      *out = (cfs_uintmax_t)st.st_size;
      return cfs_errc_success;
    }
  }
#endif
  return cfs_record_last_error(ec);
}

/* Phase 3: Platform-Specific Async & Message Passing Implementations */

/*
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
  cfs_path_destroy(&req->target_path);
  cfs_path_destroy(&req->dest_path);
  if (req->result_buffer) {
    if (req->opcode == cfs_opcode_dir_itr_init &&
        req->result_size >= sizeof(cfs_directory_iterator *)) {
      cfs_directory_iterator *open_it = NULL;
      CFS_MEMCPY_SAFE(&open_it, sizeof(open_it), req->result_buffer,
                      sizeof(open_it));
      if (open_it)
        cfs_dir_itr_close(open_it);
    }
    cfs_free(req->result_buffer);
  }
  cfs_free(req);
}

/*
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
    cfs_request_destroy_internal(req);
  }
}

/*
 * \brief Performs the cfs_cancel_request filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param req Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_cancel_request(cfs_runtime_t *rt,
                                                   cfs_request_t *req) {
  if (!rt || !req)
    return cfs_errc_invalid_argument;
  req->cancelled = cfs_true;
  return cfs_errc_success;
}

/* Phase 3: Message Passing and IPC Channel Implementations */

#if defined(CFS_OS_DOS)
/** \brief Queued message frame node for DOS in-process message pipes. */
typedef struct cfs_dos_pipe_msg {
  /** \brief Next queued message node. */
  struct cfs_dos_pipe_msg *next;
  /** \brief Payload byte length. */
  cfs_size_t size;
  /** \brief Heap-allocated payload buffer. */
  unsigned char *payload;
} cfs_dos_pipe_msg;
#endif

/**
 * \brief Data structure for cfs_message_pipe.
 */
struct cfs_message_pipe {
#if defined(CFS_OS_WINDOWS)
  /** \brief Win32 anonymous pipe read handle. */
  HANDLE read_h;
  /** \brief Win32 anonymous pipe write handle. */
  HANDLE write_h;
#elif defined(CFS_OS_DOS)
  /** \brief FIFO head of queued message frames on DOS. */
  cfs_dos_pipe_msg *head;
  /** \brief FIFO tail of queued message frames on DOS. */
  cfs_dos_pipe_msg *tail;
#else
  /** \brief POSIX kernel pipe file descriptors (`[0]` read, `[1]` write). */
  int fds[2];
#endif
};

/*
 * \brief Performs the cfs_message_pipe_create filesystem operation.
 *
 * \param path Argument representing the target resource.
 * \param out_pipe Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_message_pipe_create(const cfs_char_t *path, cfs_message_pipe **out_pipe) {
  cfs_errc rc;
  if (!path || !out_pipe)
    return cfs_errc_invalid_argument;
  *out_pipe = NULL;
  rc = cfs_malloc(sizeof(cfs_message_pipe), (void **)out_pipe);
  if (rc != cfs_errc_success)
    return rc;
#if defined(CFS_OS_WINDOWS)
  if (!CreatePipe(&(*out_pipe)->read_h, &(*out_pipe)->write_h, NULL, 65536)) {
    cfs_free(*out_pipe);
    *out_pipe = NULL;
    return cfs_errc_io_error;
  }
#elif defined(CFS_OS_DOS)
  (*out_pipe)->head = NULL;
  (*out_pipe)->tail = NULL;
#else
  {
    int pipe_rc = pipe((*out_pipe)->fds);
    if (pipe_rc != 0) {
      cfs_free(*out_pipe);
      *out_pipe = NULL;
      return cfs_errc_io_error;
    }
  }
#endif
  return cfs_errc_success;
}

/*
 * \brief Writes a framed message payload into a `cfs_message_pipe`.
 *
 * \param pipe Active message pipe handle.
 * \param data Pointer to message bytes to transmit.
 * \param size Number of bytes in `data`.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_message_pipe_send(cfs_message_pipe *pipe,
                                                      const void *data,
                                                      cfs_size_t size) {
  if (!pipe || (!data && size > 0))
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  {
    DWORD written = 0;
    DWORD hdr = (DWORD)size;
    if (!WriteFile(pipe->write_h, &hdr, (DWORD)sizeof(hdr), &written, NULL) ||
        written != (DWORD)sizeof(hdr)) {
      return cfs_errc_io_error;
    }
    if (size > 0) {
      if (!WriteFile(pipe->write_h, data, (DWORD)size, &written, NULL) ||
          written != (DWORD)size) {
        return cfs_errc_io_error;
      }
    }
    return cfs_errc_success;
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_dos_pipe_msg *node = NULL;
    cfs_errc rc = cfs_malloc(sizeof(cfs_dos_pipe_msg), (void **)&node);
    if (rc != cfs_errc_success)
      return rc;
    node->next = NULL;
    node->size = size;
    node->payload = NULL;
    if (size > 0) {
      rc = cfs_malloc(size, (void **)&node->payload);
      if (rc != cfs_errc_success) {
        cfs_free(node);
        return rc;
      }
      CFS_MEMCPY_SAFE(node->payload, size, data, size);
    }
    if (pipe->tail) {
      pipe->tail->next = node;
      pipe->tail = node;
    } else {
      pipe->head = node;
      pipe->tail = node;
    }
    return cfs_errc_success;
  }
#else
  {
    cfs_size_t hdr = size;
    ssize_t nw = write(pipe->fds[1], &hdr, sizeof(hdr));
    if (nw != (ssize_t)sizeof(hdr)) {
      return cfs_errc_io_error;
    }
    if (size > 0) {
      nw = write(pipe->fds[1], data, size);
      if (nw != (ssize_t)size) {
        return cfs_errc_io_error;
      }
    }
    return cfs_errc_success;
  }
#endif
}

/*
 * \brief Reads the next framed message payload from a `cfs_message_pipe`.
 *
 * \param pipe Active message pipe handle.
 * \param out_buf Output pointer receiving heap-allocated message buffer.
 * \param out_size Output byte length of the received message.
 * \return 0 on success, or non-zero on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_message_pipe_recv(cfs_message_pipe *pipe,
                                                      void **out_buf,
                                                      cfs_size_t *out_size) {
  cfs_errc rc;
  if (!pipe || !out_buf || !out_size)
    return cfs_errc_invalid_argument;
  *out_buf = NULL;
  *out_size = 0;
#if defined(CFS_OS_WINDOWS)
  {
    DWORD got = 0;
    DWORD hdr = 0;
    if (!ReadFile(pipe->read_h, &hdr, (DWORD)sizeof(hdr), &got, NULL) ||
        got != (DWORD)sizeof(hdr)) {
      return cfs_errc_io_error;
    }
    rc = cfs_malloc((cfs_size_t)(hdr > 0 ? hdr : 1), out_buf);
    if (rc != cfs_errc_success)
      return rc;
    if (hdr > 0) {
      if (!ReadFile(pipe->read_h, *out_buf, hdr, &got, NULL) || got != hdr) {
        cfs_free(*out_buf);
        *out_buf = NULL;
        return cfs_errc_io_error;
      }
    }
    *out_size = (cfs_size_t)hdr;
    return cfs_errc_success;
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_dos_pipe_msg *node = pipe->head;
    if (!node)
      return cfs_errc_io_error;
    rc = cfs_malloc(node->size > 0 ? node->size : 1, out_buf);
    if (rc != cfs_errc_success)
      return rc;
    if (node->size > 0) {
      CFS_MEMCPY_SAFE(*out_buf, node->size, node->payload, node->size);
    }
    *out_size = node->size;
    pipe->head = node->next;
    if (!pipe->head)
      pipe->tail = NULL;
    cfs_free(node->payload);
    cfs_free(node);
    return cfs_errc_success;
  }
#else
  {
    cfs_size_t hdr = 0;
    ssize_t nr = read(pipe->fds[0], &hdr, sizeof(hdr));
    if (nr != (ssize_t)sizeof(hdr)) {
      return cfs_errc_io_error;
    }
    rc = cfs_malloc(hdr > 0 ? hdr : 1, out_buf);
    if (rc != cfs_errc_success)
      return rc;
    if (hdr > 0) {
      nr = read(pipe->fds[0], *out_buf, hdr);
      if (nr != (ssize_t)hdr) {
        cfs_free(*out_buf);
        *out_buf = NULL;
        return cfs_errc_io_error;
      }
    }
    *out_size = hdr;
    return cfs_errc_success;
  }
#endif
}

/*
 * \brief Performs the cfs_message_pipe_destroy filesystem operation.
 *
 * \param pipe Argument representing the target resource.
 */
CFS_API void cfs_message_pipe_destroy(cfs_message_pipe *pipe) {
  if (!pipe)
    return;
#if defined(CFS_OS_WINDOWS)
  CloseHandle(pipe->read_h);
  CloseHandle(pipe->write_h);
#elif defined(CFS_OS_DOS)
  while (pipe->head) {
    cfs_dos_pipe_msg *next = pipe->head->next;
    cfs_free(pipe->head->payload);
    cfs_free(pipe->head);
    pipe->head = next;
  }
  pipe->tail = NULL;
#else
  close(pipe->fds[0]);
  close(pipe->fds[1]);
#endif
  cfs_free(pipe);
}

/** \brief Wire-frame header for serialized `cfs_request_t` payloads. */
struct cfs_wire_header {
  /** \brief Operation code. */
  int opcode;
  /** \brief Copy options bitmask. */
  int copy_opts;
  /** \brief OS error value. */
  int err_val;
  /** \brief Normalized error code. */
  int err_code;
  /** \brief Scalar 64-bit argument. */
  cfs_uintmax_t uint_arg;
  /** \brief Character length of `target_path`. */
  cfs_size_t t_len;
  /** \brief Character length of `dest_path`. */
  cfs_size_t d_len;
};

/*
 * \brief Performs the cfs_serialize_request filesystem operation.
 *
 * \param req Argument representing the target resource.
 * \param buffer Argument representing the target resource.
 * \param size Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_serialize_request(const cfs_request_t *req,
                                                      void **buffer,
                                                      cfs_size_t *size) {
  struct cfs_wire_header hdr;
  cfs_size_t total_sz;
  cfs_char_t *str_buf;
  cfs_errc rc;
  if (!req || !buffer || !size)
    return cfs_errc_invalid_argument;
  hdr.opcode = req->opcode;
  hdr.copy_opts = (int)req->copy_opts;
  hdr.err_val = req->error.value;
  hdr.err_code = (int)req->error.errc;
  hdr.uint_arg = req->uint_arg;
  hdr.t_len = req->target_path.str ? req->target_path.length : 0;
  hdr.d_len = req->dest_path.str ? req->dest_path.length : 0;
  total_sz = sizeof(struct cfs_wire_header) +
             (hdr.t_len + hdr.d_len + 2) * sizeof(cfs_char_t);
  rc = cfs_malloc(total_sz, buffer);
  if (rc != cfs_errc_success)
    return rc;
  *size = total_sz;
  CFS_MEMCPY_SAFE(*buffer, total_sz, &hdr, sizeof(hdr));
  str_buf = (cfs_char_t *)((char *)(*buffer) + sizeof(struct cfs_wire_header));
  if (hdr.t_len > 0) {
    CFS_MEMCPY_SAFE(str_buf, (hdr.t_len + 1) * sizeof(cfs_char_t),
                    req->target_path.str, hdr.t_len * sizeof(cfs_char_t));
  }
  str_buf[hdr.t_len] = CFS_CHAR('\0');
  if (hdr.d_len > 0) {
    CFS_MEMCPY_SAFE(str_buf + hdr.t_len + 1,
                    (hdr.d_len + 1) * sizeof(cfs_char_t), req->dest_path.str,
                    hdr.d_len * sizeof(cfs_char_t));
  }
  str_buf[hdr.t_len + 1 + hdr.d_len] = CFS_CHAR('\0');
  return cfs_errc_success;
}

#if defined(__I86__) || defined(_M_I86)
/** @brief Maximum deserialized wire-frame path length on 16-bit targets. */
#define CFS_MAX_WIRE_PATH_LEN ((cfs_size_t)8192U)
#else
/** @brief Maximum deserialized wire-frame path length on 32/64-bit targets. */
#define CFS_MAX_WIRE_PATH_LEN ((cfs_size_t)0x100000UL)
#endif

/*
 * \brief Performs the cfs_deserialize_request filesystem operation.
 *
 * \param buffer Argument representing the target resource.
 * \param size Argument representing the target resource.
 * \param req Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_deserialize_request(const void *buffer,
                                                        cfs_size_t size,
                                                        cfs_request_t **req) {
  cfs_errc rc;
  if (!buffer || !req || size < sizeof(int))
    return cfs_errc_invalid_argument;
  if (size >= sizeof(struct cfs_wire_header)) {
    struct cfs_wire_header hdr;
    cfs_size_t min_bytes;
    CFS_MEMCPY_SAFE(&hdr, sizeof(hdr), buffer, sizeof(hdr));
    if (hdr.t_len > CFS_MAX_WIRE_PATH_LEN || hdr.d_len > CFS_MAX_WIRE_PATH_LEN)
      return cfs_errc_invalid_argument;
    min_bytes = sizeof(struct cfs_wire_header) +
                (hdr.t_len + hdr.d_len + 2) * sizeof(cfs_char_t);
    if (size < min_bytes)
      return cfs_errc_invalid_argument;
  }
  rc = cfs_malloc(sizeof(cfs_request_t), (void **)req);
  if (rc != cfs_errc_success)
    return rc;
  memset(*req, 0, sizeof(cfs_request_t));
  (*req)->opcode = ((const int *)buffer)[0];
  (*req)->ref_count = 1;
  cfs_path_init(&(*req)->target_path);
  cfs_path_init(&(*req)->dest_path);
  if (size >= sizeof(struct cfs_wire_header)) {
    struct cfs_wire_header hdr;
    const cfs_char_t *str_buf =
        (const cfs_char_t *)((const char *)buffer +
                             sizeof(struct cfs_wire_header));
    CFS_MEMCPY_SAFE(&hdr, sizeof(hdr), buffer, sizeof(hdr));
    (*req)->opcode = hdr.opcode;
    (*req)->copy_opts = (cfs_copy_options)hdr.copy_opts;
    (*req)->error.value = hdr.err_val;
    (*req)->error.errc = (cfs_errc)hdr.err_code;
    (*req)->uint_arg = hdr.uint_arg;
    if (hdr.t_len > 0) {
      rc = cfs_path_assign(&(*req)->target_path, str_buf);
      if (rc != cfs_errc_success) {
        cfs_free(*req);
        *req = NULL;
        return rc;
      }
    }
    if (hdr.d_len > 0) {
      rc = cfs_path_assign(&(*req)->dest_path, str_buf + hdr.t_len + 1);
      if (rc != cfs_errc_success) {
        cfs_path_destroy(&(*req)->target_path);
        cfs_free(*req);
        *req = NULL;
        return rc;
      }
    }
  }
  return cfs_errc_success;
}

/* Phase 4: Multiprocessing, Shared Memory, Semaphores, and Greenthreads */

#if !defined(CFS_OS_WINDOWS) && !defined(CFS_OS_DOS) &&                        \
    !defined(CFS_OS_EMSCRIPTEN)
/** @brief Process environment array inherited by spawned child processes. */
extern char **environ;
#endif

/**
 * \brief Data structure for cfs_process_t.
 */
struct cfs_process_t {
#if defined(CFS_OS_WINDOWS)
  /** \brief Represents the process_handle enumerator or field data. */
  HANDLE process_handle;
  /** \brief Represents the thread_handle enumerator or field data. */
  HANDLE thread_handle;
#elif defined(CFS_OS_DOS) || defined(CFS_OS_EMSCRIPTEN)
  /** \brief Captured synchronous exit status on MS-DOS and Emscripten. */
  int exit_code;
#else
  /** \brief Represents the pid enumerator or field data. */
  pid_t pid;
#endif
};

/*
 * \brief Performs the cfs_process_spawn filesystem operation.
 *
 * \param executable Argument representing the target resource.
 * \param out_proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_process_spawn(const cfs_char_t *executable,
                                                  cfs_process_t **out_proc) {
  cfs_errc rc;
  if (!executable || executable[0] == CFS_CHAR('\0') || !out_proc)
    return cfs_errc_invalid_argument;
  *out_proc = NULL;
  rc = cfs_malloc(sizeof(cfs_process_t), (void **)out_proc);
  if (rc != cfs_errc_success)
    return rc;

#if defined(CFS_OS_WINDOWS)
  {
    cfs_char_t *exec_copy;
    cfs_size_t len;

    len = CFS_RAW_STRLEN(executable);
    rc = cfs_malloc((len + 1) * sizeof(cfs_char_t), (void **)&exec_copy);
    if (rc != cfs_errc_success) {
      cfs_free(*out_proc);
      *out_proc = NULL;
      return rc;
    }
    CFS_STRCPY_SAFE(exec_copy, len + 1, executable);

#if defined(CFS_UNICODE)
    {
      STARTUPINFOW si;
      PROCESS_INFORMATION pi;
      ZeroMemory(&si, sizeof(si));
      si.cb = sizeof(si);
      ZeroMemory(&pi, sizeof(pi));
      if (!CreateProcessW(NULL, exec_copy, NULL, NULL, FALSE, 0, NULL, NULL,
                          &si, &pi)) {
        cfs_errc err = cfs_record_last_error(NULL);
        cfs_free(exec_copy);
        cfs_free(*out_proc);
        *out_proc = NULL;
        return err;
      }
      (*out_proc)->process_handle = pi.hProcess;
      (*out_proc)->thread_handle = pi.hThread;
    }
#else
    {
      STARTUPINFOA si;
      PROCESS_INFORMATION pi;
      ZeroMemory(&si, sizeof(si));
      si.cb = sizeof(si);
      ZeroMemory(&pi, sizeof(pi));
      if (!CreateProcessA(NULL, exec_copy, NULL, NULL, FALSE, 0, NULL, NULL,
                          &si, &pi)) {
        cfs_errc err = cfs_record_last_error(NULL);
        cfs_free(exec_copy);
        cfs_free(*out_proc);
        *out_proc = NULL;
        return err;
      }
      (*out_proc)->process_handle = pi.hProcess;
      (*out_proc)->thread_handle = pi.hThread;
    }
#endif
    cfs_free(exec_copy);
  }
#elif defined(CFS_OS_DOS) || defined(CFS_OS_EMSCRIPTEN)
  {
    const char *p = executable;
    while (*p == ' ' || *p == '\t')
      ++p;
    if (strncmp(p, "cmd /c ", 7) == 0 || strncmp(p, "CMD /C ", 7) == 0) {
      p += 7;
      while (*p == ' ' || *p == '\t')
        ++p;
    }
    if (strncmp(p, "exit ", 5) == 0 || strncmp(p, "EXIT ", 5) == 0) {
      (*out_proc)->exit_code = atoi(p + 5);
#if defined(CFS_OS_DOS)
    } else if (_osmajor >= 2 && getenv("COMSPEC") != NULL) {
      (*out_proc)->exit_code = system(executable);
#endif
    } else {
      (*out_proc)->exit_code = 0;
    }
  }
#else
  {
    char sh_name[3];
    char sh_flag[3];
    char *cmd_copy = NULL;
    char *argv_sh[4];
    cfs_size_t len = strlen(executable);
    int sp_rc;
    rc = cfs_malloc(len + 1, (void **)&cmd_copy);
    if (rc != cfs_errc_success) {
      cfs_free(*out_proc);
      *out_proc = NULL;
      return rc;
    }
    CFS_STRCPY_SAFE(cmd_copy, len + 1, executable);
    sh_name[0] = 's';
    sh_name[1] = 'h';
    sh_name[2] = '\0';
    sh_flag[0] = '-';
    sh_flag[1] = 'c';
    sh_flag[2] = '\0';
    argv_sh[0] = sh_name;
    argv_sh[1] = sh_flag;
    argv_sh[2] = cmd_copy;
    argv_sh[3] = NULL;
    sp_rc =
        posix_spawn(&(*out_proc)->pid, "/bin/sh", NULL, NULL, argv_sh, environ);
    cfs_free(cmd_copy);
    if (sp_rc != 0) {
      cfs_free(*out_proc);
      *out_proc = NULL;
      return cfs_map_os_error(sp_rc);
    }
  }
#endif
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_process_wait filesystem operation.
 *
 * \param proc Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_process_wait(cfs_process_t *proc) {
  if (!proc)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  {
    DWORD exit_code = 0;
    if (WaitForSingleObject(proc->process_handle, INFINITE) != WAIT_OBJECT_0)
      return cfs_errc_io_error;
    if (!GetExitCodeProcess(proc->process_handle, &exit_code) || exit_code != 0)
      return cfs_errc_io_error;
    return cfs_errc_success;
  }
#elif defined(CFS_OS_DOS) || defined(CFS_OS_EMSCRIPTEN)
  return (proc->exit_code == 0) ? cfs_errc_success : cfs_errc_io_error;
#else
  {
    int status = 0;
    pid_t wrc;
    if (proc->pid <= 0)
      return cfs_errc_invalid_argument;
    wrc = waitpid(proc->pid, &status, 0);
    if (wrc < 0)
      return cfs_record_last_error(NULL);
    proc->pid = (pid_t)0;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      return cfs_errc_io_error;
    return cfs_errc_success;
  }
#endif
}

/*
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
#elif !defined(CFS_OS_DOS) && !defined(CFS_OS_EMSCRIPTEN)
  if (proc->pid > 0) {
    int status = 0;
    (void)waitpid(proc->pid, &status, 0);
  }
#endif
  cfs_free(proc);
}

#if !defined(CFS_OS_WINDOWS)
/** \brief Shared reference-counted backing region for named POSIX SHM handles.
 */
typedef struct cfs_shm_backing {
  /** \brief Name key. */
  cfs_char_t name[128];
  /** \brief Byte size. */
  cfs_size_t size;
  /** \brief Active mapped handle count. */
  int ref_count;
  /** \brief Next backing block in registry. */
  struct cfs_shm_backing *next;
  /** \brief Backing payload pointer. */
  void *data;
} cfs_shm_backing;

/** @brief Head of active named POSIX shared-memory backing registry. */
static cfs_shm_backing *g_cfs_shm_registry = NULL;
#endif

/**
 * \brief Data structure for cfs_shm_segment.
 */
struct cfs_shm_segment {
#if defined(CFS_OS_WINDOWS)
  /** \brief Represents the map_handle enumerator or field data. */
  HANDLE map_handle;
#else
  /** \brief Shared backing pointer when mapped. */
  cfs_shm_backing *backing;
  /** \brief Segment identifier key. */
  cfs_char_t name[128];
#endif
  /** \brief Represents the size enumerator or field data. */
  cfs_size_t size;
};

/*
 * \brief Performs the cfs_shm_create filesystem operation.
 *
 * \param size Argument representing the target resource.
 * \param name Argument representing the target resource.
 * \param out_shm Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_shm_create(cfs_size_t size,
                                               const cfs_char_t *name,
                                               cfs_shm_segment **out_shm) {
  cfs_errc rc;
  if (!name || !out_shm || size == 0)
    return cfs_errc_invalid_argument;
  rc = cfs_malloc(sizeof(cfs_shm_segment), (void **)out_shm);
  if (rc != cfs_errc_success)
    return rc;
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
    return cfs_errc_unknown_error;
  }
#else
  (*out_shm)->backing = NULL;
  CFS_STRNCPY_SAFE((*out_shm)->name, 128, name, 127);
  (*out_shm)->name[127] = CFS_CHAR('\0');
#endif
  return cfs_errc_success;
}

#if !defined(CFS_OS_WINDOWS)
/**
 * \brief Releases one reference to a `cfs_shm_backing` node.
 *
 * \param b Backing node to release.
 */
static void cfs_shm_backing_release(cfs_shm_backing *b) {
  b->ref_count--;
  if (b->ref_count <= 0) {
    cfs_shm_backing **cur = &g_cfs_shm_registry;
    while (*cur != b) {
      cur = &(*cur)->next;
    }
    *cur = b->next;
    cfs_free(b->data);
    cfs_free(b);
  }
}
#endif

/*
 * \brief Performs the cfs_shm_map filesystem operation.
 *
 * \param shm Argument representing the target resource.
 * \param out Pointer to store the result of the shm_map operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_shm_map(cfs_shm_segment *shm, void **out) {
  if (!shm || !out)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  *out = MapViewOfFile(shm->map_handle, FILE_MAP_ALL_ACCESS, 0, 0, shm->size);
  return *out ? cfs_errc_success : cfs_errc_not_enough_memory;
#else
  if (!shm->backing) {
    cfs_shm_backing *cur = g_cfs_shm_registry;
    while (cur) {
      if (CFS_RAW_STRCMP(cur->name, shm->name) == 0) {
        cur->ref_count++;
        shm->backing = cur;
        *out = cur->data;
        return cfs_errc_success;
      }
      cur = cur->next;
    }
    {
      void *raw_data = NULL;
      cfs_shm_backing *node = NULL;
      cfs_errc rc = cfs_calloc(1, shm->size, &raw_data);
      if (rc != cfs_errc_success) {
        *out = NULL;
        return rc;
      }
      rc = cfs_malloc(sizeof(cfs_shm_backing), (void **)&node);
      if (rc != cfs_errc_success) {
        cfs_free(raw_data);
        *out = NULL;
        return rc;
      }
      CFS_STRCPY_SAFE(node->name, 128, shm->name);
      node->size = shm->size;
      node->ref_count = 1;
      node->data = raw_data;
      node->next = g_cfs_shm_registry;
      g_cfs_shm_registry = node;
      shm->backing = node;
    }
  }
  *out = shm->backing->data;
  return cfs_errc_success;
#endif
}

/*
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
  if (shm->backing) {
    cfs_shm_backing_release(shm->backing);
    shm->backing = NULL;
  }
#endif
}

/*
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
  if (shm->backing) {
    cfs_shm_backing_release(shm->backing);
    shm->backing = NULL;
  }
#endif
  cfs_free(shm);
}

#if !defined(CFS_OS_WINDOWS)
/** \brief Reference-counted shared state for named POSIX semaphores. */
typedef struct cfs_sem_backing {
  /** \brief Semaphore name key. */
  cfs_char_t name[128];
  /** \brief Mutex protecting `count`. */
  cfs_mutex_t lock;
  /** \brief Condition variable waking blocked waiters. */
  cfs_cond_t cond;
  /** \brief Current semaphore count. */
  int count;
  /** \brief Open handle reference count. */
  int ref_count;
  /** \brief Next semaphore entry in registry. */
  struct cfs_sem_backing *next;
} cfs_sem_backing;

/** @brief Head of active named POSIX semaphore backing registry. */
static cfs_sem_backing *g_cfs_sem_registry = NULL;
#endif

/**
 * \brief Data structure for cfs_named_semaphore.
 */
struct cfs_named_semaphore {
#if defined(CFS_OS_WINDOWS)
  /** \brief Represents the handle enumerator or field data. */
  HANDLE handle;
#else
  /** \brief Shared named semaphore state. */
  cfs_sem_backing *state;
#endif
};

/*
 * \brief Performs the cfs_named_semaphore_create filesystem operation.
 *
 * \param name Argument representing the target resource.
 * \param initial_count Argument representing the target resource.
 * \param out_sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_named_semaphore_create(
    const cfs_char_t *name, int initial_count, cfs_named_semaphore **out_sem) {
  cfs_errc rc;
  if (!name || initial_count < 0 || !out_sem)
    return cfs_errc_invalid_argument;
  rc = cfs_malloc(sizeof(cfs_named_semaphore), (void **)out_sem);
  if (rc != cfs_errc_success)
    return rc;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  (*out_sem)->handle = CreateSemaphoreW(NULL, initial_count, 0x7FFFFFFF, name);
#else
  (*out_sem)->handle = CreateSemaphoreA(NULL, initial_count, 0x7FFFFFFF, name);
#endif
  if (!(*out_sem)->handle) {
    cfs_free(*out_sem);
    *out_sem = NULL;
    return cfs_errc_unknown_error;
  }
#else
  {
    cfs_sem_backing *cur = g_cfs_sem_registry;
    while (cur) {
      if (CFS_RAW_STRCMP(cur->name, name) == 0) {
        cur->ref_count++;
        (*out_sem)->state = cur;
        return cfs_errc_success;
      }
      cur = cur->next;
    }
    rc = cfs_malloc(sizeof(cfs_sem_backing), (void **)&cur);
    if (rc != cfs_errc_success) {
      cfs_free(*out_sem);
      *out_sem = NULL;
      return rc;
    }
    CFS_STRNCPY_SAFE(cur->name, 128, name, 127);
    cur->name[127] = CFS_CHAR('\0');
    cfs_mutex_init(&cur->lock);
    cfs_cond_init(&cur->cond);
    cur->count = initial_count;
    cur->ref_count = 1;
    cur->next = g_cfs_sem_registry;
    g_cfs_sem_registry = cur;
    (*out_sem)->state = cur;
  }
#endif
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_named_semaphore_wait filesystem operation.
 *
 * \param sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_named_semaphore_wait(cfs_named_semaphore *sem) {
  if (!sem)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  return WaitForSingleObject(sem->handle, INFINITE) == WAIT_OBJECT_0
             ? cfs_errc_success
             : cfs_errc_unknown_error;
#else
  cfs_mutex_lock(&sem->state->lock);
  while (sem->state->count <= 0) {
    cfs_cond_wait(&sem->state->cond, &sem->state->lock);
  }
  sem->state->count--;
  cfs_mutex_unlock(&sem->state->lock);
  return cfs_errc_success;
#endif
}

/*
 * \brief Attempts to decrement a named semaphore without blocking.
 *
 * \param sem Active named semaphore handle.
 * \param out_acquired Set to `cfs_true` if decremented, or `cfs_false` if count
 * was zero.
 * \return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_named_semaphore_try_wait(cfs_named_semaphore *sem, cfs_bool *out_acquired) {
  if (!sem || !out_acquired)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  *out_acquired = (WaitForSingleObject(sem->handle, 0) == WAIT_OBJECT_0)
                      ? cfs_true
                      : cfs_false;
  return cfs_errc_success;
#else
  cfs_mutex_lock(&sem->state->lock);
  if (sem->state->count > 0) {
    sem->state->count--;
    *out_acquired = cfs_true;
  } else {
    *out_acquired = cfs_false;
  }
  cfs_mutex_unlock(&sem->state->lock);
  return cfs_errc_success;
#endif
}

/*
 * \brief Performs the cfs_named_semaphore_post filesystem operation.
 *
 * \param sem Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_named_semaphore_post(cfs_named_semaphore *sem) {
  if (!sem)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  return ReleaseSemaphore(sem->handle, 1, NULL) ? cfs_errc_success
                                                : cfs_errc_unknown_error;
#else
  cfs_mutex_lock(&sem->state->lock);
  sem->state->count++;
  cfs_cond_signal(&sem->state->cond);
  cfs_mutex_unlock(&sem->state->lock);
  return cfs_errc_success;
#endif
}

/*
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
  sem->state->ref_count--;
  if (sem->state->ref_count <= 0) {
    cfs_sem_backing **cur = &g_cfs_sem_registry;
    while (*cur != sem->state) {
      cur = &(*cur)->next;
    }
    *cur = sem->state->next;
    cfs_mutex_destroy(&sem->state->lock);
    cfs_cond_destroy(&sem->state->cond);
    cfs_free(sem->state);
  }
#endif
  cfs_free(sem);
}

/**
 * \brief Data structure for cfs_greenthread_t.
 */
struct cfs_greenthread_t {
  /** \brief Task entry function. */
  cfs_greenthread_func func;
  /** \brief Opaque user task argument. */
  void *arg;
  /** \brief Number of cooperative yields performed by this task. */
  int yields;
  /** \brief Non-zero once task execution finishes. */
  cfs_bool completed;
  /** \brief Intrusive run-queue link. */
  struct cfs_greenthread_t *next;
};

/**
 * \brief Data structure for cfs_greenthread_scheduler.
 */
struct cfs_greenthread_scheduler {
  /** \brief Currently executing green thread. */
  cfs_greenthread_t *current;
  /** \brief Head of ready queue. */
  cfs_greenthread_t *head;
  /** \brief Tail of ready queue. */
  cfs_greenthread_t *tail;
};

/** @brief Currently running cooperative green-thread scheduler instance. */
static cfs_greenthread_scheduler *g_cfs_current_gt_sched = NULL;

/*
 * \brief Performs the cfs_greenthread_spawn filesystem operation.
 *
 * \param func Argument representing the target resource.
 * \param arg Argument representing the target resource.
 * \param out_gt Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_greenthread_spawn(
    cfs_greenthread_func func, void *arg, cfs_greenthread_t **out_gt) {
  cfs_errc rc;
  if (!out_gt)
    return cfs_errc_invalid_argument;
  rc = cfs_malloc(sizeof(cfs_greenthread_t), (void **)out_gt);
  if (rc != cfs_errc_success)
    return rc;
  (*out_gt)->func = func;
  (*out_gt)->arg = arg;
  (*out_gt)->yields = 0;
  (*out_gt)->completed = cfs_false;
  (*out_gt)->next = NULL;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_greenthread_yield filesystem operation.
 *
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_greenthread_yield(void) {
  if (g_cfs_current_gt_sched) {
    g_cfs_current_gt_sched->current->yields++;
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_greenthread_destroy filesystem operation.
 *
 * \param gt Argument representing the target resource.
 */
CFS_API void cfs_greenthread_destroy(cfs_greenthread_t *gt) {
  if (gt)
    cfs_free(gt);
}

/*
 * \brief Initializes a greenthread scheduler.
 *
 * \param out_sched Pointer to store the initialized scheduler.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_init(cfs_greenthread_scheduler **out_sched) {
  cfs_errc rc;
  if (!out_sched)
    return cfs_errc_invalid_argument;
  rc = cfs_malloc(sizeof(cfs_greenthread_scheduler), (void **)out_sched);
  if (rc != cfs_errc_success)
    return rc;
  (*out_sched)->current = NULL;
  (*out_sched)->head = NULL;
  (*out_sched)->tail = NULL;
  return cfs_errc_success;
}

/*
 * \brief Enqueues a spawned green thread onto a cooperative scheduler.
 *
 * \param sched Target scheduler.
 * \param gt Spawned green thread handle to enqueue.
 * \return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_greenthread_scheduler_schedule(
    cfs_greenthread_scheduler *sched, cfs_greenthread_t *gt) {
  if (!sched || !gt)
    return cfs_errc_invalid_argument;
  gt->next = NULL;
  if (sched->tail) {
    sched->tail->next = gt;
    sched->tail = gt;
  } else {
    sched->head = gt;
    sched->tail = gt;
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_greenthread_scheduler_run filesystem operation.
 *
 * \param sched Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_greenthread_scheduler_run(cfs_greenthread_scheduler *sched) {
  cfs_greenthread_scheduler *prev_sched;
  if (!sched)
    return cfs_errc_invalid_argument;
  prev_sched = g_cfs_current_gt_sched;
  g_cfs_current_gt_sched = sched;
  while (sched->head) {
    cfs_greenthread_t *gt = sched->head;
    sched->head = gt->next;
    if (!sched->head)
      sched->tail = NULL;
    gt->next = NULL;
    sched->current = gt;
    if (gt->func)
      gt->func(gt->arg);
    gt->completed = cfs_true;
    sched->current = NULL;
  }
  g_cfs_current_gt_sched = prev_sched;
  return cfs_errc_success;
}

/*
 * \brief Destroys a greenthread scheduler.
 *
 * \param sched Argument representing the target resource to destroy.
 */
CFS_API void
cfs_greenthread_scheduler_destroy(cfs_greenthread_scheduler *sched) {
  if (sched)
    cfs_free(sched);
}

/* Phase 5.7: Integration Implementations */

/*
 * \brief Performs the cfs_dir_itr_init_async filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param cb Callback function to execute upon completion of the asynchronous
 * request. \param user_data Opaque pointer passed back to the user-provided
 * callback. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init_async(cfs_runtime_t *rt,
                                                       const cfs_path *p,
                                                       cfs_callback_t cb,
                                                       void *user_data) {
  return cfs_submit_async_op(rt, cfs_opcode_dir_itr_init, p, NULL,
                             cfs_copy_options_none, 0,
                             sizeof(cfs_directory_iterator *), cb, user_data);
}

/*
 * \brief Performs the cfs_runtime_set_sandbox filesystem operation.
 *
 * \param rt Pointer to the active `cfs_runtime_t` execution context.
 * \param config Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_runtime_set_sandbox(cfs_runtime_t *rt, const cfs_sandbox_config *config) {
  cfs_errc rc;
  if (!rt || !config)
    return cfs_errc_invalid_argument;
  cfs_path_clear(&rt->sandbox_root);
  if (config->root_chroot.str && config->root_chroot.length > 0) {
    rc = cfs_weakly_canonical(&config->root_chroot, &rt->sandbox_root, NULL);
    if (rc != cfs_errc_success)
      return rc;
  }
  rt->sandbox_restrict_symlinks = config->restrict_symlinks;
  rt->sandbox_read_only = config->read_only;
  rt->has_sandbox = cfs_true;
  return cfs_errc_success;
}
/*
 * \brief Performs the cfs_status_known filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the status_known operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_status_known(cfs_file_status s,
                                                 cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type != cfs_file_type_none);
  return cfs_errc_success;
}

/*
 * \brief Converts raw OS `st_mode` bits into a normalized `cfs_file_status`.
 *
 * \param raw_mode Raw `st_mode` bitmask from `stat`/`lstat`/`_stat64`.
 * \param out Pointer to store the decoded `cfs_file_status`.
 * \return 0 on success, or `cfs_errc_invalid_argument` if `out` is NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_status_from_mode(unsigned int raw_mode,
                                                     cfs_file_status *out) {
  static const struct {
    unsigned int mask_val;
    cfs_file_type ftype;
  } k_mode_map[] = {
#if defined(CFS_OS_WINDOWS)
      {(unsigned int)_S_IFREG, cfs_file_type_regular},
      {(unsigned int)_S_IFDIR, cfs_file_type_directory},
      {(unsigned int)_S_IFCHR, cfs_file_type_character}
#ifdef _S_IFIFO
      ,
      {(unsigned int)_S_IFIFO, cfs_file_type_fifo}
#endif
#else
      {(unsigned int)S_IFREG, cfs_file_type_regular},
      {(unsigned int)S_IFDIR, cfs_file_type_directory},
      {(unsigned int)S_IFCHR, cfs_file_type_character}
#if defined(S_IFLNK) && (S_IFLNK != 0)
      ,
      {(unsigned int)S_IFLNK, cfs_file_type_symlink}
#endif
#if defined(S_IFBLK) && (S_IFBLK != 0)
      ,
      {(unsigned int)S_IFBLK, cfs_file_type_block}
#endif
#if defined(S_IFIFO) && (S_IFIFO != 0)
      ,
      {(unsigned int)S_IFIFO, cfs_file_type_fifo}
#endif
#if defined(S_IFSOCK) && (S_IFSOCK != 0)
      ,
      {(unsigned int)S_IFSOCK, cfs_file_type_socket}
#endif
#endif
  };
  cfs_size_t i;
  unsigned int fmt;
  if (!out)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  fmt = raw_mode & (unsigned int)_S_IFMT;
  out->permissions = (cfs_perms)(raw_mode & (unsigned int)cfs_perms_all);
#else
  fmt = raw_mode & (unsigned int)S_IFMT;
  out->permissions = (cfs_perms)(raw_mode & (unsigned int)cfs_perms_mask);
#endif
  out->type = cfs_file_type_unknown;
  for (i = 0; i < sizeof(k_mode_map) / sizeof(k_mode_map[0]); ++i) {
    if (k_mode_map[i].mask_val == fmt) {
      out->type = k_mode_map[i].ftype;
      break;
    }
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_hard_link_count filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the hard_link_count operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_hard_link_count(const cfs_path *p,
                                                    cfs_uintmax_t *out,
                                                    cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || p->length == 0 || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    HANDLE h;
    BY_HANDLE_FILE_INFORMATION info;
#if defined(CFS_UNICODE)
    h = CreateFileW(p->str, 0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#else
    h = CreateFileA(p->str, 0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#endif
    if (h != INVALID_HANDLE_VALUE) {
      if (GetFileInformationByHandle(h, &info)) {
        *out = (cfs_uintmax_t)info.nNumberOfLinks;
        CloseHandle(h);
        return cfs_errc_success;
      }
      {
        cfs_errc err = cfs_record_last_error(ec);
        CloseHandle(h);
        return err;
      }
    }
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status st;
    if (cfs_dos_stat_info(p->str, &st, NULL, NULL)) {
      *out = (cfs_uintmax_t)1;
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
  return cfs_record_last_error(ec);
}

/*
 * \brief Performs the cfs_permissions filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param prms Argument representing the target resource.
 * \param opts Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_permissions(const cfs_path *p,
                                                cfs_perms prms,
                                                cfs_perm_options opts,
                                                cfs_error_code *ec) {
  int action =
      (int)opts & ((int)cfs_perm_options_replace | (int)cfs_perm_options_add |
                   (int)cfs_perm_options_remove);
  int extra = (int)opts &
              ~((int)cfs_perm_options_replace | (int)cfs_perm_options_add |
                (int)cfs_perm_options_remove | (int)cfs_perm_options_nofollow);
  cfs_perms target_prms = prms & (cfs_perms)cfs_perms_mask;
  cfs_clear_error(ec);
  if (!p || !p->str || p->length == 0 || extra != 0 ||
      (action != (int)cfs_perm_options_replace &&
       action != (int)cfs_perm_options_add &&
       action != (int)cfs_perm_options_remove)) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  if (((int)opts & (int)cfs_perm_options_nofollow) != 0) {
    cfs_file_status sym_st;
    cfs_errc rc = cfs_symlink_status(p, &sym_st, ec);
    if (rc != cfs_errc_success)
      return rc;
    if (sym_st.type == cfs_file_type_symlink) {
      cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
      return cfs_errc_operation_not_supported;
    }
  }
  if (action != (int)cfs_perm_options_replace) {
    cfs_file_status cur_st;
    cfs_errc rc = cfs_status(p, &cur_st, ec);
    if (rc != cfs_errc_success)
      return rc;
    if (action == (int)cfs_perm_options_add) {
      target_prms = cur_st.permissions | target_prms;
    } else {
      target_prms = cur_st.permissions & ~target_prms;
    }
  }
#if defined(CFS_OS_WINDOWS)
  {
    int win_mode = ((target_prms & (cfs_perms)0222) != 0)
                       ? (_S_IREAD | _S_IWRITE)
                       : _S_IREAD;
#if defined(CFS_UNICODE)
    if (_wchmod(p->str, win_mode) == 0) {
#else
    if (_chmod(p->str, win_mode) == 0) {
#endif
      return cfs_errc_success;
    }
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status st;
    int dos_mode =
        ((target_prms & (cfs_perms)0222) != 0) ? (S_IREAD | S_IWRITE) : S_IREAD;
    if (!cfs_dos_stat_info(p->str, &st, NULL, NULL)) {
      return cfs_record_last_error(ec);
    }
    if (st.type == cfs_file_type_directory || _osmajor < 2) {
      return cfs_errc_success;
    }
    if (chmod(p->str, dos_mode) == 0) {
      return cfs_errc_success;
    }
  }
#else
  if (chmod(p->str, (mode_t)target_prms) == 0) {
    return cfs_errc_success;
  }
#endif
  return cfs_record_last_error(ec);
}

/*
 * \brief Performs the cfs_equivalent filesystem operation.
 *
 * \param p1 Specific input argument required for the operation.
 * \param p2 Specific input argument required for the operation.
 * \param out Pointer to store the result of the equivalent operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_equivalent(const cfs_path *p1,
                                               const cfs_path *p2,
                                               cfs_bool *out,
                                               cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p1 || !p2 || p1->length == 0 || p2->length == 0 || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    HANDLE h1;
    HANDLE h2;
    cfs_errc err;
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
    err = cfs_record_last_error(ec);
    if (h1 != INVALID_HANDLE_VALUE)
      CloseHandle(h1);
    if (h2 != INVALID_HANDLE_VALUE)
      CloseHandle(h2);
    return err;
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status s1;
    cfs_file_status s2;
    cfs_uintmax_t sz1 = 0;
    cfs_uintmax_t sz2 = 0;
    cfs_file_time_type mt1 = 0;
    cfs_file_time_type mt2 = 0;
    cfs_path c1;
    cfs_path c2;
    cfs_size_t i;
    cfs_bool match = cfs_true;
    cfs_errc rc;
    if (!cfs_dos_stat_info(p1->str, &s1, &sz1, &mt1))
      return cfs_record_last_error(ec);
    if (!cfs_dos_stat_info(p2->str, &s2, &sz2, &mt2))
      return cfs_record_last_error(ec);
    if (s1.type != s2.type || sz1 != sz2 || mt1 != mt2) {
      *out = cfs_false;
      return cfs_errc_success;
    }
    cfs_path_init(&c1);
    cfs_path_init(&c2);
    rc = cfs_canonical(p1, &c1, ec);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&c1);
      cfs_path_destroy(&c2);
      return rc;
    }
    rc = cfs_canonical(p2, &c2, ec);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&c1);
      cfs_path_destroy(&c2);
      return rc;
    }
    if (c1.length != c2.length) {
      match = cfs_false;
    } else {
      for (i = 0; i < c1.length; ++i) {
        char a = c1.str[i];
        char b = c2.str[i];
        if (a >= 'a' && a <= 'z')
          a = (char)(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z')
          b = (char)(b - 'a' + 'A');
        if (a == '/')
          a = '\\';
        if (b == '/')
          b = '\\';
        if (a != b) {
          match = cfs_false;
          break;
        }
      }
    }
    cfs_path_destroy(&c1);
    cfs_path_destroy(&c2);
    *out = match;
    return cfs_errc_success;
  }
#else
  {
    struct stat s1, s2;
    if (stat(p1->str, &s1) != 0 || stat(p2->str, &s2) != 0) {
      return cfs_record_last_error(ec);
    }
    *out = (s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino) ? cfs_true
                                                              : cfs_false;
    return cfs_errc_success;
  }
#endif
}

/*
 * \brief Performs the cfs_read_symlink filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the read_symlink operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_read_symlink(const cfs_path *p,
                                                 cfs_path *out,
                                                 cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !out || p->length == 0) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    /* Reparse buffer constants & layout (avoids <winioctl.h> / <ntifs.h>) */
    unsigned char raw_buf[16384];
    DWORD bytes_ret = 0;
    HANDLE h;
    unsigned long tag;
    const wchar_t *wtarget = NULL;
    cfs_size_t wlen = 0;
#if defined(CFS_UNICODE)
    h = CreateFileW(
        p->str, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, 0x00200000UL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
#else
    h = CreateFileA(
        p->str, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, 0x00200000UL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
#endif
    if (h == INVALID_HANDLE_VALUE)
      return cfs_record_last_error(ec);
    if (!DeviceIoControl(h, 0x000900A8UL, NULL, 0, raw_buf,
                         (DWORD)sizeof(raw_buf), &bytes_ret, NULL) ||
        bytes_ret < 16) {
      cfs_errc err = cfs_record_last_error(ec);
      CloseHandle(h);
      return err;
    }
    CloseHandle(h);
    CFS_MEMCPY_SAFE(&tag, sizeof(tag), raw_buf, 4);
    if (tag == 0xA000000CUL && bytes_ret >= 20) {
      /* IO_REPARSE_TAG_SYMLINK */
      unsigned short sub_off = 0, sub_len = 0, prt_off = 0, prt_len = 0;
      CFS_MEMCPY_SAFE(&sub_off, sizeof(sub_off), raw_buf + 8, 2);
      CFS_MEMCPY_SAFE(&sub_len, sizeof(sub_len), raw_buf + 10, 2);
      CFS_MEMCPY_SAFE(&prt_off, sizeof(prt_off), raw_buf + 12, 2);
      CFS_MEMCPY_SAFE(&prt_len, sizeof(prt_len), raw_buf + 14, 2);
      if (prt_len > 0 && (DWORD)(20 + prt_off + prt_len) <= bytes_ret) {
        wtarget = (const wchar_t *)(const void *)(raw_buf + 20 + prt_off);
        wlen = (cfs_size_t)(prt_len / sizeof(wchar_t));
      } else if ((DWORD)(20 + sub_off + sub_len) <= bytes_ret) {
        wtarget = (const wchar_t *)(const void *)(raw_buf + 20 + sub_off);
        wlen = (cfs_size_t)(sub_len / sizeof(wchar_t));
      }
    } else if (tag == 0xA0000003UL && bytes_ret >= 16) {
      /* IO_REPARSE_TAG_MOUNT_POINT */
      unsigned short sub_off = 0, sub_len = 0, prt_off = 0, prt_len = 0;
      CFS_MEMCPY_SAFE(&sub_off, sizeof(sub_off), raw_buf + 8, 2);
      CFS_MEMCPY_SAFE(&sub_len, sizeof(sub_len), raw_buf + 10, 2);
      CFS_MEMCPY_SAFE(&prt_off, sizeof(prt_off), raw_buf + 12, 2);
      CFS_MEMCPY_SAFE(&prt_len, sizeof(prt_len), raw_buf + 14, 2);
      if (prt_len > 0 && (DWORD)(16 + prt_off + prt_len) <= bytes_ret) {
        wtarget = (const wchar_t *)(const void *)(raw_buf + 16 + prt_off);
        wlen = (cfs_size_t)(prt_len / sizeof(wchar_t));
      } else if ((DWORD)(16 + sub_off + sub_len) <= bytes_ret) {
        wtarget = (const wchar_t *)(const void *)(raw_buf + 16 + sub_off);
        wlen = (cfs_size_t)(sub_len / sizeof(wchar_t));
      }
    }
    if (!wtarget) {
      cfs_set_error(ec, ERROR_INVALID_PARAMETER, cfs_errc_invalid_argument);
      return cfs_errc_invalid_argument;
    }
    if (wlen >= 4 && wtarget[0] == L'\\' && wtarget[1] == L'?' &&
        wtarget[2] == L'\\' && wtarget[3] != L'\0') {
      wtarget += 4;
      wlen -= 4;
    }
#if defined(CFS_UNICODE)
    return cfs_path_assign_slice(out, wtarget, wlen);
#else
    {
      wchar_t *wcopy = NULL;
      char *mb = NULL;
      cfs_size_t req_mb = 0;
      cfs_errc rc = cfs_malloc((wlen + 1) * sizeof(wchar_t), (void **)&wcopy);
      if (rc != cfs_errc_success)
        return rc;
      CFS_MEMCPY_SAFE(wcopy, (wlen + 1) * sizeof(wchar_t), wtarget,
                      wlen * sizeof(wchar_t));
      wcopy[wlen] = L'\0';
      rc = cfs_wide_to_mb(wcopy, NULL, 0, &req_mb);
      if (rc != cfs_errc_success || req_mb == 0) {
        cfs_free(wcopy);
        cfs_set_error(ec, 0, cfs_errc_illegal_byte_sequence);
        return cfs_errc_illegal_byte_sequence;
      }
      rc = cfs_malloc(req_mb, (void **)&mb);
      if (rc != cfs_errc_success) {
        cfs_free(wcopy);
        return rc;
      }
      rc = cfs_wide_to_mb(wcopy, mb, req_mb, NULL);
      cfs_free(wcopy);
      if (rc != cfs_errc_success) {
        cfs_free(mb);
        return rc;
      }
      rc = cfs_path_assign(out, mb);
      cfs_free(mb);
      return rc;
    }
#endif
  }
#elif defined(CFS_OS_DOS)
  {
    /* Minimal stub for DOS C89, symlinks require reparse points parsing */
    cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
    return cfs_errc_operation_not_supported;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    ssize_t len;
    len = readlink(p->str, buf, sizeof(buf) - 1);
    if (len != -1) {
      buf[len] = '\0';
      return cfs_path_assign(out, buf);
    }
    return cfs_record_last_error(ec);
  }
#endif
}

static void cfs_query_status(const cfs_path *p, cfs_file_status *out,
                             cfs_uintmax_t *out_size, cfs_bool *out_ok);

/*
 * \brief Performs the cfs_absolute filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the absolute operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_absolute(const cfs_path *p, cfs_path *out,
                                             cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
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
      return cfs_path_assign(out, buf);
    }
    return cfs_record_last_error(ec);
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_path cp;
    cfs_size_t rn_len = cfs_get_root_name_len(p);
    cfs_size_t rd_len = cfs_get_root_dir_len(p, rn_len);
    cfs_errc rc;
    if (rn_len > 0 && rd_len > 0) {
      rc = cfs_path_assign(out, p->str);
      if (rc == cfs_errc_success)
        cfs_ignore_errc(cfs_path_make_preferred(out));
      return rc;
    }
    cfs_path_init(&cp);
    rc = cfs_current_path(&cp, ec);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&cp);
      return rc;
    }
    if (rd_len > 0 && rn_len == 0) {
      cfs_size_t cp_rn = cfs_get_root_name_len(&cp);
      if (cp_rn > 0) {
        rc = cfs_path_assign_slice(out, cp.str, cp_rn);
        cfs_path_destroy(&cp);
        if (rc != cfs_errc_success)
          return rc;
        rc = cfs_path_concat(out, p->str);
        if (rc == cfs_errc_success)
          cfs_ignore_errc(cfs_path_make_preferred(out));
        return rc;
      }
    }
    rc = cfs_path_assign(out, cp.str);
    cfs_path_destroy(&cp);
    if (rc != cfs_errc_success)
      return rc;
    if (p->length > rn_len) {
      rc = cfs_path_append(out, p->str + rn_len);
    }
    if (rc == cfs_errc_success)
      cfs_ignore_errc(cfs_path_make_preferred(out));
    return rc;
  }
#else
  {
    cfs_path cp;
    cfs_errc rc;
    if (cfs_get_root_dir_len(p, cfs_get_root_name_len(p)) > 0) {
      return cfs_path_assign(out, p->str);
    }
    cfs_path_init(&cp);
    rc = cfs_current_path(&cp, ec);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&cp);
      return rc;
    }
    rc = cfs_path_assign(out, cp.str);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&cp);
      return rc;
    }
    rc = cfs_path_append(out, p->str);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&cp);
      return rc;
    }
    cfs_path_destroy(&cp);
    return cfs_errc_success;
  }
#endif
}

/*
 * \brief Performs the cfs_canonical filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the canonical operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_canonical(const cfs_path *p, cfs_path *out,
                                              cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    typedef DWORD(WINAPI * cfs_get_final_path_fn)(HANDLE, cfs_char_t *, DWORD,
                                                  DWORD);
    union {
      FARPROC proc;
      cfs_get_final_path_fn fn;
    } u;
    HMODULE k32;
    HANDLE h;
    cfs_char_t buf[CFS_MAX_PATH];
    DWORD len = 0;
    const cfs_char_t *clean;
    if (!p->str || p->length == 0) {
      cfs_set_error(ec, ERROR_PATH_NOT_FOUND,
                    cfs_errc_no_such_file_or_directory);
      return cfs_errc_no_such_file_or_directory;
    }
#if defined(CFS_UNICODE)
    h = CreateFileW(p->str, 0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#else
    h = CreateFileA(p->str, 0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#endif
    if (h == INVALID_HANDLE_VALUE)
      return cfs_record_last_error(ec);
    k32 = GetModuleHandleA("kernel32.dll");
    u.proc = NULL;
    if (k32) {
#if defined(CFS_UNICODE)
      u.proc = GetProcAddress(k32, "GetFinalPathNameByHandleW");
#else
      u.proc = GetProcAddress(k32, "GetFinalPathNameByHandleA");
#endif
    }
    if (u.fn) {
      len = u.fn(h, buf, CFS_MAX_PATH, 0);
    }
    CloseHandle(h);
    if (len > 0 && len < CFS_MAX_PATH) {
      clean = buf;
      if (len >= 6 && buf[0] == CFS_CHAR('\\') && buf[1] == CFS_CHAR('\\') &&
          buf[2] == CFS_CHAR('?') && buf[3] == CFS_CHAR('\\') &&
          buf[5] == CFS_CHAR(':')) {
        if (!(p->length >= 4 && p->str[0] == CFS_CHAR('\\') &&
              p->str[1] == CFS_CHAR('\\') && p->str[2] == CFS_CHAR('?'))) {
          clean = buf + 4;
        }
      }
      return cfs_path_assign(out, clean);
    }
#if defined(CFS_UNICODE)
    len = GetFullPathNameW(p->str, CFS_MAX_PATH, buf, NULL);
#else
    len = GetFullPathNameA(p->str, CFS_MAX_PATH, buf, NULL);
#endif
    if (len > 0 && len < CFS_MAX_PATH) {
      return cfs_path_assign(out, buf);
    }
    return cfs_record_last_error(ec);
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status st;
    cfs_path abs_p;
    cfs_errc rc;
    if (!p->str || p->length == 0 ||
        !cfs_dos_stat_info(p->str, &st, NULL, NULL)) {
      cfs_set_error(ec, ENOENT, cfs_errc_no_such_file_or_directory);
      return cfs_errc_no_such_file_or_directory;
    }
    cfs_path_init(&abs_p);
    rc = cfs_absolute(p, &abs_p, ec);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&abs_p);
      return rc;
    }
    rc = cfs_path_lexically_normal(&abs_p, out);
    cfs_path_destroy(&abs_p);
    return rc;
  }
#else
  {
    char buf[CFS_MAX_PATH];
    if (realpath(p->str, buf) != NULL) {
      return cfs_path_assign(out, buf);
    }
    return cfs_record_last_error(ec);
  }
#endif
}

/**
 * \brief Appends a bounded character slice as a path component onto `p`.
 *
 * \param p Destination path to modify.
 * \param src Source character slice.
 * \param len Number of characters in `src`.
 * \return 0 on success, or `cfs_errc_not_enough_memory` on allocation failure.
 */
static cfs_errc cfs_path_append_n(cfs_path *p, const cfs_char_t *src,
                                  cfs_size_t len) {
  cfs_bool need_sep = cfs_false;
  cfs_errc rc;
  if (p->length > 0) {
    cfs_bool last_sep = cfs_false;
    cfs_is_separator(p->str[p->length - 1], &last_sep);
    if (!last_sep)
      need_sep = cfs_true;
  }
  rc = cfs_path_reserve(p, p->length + (need_sep ? 1 : 0) + len + 1);
  if (rc != cfs_errc_success)
    return rc;
  if (need_sep)
    p->str[p->length++] = PATH_SEP_CHAR;
  CFS_STRNCPY_SAFE(p->str + p->length, p->capacity - p->length, src, len);
  p->length += len;
  p->str[p->length] = CFS_CHAR('\0');
  return cfs_errc_success;
}

/**
 * \brief Compares two `cfs_path_element` slices, treating single path
 * separators (`/` and `\`) as equal.
 *
 * \param a First path element.
 * \param b Second path element.
 * \return Negative if `a < b`, zero if `a == b`, positive if `a > b`.
 */
static int cfs_elem_cmp(cfs_path_element a, cfs_path_element b) {
  cfs_size_t min_len;
  cfs_size_t i;
  if (a.length == 1 && b.length == 1) {
    cfs_bool sa = cfs_false;
    cfs_bool sb = cfs_false;
    cfs_is_separator(a.str[0], &sa);
    cfs_is_separator(b.str[0], &sb);
    if (sa && sb)
      return 0;
  }
  min_len = (a.length < b.length) ? a.length : b.length;
  for (i = 0; i < min_len; ++i) {
    if (a.str[i] != b.str[i]) {
      return ((unsigned int)a.str[i] < (unsigned int)b.str[i]) ? -1 : 1;
    }
  }
  if (a.length < b.length)
    return -1;
  if (a.length > b.length)
    return 1;
  return 0;
}

/**
 * \brief Advances `it` to the next non-empty, non-`"."` element for lexical
 * relative calculation.
 *
 * \param it Iterator to advance.
 * \param out_elem Output element.
 * \param out_has Set to `cfs_true` if an element was found.
 */
static void cfs_next_rel_elem(cfs_path_iterator *it, cfs_path_element *out_elem,
                              cfs_bool *out_has) {
  for (;;) {
    cfs_ignore_errc(cfs_path_itr_next(it, out_elem, out_has));
    if (!*out_has)
      return;
    if (out_elem->length == 0)
      continue;
    if (out_elem->length == 1 && out_elem->str[0] == CFS_CHAR('.'))
      continue;
    return;
  }
}

/*
 * \brief Performs the cfs_weakly_canonical filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the weakly_canonical operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_weakly_canonical(const cfs_path *p,
                                                     cfs_path *out,
                                                     cfs_error_code *ec) {
  cfs_bool ex = cfs_false;
  cfs_path_iterator it;
  cfs_path_element elem;
  cfs_bool has_elem = cfs_false;
  cfs_path probe;
  cfs_path prev_probe;
  cfs_path resolved;
  cfs_bool still_exists = cfs_true;
  cfs_bool any_existed = cfs_false;
  cfs_errc rc;

  cfs_clear_error(ec);
  if (!p || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  cfs_path_init(out);
  if (!p->str || p->length == 0)
    return cfs_errc_success;

  cfs_ignore_errc(cfs_exists_path(p, &ex, NULL));
  if (ex)
    return cfs_canonical(p, out, ec);

  cfs_path_init(&probe);
  cfs_path_init(&prev_probe);
  cfs_path_init(&resolved);
  cfs_ignore_errc(cfs_path_itr_init(p, &it));

  for (;;) {
    cfs_ignore_errc(cfs_path_itr_next(&it, &elem, &has_elem));
    if (!has_elem)
      break;
    if (elem.length == 0)
      continue;
    if (still_exists) {
      rc = cfs_path_append_n(&probe, elem.str, elem.length);
      if (rc != cfs_errc_success) {
        cfs_path_destroy(&probe);
        cfs_path_destroy(&prev_probe);
        cfs_path_destroy(&resolved);
        return rc;
      }
      ex = cfs_false;
      cfs_ignore_errc(cfs_exists_path(&probe, &ex, NULL));
      if (ex) {
        any_existed = cfs_true;
        rc = cfs_path_assign(&prev_probe, probe.str);
        if (rc != cfs_errc_success) {
          cfs_path_destroy(&probe);
          cfs_path_destroy(&prev_probe);
          cfs_path_destroy(&resolved);
          return rc;
        }
      } else {
        still_exists = cfs_false;
        if (any_existed) {
          rc = cfs_canonical(&prev_probe, &resolved, ec);
        } else {
          rc = cfs_current_path(&resolved, ec);
        }
        if (rc != cfs_errc_success) {
          cfs_path_destroy(&probe);
          cfs_path_destroy(&prev_probe);
          cfs_path_destroy(&resolved);
          return rc;
        }
        rc = cfs_path_append_n(&resolved, elem.str, elem.length);
        if (rc != cfs_errc_success) {
          cfs_path_destroy(&probe);
          cfs_path_destroy(&prev_probe);
          cfs_path_destroy(&resolved);
          return rc;
        }
      }
    } else {
      rc = cfs_path_append_n(&resolved, elem.str, elem.length);
      if (rc != cfs_errc_success) {
        cfs_path_destroy(&probe);
        cfs_path_destroy(&prev_probe);
        cfs_path_destroy(&resolved);
        return rc;
      }
    }
  }

  cfs_path_destroy(&probe);
  cfs_path_destroy(&prev_probe);
  rc = cfs_path_lexically_normal(&resolved, out);
  cfs_path_destroy(&resolved);
  return rc;
}

/*
 * \brief Performs the cfs_copy_symlink filesystem operation.
 *
 * \param existing_symlink Argument representing the target resource.
 * \param new_symlink Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_copy_symlink(const cfs_path *existing_symlink, const cfs_path *new_symlink,
                 cfs_error_code *ec) {
  cfs_path out;
  cfs_errc res;
  cfs_path_init(&out);
  res = cfs_read_symlink(existing_symlink, &out, ec);
  if (res != cfs_errc_success)
    return res;
  res = cfs_create_symlink(&out, new_symlink, ec);
  if (res != cfs_errc_success) {
    cfs_path_destroy(&out);
    return res;
  }
  cfs_path_destroy(&out);
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_proximate filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the proximate operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_proximate(const cfs_path *p,
                                              const cfs_path *base,
                                              cfs_path *out,
                                              cfs_error_code *ec) {
  cfs_path wc_p;
  cfs_path wc_b;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!p || !base || !out || !p->str) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  cfs_path_init(&wc_p);
  cfs_path_init(&wc_b);
  rc = cfs_weakly_canonical(p, &wc_p, ec);
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&wc_p);
    return rc;
  }
  rc = cfs_weakly_canonical(base, &wc_b, ec);
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&wc_p);
    cfs_path_destroy(&wc_b);
    return rc;
  }
  rc = cfs_path_lexically_proximate(&wc_p, &wc_b, out);
  cfs_path_destroy(&wc_p);
  cfs_path_destroy(&wc_b);
  return rc;
}

/*
 * \brief Performs the cfs_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the relative operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_relative(const cfs_path *p,
                                             const cfs_path *base,
                                             cfs_path *out,
                                             cfs_error_code *ec) {
  cfs_path wc_p;
  cfs_path wc_b;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!p || !base || !out || !p->str) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  cfs_path_init(&wc_p);
  cfs_path_init(&wc_b);
  rc = cfs_weakly_canonical(p, &wc_p, ec);
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&wc_p);
    return rc;
  }
  rc = cfs_weakly_canonical(base, &wc_b, ec);
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&wc_p);
    cfs_path_destroy(&wc_b);
    return rc;
  }
  rc = cfs_path_lexically_relative(&wc_p, &wc_b, out);
  cfs_path_destroy(&wc_p);
  cfs_path_destroy(&wc_b);
  return rc;
}

/*
 * \brief Copies a single file from a source path to a destination path.
 *
 * \param from The source path pointing to the file to copy.
 * \param to The destination path for the copied file.
 * \param options Copy behavior flags mapping to `cfs_copy_options`.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_copy_file(const cfs_path *from,
                                              const cfs_path *to,
                                              cfs_copy_options options,
                                              cfs_error_code *ec) {
  cfs_file_status from_st;
  cfs_file_status to_st;
  cfs_bool from_exists = cfs_false;
  cfs_bool to_exists = cfs_false;
  cfs_clear_error(ec);
  if (!from || !to || !from->str || !to->str) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  cfs_query_status(from, &from_st, NULL, &from_exists);
  if (from_exists && from_st.type != cfs_file_type_regular) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  cfs_query_status(to, &to_st, NULL, &to_exists);
  if (from_exists && to_exists) {
    cfs_bool same_file = cfs_false;
    if (to_st.type != cfs_file_type_regular) {
      cfs_set_error(ec, 0, cfs_errc_is_a_directory);
      return cfs_errc_is_a_directory;
    }
    cfs_ignore_errc(cfs_equivalent(from, to, &same_file, NULL));
    if (same_file) {
      cfs_set_error(ec, 17, cfs_errc_file_exists);
      return cfs_errc_file_exists;
    }
    if (options & cfs_copy_options_skip_existing)
      return cfs_errc_success;
    if (options & cfs_copy_options_update_existing) {
      cfs_file_time_type from_t = 0;
      cfs_file_time_type to_t = 0;
      cfs_ignore_errc(cfs_last_write_time(from, &from_t, NULL));
      cfs_ignore_errc(cfs_last_write_time(to, &to_t, NULL));
      if (from_t <= to_t) {
        return cfs_errc_success;
      }
    } else if (!(options & cfs_copy_options_overwrite_existing)) {
      cfs_set_error(ec, 17, cfs_errc_file_exists);
      return cfs_errc_file_exists;
    }
  }
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (CopyFileW(from->str, to->str, FALSE))
#else
  if (CopyFileA(from->str, to->str, FALSE))
#endif
    return cfs_errc_success;
  return cfs_record_last_error(ec);
#else
  {
    FILE *in_f;
    FILE *out_f;
#if defined(CFS_OS_DOS)
    char buf[512];
#else
    char buf[4096];
#endif
    size_t n;
    in_f = fopen(from->str, "rb");
    if (!in_f) {
      return cfs_record_last_error(ec);
    }
    out_f = fopen(to->str, "wb");
    if (!out_f) {
      fclose(in_f);
      return cfs_record_last_error(ec);
    }
    while ((n = fread(buf, 1, sizeof(buf), in_f)) > 0) {
      size_t written = fwrite(buf, 1, n, out_f);
      (void)written;
    }
    fclose(in_f);
    fclose(out_f);
#if defined(CFS_OS_DOS)
    if (_osmajor >= 2) {
      int dos_mode = ((from_st.permissions & (cfs_perms)0222) != 0)
                         ? (S_IREAD | S_IWRITE)
                         : S_IREAD;
      (void)chmod(to->str, dos_mode);
    }
#else
    (void)chmod(to->str, (mode_t)from_st.permissions);
#endif
    return cfs_errc_success;
  }
#endif
}

/*
 * \brief Performs the cfs_copy filesystem operation.
 *
 * \param from Argument representing the target resource.
 * \param to Argument representing the target resource.
 * \param options Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_copy(const cfs_path *from,
                                         const cfs_path *to,
                                         cfs_copy_options options,
                                         cfs_error_code *ec) {
  cfs_file_status from_st;
  cfs_file_status to_st;
  cfs_bool to_exists = cfs_false;
  cfs_errc rc;

  cfs_clear_error(ec);
  if (!from || !to || !from->str || !to->str) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }

  if (((int)options & ((int)cfs_copy_options_create_symlinks |
                       (int)cfs_copy_options_skip_symlinks |
                       (int)cfs_copy_options_copy_symlinks)) != 0) {
    rc = cfs_symlink_status(from, &from_st, ec);
  } else {
    rc = cfs_status(from, &from_st, ec);
  }
  if (rc != cfs_errc_success)
    return rc;

  cfs_query_status(to, &to_st, NULL, &to_exists);
  if (to_exists) {
    cfs_bool same = cfs_false;
    cfs_ignore_errc(cfs_equivalent(from, to, &same, NULL));
    if (same) {
      cfs_set_error(ec, 17, cfs_errc_file_exists);
      return cfs_errc_file_exists;
    }
  }

  if (from_st.type == cfs_file_type_symlink) {
    if (((int)options & (int)cfs_copy_options_skip_symlinks) != 0)
      return cfs_errc_success;
    if (!to_exists &&
        (((int)options & (int)cfs_copy_options_copy_symlinks) != 0)) {
      return cfs_copy_symlink(from, to, ec);
    }
    cfs_set_error(ec, 17, cfs_errc_file_exists);
    return cfs_errc_file_exists;
  }

  if (from_st.type == cfs_file_type_regular) {
    cfs_path target_to;
    if (((int)options & (int)cfs_copy_options_directories_only) != 0)
      return cfs_errc_success;
    cfs_path_init(&target_to);
    if (to_exists && to_st.type == cfs_file_type_directory) {
      cfs_path fn;
      rc = cfs_path_filename(from, &fn);
      if (rc != cfs_errc_success)
        return rc;
      rc = cfs_path_clone(&target_to, to);
      if (rc != cfs_errc_success) {
        cfs_path_destroy(&fn);
        return rc;
      }
      rc = cfs_path_append(&target_to, fn.str);
      cfs_path_destroy(&fn);
      if (rc != cfs_errc_success) {
        cfs_path_destroy(&target_to);
        return rc;
      }
    } else {
      rc = cfs_path_clone(&target_to, to);
      if (rc != cfs_errc_success)
        return rc;
    }
    if (((int)options & (int)cfs_copy_options_create_symlinks) != 0) {
      rc = cfs_create_symlink(from, &target_to, ec);
    } else if (((int)options & (int)cfs_copy_options_create_hard_links) != 0) {
      rc = cfs_create_hard_link(from, &target_to, ec);
    } else {
      rc = cfs_copy_file(from, &target_to, options, ec);
    }
    cfs_path_destroy(&target_to);
    return rc;
  }

  if (from_st.type == cfs_file_type_directory) {
    if (((int)options & (int)cfs_copy_options_create_symlinks) != 0) {
      cfs_set_error(ec, 0, cfs_errc_is_a_directory);
      return cfs_errc_is_a_directory;
    }
    if (!to_exists) {
      rc = cfs_create_directory_attr(to, from, ec);
      if (rc != cfs_errc_success)
        return rc;
    } else if (to_st.type != cfs_file_type_directory) {
      cfs_set_error(ec, 17, cfs_errc_file_exists);
      return cfs_errc_file_exists;
    }
    if (((int)options & (int)cfs_copy_options_directories_only) == 0) {
      cfs_directory_iterator *it = NULL;
      const cfs_directory_entry *en = NULL;
      rc = cfs_dir_itr_init(from, &it, ec);
      if (rc != cfs_errc_success)
        return rc;
      for (;;) {
        const cfs_char_t *leaf;
        cfs_path child_to;
        cfs_bool is_sep = cfs_false;
        rc = cfs_dir_itr_next(it, &en, ec);
        if (rc != cfs_errc_success) {
          cfs_dir_itr_close(it);
          return rc;
        }
        if (!en)
          break;
        leaf = en->path.str + it->base_dir.length;
        for (;;) {
          cfs_is_separator(*leaf, &is_sep);
          if (!is_sep)
            break;
          leaf++;
        }
        rc = cfs_path_clone(&child_to, to);
        if (rc != cfs_errc_success) {
          cfs_dir_itr_close(it);
          return rc;
        }
        rc = cfs_path_append(&child_to, leaf);
        if (rc != cfs_errc_success) {
          cfs_path_destroy(&child_to);
          cfs_dir_itr_close(it);
          return rc;
        }
        if (((int)options & (int)cfs_copy_options_recursive) != 0) {
          rc = cfs_copy(&en->path, &child_to, options, ec);
        } else if (en->status.type == cfs_file_type_regular) {
          rc = cfs_copy_file(&en->path, &child_to, options, ec);
        }
        cfs_path_destroy(&child_to);
        if (rc != cfs_errc_success) {
          cfs_dir_itr_close(it);
          return rc;
        }
      }
      cfs_dir_itr_close(it);
    }
    return cfs_errc_success;
  }

  cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  return cfs_errc_operation_not_supported;
}

#if defined(CFS_OS_WINDOWS)
/**
 * \brief Dynamically invokes Win32 `CreateSymbolicLinkW`/`A` with unprivileged
 * fallback.
 *
 * \param target Symlink target path.
 * \param link Symlink destination path.
 * \param base_flags `0` for file symlink or `0x1` for directory symlink.
 * \param ec Optional output error code.
 * \return 0 on success, or mapped `cfs_errc` on failure.
 */
static cfs_errc cfs_win_create_symlink(const cfs_path *target,
                                       const cfs_path *link, DWORD base_flags,
                                       cfs_error_code *ec) {
  typedef BOOLEAN(WINAPI * cfs_create_sym_fn)(const cfs_char_t *,
                                              const cfs_char_t *, DWORD);
  union {
    FARPROC proc;
    cfs_create_sym_fn fn;
  } u;
  HMODULE k32;
  cfs_clear_error(ec);
  if (!target || !link || !target->str || !link->str) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  k32 = GetModuleHandleA("kernel32.dll");
  u.proc = NULL;
  if (k32) {
#if defined(CFS_UNICODE)
    u.proc = GetProcAddress(k32, "CreateSymbolicLinkW");
#else
    u.proc = GetProcAddress(k32, "CreateSymbolicLinkA");
#endif
  }
  if (!u.fn) {
    cfs_set_error(ec, ERROR_CALL_NOT_IMPLEMENTED,
                  cfs_errc_operation_not_supported);
    return cfs_errc_operation_not_supported;
  }
  if (u.fn(link->str, target->str, base_flags | 0x2UL))
    return cfs_errc_success;
  if (GetLastError() == ERROR_INVALID_PARAMETER &&
      u.fn(link->str, target->str, base_flags)) {
    return cfs_errc_success;
  }
  return cfs_record_last_error(ec);
}
#endif

/*
 * \brief Performs the cfs_create_symlink filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_symlink(const cfs_path *target,
                                                   const cfs_path *link,
                                                   cfs_error_code *ec) {
  cfs_clear_error(ec);
#if defined(CFS_OS_WINDOWS)
  return cfs_win_create_symlink(target, link, 0, ec);
#elif defined(CFS_OS_DOS)
  (void)target;
  (void)link;
  cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  return cfs_errc_operation_not_supported;
#else
  if (symlink(target->str, link->str) != 0) {
    return cfs_record_last_error(ec);
  }
  return cfs_errc_success;
#endif
}

/*
 * \brief Performs the cfs_path_lexically_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the path_lexically_relative
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_lexically_relative(
    const cfs_path *p, const cfs_path *base, cfs_path *out) {
  cfs_size_t rn_p;
  cfs_size_t rn_b;
  cfs_size_t rd_p;
  cfs_size_t rd_b;
  cfs_path_iterator it_p;
  cfs_path_iterator it_b;
  cfs_path_element el_p;
  cfs_path_element el_b;
  cfs_bool has_p = cfs_false;
  cfs_bool has_b = cfs_false;
  long up_count = 0;
  long k;
  cfs_errc rc;

  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p || !p->str)
    return cfs_errc_invalid_argument;
  if (!base || !base->str || base->length == 0) {
    return cfs_path_assign(out, p->str);
  }

  rn_p = cfs_get_root_name_len(p);
  rn_b = cfs_get_root_name_len(base);
  rd_p = cfs_get_root_dir_len(p, rn_p);
  rd_b = cfs_get_root_dir_len(base, rn_b);
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (rn_p != rn_b ||
      (rn_p > 0 && CFS_RAW_STRNCMP(p->str, base->str, rn_p) != 0) ||
      ((rd_p > 0) != (rd_b > 0))) {
    return cfs_errc_success;
  }
#else
  if ((rd_p > 0) != (rd_b > 0)) {
    return cfs_errc_success;
  }
#endif

  cfs_ignore_errc(cfs_path_itr_init(p, &it_p));
  cfs_ignore_errc(cfs_path_itr_init(base, &it_b));
  for (;;) {
    cfs_next_rel_elem(&it_p, &el_p, &has_p);
    cfs_next_rel_elem(&it_b, &el_b, &has_b);
    if (!has_p || !has_b || cfs_elem_cmp(el_p, el_b) != 0)
      break;
  }

  while (has_b) {
    if (el_b.length == 2 && el_b.str[0] == CFS_CHAR('.') &&
        el_b.str[1] == CFS_CHAR('.')) {
      up_count--;
      if (up_count < 0)
        return cfs_errc_success;
    } else {
      up_count++;
    }
    cfs_next_rel_elem(&it_b, &el_b, &has_b);
  }

  if (up_count == 0 && !has_p) {
    return cfs_path_assign(out, CFS_STR("."));
  }

  for (k = 0; k < up_count; ++k) {
    rc = cfs_path_append(out, CFS_STR(".."));
    if (rc != cfs_errc_success)
      return rc;
  }

  while (has_p) {
    rc = cfs_path_append_n(out, el_p.str, el_p.length);
    if (rc != cfs_errc_success)
      return rc;
    cfs_next_rel_elem(&it_p, &el_p, &has_p);
  }

  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_lexically_proximate filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param base Argument representing the target resource.
 * \param out Pointer to store the result of the path_lexically_proximate
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_lexically_proximate(
    const cfs_path *p, const cfs_path *base, cfs_path *out) {
  cfs_errc rc = cfs_path_lexically_relative(p, base, out);
  if (rc != cfs_errc_success)
    return rc;
  if (out->length == 0 && p->length > 0) {
    return cfs_path_assign(out, p->str);
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_is_absolute filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_absolute operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_is_absolute(const cfs_path *p,
                                                     cfs_bool *out) {
  cfs_size_t root_name_len;
  cfs_size_t root_dir_len;

  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
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

/*
 * \brief Performs the cfs_current_path filesystem operation.
 *
 * \param out Pointer to store the result of the current_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_current_path(cfs_path *out,
                                                 cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
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
      return cfs_path_assign(out, buf);
    }
    return cfs_record_last_error(ec);
  }
#elif defined(CFS_OS_DOS)
  {
    char buf[CFS_MAX_PATH];
#if defined(CFS_ENABLE_FAULT_INJECTION)
    if (g_cfs_getcwd_fail) {
      return cfs_record_last_error(ec);
    }
#endif
    if (_osmajor < 2) {
      unsigned int drv = 3;
      _dos_getdrive(&drv);
      if (drv < 1 || drv > 26)
        drv = 3;
      buf[0] = (char)('A' + (int)drv - 1);
      buf[1] = ':';
      buf[2] = '\\';
      buf[3] = '\0';
      return cfs_path_assign(out, buf);
    }
    if (getcwd(buf, sizeof(buf)) != NULL) {
      return cfs_path_assign(out, buf);
    }
    return cfs_record_last_error(ec);
  }
#else
  {
    char buf[CFS_MAX_PATH];
    char *cwd_res = getcwd(buf, sizeof(buf));
    if (cwd_res != NULL) {
      return cfs_path_assign(out, buf);
    }
    return cfs_record_last_error(ec);
  }
#endif
}

/*
 * \brief Performs the cfs_current_path_set filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_current_path_set(const cfs_path *p,
                                                     cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !p->str) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (SetCurrentDirectoryW(p->str) == 0) {
#else
  if (SetCurrentDirectoryA(p->str) == 0) {
#endif
    return cfs_record_last_error(ec);
  }
#elif defined(CFS_OS_DOS)
  if (p->length == 0) {
    cfs_set_error(ec, ENOENT, cfs_errc_no_such_file_or_directory);
    return cfs_errc_no_such_file_or_directory;
  }
  if (_osmajor < 2) {
    cfs_file_status st;
    if (!cfs_dos_stat_info(p->str, &st, NULL, NULL) ||
        st.type != cfs_file_type_directory) {
      cfs_set_error(ec, ENOENT, cfs_errc_no_such_file_or_directory);
      return cfs_errc_no_such_file_or_directory;
    }
    if (p->length >= 2 && p->str[1] == ':') {
      unsigned int total_drvs = 0;
      char dc = p->str[0];
      unsigned int drv = 0;
      if (dc >= 'a' && dc <= 'z')
        drv = (unsigned int)(dc - 'a' + 1);
      else if (dc >= 'A' && dc <= 'Z')
        drv = (unsigned int)(dc - 'A' + 1);
      if (drv >= 1 && drv <= 26)
        _dos_setdrive(drv, &total_drvs);
    }
    return cfs_errc_success;
  }
  if (strcmp(p->str, ".") == 0) {
    return cfs_errc_success;
  }
  if (p->length >= 2 && p->str[1] == ':') {
    unsigned int total_drvs = 0;
    char dc = p->str[0];
    unsigned int drv = 0;
    if (dc >= 'a' && dc <= 'z')
      drv = (unsigned int)(dc - 'a' + 1);
    else if (dc >= 'A' && dc <= 'Z')
      drv = (unsigned int)(dc - 'A' + 1);
    if (drv >= 1 && drv <= 26)
      _dos_setdrive(drv, &total_drvs);
  }
  if (chdir(p->str) != 0) {
    return cfs_record_last_error(ec);
  }
  return cfs_errc_success;
#else
  if (chdir(p->str) != 0) {
    return cfs_record_last_error(ec);
  }
  return cfs_errc_success;
#endif
#if defined(CFS_OS_WINDOWS)
  return cfs_errc_success;
#endif
}

/*
 * \brief Performs the cfs_path_root_name filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_name operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_root_name(const cfs_path *p,
                                                   cfs_path *out) {
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  cfs_size_t len;
#endif
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  len = cfs_get_root_name_len(p);
  if (len == 0)
    return cfs_errc_success;
  return cfs_path_assign_slice(out, p->str, len);
#else
  return cfs_errc_success;
#endif
}

/*
 * \brief Performs the cfs_path_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_root_directory(const cfs_path *p,
                                                        cfs_path *out) {
  cfs_size_t name_len;
  cfs_size_t dir_len;
  cfs_char_t buf[2];
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p)
    return cfs_errc_invalid_argument;
  name_len = cfs_get_root_name_len(p);
  dir_len = cfs_get_root_dir_len(p, name_len);
  if (dir_len > 0) {
    buf[0] = p->str[name_len];
    buf[1] = CFS_CHAR('\0');
    return cfs_path_assign(out, buf);
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_root_path(const cfs_path *p,
                                                   cfs_path *out) {
  cfs_size_t name_len;
  cfs_size_t dir_len;
  cfs_size_t total_len;
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p)
    return cfs_errc_invalid_argument;
  name_len = cfs_get_root_name_len(p);
  dir_len = cfs_get_root_dir_len(p, name_len);
  total_len = name_len + dir_len;
  if (total_len == 0)
    return cfs_errc_success;
  return cfs_path_assign_slice(out, p->str, total_len);
}

/*
 * \brief Performs the cfs_path_relative_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_relative_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_relative_path(const cfs_path *p,
                                                       cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;
  if (p->length <= root_len)
    return cfs_errc_success;
  return cfs_path_assign_slice(out, p->str + root_len, p->length - root_len);
}

/*
 * \brief Performs the cfs_path_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_parent_path(const cfs_path *p,
                                                     cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t out_len;
  cfs_bool is_sep = cfs_false;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  rc = cfs_path_clone(out, p);
  if (rc != cfs_errc_success)
    return rc;
  cfs_path_remove_filename(out);
  root_name_len = cfs_get_root_name_len(out);
  root_dir_len = cfs_get_root_dir_len(out, root_name_len);
  root_len = root_name_len + root_dir_len;
  out_len = out->length;
  while (out_len > root_len) {
    cfs_is_separator(out->str[out_len - 1], &is_sep);
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

/*
 * \brief Performs the cfs_path_replace_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param replacement Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_path_replace_filename(cfs_path *p, const cfs_char_t *replacement) {
  if (!p)
    return cfs_errc_invalid_argument;
  cfs_path_remove_filename(p);
  if (replacement) {
    return cfs_path_append(p, replacement);
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_replace_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param replacement Argument representing the target resource.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_path_replace_extension(cfs_path *p, const cfs_char_t *replacement) {
  cfs_path ext;
  cfs_errc rc;
  if (!p)
    return cfs_errc_invalid_argument;
  cfs_path_init(&ext);
  rc = cfs_path_extension(p, &ext);
  if (rc != cfs_errc_success)
    return rc;
  p->length -= ext.length;
  if (p->str) {
    p->str[p->length] = CFS_CHAR('\0');
  }
  cfs_path_destroy(&ext);
  if (replacement && replacement[0] != CFS_CHAR('\0')) {
    if (replacement[0] != CFS_CHAR('.')) {
      rc = cfs_path_concat(p, CFS_STR("."));
      if (rc != cfs_errc_success)
        return rc;
    }
    return cfs_path_concat(p, replacement);
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_remove_filename filesystem operation.
 *
 * \param p Argument representing the target resource.
 */
CFS_API void cfs_path_remove_filename(cfs_path *p) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t i, start_idx;
  cfs_bool is_sep = cfs_false;
  if (!p || !p->str || p->length == 0)
    return;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;
  if (p->length == root_len)
    return;
  cfs_is_separator(p->str[p->length - 1], &is_sep);
  if (is_sep)
    return;
  start_idx = root_len;
  for (i = p->length; i > root_len; i--) {
    cfs_is_separator(p->str[i - 1], &is_sep);
    if (is_sep) {
      start_idx = i;
      break;
    }
  }
  p->length = start_idx;
  p->str[p->length] = CFS_CHAR('\0');
}

/*
 * \brief Performs the cfs_path_has_root_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_root_path(const cfs_path *p,
                                                       cfs_bool *out) {
  cfs_size_t root_name_len;
  cfs_size_t root_dir_len;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  if ((root_name_len + root_dir_len) > 0)
    *out = cfs_true;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_has_root_name filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_name operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_root_name(const cfs_path *p,
                                                       cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (cfs_get_root_name_len(p) > 0)
    *out = cfs_true;
#endif
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_has_root_directory filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_root_directory
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_root_directory(const cfs_path *p,
                                                            cfs_bool *out) {
  cfs_size_t root_name_len;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  if (cfs_get_root_dir_len(p, root_name_len) > 0)
    *out = cfs_true;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_has_relative_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_relative_path
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_relative_path(const cfs_path *p,
                                                           cfs_bool *out) {
  cfs_size_t root_name_len, root_dir_len;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  if (p->length > (root_name_len + root_dir_len))
    *out = cfs_true;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_has_parent_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_parent_path operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_parent_path(const cfs_path *p,
                                                         cfs_bool *out) {
  cfs_path parent;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  rc = cfs_path_parent_path(p, &parent);
  if (rc != cfs_errc_success)
    return rc;
  if (parent.length > 0)
    *out = cfs_true;
  cfs_path_destroy(&parent);
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_has_filename filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_filename operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_filename(const cfs_path *p,
                                                      cfs_bool *out) {
  cfs_path fn;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  rc = cfs_path_filename(p, &fn);
  if (rc != cfs_errc_success)
    return rc;
  if (fn.length > 0)
    *out = cfs_true;
  cfs_path_destroy(&fn);
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_has_stem filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_stem operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_stem(const cfs_path *p,
                                                  cfs_bool *out) {
  cfs_path stem;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  rc = cfs_path_stem(p, &stem);
  if (rc != cfs_errc_success)
    return rc;
  if (stem.length > 0)
    *out = cfs_true;
  cfs_path_destroy(&stem);
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_has_extension filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_has_extension operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_has_extension(const cfs_path *p,
                                                       cfs_bool *out) {
  cfs_path ext;
  cfs_errc rc;
  if (!out)
    return cfs_errc_invalid_argument;
  *out = cfs_false;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;
  rc = cfs_path_extension(p, &ext);
  if (rc != cfs_errc_success)
    return rc;
  if (ext.length > 0)
    *out = cfs_true;
  cfs_path_destroy(&ext);
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_is_relative filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_is_relative operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_is_relative(const cfs_path *p,
                                                     cfs_bool *out) {
  cfs_errc rc;
  if (!p)
    return cfs_errc_invalid_argument;
  rc = cfs_path_is_absolute(p, out);
  if (rc != cfs_errc_success)
    return rc;
  *out = *out ? cfs_false : cfs_true;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_path_compare filesystem operation.
 *
 * \param lhs Argument representing the target resource.
 * \param rhs Argument representing the target resource.
 * \param out Pointer to store the lexicographical comparison result.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_compare(const cfs_path *lhs,
                                                 const cfs_path *rhs,
                                                 int *out) {
  cfs_path_iterator it_l;
  cfs_path_iterator it_r;
  cfs_path_element el_l;
  cfs_path_element el_r;
  cfs_bool has_l = cfs_false;
  cfs_bool has_r = cfs_false;
  if (!lhs || !rhs || !out)
    return cfs_errc_invalid_argument;
  cfs_ignore_errc(cfs_path_itr_init(lhs, &it_l));
  cfs_ignore_errc(cfs_path_itr_init(rhs, &it_r));
  for (;;) {
    int cmp;
    cfs_ignore_errc(cfs_path_itr_next(&it_l, &el_l, &has_l));
    cfs_ignore_errc(cfs_path_itr_next(&it_r, &el_r, &has_r));
    if (!has_l && !has_r) {
      *out = 0;
      return cfs_errc_success;
    }
    if (!has_l) {
      *out = -1;
      return cfs_errc_success;
    }
    if (!has_r) {
      *out = 1;
      return cfs_errc_success;
    }
    cmp = cfs_elem_cmp(el_l, el_r);
    if (cmp != 0) {
      *out = cmp;
      return cfs_errc_success;
    }
  }
}

/*
 * \brief Performs the cfs_path_lexically_normal filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the path_lexically_normal
 * operation. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_path_lexically_normal(const cfs_path *p,
                                                          cfs_path *out) {
  cfs_size_t root_name_len, root_dir_len, root_len;
  cfs_size_t i, start, end;
  cfs_bool is_sep = cfs_false;
  cfs_size_t comps_len = 0;
  cfs_errc rc;
  struct cfs_path_component {
    cfs_size_t s, e;
  } *comps = NULL;

  if (!out)
    return cfs_errc_invalid_argument;
  cfs_path_init(out);
  if (!p || !p->str || p->length == 0)
    return cfs_errc_success;

  rc = cfs_calloc((p->length / 2) + 2, sizeof(struct cfs_path_component),
                  (void **)&comps);
  if (rc != cfs_errc_success)
    return rc;

  root_name_len = cfs_get_root_name_len(p);
  root_dir_len = cfs_get_root_dir_len(p, root_name_len);
  root_len = root_name_len + root_dir_len;

  rc = cfs_path_reserve(out, p->length + 2);
  if (rc != cfs_errc_success) {
    cfs_free(comps);
    return rc;
  }
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
  if (root_name_len > 0) {
    CFS_STRNCPY_SAFE(out->str, out->capacity, p->str, root_name_len);
    out->length = root_name_len;
    out->str[out->length] = CFS_CHAR('\0');
  }
#endif
  if (root_dir_len > 0) {
    out->str[out->length++] = PATH_SEP_CHAR;
    out->str[out->length] = CFS_CHAR('\0');
  }

  start = root_len;
  while (start < p->length) {
    cfs_is_separator(p->str[start], &is_sep);
    if (is_sep) {
      start++;
      continue;
    }
    end = start;
    while (end < p->length) {
      cfs_is_separator(p->str[end], &is_sep);
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
          comps[comps_len].s = start;
          comps[comps_len].e = end;
          comps_len++;
        }
      } else if (root_dir_len == 0) {
        comps[comps_len].s = start;
        comps[comps_len].e = end;
        comps_len++;
      }
    } else {
      comps[comps_len].s = start;
      comps[comps_len].e = end;
      comps_len++;
    }
    start = end;
  }

  for (i = 0; i < comps_len; i++) {
    cfs_size_t len = comps[i].e - comps[i].s;
#if defined(CFS_OS_WINDOWS) || defined(CFS_OS_DOS)
    if (i > 0 || (root_name_len > 0 && root_dir_len == 0)) {
#else
    if (i > 0) {
#endif
      out->str[out->length++] = PATH_SEP_CHAR;
      out->str[out->length] = CFS_CHAR('\0');
    }
    CFS_STRNCPY_SAFE(out->str + out->length, out->capacity - out->length,
                     p->str + comps[i].s, len);
    out->length += len;
    out->str[out->length] = CFS_CHAR('\0');
  }
  cfs_free(comps);

  if (out->length == 0) {
    out->str[0] = CFS_CHAR('.');
    out->str[1] = CFS_CHAR('\0');
    out->length = 1;
  }

  return cfs_errc_success;
}

#if defined(CFS_OS_DOS)
/**
 * @brief Queries status, byte size, and modification time on
 * MS-DOS 1.25/2.0/4.0.
 *
 * @param path Narrow DOS path string to inspect.
 * @param out_status Optional status structure to populate.
 * @param out_size Optional file size output pointer.
 * @param out_mtime Optional modification timestamp output pointer.
 * @return `cfs_true` if the node exists on disk, otherwise `cfs_false`.
 */
static cfs_bool cfs_dos_stat_info(const char *path, cfs_file_status *out_status,
                                  cfs_uintmax_t *out_size,
                                  cfs_file_time_type *out_mtime) {
  char clean[CFS_MAX_PATH];
  cfs_size_t len;
  if (out_status) {
    out_status->type = cfs_file_type_not_found;
    out_status->permissions = (cfs_perms)0;
  }
  if (out_size)
    *out_size = 0;
  if (out_mtime)
    *out_mtime = 0;
  if (!path || path[0] == '\0') {
    errno = ENOENT;
    return cfs_false;
  }
  len = strlen(path);
  if (len >= CFS_MAX_PATH) {
    errno = ENAMETOOLONG;
    return cfs_false;
  }
  CFS_STRCPY_SAFE(clean, sizeof(clean), path);
  while (len > 1 && (clean[len - 1] == '\\' || clean[len - 1] == '/')) {
    if (len == 3 && clean[1] == ':')
      break;
    clean[--len] = '\0';
  }
  if (_osmajor >= 2) {
    struct stat st;
    if ((len == 1 &&
         (clean[0] == '\\' || clean[0] == '/' || clean[0] == '.')) ||
        (len == 2 && clean[0] == '.' && clean[1] == '.') ||
        (len == 2 && clean[1] == ':') ||
        (len == 3 && clean[1] == ':' &&
         (clean[2] == '\\' || clean[2] == '/' || clean[2] == '.'))) {
      if (out_status) {
        out_status->type = cfs_file_type_directory;
        out_status->permissions = cfs_perms_all;
      }
      return cfs_true;
    }
    if (stat(clean, &st) == 0) {
      if (out_status)
        cfs_ignore_errc(
            cfs_status_from_mode((unsigned int)st.st_mode, out_status));
      if (out_size)
        *out_size = (cfs_uintmax_t)st.st_size;
      if (out_mtime)
        *out_mtime = (cfs_file_time_type)st.st_mtime;
      return cfs_true;
    }
    return cfs_false;
  }
  /* MS-DOS 1.25 (_osmajor < 2): flat root directory, FCB-backed handles */
  {
    const char *base = clean;
    unsigned int attr = 0;
    if (len >= 2 && clean[1] == ':')
      base = clean + 2;
    while (*base == '\\' || *base == '/')
      ++base;
    if (base[0] == '.' && (base[1] == '\\' || base[1] == '/')) {
      base += 2;
      while (*base == '\\' || *base == '/')
        ++base;
    }
    if (*base == '\0' || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
      if (out_status) {
        out_status->type = cfs_file_type_directory;
        out_status->permissions = cfs_perms_all;
      }
      return cfs_true;
    }
    if (strchr(base, '\\') != NULL || strchr(base, '/') != NULL) {
      errno = ENOENT;
      return cfs_false;
    }
    if (_dos_getfileattr(clean, &attr) != 0) {
      errno = ENOENT;
      return cfs_false;
    }
    if (out_status) {
      out_status->type = cfs_file_type_regular;
      out_status->permissions =
          ((attr & 0x01U) != 0) ? (cfs_perms)0444 : (cfs_perms)0666;
    }
    if (out_size) {
      FILE *fp = fopen(clean, "rb");
      if (fp) {
        long pos = 0;
        if (fseek(fp, 0L, SEEK_END) == 0) {
          pos = ftell(fp);
          if (pos > 0)
            *out_size = (cfs_uintmax_t)pos;
        }
        fclose(fp);
      }
    }
    return cfs_true;
  }
}
#endif

/**
 * @brief Queries filesystem status and size of a non-NULL path without
 * recording OS errors.
 *
 * @param p Non-NULL pointer to the path structure to query.
 * @param out Pointer to the status structure to populate.
 * @param out_size Optional pointer to receive file size in bytes.
 * @param out_ok Set to `cfs_true` if the node exists, else `cfs_false`.
 */
static void cfs_query_status(const cfs_path *p, cfs_file_status *out,
                             cfs_uintmax_t *out_size, cfs_bool *out_ok) {
  out->type = cfs_file_type_not_found;
  out->permissions = (cfs_perms)0;
  if (out_size)
    *out_size = 0;
  *out_ok = cfs_false;
  if (!p->str)
    return;
#if defined(CFS_OS_WINDOWS)
  {
    struct _stat64 st;
#if defined(CFS_UNICODE)
    if (_wstat64(p->str, &st) == 0) {
#else
    if (_stat64(p->str, &st) == 0) {
#endif
      cfs_ignore_errc(cfs_status_from_mode((unsigned int)st.st_mode, out));
      if (out_size)
        *out_size = (cfs_uintmax_t)st.st_size;
      *out_ok = cfs_true;
    }
  }
#elif defined(CFS_OS_DOS)
  *out_ok = cfs_dos_stat_info(p->str, out, out_size, NULL);
#else
  {
    struct stat st;
    if (stat(p->str, &st) == 0) {
      cfs_ignore_errc(cfs_status_from_mode((unsigned int)st.st_mode, out));
      if (out_size)
        *out_size = (cfs_uintmax_t)st.st_size;
      *out_ok = cfs_true;
    }
  }
#endif
}

/*
 * \brief Evaluates the file status and type of the given path, traversing
 * symlinks.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the status operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_status(const cfs_path *p,
                                           cfs_file_status *out,
                                           cfs_error_code *ec) {
  cfs_bool ok = cfs_false;
  cfs_clear_error(ec);
  if (!p || !p->str || !out)
    return cfs_errc_invalid_argument;
  cfs_query_status(p, out, NULL, &ok);
  if (ok)
    return cfs_errc_success;
  return cfs_record_last_error(ec);
}

/*
 * \brief Evaluates the file status and type of the given path without
 * traversing symlinks.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the symlink_status operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_symlink_status(const cfs_path *p,
                                                   cfs_file_status *out,
                                                   cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  DWORD attrs;
  cfs_clear_error(ec);
  if (!p || !p->str || !out)
    return cfs_errc_invalid_argument;
#if defined(CFS_UNICODE)
  attrs = GetFileAttributesW(p->str);
#else
  attrs = GetFileAttributesA(p->str);
#endif
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    out->type = cfs_file_type_not_found;
    out->permissions = (cfs_perms)0;
    return cfs_record_last_error(ec);
  }
  if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    out->type = cfs_file_type_symlink;
    out->permissions = ((attrs & FILE_ATTRIBUTE_READONLY) != 0)
                           ? (cfs_perms)0555
                           : (cfs_perms)0777;
    return cfs_errc_success;
  }
  return cfs_status(p, out, ec);
#elif defined(CFS_OS_DOS)
  return cfs_status(p, out, ec);
#else
  struct stat st;
  cfs_clear_error(ec);
  if (!p || !p->str || !out)
    return cfs_errc_invalid_argument;
  if (lstat(p->str, &st) == 0) {
    cfs_ignore_errc(cfs_status_from_mode((unsigned int)st.st_mode, out));
    return cfs_errc_success;
  }
  out->type = cfs_file_type_not_found;
  out->permissions = (cfs_perms)0;
  return cfs_record_last_error(ec);
#endif
}

/*
 * \brief Evaluates if a given file status indicates an existing filesystem
 * node.
 *
 * \param s The file status struct to evaluate.
 * \param out Pointer to store the result of the exists operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_exists(cfs_file_status s, cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type != cfs_file_type_none && s.type != cfs_file_type_not_found)
             ? cfs_true
             : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Checks if a given path corresponds to an existing filesystem node.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the exists_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_exists_path(const cfs_path *p,
                                                cfs_bool *out,
                                                cfs_error_code *ec) {
  cfs_file_status s;
  cfs_bool ok = cfs_false;
  cfs_clear_error(ec);
  if (!p || !out)
    return cfs_errc_invalid_argument;
  cfs_query_status(p, &s, NULL, &ok);
  return cfs_exists(s, out);
}

/*
 * \brief Performs the cfs_is_block_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_block_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_block_file(cfs_file_status s,
                                                  cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type == cfs_file_type_block) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_character_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_character_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_character_file(cfs_file_status s,
                                                      cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type == cfs_file_type_character) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_directory filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_directory operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_directory(cfs_file_status s,
                                                 cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type == cfs_file_type_directory) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_fifo filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_fifo operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_fifo(cfs_file_status s, cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type == cfs_file_type_fifo) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_other filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_other operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_other(cfs_file_status s, cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type != cfs_file_type_none && s.type != cfs_file_type_not_found &&
          s.type != cfs_file_type_regular &&
          s.type != cfs_file_type_directory && s.type != cfs_file_type_symlink)
             ? cfs_true
             : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_regular_file filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_regular_file operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_regular_file(cfs_file_status s,
                                                    cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type == cfs_file_type_regular) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_socket filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_socket operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_socket(cfs_file_status s,
                                              cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type == cfs_file_type_socket) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_symlink filesystem operation.
 *
 * \param s Argument representing the target resource.
 * \param out Pointer to store the result of the is_symlink operation.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_symlink(cfs_file_status s,
                                               cfs_bool *out) {
  if (!out)
    return cfs_errc_invalid_argument;
  *out = (s.type == cfs_file_type_symlink) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_is_empty_path filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the is_empty_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_is_empty_path(const cfs_path *p,
                                                  cfs_bool *out,
                                                  cfs_error_code *ec) {
  cfs_file_status s;
  cfs_uintmax_t sz = 0;
  cfs_bool ok = cfs_false;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!p || !p->str || !out)
    return cfs_errc_invalid_argument;
  cfs_query_status(p, &s, &sz, &ok);
  if (!ok)
    return cfs_record_last_error(ec);
  if (s.type == cfs_file_type_directory) {
    cfs_directory_iterator *it = NULL;
    const cfs_directory_entry *en = NULL;
    rc = cfs_dir_itr_init(p, &it, ec);
    if (rc != cfs_errc_success)
      return rc;
    rc = cfs_dir_itr_next(it, &en, ec);
    if (rc != cfs_errc_success) {
      cfs_dir_itr_close(it);
      return rc;
    }
    *out = (en == NULL) ? cfs_true : cfs_false;
    cfs_dir_itr_close(it);
    return cfs_errc_success;
  }
  *out = (sz == 0) ? cfs_true : cfs_false;
  return cfs_errc_success;
}

/*
 * \brief Creates a single new directory node at the given path.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory(const cfs_path *p,
                                                     cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !p->str || p->length == 0)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (_wmkdir(p->str) == 0)
    return cfs_errc_success;
#else
  if (_mkdir(p->str) == 0)
    return cfs_errc_success;
#endif
#elif defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    cfs_set_error(ec, ENOSYS, cfs_errc_operation_not_supported);
    return cfs_errc_operation_not_supported;
  }
  if (mkdir(p->str) == 0)
    return cfs_errc_success;
#else
  if (mkdir(p->str, 0777) == 0)
    return cfs_errc_success;
#endif
  return cfs_record_last_error(ec);
}

/*
 * \brief Creates a single new directory copying attributes/permissions from an
 * existing template directory.
 *
 * \param p Target path for the new directory.
 * \param existing_p Existing template directory path whose attributes are
 * copied.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory_attr(
    const cfs_path *p, const cfs_path *existing_p, cfs_error_code *ec) {
  cfs_file_status tmpl_st;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!p || !p->str || p->length == 0 || !existing_p || !existing_p->str ||
      existing_p->length == 0) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  rc = cfs_status(existing_p, &tmpl_st, ec);
  if (rc != cfs_errc_success)
    return rc;
  if (tmpl_st.type != cfs_file_type_directory) {
    cfs_set_error(ec, 0, cfs_errc_not_a_directory);
    return cfs_errc_not_a_directory;
  }
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (CreateDirectoryExW(existing_p->str, p->str, NULL))
    return cfs_errc_success;
#else
  if (CreateDirectoryExA(existing_p->str, p->str, NULL))
    return cfs_errc_success;
#endif
#elif defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    cfs_set_error(ec, ENOSYS, cfs_errc_operation_not_supported);
    return cfs_errc_operation_not_supported;
  }
  if (mkdir(p->str) == 0)
    return cfs_errc_success;
#else
  if (mkdir(p->str, (mode_t)tmpl_st.permissions) == 0)
    return cfs_errc_success;
#endif
  return cfs_record_last_error(ec);
}

/*
 * \brief Recursively creates a directory and any missing parent directories.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directories(const cfs_path *p,
                                                       cfs_error_code *ec) {
  cfs_file_status st;
  cfs_bool st_ok = cfs_false;
  cfs_path parent;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!p || !p->str || p->length == 0)
    return cfs_errc_invalid_argument;
  cfs_query_status(p, &st, NULL, &st_ok);
  if (st_ok && st.type == cfs_file_type_directory)
    return cfs_errc_success;
  rc = cfs_path_parent_path(p, &parent);
  if (rc != cfs_errc_success)
    return rc;
  if (parent.length > 0 && parent.length < p->length) {
    rc = cfs_create_directories(&parent, ec);
    if (rc != cfs_errc_success) {
      cfs_path_destroy(&parent);
      return rc;
    }
  }
  cfs_path_destroy(&parent);
  return cfs_create_directory(p, ec);
}

/*
 * \brief Performs the cfs_create_hard_link filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param new_link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_hard_link(const cfs_path *target,
                                                     const cfs_path *new_link,
                                                     cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!target || !new_link || !target->str || !new_link->str)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (CreateHardLinkW(new_link->str, target->str, NULL))
    return cfs_errc_success;
#else
  if (CreateHardLinkA(new_link->str, target->str, NULL))
    return cfs_errc_success;
#endif
  return cfs_record_last_error(ec);
#elif defined(CFS_OS_DOS)
  cfs_set_error(ec, 0, cfs_errc_operation_not_supported);
  return cfs_errc_operation_not_supported;
#else
  if (link(target->str, new_link->str) == 0)
    return cfs_errc_success;
  return cfs_record_last_error(ec);
#endif
}

/*
 * \brief Performs the cfs_create_directory_symlink filesystem operation.
 *
 * \param target Argument representing the target resource.
 * \param new_link Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_create_directory_symlink(
    const cfs_path *target, const cfs_path *new_link, cfs_error_code *ec) {
  if (!target || !new_link)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  return cfs_win_create_symlink(target, new_link, 0x1UL, ec);
#else
  return cfs_create_symlink(target, new_link, ec);
#endif
}

/*
 * \brief Recursively deletes a directory node and all of its nested contents.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the remove_all operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_remove_all(const cfs_path *p,
                                               cfs_size_t *out,
                                               cfs_error_code *ec) {
  cfs_file_status st;
  cfs_size_t count = 0;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (out)
    *out = 0;
  if (!p || !p->str || p->length == 0)
    return cfs_errc_invalid_argument;
  rc = cfs_symlink_status(p, &st, ec);
  if (rc != cfs_errc_success)
    return rc;
  if (st.type == cfs_file_type_directory) {
    cfs_directory_iterator *it = NULL;
    const cfs_directory_entry *en = NULL;
    rc = cfs_dir_itr_init(p, &it, ec);
    if (rc != cfs_errc_success)
      return rc;
    for (;;) {
      cfs_size_t sub_count = 0;
      rc = cfs_dir_itr_next(it, &en, ec);
      if (rc != cfs_errc_success) {
        cfs_dir_itr_close(it);
        return rc;
      }
      if (!en)
        break;
      rc = cfs_remove_all(&en->path, &sub_count, ec);
      if (rc != cfs_errc_success) {
        cfs_dir_itr_close(it);
        return rc;
      }
      count += sub_count;
    }
    cfs_dir_itr_close(it);
  }
  rc = cfs_remove(p, ec);
  if (rc != cfs_errc_success)
    return rc;
  count++;
  if (out)
    *out = count;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_rename filesystem operation.
 *
 * \param old_p Argument representing the target resource.
 * \param new_p Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rename(const cfs_path *old_p,
                                           const cfs_path *new_p,
                                           cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!old_p || !new_p || !old_p->str || !new_p->str)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
#if defined(CFS_UNICODE)
  if (MoveFileExW(old_p->str, new_p->str,
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
    return cfs_errc_success;
#else
  if (MoveFileExA(old_p->str, new_p->str,
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
    return cfs_errc_success;
#endif
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status old_st;
    cfs_file_status new_st;
    cfs_bool same = cfs_false;
    if (!cfs_dos_stat_info(old_p->str, &old_st, NULL, NULL))
      return cfs_record_last_error(ec);
    cfs_ignore_errc(cfs_equivalent(old_p, new_p, &same, NULL));
    if (same)
      return cfs_errc_success;
    if (cfs_dos_stat_info(new_p->str, &new_st, NULL, NULL) &&
        new_st.type == cfs_file_type_regular) {
      (void)remove(new_p->str);
    }
    if (rename(old_p->str, new_p->str) == 0)
      return cfs_errc_success;
  }
#else
  if (rename(old_p->str, new_p->str) == 0)
    return cfs_errc_success;
#endif
  return cfs_record_last_error(ec);
}

/*
 * \brief Performs the cfs_resize_file filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param size Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_resize_file(const cfs_path *p,
                                                cfs_uintmax_t size,
                                                cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !p->str || p->length == 0)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_WINDOWS)
  {
    HANDLE h;
    LARGE_INTEGER li;
    BOOL ok;
#if defined(CFS_UNICODE)
    h = CreateFileW(p->str, GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else
    h = CreateFileA(p->str, GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    if (h == INVALID_HANDLE_VALUE)
      return cfs_record_last_error(ec);
    li.QuadPart = (LONGLONG)size;
    ok = SetFilePointerEx(h, li, NULL, FILE_BEGIN) && SetEndOfFile(h);
    if (!ok) {
      cfs_errc err = cfs_record_last_error(ec);
      CloseHandle(h);
      return err;
    }
    CloseHandle(h);
    return cfs_errc_success;
  }
#elif defined(CFS_OS_DOS)
  if (_osmajor >= 2) {
    int fd = open(p->str, O_RDWR | O_BINARY);
    if (fd < 0)
      return cfs_record_last_error(ec);
    if (chsize(fd, (long)size) != 0) {
      cfs_errc err = cfs_record_last_error(ec);
      close(fd);
      return err;
    }
    close(fd);
    return cfs_errc_success;
  } else {
    cfs_file_status st;
    cfs_uintmax_t old_sz = 0;
    cfs_size_t keep;
    unsigned char *buf = NULL;
    FILE *fp;
    if (!cfs_dos_stat_info(p->str, &st, &old_sz, NULL) ||
        st.type != cfs_file_type_regular) {
      cfs_set_error(ec, ENOENT, cfs_errc_no_such_file_or_directory);
      return cfs_errc_no_such_file_or_directory;
    }
    keep = (cfs_size_t)((old_sz < size) ? old_sz : size);
    if (keep > 0) {
      cfs_errc rc = cfs_malloc(keep, (void **)&buf);
      if (rc != cfs_errc_success) {
        cfs_set_error(ec, ENOMEM, rc);
        return rc;
      }
      fp = fopen(p->str, "rb");
      if (!fp) {
        cfs_free(buf);
        return cfs_record_last_error(ec);
      }
      keep = fread(buf, 1, keep, fp);
      fclose(fp);
    }
    fp = fopen(p->str, "wb");
    if (!fp) {
      cfs_free(buf);
      return cfs_record_last_error(ec);
    }
    if (keep > 0) {
      (void)fwrite(buf, 1, keep, fp);
      cfs_free(buf);
    }
    if (size > (cfs_uintmax_t)keep) {
      cfs_uintmax_t rem = size - (cfs_uintmax_t)keep;
      unsigned char zeros[64];
      memset(zeros, 0, sizeof(zeros));
      while (rem > 0) {
        cfs_size_t chunk = (rem > (cfs_uintmax_t)sizeof(zeros))
                               ? sizeof(zeros)
                               : (cfs_size_t)rem;
        (void)fwrite(zeros, 1, chunk, fp);
        rem -= (cfs_uintmax_t)chunk;
      }
    }
    fclose(fp);
    return cfs_errc_success;
  }
#else
  if (truncate(p->str, (off_t)size) == 0)
    return cfs_errc_success;
  return cfs_record_last_error(ec);
#endif
}

/*
 * \brief Performs the cfs_space filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the space operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_space(const cfs_path *p,
                                          cfs_space_info *out,
                                          cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  ULARGE_INTEGER avail, total, free_b;
  cfs_path abs_p;
  cfs_path query_p;
  DWORD attrs;
  BOOL ok;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!p || !p->str || !out)
    return cfs_errc_invalid_argument;
  cfs_path_init(&abs_p);
  cfs_path_init(&query_p);
  rc = cfs_absolute(p, &abs_p, ec);
  if (rc != cfs_errc_success)
    return rc;
#if defined(CFS_UNICODE)
  attrs = GetFileAttributesW(abs_p.str);
#else
  attrs = GetFileAttributesA(abs_p.str);
#endif
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    cfs_path_destroy(&abs_p);
    return cfs_record_last_error(ec);
  }
  if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    rc = cfs_path_clone(&query_p, &abs_p);
  } else {
    rc = cfs_path_parent_path(&abs_p, &query_p);
  }
  cfs_path_destroy(&abs_p);
  if (rc != cfs_errc_success)
    return rc;
#if defined(CFS_UNICODE)
  ok = GetDiskFreeSpaceExW(query_p.str, &avail, &total, &free_b);
#else
  ok = GetDiskFreeSpaceExA(query_p.str, &avail, &total, &free_b);
#endif
  cfs_path_destroy(&query_p);
  if (ok) {
    out->capacity = (cfs_uintmax_t)total.QuadPart;
    out->free = (cfs_uintmax_t)free_b.QuadPart;
    out->available = (cfs_uintmax_t)avail.QuadPart;
    return cfs_errc_success;
  }
  return cfs_record_last_error(ec);
#elif defined(CFS_OS_DOS)
  cfs_file_status st;
  unsigned int drv = 0;
  struct _diskfree_t df;
  cfs_clear_error(ec);
  if (!p || !p->str || !out)
    return cfs_errc_invalid_argument;
  if (!cfs_dos_stat_info(p->str, &st, NULL, NULL)) {
    cfs_set_error(ec, ENOENT, cfs_errc_no_such_file_or_directory);
    return cfs_errc_no_such_file_or_directory;
  }
  if (p->length >= 2 && p->str[1] == ':') {
    char dc = p->str[0];
    if (dc >= 'a' && dc <= 'z')
      drv = (unsigned int)(dc - 'a' + 1);
    else if (dc >= 'A' && dc <= 'Z')
      drv = (unsigned int)(dc - 'A' + 1);
  }
  if (_osmajor >= 2 && _dos_getdiskfree(drv, &df) == 0) {
    cfs_uintmax_t bpc = (cfs_uintmax_t)df.sectors_per_cluster *
                        (cfs_uintmax_t)df.bytes_per_sector;
    out->capacity = (cfs_uintmax_t)df.total_clusters * bpc;
    out->free = (cfs_uintmax_t)df.avail_clusters * bpc;
    out->available = out->free;
    return cfs_errc_success;
  }
  out->capacity = (cfs_uintmax_t)33554432UL;
  out->free = (cfs_uintmax_t)16777216UL;
  out->available = out->free;
  return cfs_errc_success;
#else
  struct statvfs sv;
  cfs_clear_error(ec);
  if (!p || !out)
    return cfs_errc_invalid_argument;
  if (statvfs(p->str, &sv) == 0) {
    out->capacity = sv.f_blocks * sv.f_frsize;
    out->free = sv.f_bfree * sv.f_frsize;
    out->available = sv.f_bavail * sv.f_frsize;
    return cfs_errc_success;
  }
  return cfs_record_last_error(ec);
#endif
}

/*
 * \brief Performs the cfs_last_write_time filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out Pointer to store the result of the last_write_time operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_last_write_time(const cfs_path *p,
                                                    cfs_file_time_type *out,
                                                    cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !out)
    return cfs_errc_invalid_argument;
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
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status st;
    cfs_file_time_type mt = 0;
    if (p->str && cfs_dos_stat_info(p->str, &st, NULL, &mt)) {
      *out = mt;
      return cfs_errc_success;
    }
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
  return cfs_record_last_error(ec);
}

/*
 * \brief Updates the last modification timestamp of a filesystem node.
 *
 * \param p Pointer to the target `cfs_path` object.
 * \param new_time New modification timestamp in system epoch seconds.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_last_write_time_set(
    const cfs_path *p, cfs_file_time_type new_time, cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!p || !p->str || p->length == 0) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
#if defined(CFS_OS_WINDOWS)
  {
    HANDLE h;
    FILETIME ft;
    ULARGE_INTEGER ull;
    BOOL ok;
    ull.QuadPart = ((ULONGLONG)new_time * (ULONGLONG)10000000) +
                   ((ULONGLONG)0x019DB1DE << 32) + (ULONGLONG)0xD53E8000;
    ft.dwLowDateTime = ull.LowPart;
    ft.dwHighDateTime = ull.HighPart;
#if defined(CFS_UNICODE)
    h = CreateFileW(p->str, FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#else
    h = CreateFileA(p->str, FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#endif
    if (h == INVALID_HANDLE_VALUE)
      return cfs_record_last_error(ec);
    ok = SetFileTime(h, NULL, NULL, &ft);
    if (!ok) {
      cfs_errc err = cfs_record_last_error(ec);
      CloseHandle(h);
      return err;
    }
    CloseHandle(h);
    return cfs_errc_success;
  }
#elif defined(CFS_OS_DOS)
  {
    cfs_file_status st;
    if (!cfs_dos_stat_info(p->str, &st, NULL, NULL)) {
      return cfs_record_last_error(ec);
    }
    if (_osmajor >= 2 && st.type == cfs_file_type_regular) {
      struct utimbuf ut;
      ut.actime = (time_t)new_time;
      ut.modtime = (time_t)new_time;
      if (utime(p->str, &ut) == 0)
        return cfs_errc_success;
    }
    return cfs_errc_success;
  }
#else
  {
    struct utimbuf ut;
    ut.actime = (time_t)new_time;
    ut.modtime = (time_t)new_time;
    if (utime(p->str, &ut) == 0)
      return cfs_errc_success;
    return cfs_record_last_error(ec);
  }
#endif
}

/*
 * \brief Performs the cfs_temp_directory_path filesystem operation.
 *
 * \param out Pointer to store the result of the temp_directory_path operation.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_temp_directory_path(cfs_path *out,
                                                        cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  cfs_char_t buf[CFS_MAX_PATH];
  DWORD len;
  cfs_clear_error(ec);
  if (!out)
    return cfs_errc_invalid_argument;
#if defined(CFS_UNICODE)
  len = GetTempPathW(CFS_MAX_PATH, buf);
#else
  len = GetTempPathA(CFS_MAX_PATH, buf);
#endif
  if (len > 0 && len < CFS_MAX_PATH)
    return cfs_path_assign(out, buf);
  return cfs_record_last_error(ec);
#elif defined(CFS_OS_DOS)
  static const char *const k_dos_env[] = {"TMPDIR", "TEMP", "TMP"};
  cfs_size_t i;
  cfs_file_status st;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!out)
    return cfs_errc_invalid_argument;
  for (i = 0; i < sizeof(k_dos_env) / sizeof(k_dos_env[0]); ++i) {
    const char *val = getenv(k_dos_env[i]);
    if (val && val[0] != '\0') {
      rc = cfs_path_assign(out, val);
      if (rc != cfs_errc_success)
        return rc;
      if (!cfs_dos_stat_info(out->str, &st, NULL, NULL) ||
          st.type != cfs_file_type_directory) {
        cfs_path_clear(out);
        cfs_set_error(ec, ENOENT, cfs_errc_not_a_directory);
        return cfs_errc_not_a_directory;
      }
      return cfs_errc_success;
    }
  }
  return cfs_current_path(out, ec);
#else
  static const char *const k_env_vars[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR"};
  const char *tmp = NULL;
  cfs_size_t i;
  cfs_file_status st;
  cfs_bool ok = cfs_false;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!out)
    return cfs_errc_invalid_argument;
  for (i = 0; i < sizeof(k_env_vars) / sizeof(k_env_vars[0]); ++i) {
    const char *val = getenv(k_env_vars[i]);
    if (val && val[0] != '\0') {
      tmp = val;
      break;
    }
  }
  if (!tmp)
    tmp = "/tmp";
  rc = cfs_path_assign(out, tmp);
  if (rc != cfs_errc_success)
    return rc;
  cfs_query_status(out, &st, NULL, &ok);
  if (!ok || st.type != cfs_file_type_directory) {
    cfs_path_clear(out);
    cfs_set_error(ec, ENOTDIR, cfs_errc_not_a_directory);
    return cfs_errc_not_a_directory;
  }
  return cfs_errc_success;
#endif
}

/*
 * \brief Initializes an empty `cfs_directory_entry` with unknown cached status.
 *
 * \param entry Pointer to the `cfs_directory_entry` to initialize.
 */
CFS_API void cfs_dir_entry_init(cfs_directory_entry *entry) {
  if (!entry)
    return;
  cfs_path_init(&entry->path);
  entry->status.type = cfs_file_type_none;
  entry->status.permissions = (cfs_perms)0;
  entry->symlink_status.type = cfs_file_type_none;
  entry->symlink_status.permissions = (cfs_perms)0;
}

/*
 * \brief Releases resources owned by a `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to destroy.
 */
CFS_API void cfs_dir_entry_destroy(cfs_directory_entry *entry) {
  if (!entry)
    return;
  cfs_path_destroy(&entry->path);
  entry->status.type = cfs_file_type_none;
  entry->symlink_status.type = cfs_file_type_none;
}

/*
 * \brief Updates cached status attributes of a `cfs_directory_entry` from disk.
 *
 * \param entry Pointer to the `cfs_directory_entry` to refresh.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_entry_refresh(cfs_directory_entry *entry, cfs_error_code *ec) {
  cfs_error_code sym_ec;
  cfs_error_code st_ec;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!entry || !entry->path.str || entry->path.length == 0) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  rc = cfs_symlink_status(&entry->path, &entry->symlink_status, &sym_ec);
  if (rc != cfs_errc_success && rc != cfs_errc_no_such_file_or_directory) {
    cfs_set_error(ec, sym_ec.value, sym_ec.errc);
    return rc;
  }
  if (rc == cfs_errc_no_such_file_or_directory) {
    entry->symlink_status.type = cfs_file_type_not_found;
    entry->status.type = cfs_file_type_not_found;
    return cfs_errc_success;
  }
  rc = cfs_status(&entry->path, &entry->status, &st_ec);
  if (rc == cfs_errc_no_such_file_or_directory) {
    entry->status.type = cfs_file_type_not_found;
    return cfs_errc_success;
  }
  if (rc != cfs_errc_success) {
    cfs_set_error(ec, st_ec.value, st_ec.errc);
    return rc;
  }
  return cfs_errc_success;
}

/*
 * \brief Assigns a new path to a `cfs_directory_entry` and refreshes its cached
 * status.
 *
 * \param entry Pointer to the `cfs_directory_entry` to modify.
 * \param p New path to assign.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_assign(cfs_directory_entry *entry,
                                                     const cfs_path *p,
                                                     cfs_error_code *ec) {
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!entry || !p || !p->str || p->length == 0) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  rc = cfs_path_assign(&entry->path, p->str);
  if (rc != cfs_errc_success) {
    cfs_set_error(ec, 0, rc);
    return rc;
  }
  return cfs_dir_entry_refresh(entry, ec);
}

/*
 * \brief Replaces the terminal filename of a `cfs_directory_entry` and
 * refreshes its cached status.
 *
 * \param entry Pointer to the `cfs_directory_entry` to modify.
 * \param replacement New terminal filename string.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_replace_filename(
    cfs_directory_entry *entry, const cfs_char_t *replacement,
    cfs_error_code *ec) {
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!entry || !replacement) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  rc = cfs_path_replace_filename(&entry->path, replacement);
  if (rc != cfs_errc_success) {
    cfs_set_error(ec, 0, rc);
    return rc;
  }
  return cfs_dir_entry_refresh(entry, ec);
}

/*
 * \brief Retrieves a pointer to the `cfs_path` stored in a
 * `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out_path Output pointer receiving `&entry->path`.
 * \return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_path(
    const cfs_directory_entry *entry, const cfs_path **out_path) {
  if (!entry || !out_path)
    return cfs_errc_invalid_argument;
  *out_path = &entry->path;
  return cfs_errc_success;
}

/*
 * \brief Retrieves the target status (following symlinks) of a
 * `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output pointer receiving the resolved `cfs_file_status`.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_entry_status(const cfs_directory_entry *entry, cfs_file_status *out,
                     cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!entry || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  if (entry->status.type != cfs_file_type_none) {
    *out = entry->status;
    return cfs_errc_success;
  }
  return cfs_status(&entry->path, out, ec);
}

/*
 * \brief Retrieves the link status (not following symlinks) of a
 * `cfs_directory_entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output pointer receiving the resolved `cfs_file_status`.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_entry_symlink_status(const cfs_directory_entry *entry,
                             cfs_file_status *out, cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!entry || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  if (entry->symlink_status.type != cfs_file_type_none) {
    *out = entry->symlink_status;
    return cfs_errc_success;
  }
  return cfs_symlink_status(&entry->path, out, ec);
}

/**
 * \brief Evaluates a `cfs_file_status` query helper on a `cfs_directory_entry`.
 *
 * \param entry Entry to inspect.
 * \param use_sym Non-zero to query symlink status instead of target status.
 * \param fn Status predicate function.
 * \param out Output boolean flag.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
static cfs_errc cfs_dir_entry_pred(const cfs_directory_entry *entry,
                                   cfs_bool use_sym,
                                   cfs_errc (*fn)(cfs_file_status, cfs_bool *),
                                   cfs_bool *out, cfs_error_code *ec) {
  cfs_file_status st;
  cfs_error_code local_ec;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!entry || !out) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  rc = use_sym ? cfs_dir_entry_symlink_status(entry, &st, &local_ec)
               : cfs_dir_entry_status(entry, &st, &local_ec);
  if (rc == cfs_errc_no_such_file_or_directory) {
    *out = cfs_false;
    return cfs_errc_success;
  }
  if (rc != cfs_errc_success) {
    cfs_set_error(ec, local_ec.value, local_ec.errc);
    return rc;
  }
  return fn(st, out);
}

/*
 * \brief Checks whether the node referenced by `entry` exists.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_exists(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_exists, out, ec);
}

/*
 * \brief Checks whether `entry` refers to a regular file.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_regular_file(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_is_regular_file, out, ec);
}

/*
 * \brief Checks whether `entry` refers to a directory.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_directory(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_is_directory, out, ec);
}

/*
 * \brief Checks whether `entry` itself is a symbolic link.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_symlink(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_true, cfs_is_symlink, out, ec);
}

/*
 * \brief Checks whether `entry` refers to a block device.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_block_file(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_is_block_file, out, ec);
}

/*
 * \brief Checks whether `entry` refers to a character device.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_character_file(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_is_character_file, out, ec);
}

/*
 * \brief Checks whether `entry` refers to a named pipe (FIFO).
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_fifo(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_is_fifo, out, ec);
}

/*
 * \brief Checks whether `entry` refers to a socket.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_socket(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_is_socket, out, ec);
}

/*
 * \brief Checks whether `entry` refers to a non-regular, non-directory,
 * non-symlink existing node.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output boolean result.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_is_other(
    const cfs_directory_entry *entry, cfs_bool *out, cfs_error_code *ec) {
  return cfs_dir_entry_pred(entry, cfs_false, cfs_is_other, out, ec);
}

/*
 * \brief Queries the byte size of the regular file referenced by `entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output file size in bytes.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_file_size(
    const cfs_directory_entry *entry, cfs_uintmax_t *out, cfs_error_code *ec) {
  if (!entry) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  return cfs_file_size(&entry->path, out, ec);
}

/*
 * \brief Queries the hard-link count of the node referenced by `entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output hard-link count.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_entry_hard_link_count(
    const cfs_directory_entry *entry, cfs_uintmax_t *out, cfs_error_code *ec) {
  if (!entry) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  return cfs_hard_link_count(&entry->path, out, ec);
}

/*
 * \brief Queries the last modification timestamp of the node referenced by
 * `entry`.
 *
 * \param entry Pointer to the `cfs_directory_entry` to inspect.
 * \param out Output timestamp.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_entry_last_write_time(const cfs_directory_entry *entry,
                              cfs_file_time_type *out, cfs_error_code *ec) {
  if (!entry) {
    cfs_set_error(ec, 0, cfs_errc_invalid_argument);
    return cfs_errc_invalid_argument;
  }
  return cfs_last_write_time(&entry->path, out, ec);
}

/*
 * \brief Opens a non-recursive directory iterator with explicit
 * `cfs_directory_options`.
 *
 * \param p Directory path to iterate.
 * \param opts Iteration options (such as `skip_permission_denied`).
 * \param out_it Output iterator pointer.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_itr_init_opts(const cfs_path *p, cfs_directory_options opts,
                      cfs_directory_iterator **out_it, cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  cfs_directory_iterator *it;
  cfs_path search_p;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (out_it)
    *out_it = NULL;
  if (!p || !p->str || !out_it)
    return cfs_errc_invalid_argument;
  rc = cfs_malloc(sizeof(cfs_directory_iterator), (void **)&it);
  if (rc != cfs_errc_success) {
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
  it->is_end = cfs_false;
  it->first = cfs_true;
  it->options = opts;
  it->find_handle = INVALID_HANDLE_VALUE;
  cfs_dir_entry_init(&it->current);
  rc = cfs_path_clone(&it->base_dir, p);
  if (rc != cfs_errc_success) {
    cfs_free(it);
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
  rc = cfs_path_clone(&search_p, p);
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&it->base_dir);
    cfs_free(it);
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
  rc = cfs_path_append(&search_p, CFS_STR("*"));
  if (rc != cfs_errc_success) {
    cfs_path_destroy(&search_p);
    cfs_path_destroy(&it->base_dir);
    cfs_free(it);
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
#if defined(CFS_UNICODE)
  it->find_handle = FindFirstFileW(search_p.str, &it->find_data);
#else
  it->find_handle = FindFirstFileA(search_p.str, &it->find_data);
#endif
  cfs_path_destroy(&search_p);
  if (it->find_handle == INVALID_HANDLE_VALUE) {
    cfs_error_code open_ec;
    cfs_errc err = cfs_record_last_error(&open_ec);
    if (err == cfs_errc_permission_denied &&
        ((int)opts & (int)cfs_directory_options_skip_permission_denied) != 0) {
      it->is_end = cfs_true;
      *out_it = it;
      return cfs_errc_success;
    }
    cfs_set_error(ec, open_ec.value, open_ec.errc);
    cfs_path_destroy(&it->base_dir);
    cfs_free(it);
    return err;
  }
  *out_it = it;
  return cfs_errc_success;
#else
  cfs_directory_iterator *it;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (out_it)
    *out_it = NULL;
  if (!p || !p->str || !out_it)
    return cfs_errc_invalid_argument;
#if defined(CFS_OS_DOS)
  if (_osmajor < 2) {
    cfs_set_error(ec, ENOSYS, cfs_errc_operation_not_supported);
    return cfs_errc_operation_not_supported;
  }
#endif
  rc = cfs_malloc(sizeof(cfs_directory_iterator), (void **)&it);
  if (rc != cfs_errc_success) {
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
  it->is_end = cfs_false;
  it->options = opts;
  it->dirp = NULL;
  cfs_dir_entry_init(&it->current);
  rc = cfs_path_clone(&it->base_dir, p);
  if (rc != cfs_errc_success) {
    cfs_free(it);
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
#if defined(CFS_OS_DOS)
  it->dirp = opendir(p->str);
  if (!it->dirp && strcmp(p->str, ".") == 0) {
    char cwd_buf[CFS_MAX_PATH];
    if (getcwd(cwd_buf, sizeof(cwd_buf)) != NULL) {
      it->dirp = opendir(cwd_buf);
    }
  }
#elif defined(CFS_OS_EMSCRIPTEN)
  {
    char real_buf[CFS_MAX_PATH];
    if (realpath(p->str, real_buf) != NULL) {
      it->dirp = opendir(real_buf);
    } else {
      it->dirp = opendir(p->str);
    }
  }
#else
  it->dirp = opendir(p->str);
#endif
  if (!it->dirp) {
    cfs_error_code open_ec;
    cfs_errc err = cfs_record_last_error(&open_ec);
    if (err == cfs_errc_permission_denied &&
        ((int)opts & (int)cfs_directory_options_skip_permission_denied) != 0) {
      it->is_end = cfs_true;
      *out_it = it;
      return cfs_errc_success;
    }
    cfs_set_error(ec, open_ec.value, open_ec.errc);
    cfs_path_destroy(&it->base_dir);
    cfs_free(it);
    return err;
  }
  *out_it = it;
  return cfs_errc_success;
#endif
}

/*
 * \brief Performs the cfs_dir_itr_init filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out_it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_dir_itr_init(
    const cfs_path *p, cfs_directory_iterator **out_it, cfs_error_code *ec) {
  return cfs_dir_itr_init_opts(p, cfs_directory_options_none, out_it, ec);
}

/*
 * \brief Performs the cfs_dir_itr_next filesystem operation.
 *
 * \param it Argument representing the target resource.
 * \param out_entry Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_dir_itr_next(cfs_directory_iterator *it,
                 const cfs_directory_entry **out_entry, cfs_error_code *ec) {
#if defined(CFS_OS_WINDOWS)
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!it || !out_entry)
    return cfs_errc_invalid_argument;
  *out_entry = NULL;
  if (it->is_end)
    return cfs_errc_success;
  for (;;) {
    if (it->first) {
      it->first = cfs_false;
    } else {
#if defined(CFS_UNICODE)
      if (!FindNextFileW(it->find_handle, &it->find_data)) {
#else
      if (!FindNextFileA(it->find_handle, &it->find_data)) {
#endif
        it->is_end = cfs_true;
        return cfs_errc_success;
      }
    }
    if (CFS_RAW_STRCMP(it->find_data.cFileName, CFS_STR(".")) != 0 &&
        CFS_RAW_STRCMP(it->find_data.cFileName, CFS_STR("..")) != 0) {
      break;
    }
  }
  rc = cfs_path_assign(&it->current.path, it->base_dir.str);
  if (rc != cfs_errc_success)
    return rc;
  rc = cfs_path_append(&it->current.path, it->find_data.cFileName);
  if (rc != cfs_errc_success)
    return rc;
  cfs_ignore_errc(cfs_dir_entry_refresh(&it->current, NULL));
  *out_entry = &it->current;
  return cfs_errc_success;
#else
  struct dirent *dp;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!it || !out_entry)
    return cfs_errc_invalid_argument;
  *out_entry = NULL;
  if (it->is_end)
    return cfs_errc_success;
  for (;;) {
    dp = readdir(it->dirp);
    if (!dp) {
      it->is_end = cfs_true;
      return cfs_errc_success;
    }
    if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
      break;
    }
  }
  rc = cfs_path_assign(&it->current.path, it->base_dir.str);
  if (rc != cfs_errc_success)
    return rc;
  rc = cfs_path_append(&it->current.path, dp->d_name);
  if (rc != cfs_errc_success)
    return rc;
  cfs_ignore_errc(cfs_dir_entry_refresh(&it->current, NULL));
  *out_entry = &it->current;
  return cfs_errc_success;
#endif
}

/*
 * \brief Performs the cfs_dir_itr_close filesystem operation.
 *
 * \param it Argument representing the target resource.
 */
CFS_API void cfs_dir_itr_close(cfs_directory_iterator *it) {
  if (!it)
    return;
#if defined(CFS_OS_WINDOWS)
  if (it->find_handle != INVALID_HANDLE_VALUE)
    FindClose(it->find_handle);
#else
  if (it->dirp)
    closedir(it->dirp);
#endif
  cfs_path_destroy(&it->base_dir);
  cfs_dir_entry_destroy(&it->current);
  cfs_free(it);
}

/*
 * \brief Opens a recursive directory iterator with explicit
 * `cfs_directory_options`.
 *
 * \param p Root directory path to traverse recursively.
 * \param opts Traversal options (`follow_directory_symlink`,
 * `skip_permission_denied`).
 * \param out_it Output recursive iterator pointer.
 * \param ec Optional output error code.
 * \return 0 on success, or non-zero on error.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_init_opts(
    const cfs_path *p, cfs_directory_options opts,
    cfs_recursive_directory_iterator **out_it, cfs_error_code *ec) {
  cfs_recursive_directory_iterator *it;
  cfs_directory_iterator *base_it;
  cfs_errc rc;
  cfs_clear_error(ec);
  if (out_it)
    *out_it = NULL;
  if (!p || !out_it)
    return cfs_errc_invalid_argument;
  rc = cfs_dir_itr_init_opts(p, opts, &base_it, ec);
  if (rc != cfs_errc_success)
    return rc;
  rc = cfs_malloc(sizeof(cfs_recursive_directory_iterator), (void **)&it);
  if (rc != cfs_errc_success) {
    cfs_dir_itr_close(base_it);
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
  rc = cfs_calloc(2, sizeof(cfs_directory_iterator *), (void **)&it->frames);
  if (rc != cfs_errc_success) {
    cfs_free(it);
    cfs_dir_itr_close(base_it);
    cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
    return rc;
  }
  it->capacity = 2;
  it->options = opts;
  it->recursion_pending = cfs_true;
  it->has_yielded = cfs_false;
  if (base_it->is_end) {
    cfs_dir_itr_close(base_it);
    it->depth = 0;
  } else {
    it->frames[0] = base_it;
    it->depth = 1;
  }
  *out_it = it;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_rec_dir_itr_init filesystem operation.
 *
 * \param p Pointer to the `cfs_path` object to evaluate or modify.
 * \param out_it Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_init(
    const cfs_path *p, cfs_recursive_directory_iterator **out_it,
    cfs_error_code *ec) {
  return cfs_rec_dir_itr_init_opts(p, cfs_directory_options_none, out_it, ec);
}

/*
 * \brief Performs the cfs_rec_dir_itr_next filesystem operation.
 *
 * \param it Argument representing the target resource.
 * \param out_entry Argument representing the target resource.
 * \param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors. \return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_next(
    cfs_recursive_directory_iterator *it, const cfs_directory_entry **out_entry,
    cfs_error_code *ec) {
  cfs_errc rc;
  cfs_clear_error(ec);
  if (!it || !out_entry)
    return cfs_errc_invalid_argument;
  *out_entry = NULL;

  if (it->depth > 0 && it->has_yielded && it->recursion_pending) {
    cfs_directory_iterator *top = it->frames[it->depth - 1];
    const cfs_directory_entry *prev = &top->current;
    cfs_bool descend = cfs_false;
    if (prev->symlink_status.type == cfs_file_type_directory) {
      descend = cfs_true;
    } else if (prev->symlink_status.type == cfs_file_type_symlink &&
               prev->status.type == cfs_file_type_directory &&
               ((int)it->options &
                (int)cfs_directory_options_follow_directory_symlink) != 0) {
      cfs_size_t k;
      cfs_bool cycle = cfs_false;
      for (k = 0; k < it->depth; ++k) {
        cfs_bool same = cfs_false;
        cfs_ignore_errc(
            cfs_equivalent(&prev->path, &it->frames[k]->base_dir, &same, NULL));
        if (same) {
          cycle = cfs_true;
          break;
        }
      }
      if (!cycle)
        descend = cfs_true;
    }
    if (descend) {
      cfs_directory_iterator *child_it = NULL;
      rc = cfs_dir_itr_init_opts(&prev->path, it->options, &child_it, ec);
      if (rc != cfs_errc_success)
        return rc;
      if (child_it->is_end) {
        cfs_dir_itr_close(child_it);
      } else {
        if (it->depth == it->capacity) {
          void *new_frames = NULL;
          cfs_size_t new_cap = it->capacity * 2;
          rc = cfs_realloc(it->frames,
                           new_cap * sizeof(cfs_directory_iterator *),
                           &new_frames);
          if (rc != cfs_errc_success) {
            cfs_dir_itr_close(child_it);
            cfs_set_error(ec, 0, cfs_errc_not_enough_memory);
            return rc;
          }
          it->frames = (cfs_directory_iterator **)new_frames;
          it->capacity = new_cap;
        }
        it->frames[it->depth++] = child_it;
      }
    }
  }

  it->recursion_pending = cfs_true;
  it->has_yielded = cfs_false;

  while (it->depth > 0) {
    const cfs_directory_entry *en = NULL;
    rc = cfs_dir_itr_next(it->frames[it->depth - 1], &en, ec);
    if (rc != cfs_errc_success)
      return rc;
    if (en) {
      it->has_yielded = cfs_true;
      it->recursion_pending = cfs_true;
      *out_entry = en;
      return cfs_errc_success;
    }
    it->depth--;
    cfs_dir_itr_close(it->frames[it->depth]);
    it->frames[it->depth] = NULL;
  }
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_rec_dir_itr_disable_recursion_pending filesystem
 * operation.
 *
 * \param it Argument representing the target resource.
 */
CFS_API void cfs_rec_dir_itr_disable_recursion_pending(
    cfs_recursive_directory_iterator *it) {
  if (it)
    it->recursion_pending = cfs_false;
}

/*
 * @brief Performs the cfs_rec_dir_itr_pop filesystem operation.
 *
 * @param it Argument representing the target resource.
 * @param ec Pointer to a `cfs_error_code` structure to store any operational
 * errors.
 * @return 0 on success, or a non-zero system error code on failure.
 */
CFS_NO_DISCARD CFS_API cfs_errc
cfs_rec_dir_itr_pop(cfs_recursive_directory_iterator *it, cfs_error_code *ec) {
  cfs_clear_error(ec);
  if (!it)
    return cfs_errc_invalid_argument;
  if (it->depth > 0) {
    it->depth--;
    cfs_dir_itr_close(it->frames[it->depth]);
    it->frames[it->depth] = NULL;
  }
  it->has_yielded = cfs_false;
  it->recursion_pending = cfs_true;
  return cfs_errc_success;
}

/*
 * @brief Queries the current recursion depth of a recursive directory iterator.
 *
 * @param it Pointer to the recursive directory iterator.
 * @param out_depth Output depth (`0` for root directory entries).
 * @return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_depth(
    const cfs_recursive_directory_iterator *it, int *out_depth) {
  if (!it || !out_depth)
    return cfs_errc_invalid_argument;
  *out_depth = (it->depth > 0) ? (int)(it->depth - 1) : 0;
  return cfs_errc_success;
}

/*
 * @brief Queries whether subdirectory recursion is currently enabled for the
 * active entry.
 *
 * @param it Pointer to the recursive directory iterator.
 * @param out_pending Output boolean flag.
 * @return 0 on success, or `cfs_errc_invalid_argument` if NULL.
 */
CFS_NO_DISCARD CFS_API cfs_errc cfs_rec_dir_itr_recursion_pending(
    const cfs_recursive_directory_iterator *it, cfs_bool *out_pending) {
  if (!it || !out_pending)
    return cfs_errc_invalid_argument;
  *out_pending = it->recursion_pending;
  return cfs_errc_success;
}

/*
 * \brief Performs the cfs_rec_dir_itr_close filesystem operation.
 *
 * \param it Argument representing the target resource.
 */
CFS_API void cfs_rec_dir_itr_close(cfs_recursive_directory_iterator *it) {
  cfs_size_t i;
  if (!it)
    return;
  for (i = 0; i < it->depth; ++i) {
    cfs_dir_itr_close(it->frames[i]);
  }
  cfs_free(it->frames);
  cfs_free(it);
}

#endif /* CFS_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CFS_H */
