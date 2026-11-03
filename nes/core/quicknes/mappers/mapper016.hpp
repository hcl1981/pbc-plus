/*
 * QuickNES - NES emulator core
 * Mappers 16 and 159 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "nes_mapper.h"

// https://www.nesdev.org/wiki/Bandai_FCG_board
//
// Bandai FCG-1/2 and LZ93D50 (Dragon Ball Z, SD Gundam): eight 1 KB CHR
// banks, a 16 KB PRG bank at $8000, mirroring and a 16-bit IRQ counter
// that counts CPU cycles down and interrupts at zero. The FCG chips sit at
// $6000-$7FFF and load the counter directly, the LZ93D50 sits at
// $8000-$FFFF and loads it from a latch when the IRQ is switched on;
// mapper 16 is both, 159 the LZ93D50. The serial EEPROM some boards save
// to is not there: its data line reads 0.

class Mapper016 : public Nes_Mapper {
	uint8_t chr [8], prg, mirr;
	uint8_t irq_enabled, irq_pending;
	uint16_t latch, counter;
	nes_time_t last_time;
	bool fcg_regs;                  // also at $6000 (mapper 16)
public:
	Mapper016( bool fcg = true ) : fcg_regs( fcg )
	{
		register_state( chr, sizeof chr );
	}

	virtual void reset_state()
	{
		prg = mirr = 0;
		irq_enabled = irq_pending = 0;
		latch = counter = 0;
		last_time = 0;
		if ( fcg_regs )
			intercept_writes( 0x6000, 0x2000 );
		intercept_reads( 0x6000, 0x2000 );
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
		for ( int i = 0; i < 8; i++ )
			set_chr_bank( i * 0x400, bank_1k, chr [i] );
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
		if ( n > counter )              // passes zero: interrupt
		{
			irq_pending = true;
			counter = (uint16_t) (counter - n);
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

	virtual int read( nes_time_t, nes_addr_t addr )
	{
		if ( addr >= 0x6000 && addr < 0x8000 )
			return 0;                   // EEPROM data line (bit 4) low, open bus
		return -1;
	}

	void write_reg( nes_time_t time, int reg, int data, bool lz )
	{
		switch ( reg )
		{
		case 0x0: case 0x1: case 0x2: case 0x3:
		case 0x4: case 0x5: case 0x6: case 0x7:
			chr [reg] = data;
			set_chr_bank( reg * 0x400, bank_1k, data );
			break;
		case 0x8:
			prg = data & 0x0F;
			set_prg_bank( 0x8000, bank_16k, prg );
			break;
		case 0x9:
			mirr = data;
			set_mirroring();
			break;
		case 0xA:
			run_until( time );
			irq_enabled = data & 1;
			irq_pending = false;
			if ( lz )
				counter = latch;
			irq_changed();
			break;
		case 0xB:
			run_until( time );
			latch = (latch & 0xFF00) | data;
			if ( !lz )
				counter = (counter & 0xFF00) | data;
			irq_changed();
			break;
		case 0xC:
			run_until( time );
			latch = (latch & 0x00FF) | data << 8;
			if ( !lz )
				counter = (counter & 0x00FF) | data << 8;
			irq_changed();
			break;
		}
	}

	virtual bool write_intercepted( nes_time_t time, nes_addr_t addr, int data )
	{
		if ( addr < 0x6000 || addr >= 0x8000 )
			return false;
		write_reg( time, addr & 0x0F, data, false );
		return true;
	}

	virtual void write( nes_time_t time, nes_addr_t addr, int data )
	{
		write_reg( time, addr & 0x0F, data, true );
	}
};

class Mapper159 : public Mapper016 {
public:
	Mapper159() : Mapper016( false ) { }
};
