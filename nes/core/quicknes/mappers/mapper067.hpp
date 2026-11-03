/*
 * QuickNES - NES emulator core
 * Mapper 67 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "nes_mapper.h"

// https://www.nesdev.org/wiki/INES_Mapper_067
//
// Sunsoft-3 (Fantasy Zone II in Japan, Mito Koumon II): four 2 KB CHR
// banks, a 16 KB PRG bank at $8000, mirroring and a 16-bit IRQ counter
// that counts CPU cycles down; when it wraps from 0 to $FFFF it
// interrupts and stops. The counter is written high byte first through
// one register; any write to $8000-$87FF acknowledges the interrupt.

class Mapper067 : public Nes_Mapper {
	uint8_t chr [4], prg, mirr;
	uint8_t irq_enabled, irq_pending, toggle;
	uint16_t counter;
	nes_time_t last_time;
public:
	Mapper067()
	{
		register_state( chr, sizeof chr );
	}

	virtual void reset_state()
	{
		prg = mirr = 0;
		irq_enabled = irq_pending = toggle = 0;
		counter = 0;
		last_time = 0;
	}

	void set_mirroring()
	{
		switch ( mirr & 3 )
		{
		case 0: mirror_vert(); break;
		case 1: mirror_horiz(); break;
		case 2: mirror_single( 0 ); break;
		case 3: mirror_single( 1 ); break;
		}
	}

	virtual void apply_mapping()
	{
		for ( int i = 0; i < 4; i++ )
			set_chr_bank( i * 0x800, bank_2k, chr [i] );
		set_prg_bank( 0x8000, bank_16k, prg );
		set_prg_bank( 0xC000, bank_16k, last_bank );
		set_mirroring();
	}

	virtual void run_until( nes_time_t end_time )
	{
		long n = end_time - last_time;
		last_time = end_time;
		if ( !irq_enabled || n <= 0 )
			return;
		if ( n > counter )              // wraps: interrupt, counting stops
		{
			irq_pending = true;
			irq_enabled = false;
			counter = 0xFFFF;
		}
		else
			counter -= n;
	}

	virtual nes_time_t next_irq( nes_time_t present )
	{
		run_until( present );
		if ( irq_pending )
			return 0;
		if ( !irq_enabled )
			return no_irq;
		return last_time + counter + 1;
	}

	virtual void end_frame( nes_time_t end_time )
	{
		run_until( end_time );
		last_time -= end_time;
	}

	virtual void write( nes_time_t time, nes_addr_t addr, int data )
	{
		switch ( addr & 0xF800 )
		{
		case 0x8000:                    // IRQ acknowledge
			run_until( time );
			irq_pending = false;
			irq_changed();
			break;
		case 0x8800: case 0x9800: case 0xA800: case 0xB800: {
			int i = (addr >> 12) & 3;
			chr [i] = data;
			set_chr_bank( i * 0x800, bank_2k, data );
			break;
		}
		case 0xC800:
			run_until( time );
			if ( toggle )
				counter = (counter & 0xFF00) | data;
			else
				counter = (counter & 0x00FF) | data << 8;
			toggle ^= 1;
			irq_changed();
			break;
		case 0xD800:
			run_until( time );
			irq_enabled = data >> 4 & 1;
			irq_pending = false;
			toggle = 0;
			irq_changed();
			break;
		case 0xE800:
			mirr = data;
			set_mirroring();
			break;
		case 0xF800:
			prg = data;
			set_prg_bank( 0x8000, bank_16k, prg );
			break;
		}
	}
};
