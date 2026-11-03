/*
 * QuickNES - NES emulator core
 * Mapper 228 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "nes_mapper.h"

// https://www.nesdev.org/wiki/INES_Mapper_228
//
// Active Enterprises (Cheetahmen II; Action 52 has 512 KB of CHR, more
// than the tile cache can hold). A write to $8000-$FFFF sets everything
// from its address and data:
//   address  ..MC CPPP PPS. cccc   M mirroring (1 horizontal), CC PRG chip,
//                                  PPPPP 16 KB bank, S 16 KB mode (else
//                                  32 KB, lowest P ignored), cccc CHR
//   data     .... ..cc             low CHR bits
// There is no chip 2: chip 3 follows chip 1 in the file.

class Mapper228 : public Nes_Mapper {
	uint16_t addr_;
	uint8_t data_;
public:
	Mapper228()
	{
		register_state( &data_, 1 );
	}

	virtual void reset_state()
	{
		addr_ = 0x8000;
		data_ = 0;
	}

	virtual void apply_mapping()
	{
		int a = addr_;
		int bank = a >> 6 & 0x7F;           // chip and 16 KB bank, 16 KB units / 2
		int chip = bank >> 5;
		if ( chip == 3 )
			bank -= 0x20;
		if ( a & 0x20 )                     // 16 KB mode
		{
			set_prg_bank( 0x8000, bank_16k, bank );
			set_prg_bank( 0xC000, bank_16k, bank );
		}
		else
			set_prg_bank( 0x8000, bank_32k, bank >> 1 );
		set_chr_bank( 0, bank_8k, (a & 0x0F) << 2 | (data_ & 3) );
		if ( a & 0x2000 )
			mirror_horiz();
		else
			mirror_vert();
	}

	virtual void write( nes_time_t, nes_addr_t addr, int data )
	{
		addr_ = addr;
		data_ = data;
		apply_mapping();
	}
};
