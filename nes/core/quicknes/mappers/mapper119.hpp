/*
 * QuickNES - NES emulator core
 * Mapper 119 written for the PicoBoy Color Plus port.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "mapper004.hpp"

// https://www.nesdev.org/wiki/INES_Mapper_119
//
// TQROM (Pin-Bot, High Speed): an MMC3 with 64 KB of CHR ROM and 8 KB of
// CHR RAM. Bit 6 of a CHR bank register picks the RAM, bits 0-2 its 1 KB
// page (bits 1-2 for the 2 KB registers). The PPU keeps the RAM right
// behind the ROM in one CHR space (Nes_Core::open, open_chr): RAM page n
// is "bank" ROM size / 1 KB + n there.

class Mapper119 : public Mapper004 {
	int chr( int v, int shift ) const
	{
		long rom = cart().chr_size();
		if ( v & 0x40 )
			return (int) ((rom >> 10) + (v & 7)) >> shift;
		return (int) (v & 0x3F & ((rom >> 10) - 1)) >> shift;
	}
public:
	virtual void update_chr_banks()
	{
		int chr_xor = (mode >> 7 & 1) * 0x1000;
		set_chr_bank( 0x0000 ^ chr_xor, bank_2k, chr( banks [0] & 0xFE, 1 ) );
		set_chr_bank( 0x0800 ^ chr_xor, bank_2k, chr( banks [1] & 0xFE, 1 ) );
		set_chr_bank( 0x1000 ^ chr_xor, bank_1k, chr( banks [2], 0 ) );
		set_chr_bank( 0x1400 ^ chr_xor, bank_1k, chr( banks [3], 0 ) );
		set_chr_bank( 0x1800 ^ chr_xor, bank_1k, chr( banks [4], 0 ) );
		set_chr_bank( 0x1c00 ^ chr_xor, bank_1k, chr( banks [5], 0 ) );
	}
};
