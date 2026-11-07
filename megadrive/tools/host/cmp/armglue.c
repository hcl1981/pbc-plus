/* armglue.c: bare-metal entry for hostcmp on the ARM emulator (unicorn) */
#include <string.h>
#include <stdint.h>
extern char arm_args[256];
int main(int argc, char **argv);
volatile int arm_done;
void _start(void)
{
    extern char __bss_start__[], __bss_end__[];
    /* bss: the emulator maps zeroed memory and has put the ROM in it already */
    static char *argv[16];
    static char buf[256];
    memcpy(buf, arm_args, 256);
    int argc = 0;
    char *p = buf;
    while (*p && argc < 15) {
        while (*p == ' ') *p++ = 0;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
    }
    argv[argc] = 0;
    main(argc, argv);
    arm_done = 1;
    for (;;) __asm volatile("bkpt #0");
}
void *_sbrk(int incr) { extern char end[]; static char *h; if (!h) h = end; char *o = h; h += incr; return o; }
int _write(int f, const char *b, int n) { (void)f; (void)b; return n; }
int _read(int f, char *b, int n) { (void)f; (void)b; (void)n; return 0; }
int _close(int f) { return -1; }
int _lseek(int f, int o, int w) { return 0; }
int _fstat(int f, void *s) { return -1; }
int _isatty(int f) { return 1; }
int _open(const char *n, int f, int m) { return -1; }
void _exit(int c) { for (;;) __asm volatile("bkpt #1"); }
int _kill(int p, int s) { return -1; }
int _getpid(void) { return 1; }
int _gettimeofday(void *tv, void *tz) { return -1; }
extern int arm_cur;
void __attribute__((noinline)) arm_mark(int k) { arm_cur = k; __asm volatile("" ::: "memory"); }
void __attribute__((noinline)) arm_frame(int fr) { (void)fr; __asm volatile("" ::: "memory"); }
