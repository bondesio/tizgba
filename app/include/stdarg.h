/* Freestanding stdarg.h: the compiler builtins give the right va_list for
   both the 32-bit ARM TV build (AAPCS) and the x86-64 host harness. */
#ifndef TIZGBA_STDARG_H
#define TIZGBA_STDARG_H
typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, t)   __builtin_va_arg(v, t)
#define va_copy(d, s)  __builtin_va_copy(d, s)
#endif
