/* printf format macros for the fixed-width types (glibc/Linux values). */
#ifndef TIZGBA_INTTYPES_H
#define TIZGBA_INTTYPES_H
#include <stdint.h>
#define PRId8  "d"
#define PRIu8  "u"
#define PRIx8  "x"
#define PRId16 "d"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRIXPTR "lX"
#define PRIxPTR "lx"
#if __SIZEOF_LONG__ == 8
#define PRId64 "ld"
#define PRIu64 "lu"
#define PRIx64 "lx"
#else
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#endif
#endif
