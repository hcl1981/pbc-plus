/*
 * QuickNES - NES emulator core
 * Mapper 118 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "mapper004.hpp"

// https://www.nesdev.org/wiki/INES_Mapper_118
//
// TxSROM (TKSROM, TLSROM): an MMC3 whose CHR bank registers also choose the
// nametables -- bit 7 of each bank is CIRAM A10 for the nametable that
// lines up with it: R0/R1 for $2000/$2800 in 2 KB mode (CHR A12 not
// inverted), R2..R5 for $2000..$2C00 otherwise. $A000 does nothing.

class Mapper118 : public Mapper004 {
public:
	virtual void update_chr_banks()
	{
		Mapper004::update_chr_banks();
		if ( mode & 0x80 )
			mirror_manual( banks [2] >> 7, banks [3] >> 7, banks [4] >> 7, banks [5] >> 7 );
		else
			mirror_manual( banks [0] >> 7, banks [0] >> 7, banks [1] >> 7, banks [1] >> 7 );
	}

	virtual void write( nes_time_t time, nes_addr_t addr, int data )
	{
		if ( (addr & 0xE001) != 0xA000 )
			Mapper004::write( time, addr, data );
	}
};
