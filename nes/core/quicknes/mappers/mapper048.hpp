/*
 * QuickNES - NES emulator core
 * Mapper 48 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "mapper004.hpp"

// https://www.nesdev.org/wiki/INES_Mapper_048
//
// Taito TC0690 (The Flintstones: The Rescue of Dino & Hoppy, Don Doko
// Don 2, Bubble Bobble 2 in Japan): the banks of the TC0190 (mapper 33),
// mirroring at $E000 and a scanline counter that counts up from $C000's
// value and interrupts when it overflows. The MMC3's counter does the
// counting here, loaded with the number of lines left.
//
// mmc3_state_t is reused: banks [6], [7] PRG at $8000, $A000; banks [0],
// [1] 2 KB CHR; banks [2..5] 1 KB CHR.

class Mapper048 : public Mapper004 {
	uint8_t latch;                  // as written to $C000
public:
	virtual void reset_state()
	{
		Mapper004::reset_state();
		latch = 0;
		mirror = 0;
	}

	void set_mirroring()
	{
		if ( mirror )
			mirror_horiz();
		else
			mirror_vert();
	}

	virtual void apply_mapping()
	{
		set_mirroring();
		update_chr_banks();
		update_prg_banks();
		start_frame();
	}

	virtual void update_chr_banks()
	{
		set_chr_bank( 0x0000, bank_2k, banks [0] );
		set_chr_bank( 0x0800, bank_2k, banks [1] );
		for ( int i = 0; i < 4; i++ )
			set_chr_bank( 0x1000 + i * 0x400, bank_1k, banks [2 + i] );
	}

	virtual void update_prg_banks()
	{
		set_prg_bank( 0x8000, bank_8k, banks [6] );
		set_prg_bank( 0xA000, bank_8k, banks [7] );
		set_prg_bank( 0xC000, bank_8k, last_bank - 1 );
		set_prg_bank( 0xE000, bank_8k, last_bank );
	}

	virtual void write( nes_time_t time, nes_addr_t addr, int data )
	{
		switch ( addr & 0xE003 )
		{
		case 0x8000: banks [6] = data & 0x3F; update_prg_banks(); break;
		case 0x8001: banks [7] = data & 0x3F; update_prg_banks(); break;
		case 0x8002: banks [0] = data; update_chr_banks(); break;
		case 0x8003: banks [1] = data; update_chr_banks(); break;
		case 0xA000: case 0xA001: case 0xA002: case 0xA003:
			banks [2 + (addr & 3)] = data;
			update_chr_banks();
			break;

		case 0xC000:
			latch = data;
			break;

		case 0xC001:                    // counter = $C000; lines to the overflow
			run_until( time );
			irq_ctr = (uint8_t) (0x100 - latch);
			irq_latch = irq_ctr;
			irq_changed();
			break;

		case 0xC002:
			run_until( time );
			irq_enabled = true;
			irq_changed();
			break;

		case 0xC003:
			run_until( time );
			irq_enabled = false;
			irq_flag = false;
			irq_changed();
			break;

		case 0xE000:
			mirror = data >> 6 & 1;
			set_mirroring();
			break;
		}
	}
};
