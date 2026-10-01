#ifndef RAII_H
#define RAII_H

#include "unlikely.h"

#if defined(__clang__) || defined(__GNUC__)

#define DEFINE_RAII_TYPE(TypeName, BaseType, CleanupFunc, IsValidFunc, ResetFunc, ConstructorFunc, CloneFunc) \
    typedef BaseType TypeName;\
    static inline void __##TypeName##_cleanup(TypeName *ptr) { \
        if (unlikely(!ptr)) return; \
        if (likely(IsValidFunc(ptr))) { \
            CleanupFunc(ptr); \
            ResetFunc(ptr); \
        } \
    } \
    \
    static inline TypeName __##TypeName##_move_out(TypeName *ptr) { \
        TypeName val; \
        if (unlikely(!ptr || !IsValidFunc(ptr))) { \
            ResetFunc(&val); \
            return val; \
        } \
        val = *ptr; \
        ResetFunc(ptr); \
        return val; \
    } \
    \
    static inline __attribute__((unused)) typeof(ConstructorFunc)* __##TypeName##_get_constructor() { \
        return ConstructorFunc; \
    } \
    static inline TypeName __##TypeName##_clone(const TypeName *ptr) { \
        _Static_assert((void*)(CloneFunc) != NULL, "Cloning is not implemented for " #TypeName); \
        if (unlikely(!ptr || !IsValidFunc(ptr))) { \
            TypeName val; \
            ResetFunc(&val); \
            return val; \
        } \
        return CloneFunc(ptr); \
    }


#define raii(TypeName) TypeName __attribute__((cleanup(__##TypeName##_cleanup)))

#define create(TypeName, ...) (__##TypeName##_get_constructor())(__VA_ARGS__)
#define drop(TypeName, var) __##TypeName##_cleanup(&(var))
#define clone(TypeName, var) __##TypeName##_clone(&(var))
#define move(TypeName, var) __##TypeName##_move_out(&(var))


#else
#error "This project requires GCC or Clang for RAII support"

#endif

#endif //RAII_H
