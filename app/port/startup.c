/* mGBA registers log categories with GCC constructors. The reference GB
 * startup passes a null init callback, which leaves those categories at zero.
 * Run the linked init array before main, matching the ARM libc startup ABI. */
typedef void (*init_fn)(void);

extern init_fn __init_array_start[];
extern init_fn __init_array_end[];

void tizgba_run_init_array(void)
{
    for (init_fn *fn = __init_array_start; fn < __init_array_end; ++fn) {
        if (*fn) {
            (*fn)();
        }
    }
}
