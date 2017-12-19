#ifndef POBLADO_TEST_COMMON_H
#define POBLADO_TEST_COMMON_H

#include <unistd.h>
#include <string.h>

extern uint64_t START_TIME;

#define ASSERT(expr)                                      \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') \
    ? __builtin_strrchr(__FILE__, '/') + 1 \
    : __FILE__)

#define poblado_log(msg)                              \
  do {                                                \
    uint64_t time = (uv_hrtime() / 1E6) - START_TIME; \
    fprintf(stderr,                                   \
            "[%05lld](%s:%d)[0x%lx] %s\n",            \
            time,                                     \
            __FILENAME__,                             \
            __LINE__,                                 \
            (unsigned long int) uv_thread_self(),     \
            msg);                                     \
  } while (0)

#endif
