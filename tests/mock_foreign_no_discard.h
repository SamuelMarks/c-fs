/**
 * @file mock_foreign_no_discard.h
 * @brief Simulated foreign `no_discard.h` header using generic `NO_DISCARD_H`
 * guard and `NO_DISCARD_TYPE` / `NO_DISCARD_TYPEDEF` macros.
 */

#ifndef NO_DISCARD_H
#define NO_DISCARD_H

/* clang-format off */
#if defined(_MSC_VER) && _MSC_VER >= 1700
#include <sal.h>
#endif
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef NO_DISCARD
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/** @brief Foreign nodiscard macro. */
#define NO_DISCARD [[nodiscard]]
#elif defined(__cplusplus) && __cplusplus >= 201703L
/** @brief Foreign nodiscard macro. */
#define NO_DISCARD [[nodiscard]]
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
/** @brief Foreign nodiscard macro. */
#define NO_DISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Foreign nodiscard macro. */
#define NO_DISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
/** @brief Foreign nodiscard macro. */
#define NO_DISCARD _Check_return_
#else
/** @brief Foreign nodiscard macro. */
#define NO_DISCARD
#endif
#endif /* !NO_DISCARD */

#ifndef NO_DISCARD_TYPE
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/** @brief Foreign nodiscard type macro. */
#define NO_DISCARD_TYPE [[nodiscard]]
#elif defined(__cplusplus) && __cplusplus >= 201703L
/** @brief Foreign nodiscard type macro. */
#define NO_DISCARD_TYPE [[nodiscard]]
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
/** @brief Foreign nodiscard type macro. */
#define NO_DISCARD_TYPE [[nodiscard]]
#else
/** @brief Foreign nodiscard type macro. */
#define NO_DISCARD_TYPE
#endif
#endif /* !NO_DISCARD_TYPE */

#ifndef NO_DISCARD_TYPEDEF
#if !defined(__cplusplus) && defined(__clang__)
/** @brief Foreign nodiscard typedef macro. */
#define NO_DISCARD_TYPEDEF __attribute__((warn_unused_result))
#else
/** @brief Foreign nodiscard typedef macro. */
#define NO_DISCARD_TYPEDEF
#endif
#endif /* !NO_DISCARD_TYPEDEF */

/** @brief Simulated foreign error enum using NO_DISCARD_TYPE and TYPEDEF. */
typedef enum NO_DISCARD_TYPE mock_foreign_err {
  /** @brief Success status. */
  MOCK_FOREIGN_OK = 0,
  /** @brief Failure status. */
  MOCK_FOREIGN_FAIL = 1
} mock_foreign_err_t NO_DISCARD_TYPEDEF;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NO_DISCARD_H */
