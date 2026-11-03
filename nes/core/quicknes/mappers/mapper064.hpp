/*
 * QuickNES - NES emulator core
 * Mapper 64 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "mapper004.hpp"

// https://www.nesdev.org/wiki/RAMBO-1
//
// Tengen RAMBO-1 (Klax, Shinobi, Rolling Thunder, Skull & Crossbones):
// an MMC3 relative with a third switchable 8 KB PRG bank (R15), an
// optional 1 KB mode for the low CHR half (R8, R9) and an IRQ counter that
// counts either scanlines like the MMC3 (whose timing is used here) or
// CPU cycles divided by four. No PRG RAM.

class Mapper064 : public Mapper004 {
	uint8_t r8, r9, r15;
	uint8_t cycle_mode;             // $C001 bit 0
	uint8_t prescaler;              // CPU cycles towards the next count
	nes_time_t cycle_time;          // CPU time counted up to
public:
	virtual void reset_state()
	{
		Mapper004::reset_state();
		r8 = r9 = r15 = 0;
		cycle_mode = prescaler = 0;
		cycle_time = 0;
	}

	virtual void apply_mapping()
	{
		if ( !(cart().mirroring() & 0x08) )
		{
			if ( mirror & 1 )
				mirror_horiz();
			else
				mirror_vert();
		}
		update_chr_banks();
		update_prg_banks();
		start_frame();
	}

	virtual void update_chr_banks()
	{
		int x = (mode >> 7 & 1) * 0x1000;
		if ( mode & 0x20 )
		{
			set_chr_bank( 0x0000 ^ x, bank_1k, banks [0] );
			set_chr_bank( 0x0400 ^ x, bank_1k, r8 );
			set_chr_bank( 0x0800 ^ x, bank_1k, banks [1] );
			set_chr_bank( 0x0C00 ^ x, bank_1k, r9 );
		}
		else
		{
			set_chr_bank( 0x0000 ^ x, bank_2k, banks [0] >> 1 );
			set_chr_bank( 0x0800 ^ x, bank_2k, banks [1] >> 1 );
		}
		for ( int i = 0; i < 4; i++ )
			set_chr_bank( (0x1000 + i * 0x400) ^ x, bank_1k, banks [2 + i] );
	}

	virtual void update_prg_banks()
	{
		bool swap = mode & 0x40;
		set_prg_bank( 0x8000, bank_8k, swap ? r15 : banks [6] );
		set_prg_bank( 0xA000, bank_8k, swap ? banks [6] : banks [7] );
		set_prg_bank( 0xC000, bank_8k, swap ? banks [7] : r15 );
		set_prg_bank( 0xE000, bank_8k, last_bank );
	}

	// IRQ in CPU-cycle mode: one count every four cycles

	void run_cycles( nes_time_t end_time )
	{
		long n = prescaler + (end_time - cycle_time);
		cycle_time = end_time;
		if ( n < 0 )
			n = 0;
		prescaler = n & 3;
		for ( n >>= 2; n > 0; n-- )
			clock_counter();
	}

	virtual void run_until( nes_time_t end_time )
	{
		if ( cycle_mode )
			run_cycles( end_time );
		else
		{
			cycle_time = end_time;
			Mapper004::run_until( end_time );
		}
	}

	virtual void a12_clocked()
	{
		if ( !cycle_mode )
			Mapper004::a12_clocked();
	}

	virtual nes_time_t next_irq( nes_time_t present )
	{
		if ( !cycle_mode )
			return Mapper004::next_irq( present );
		run_cycles( present );
		if ( !irq_enabled )
			return no_irq;
		if ( irq_flag )
			return 0;
		long counts = irq_ctr ? irq_ctr : irq_latch + 1;
		return cycle_time + counts * 4 - prescaler;
	}

	virtual void end_frame( nes_time_t end_time )
	{
		run_until( end_time );
		cycle_time -= end_time;
		start_frame();
	}

	virtual void write( nes_time_t time, nes_addr_t addr, int data )
	{
		switch ( addr & 0xE001 )
		{
		case 0x8000:
			mode = data;
			update_chr_banks();
			update_prg_banks();
			break;

		case 0x8001:
			switch ( mode & 0x0F )
			{
			case 0x8: r8 = data; break;
			case 0x9: r9 = data; break;
			case 0xF: r15 = data; break;
			default:
				if ( (mode & 0x0F) < 8 )
					banks [mode & 7] = data;
				break;
			}
			update_chr_banks();
			update_prg_banks();
			break;

		case 0xA000:
			mirror = data;
			if ( !(cart().mirroring() & 0x08) )
			{
				if ( mirror & 1 )
					mirror_horiz();
				else
					mirror_vert();
			}
			break;

		case 0xA001:
			break;

		case 0xC000:
			run_until( time );
			irq_latch = data;
			irq_changed();
			break;

		case 0xC001:
			run_until( time );
			cycle_mode = data & 1;
			prescaler = 0;
			cycle_time = time;
			irq_ctr = 0;                    // reloads at the next count
			irq_changed();
			break;

		case 0xE000:
			run_until( time );
			irq_enabled = false;
			irq_flag = false;
			irq_changed();
			break;

		case 0xE001:
			run_until( time );
			irq_enabled = true;
			irq_changed();
			break;
		}
	}
};
