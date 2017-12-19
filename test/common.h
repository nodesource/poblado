#ifndef POBLADO_TEST_COMMON_H
#define POBLADO_TEST_COMMON_H

#include <unistd.h>
#include <string.h>
#include <ctype.h>

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

inline char* scopy(const char* s) {
  size_t len = strlen(s);
  char* cpy = new char[len + 1]();
  strncpy(cpy, s, len + 1);
  return cpy;
}

void inline uv_sleep(int msec) {
  int sec;
  int usec;

  sec = msec / 1000;
  usec = (msec % 1000) * 1000;
  if (sec > 0)
    sleep(sec);
  if (usec > 0)
    usleep(usec);
}

inline const char* string_toupper(const char* s) {
  const size_t len = strlen(s);
  char* upper = new char[len + 1]();
  for (size_t i = 0; i < len; i++) {
    upper[i] = toupper(s[i]);
  }
  upper[len] = '\0';
  return upper;
}

#endif
