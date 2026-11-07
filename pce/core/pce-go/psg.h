#pragma once

#include <stdint.h>
#include <stddef.h>

int psg_init(int samplerate, bool stereo);
void psg_term(void);
void psg_update(int16_t *output, size_t length, uint32_t channels);
/* PBC: (left + right) / 2 of psg_update's stereo, cheaper; see psg.c */
#define PSG_MONO_MAX 800
void psg_update_mono(int16_t *output, size_t length);
