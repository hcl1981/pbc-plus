/*
 * QuickNES - NES emulator core
 * Mapper 28 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "nes_mapper.h"

// https://www.nesdev.org/wiki/Action_53
//
// Action 53, the homebrew multicart mapper; a game on it may be NROM,
// CNROM, UNROM, BNROM or AOROM like. $5000-$5FFF selects a register
// ($00 CHR, $01 inner PRG bank, $80 mode, $81 outer PRG bank), $8000-$FFFF
// writes it. Mode: bits 0-1 mirroring (one screen A/B, vertical,
// horizontal), bits 2-3 PRG layout (32 KB, or 16 KB with $8000 or $C000
// fixed to the outer bank), bits 4-5 game size (32 to 256 KB). In the one
// screen modes, bit 4 of a CHR or inner bank write picks the screen.
// Only 8 KB of CHR RAM here: the CHR bank bits are not used.

class Mapper028 : public Nes_Mapper {
	uint8_t regs [4];               // CHR, inner, mode, outer
	uint8_t select;
public:
	Mapper028()
	{
		register_state( regs, sizeof regs );
	}

	virtual void reset_state()
	{
		regs [2] = 0x00;                // 32 KB layout, 32 KB game
		regs [3] = 0xFF;                // the last 32 KB: the menu's vectors
		select = 0;
		intercept_writes( 0x5000, 0x1000 );
	}

	int prg_bank( int a14 )             // 16 KB bank for $8000 (0) or $C000 (1)
	{
		static const uint8_t masks [4] = { 0x01, 0x03, 0x07, 0x0F };
		int layout = regs [2] >> 2 & 3;
		int outer = regs [3] << 1;
		if ( ((layout ^ a14) & 3) == 2 )    // the fixed half
			return outer | a14;
		int inner = (layout & 2) ? regs [1] : (regs [1] << 1 | a14);
		int mask = masks [regs [2] >> 4 & 3];
		return (outer & ~mask) | (inner & mask);
	}

	virtual void apply_mapping()
	{
		set_prg_bank( 0x8000, bank_16k, prg_bank( 0 ) );
		set_prg_bank( 0xC000, bank_16k, prg_bank( 1 ) );
		switch ( regs [2] & 3 )
		{
		case 0: mirror_single( 0 ); break;
		case 1: mirror_single( 1 ); break;
		case 2: mirror_vert(); break;
		case 3: mirror_horiz(); break;
		}
	}

	virtual bool write_intercepted( nes_time_t, nes_addr_t addr, int data )
	{
		if ( addr < 0x5000 || addr >= 0x6000 )
			return false;
		select = data & 0x81;
		return true;
	}

	virtual void write( nes_time_t, nes_addr_t, int data )
	{
		int r = (select & 1) | (select >> 6);   // $00 $01 $80 $81 -> 0 1 2 3
		if ( r < 2 && !(regs [2] & 2) )         // one screen: bit 4 picks it
			regs [2] = (regs [2] & ~1) | (data >> 4 & 1);
		regs [r] = data;
		apply_mapping();
	}
};
