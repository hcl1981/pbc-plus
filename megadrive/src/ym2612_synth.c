/* ym2612_synth.c -- core 1's YM2612.
 *
 * core/sound/ym2612.c built a second time under other names. Core 0 keeps
 * its own copy for what the game can read back (timers, status) and logs
 * every register write; core 1 replays the log into this copy and does the
 * FM synthesis (ym_core1.c). The read-only tables are shared. */
#define PBC_YM_SYNTH 1

#define YM2612Init                 synth_YM2612Init
#define YM2612Config               synth_YM2612Config
#define YM2612ResetChip            synth_YM2612ResetChip
#define YM2612Write                synth_YM2612Write
#define YM2612Read                 synth_YM2612Read
#define YM2612LoadRegs             synth_YM2612LoadRegs
#define YM2612SaveRegs             synth_YM2612SaveRegs
#define ym2612_run                 synth_ym2612_run
#define gwenesis_ym2612_load_state synth_ym2612_load_state
#define gwenesis_ym2612_save_state synth_ym2612_save_state
#define ym2612_channel_enabled     synth_ym2612_channel_enabled
#define ym2612_dac_enabled         synth_ym2612_dac_enabled
#define ym2612_fm_enabled          synth_ym2612_fm_enabled
#define ym_keyon_nonzero           synth_ym_keyon_nonzero
#define ym_read_last_status        synth_ym_read_last_status
#define ym_reads_total             synth_ym_reads_total
#define ym_reg_hist                synth_ym_reg_hist
#define ym_writes_dac              synth_ym_writes_dac
#define ym_writes_keyon            synth_ym_writes_keyon
#define ym_writes_total            synth_ym_writes_total
#define ym2612_clock               synth_ym2612_clock
#define ym2612_index               synth_ym2612_index
#define gwenesis_ym2612_buffer     synth_ym2612_buffer

#include "../core/sound/ym2612.c"
