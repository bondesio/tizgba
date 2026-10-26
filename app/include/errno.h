#ifndef TIZGBA_ERRNO_H
#define TIZGBA_ERRNO_H
extern int *__errno_location(void);
#define errno (*__errno_location())
#define ENOENT 2
#define EINTR  4
#define EIO    5
#define ENOMEM 12
#define EINVAL 22
#endif
