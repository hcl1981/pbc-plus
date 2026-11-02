/* cpuhook.h -- for the old 68000 core's HOOK_CPU: tools/host/host.c traces
 * every instruction (MDHOST_TRACE) to compare it with the new core. */
#define HOOK_M68K_E 0
#define HOOK_M68K_R 1
#define HOOK_M68K_W 2
extern void (*cpu_hook)(int type, int width, unsigned int address, unsigned int value);
