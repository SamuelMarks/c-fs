/**
 * @file no_discard.h
 * @brief Cross-platform compiler macros for nodiscard and warn_unused_result
 * attributes.
 *
 * This header provides namespaced `CFS_NO_DISCARD*` macros for `c-fs` public
 * APIs along with fallback `NO_DISCARD*` definitions so `c-fs` coexists cleanly
 * with `cdd-c`, `c-orm`, `c-abstract-http`, and `c-rest-framework` regardless
 * of header inclusion order.
 */

#ifndef CFS_NO_DISCARD_H
#define CFS_NO_DISCARD_H

/* clang-format off */
#if defined(_MSC_VER) && _MSC_VER >= 1700
#include <sal.h>
#endif
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CFS_NO_DISCARD
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/** @brief Marks a function or type so discarding its return value warns. */
#define CFS_NO_DISCARD [[nodiscard]]
#elif defined(__cplusplus) && __cplusplus >= 201703L
/** @brief Marks a function or type so discarding its return value warns. */
#define CFS_NO_DISCARD [[nodiscard]]
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
/** @brief Marks a function or type so discarding its return value warns. */
#define CFS_NO_DISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Marks a function so discarding its return value warns. */
#define CFS_NO_DISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
/** @brief Marks a function so discarding its return value warns under SAL. */
#define CFS_NO_DISCARD _Check_return_
#else
/** @brief Fallback nodiscard annotation for compilers without support. */
#define CFS_NO_DISCARD
#endif
#endif /* !CFS_NO_DISCARD */

#ifndef CFS_NO_DISCARD_TYPE
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/** @brief Marks an enum or struct declaration as nodiscard on C23+. */
#define CFS_NO_DISCARD_TYPE [[nodiscard]]
#elif defined(__cplusplus) && __cplusplus >= 201703L
/** @brief Marks an enum or struct declaration as nodiscard on C++17+. */
#define CFS_NO_DISCARD_TYPE [[nodiscard]]
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
/** @brief Marks an enum or struct declaration as nodiscard on MSVC C++17+. */
#define CFS_NO_DISCARD_TYPE [[nodiscard]]
#else
/** @brief Fallback type-level nodiscard annotation. */
#define CFS_NO_DISCARD_TYPE
#endif
#endif /* !CFS_NO_DISCARD_TYPE */

#ifndef CFS_NO_DISCARD_TYPEDEF
#if !defined(__cplusplus) && defined(__clang__)
/** @brief Marks a typedef with warn_unused_result on Clang C mode. */
#define CFS_NO_DISCARD_TYPEDEF __attribute__((warn_unused_result))
#else
/** @brief Fallback typedef-level nodiscard annotation. */
#define CFS_NO_DISCARD_TYPEDEF
#endif
#endif /* !CFS_NO_DISCARD_TYPEDEF */

#ifndef NO_DISCARD
/** @brief Fallback generic nodiscard function annotation. */
#define NO_DISCARD CFS_NO_DISCARD
#endif /* !NO_DISCARD */

#ifndef NO_DISCARD_TYPE
/** @brief Fallback generic nodiscard type annotation. */
#define NO_DISCARD_TYPE CFS_NO_DISCARD_TYPE
#endif /* !NO_DISCARD_TYPE */

#ifndef NO_DISCARD_TYPEDEF
/** @brief Fallback generic nodiscard typedef annotation. */
#define NO_DISCARD_TYPEDEF CFS_NO_DISCARD_TYPEDEF
#endif /* !NO_DISCARD_TYPEDEF */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CFS_NO_DISCARD_H */
