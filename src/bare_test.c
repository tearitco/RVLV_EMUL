/* Minimal bare-metal test for rvemu - no libc, no musl */
#include <stdint.h>

#define SYS_EXIT 93

static inline uint64_t syscall1(uint64_t a7, uint64_t a0) {
    register uint64_t r10 __asm__("a0") = a0;
    register uint64_t r17 __asm__("a7") = a7;
    __asm__ volatile ("ecall" : "+r"(r10) : "r"(r17) : "memory");
    return r10;
}

void _start(void) {
    /* Exit with code 42 */
    syscall1(SYS_EXIT, 42);
    for(;;);
}