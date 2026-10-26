/* mGBA's config code creates directories; the frontend owns paths, so mkdir
   is disabled by the app port layer (app/port/tiz_port.c). */
#ifndef TIZGBA_SYS_STAT_H
#define TIZGBA_SYS_STAT_H
int tizgba_mkdir(const char *, unsigned int);
#define mkdir tizgba_mkdir
#endif
