/* Memory-formatting only. The core never touches FILE streams, the console
   or scanf; mGBA still has code paths (default stdout logger, screenshot file
   numbering, inih's file reader) that name those functions, so they are
   renamed here to always-failing stubs in app/port/tiz_port.c instead
   of adding libc.so.6 dependencies (stdio/locale/FILE) to the TV binary. */
#ifndef TIZGBA_STDIO_H
#define TIZGBA_STDIO_H
#include <stddef.h>
#include <stdarg.h>

typedef struct tizgba_file FILE;

int snprintf(char *, size_t, const char *, ...) __attribute__((format(printf, 3, 4)));
int sprintf(char *, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int printf(const char *, ...) __attribute__((format(printf, 1, 2)));
int vprintf(const char *, va_list) __attribute__((format(printf, 1, 0)));

int   tizgba_sscanf(const char *, const char *, ...);
FILE *tizgba_fopen(const char *, const char *);
int   tizgba_fclose(FILE *);
char *tizgba_fgets(char *, int, FILE *);
#define sscanf  tizgba_sscanf
#define fopen   tizgba_fopen
#define fclose  tizgba_fclose
#define fgets   tizgba_fgets
#endif
