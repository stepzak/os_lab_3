#ifndef RAII_H
#define RAII_H

#include <stdbool.h>
#include <stdlib.h>
#include "unlikely.h"

#if defined(__clang__) || defined(__GNUC__)

#define RAII_DETAIL_CAT_IMPL(left, right) left##right
#define RAII_DETAIL_CAT(left, right) RAII_DETAIL_CAT_IMPL(left, right)
#define RAII_DETAIL_SYMBOL(TypeName, suffix) \
    RAII_DETAIL_CAT(RAII_DETAIL_CAT(raii_detail_, TypeName), suffix)
#define RAII_STACK_TYPE(TypeName) RAII_DETAIL_CAT(stack_, TypeName)
#define RAII_HEAP_TYPE(TypeName) RAII_DETAIL_CAT(heap_, TypeName)

/*
 * ResetFunc must be infallible, must not inspect the previous value and must
 * leave the object in a canonical invalid state. CleanupFunc is called only
 * for a value accepted by IsValidFunc.
 */
#define DEFINE_RAII_TYPE(TypeName, BaseType, CleanupFunc, IsValidFunc, ResetFunc) \
    typedef BaseType TypeName;\
    static inline void RAII_DETAIL_SYMBOL(TypeName, _cleanup)(TypeName *ptr) { \
        if (unlikely(!ptr)) return; \
        if (likely(IsValidFunc(ptr))) { \
            CleanupFunc(ptr); \
            ResetFunc(ptr); \
        } \
    }

/* Use only when TypeName can be safely moved with ordinary assignment. */
#define DEFINE_RAII_TRIVIAL_MOVE(TypeName, IsValidFunc, ResetFunc) \
    static inline TypeName RAII_DETAIL_SYMBOL(TypeName, _take)(TypeName *source) { \
        TypeName val = {0}; \
        ResetFunc(&val); \
        if (likely(source && IsValidFunc(source))) { \
            val = *source; \
            ResetFunc(source); \
        } \
        return val; \
    } \
    static inline void RAII_DETAIL_SYMBOL(TypeName, _move_assign)( \
            TypeName *destination, TypeName *source) { \
        if (unlikely(!destination || destination == source)) return; \
        RAII_DETAIL_SYMBOL(TypeName, _cleanup)(destination); \
        if (likely(source && IsValidFunc(source))) { \
            *destination = *source; \
            ResetFunc(source); \
        } else { \
            ResetFunc(destination); \
        } \
    }

/*
 * CloneFunc must have the form:
 *     ErrorType clone(TypeName *destination, const TypeName *source)
 * destination must be distinct and contain no live resource (normally it is
 * produced by create()). On failure it must remain without a live resource,
 * so its automatic cleanup is always safe.
 */
#define DEFINE_RAII_CLONE(TypeName, CloneFunc) \
    static inline __typeof__(CloneFunc) * \
    RAII_DETAIL_SYMBOL(TypeName, _get_clone)(void) { \
        return CloneFunc; \
    }

#define DEFINE_RAII_INIT(BaseName, InitFunc) \
    static inline __typeof__(InitFunc) *RAII_DETAIL_SYMBOL(BaseName, _get_init)(void) { \
        return InitFunc; \
    } \

#define DEFINE_RAII_INIT_VALIDATION(BaseName, IsInitSuccessFunc) \
    static inline __typeof__(IsInitSuccessFunc) *RAII_DETAIL_SYMBOL(BaseName, _get_init_valid)(void) { \
        return IsInitSuccessFunc; \
    } \

#define init_succeeded(BaseName, result) (RAII_DETAIL_SYMBOL(BaseName, _get_init_valid)())(result)

#define init_failed(BaseName, result) !init_succeeded(BaseName, result)

#define DECLARE_SHARED_RAII_TYPES(BaseName, BaseType, \
    InitFunc, DestroyFunc, ValidFunc, ResetFunc) \
    DEFINE_RAII_TYPE(RAII_STACK_TYPE(BaseName), BaseType, DestroyFunc, ValidFunc, ResetFunc) \
    \
    DEFINE_RAII_INIT(BaseName, InitFunc) \
    static inline void RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _destroy)(BaseType **ptr) { \
        if (unlikely(!ptr || !*ptr)) return; \
        if (ValidFunc(*ptr)) { \
            DestroyFunc(*ptr); \
        } \
        free(*ptr); \
        *ptr = NULL; \
    } \
    static inline void RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _reset)(BaseType **ptr) { \
        if (unlikely(!ptr)) return; \
        *ptr = NULL; \
    } \
    static inline bool RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _valid)(BaseType **ptr) { \
        return ptr != NULL && *ptr != NULL; \
    } \
    DEFINE_RAII_TYPE(RAII_HEAP_TYPE(BaseName), BaseType*, \
                     RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _destroy), \
                     RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _valid), \
                     RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _reset)) \
    DEFINE_RAII_TRIVIAL_MOVE( \
        RAII_HEAP_TYPE(BaseName), \
        RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _valid), \
        RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _reset)) \
    \
    static inline RAII_STACK_TYPE(BaseName) \
    RAII_DETAIL_SYMBOL(RAII_STACK_TYPE(BaseName), _create_empty)(void) { \
        BaseType var = {0}; \
        ResetFunc(&var); \
        return var; \
    } \
    \
    static inline RAII_HEAP_TYPE(BaseName) \
    RAII_DETAIL_SYMBOL(RAII_HEAP_TYPE(BaseName), _create_empty)(void) { \
        BaseType *var = malloc(sizeof(*var)); \
        if (likely(var)) { \
            ResetFunc(var); \
        } \
        return var; \
    }

#define raii(TypeName) \
    TypeName __attribute__((cleanup(RAII_DETAIL_SYMBOL(TypeName, _cleanup))))

#define raii_stack(TypeName) raii(RAII_STACK_TYPE(TypeName))
#define raii_heap(TypeName) raii(RAII_HEAP_TYPE(TypeName))

#define create(TypeName) RAII_DETAIL_SYMBOL(TypeName, _create_empty)()
#define create_stack(BaseName) create(RAII_STACK_TYPE(BaseName))
#define create_heap(BaseName) create(RAII_HEAP_TYPE(BaseName))

#define init(BaseName, var, ...) \
    (RAII_DETAIL_SYMBOL(BaseName, _get_init)())( \
        _Generic(&(var), \
            RAII_STACK_TYPE(BaseName)*: &(var), \
            RAII_HEAP_TYPE(BaseName)*: (var) \
        ), \
        ##__VA_ARGS__ \
    )

#define drop(TypeName, var) RAII_DETAIL_SYMBOL(TypeName, _cleanup)(&(var))
#define drop_stack(BaseName, var) drop(RAII_STACK_TYPE(BaseName), var)
#define drop_heap(BaseName, var) drop(RAII_HEAP_TYPE(BaseName), var)

#define move(TypeName, var) RAII_DETAIL_SYMBOL(TypeName, _take)(&(var))
#define move_stack(BaseName, var) move(RAII_STACK_TYPE(BaseName), var)
#define move_heap(BaseName, var) move(RAII_HEAP_TYPE(BaseName), var)

/* ONLY FOR TRIVIAL MOVE. DO NOT USE WITH NON-TRIVIAL OR NON-MOVEABLE OBJECTS(e.g. Mutex) */
#define new_stack(BaseName, init_result_ptr, ...) \
({ \
    RAII_STACK_TYPE(BaseName) raii_new_value = create_stack(BaseName); \
    __auto_type raii_new_result = \
    init(BaseName, raii_new_value, ##__VA_ARGS__); \
    *(init_result_ptr) = raii_new_result; \
    if (init_failed(BaseName, raii_new_result)) { \
        drop_stack(BaseName, raii_new_value); \
    } \
    move_stack(BaseName, raii_new_value); \
})

#define new_heap(BaseName, init_result_ptr, ...) \
({ \
    RAII_HEAP_TYPE(BaseName) raii_new_value = create_heap(BaseName); \
    __auto_type raii_new_result = \
    init(BaseName, raii_new_value, ##__VA_ARGS__); \
    *(init_result_ptr) = raii_new_result; \
    if (init_failed(BaseName, raii_new_result)) { \
    drop_heap(BaseName, raii_new_value); \
    } \
    move_heap(BaseName, raii_new_value); \
})

#define move_assign(TypeName, destination, source) \
    RAII_DETAIL_SYMBOL(TypeName, _move_assign)(&(destination), &(source))
#define move_assign_stack(BaseName, destination, source) \
    move_assign(RAII_STACK_TYPE(BaseName), destination, source)
#define move_assign_heap(BaseName, destination, source) \
    move_assign(RAII_HEAP_TYPE(BaseName), destination, source)


#define try_clone(TypeName, destination, source) \
    (RAII_DETAIL_SYMBOL(TypeName, _get_clone)())(&(destination), &(source))
#define try_clone_stack(BaseName, destination, source) \
    try_clone(RAII_STACK_TYPE(BaseName), destination, source)
#define try_clone_heap(BaseName, destination, source) \
    try_clone(RAII_HEAP_TYPE(BaseName), destination, source)

#else
#error "This project requires GCC or Clang for RAII support"
#endif

#endif //RAII_H
