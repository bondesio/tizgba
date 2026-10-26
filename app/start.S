    .syntax unified
    .text
    .align 2
    .global _start
    .type _start, %function
_start:
    mov r7, sp
    ldr r1, [r7]
    add r2, r7, #4
    ldr r0, =main
    ldr r3, =tizgba_run_init_array
    sub sp, sp, #16
    mov r12, #0
    str r12, [sp, #0]
    str r12, [sp, #4]
    str r7, [sp, #8]
    bl __libc_start_main
1:  b 1b
