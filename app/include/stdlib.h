/* Only what the vendored cores need; resolved from the TV's libc.so.6. */
#ifndef TIZGBA_STDLIB_H
#define TIZGBA_STDLIB_H
#include <stddef.h>
void abort(void) __attribute__((noreturn));
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void  free(void *);
long          strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
long long     strtoll(const char *, char **, int);
float         strtof(const char *, char **);
/* mGBA reads XDG_CONFIG_HOME etc. for its config dir; the core has none. */
char *tizgba_getenv(const char *);
#define getenv tizgba_getenv
typedef struct { int quot, rem; } div_t;
static inline div_t div(int n, int d) { div_t r; r.quot = n / d; r.rem = n % d; return r; }
static inline int abs(int v) { return v < 0 ? -v : v; }
#endif
