/* Tizen-only stubs for mGBA paths not used by the app frontend.
 *
 * ROMs, saves, diagnostics and configuration are owned by the TizGBA
 * frontend. These symbols keep optional command-line/config-file helpers
 * from reading files or creating implicit directories. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int tizgba_sscanf(const char *input, const char *format, ...)
{
    (void)input;
    (void)format;
    return 0;
}

FILE *tizgba_fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return 0;
}

int tizgba_fclose(FILE *stream)
{
    (void)stream;
    return 0;
}

char *tizgba_fgets(char *buffer, int size, FILE *stream)
{
    (void)buffer;
    (void)size;
    (void)stream;
    return 0;
}

char *tizgba_getenv(const char *name)
{
    (void)name;
    return 0;
}

int tizgba_mkdir(const char *path, unsigned int mode)
{
    (void)path;
    (void)mode;
    return -1;
}
