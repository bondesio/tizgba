/* The core needs a small set of float helpers for BIOS affine maths and the
   optional e-Reader path. sqrtf/fabsf/fmaxf are compiler builtins; the other
   functions resolve from the Q60A's own libm.so.6. */
#ifndef TIZGBA_MATH_H
#define TIZGBA_MATH_H
#define M_PI 3.14159265358979323846
float sinf(float);
float cosf(float);
float exp2f(float);
float hypotf(float, float);
static inline float sqrtf(float x) { return __builtin_sqrtf(x); }
static inline float fabsf(float x) { return __builtin_fabsf(x); }
static inline float fmaxf(float a, float b) { return a > b ? a : b; }
static inline float fminf(float a, float b) { return a < b ? a : b; }
#endif
