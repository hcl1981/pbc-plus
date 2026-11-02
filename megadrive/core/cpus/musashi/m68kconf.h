/* m68kconf.h -- Musashi configuration for the Mega Drive emulator (PBC).
 *
 * Musashi 4.10 (MIT, see readme.txt) reduced to a 68000; the switches that
 * are left are the ones m68kcpu.h and the opcode handlers still test. */

#ifndef M68KCONF__HEADER
#define M68KCONF__HEADER

#define M68K_OPT_OFF             0
#define M68K_OPT_ON              1
#define M68K_OPT_SPECIFY_HANDLER 2

/* 68000 only: every CPU_TYPE_IS_... test folds to a constant */
#define M68K_EMULATE_010            M68K_OPT_OFF
#define M68K_EMULATE_EC020          M68K_OPT_OFF
#define M68K_EMULATE_020            M68K_OPT_OFF
#define M68K_EMULATE_030            M68K_OPT_OFF
#define M68K_EMULATE_040            M68K_OPT_OFF
#define M68K_EMULATE_PMMU           M68K_OPT_OFF

/* Immediate and PC-relative reads go the fast way as well (m68kcpu.h) */
#define M68K_SEPARATE_READS         M68K_OPT_ON

#define M68K_SIMULATE_PD_WRITES     M68K_OPT_OFF

/* The VDP clears its interrupt when the CPU acknowledges it */
#define M68K_EMULATE_INT_ACK        M68K_OPT_ON

#define M68K_EMULATE_BKPT_ACK       M68K_OPT_OFF
#define M68K_EMULATE_TRACE          M68K_OPT_OFF
#define M68K_EMULATE_RESET          M68K_OPT_OFF
#define M68K_CMPILD_HAS_CALLBACK    M68K_OPT_OFF
#define M68K_RTE_HAS_CALLBACK       M68K_OPT_OFF

/* No TAS write-back: the Mega Drive's bus does not complete the
 * read-modify-write cycle (Gargoyles and Ex-Mutants depend on it). */
#define M68K_TAS_HAS_CALLBACK       M68K_OPT_SPECIFY_HANDLER
#define M68K_TAS_CALLBACK()         0

#define M68K_ILLG_HAS_CALLBACK      M68K_OPT_OFF
#define M68K_TRAP_HAS_CALLBACK      M68K_OPT_OFF
#define M68K_EMULATE_FC             M68K_OPT_OFF
#define M68K_MONITOR_PC             M68K_OPT_OFF
#define M68K_INSTRUCTION_HOOK       M68K_OPT_OFF
#define M68K_EMULATE_PREFETCH       M68K_OPT_OFF
#define M68K_EMULATE_ADDRESS_ERROR  M68K_OPT_OFF
#define M68K_LOG_ENABLE             M68K_OPT_OFF
#define M68K_LOG_1010_1111          M68K_OPT_OFF
#define M68K_USE_64_BIT             M68K_OPT_OFF

#endif /* M68KCONF__HEADER */
