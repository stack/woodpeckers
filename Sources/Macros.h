//
//  Macros.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on on 2020-11-27.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#ifndef MACROS_H
#define MACROS_H

// Feature and builtin compatibility
#if !defined(__has_attribute)
#define __has_attribute(x) 0
#endif

#if !defined(__has_builtin)
#define __has_builtin(x) 0
#endif

#if !defined(__has_extension)
#define __has_extension(x) 0
#endif

#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

// Convenience macros for exporting C symbols
#ifdef __cplusplus
#define BEGIN_DECLS extern "C" {
#define END_DECLS   }
#else
#define BEGIN_DECLS
#define END_DECLS
#endif

// Property annotations
#if __has_feature(nullability)
#define NULLABLE _Nullable
#define NONNULL _Nonnull
#define NULL_UNSPECIFIED _Null_unspecified
#elif defined(__GNUC__)
#define NULLABLE
#define NONNULL
#define NULL_UNSPECIFIED
#else
#define NULLABLE
#define NONNULL
#define NULL_UNSPECIFIED
#endif

// Swift annotations
#if __has_attribute(enum_extensibility)
#define SWIFT_ENUM __attribute__((enum_extensibility(open)))
#else
#define SWIFT_ENUM
#endif

#if __has_attribute(swift_name)
#define SWIFT_NAME(_name) __attribute__((swift_name(#_name)))
#else
#define SWIFT_NAME(_name)
#endif

#if __has_attribute(swift_newtype)
#define SWIFT_NEWTYPE(_type) __attribute__((swift_newtype(_type)))
#else
#define SWIFT_NEWTYPE(_type)
#endif

// Memory Safety
#if defined(_WIN32)
#ifdef __cplusplus
#define SAFE_DESTROY(O, R) {             \
if ((O) != nullptr) {                       \
    decltype(O) __safeDestroyTemp = (O); \
    O = nullptr;                            \
    R(__safeDestroyTemp);                \
  }                                      \
}
#else
#define SAFE_DESTROY(O, R) {       \
if ((O) != NULL) {                 \
    void *__safeDestroyTemp = (O); \
    O = NULL;                      \
    R(__safeDestroyTemp);          \
  }                                \
}
#endif
#else
#define SAFE_DESTROY(O, R) {                 \
if ((O) != NULL) {                           \
    __typeof__((O)) __safeDestroyTemp = (O); \
    O = NULL;                                \
    R(__safeDestroyTemp);                    \
  }                                          \
}
#endif

#endif /* MACROS_H */
