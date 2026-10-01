#ifndef UNLIKELY_H
#define UNLIKELY_H
#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)
#else

#define unlikely(x) (x)
#define likely(x)	(x)
#endif

#endif //UNLIKELY_H
