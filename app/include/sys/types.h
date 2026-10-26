#ifndef TIZGBA_SYS_TYPES_H
#define TIZGBA_SYS_TYPES_H
#include <stddef.h>
typedef long ssize_t;     /* 32-bit on the ARM TV, 64-bit on the host: same width as long */
typedef long off_t;       /* glibc default (no _FILE_OFFSET_BITS): long */
#endif
