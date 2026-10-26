/* Only the open() flag constants (Linux, identical on ARM and x86-64) that
   mGBA's VFile API takes as a parameter. The core build never opens files. */
#ifndef TIZGBA_FCNTL_H
#define TIZGBA_FCNTL_H
#define O_ACCMODE 3
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   0x40
#define O_TRUNC   0x200
#define O_APPEND  0x400
#endif
