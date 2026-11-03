/*
 * QuickNES - NES emulator core
 * Mapper 185 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "nes_mapper.h"

// https://www.nesdev.org/wiki/INES_Mapper_185
//
// CNROM with a lock on the CHR ROM (B-Wings, Bird Week, Mighty Bomb Jack,
// Spy vs. Spy): the games write a value that disables the CHR and check
// that the PPU then reads garbage. Which values enable it differs from
// board to board; as FCEUX: enabled when bits 0-1 are not both 0 and the
// value is not $13. Disabled, $2007 reads $FF.

class Mapper185 : public Nes_Mapper {
	uint8_t bank;
public:
	Mapper185()
	{
		register_state( &bank, 1 );
	}

	virtual void reset_state() { bank = 0x03; }

	virtual void apply_mapping()
	{
		emu().ppu.chr_off = !((bank & 3) && bank != 0x13);
	}

	virtual void write( nes_time_t, nes_addr_t addr, int data )
	{
		bank = handle_bus_conflict( addr, data );
		apply_mapping();
	}
};
