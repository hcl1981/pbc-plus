/*
 *  1541t64.h - 1541 emulation in archive-type files (.t64/LYNX/.p00)
 *
 *  Frodo Copyright (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef C1541T64_H
#define C1541T64_H

#include "IEC.h"

#include <string>
#include <vector>


/*
 *  Definitions
 */

// Archive types
enum {
	TYPE_T64,			// C64S tape file
	TYPE_LYNX,			// C64 LYNX archive
	TYPE_P00			// .p00 file
};

// Archive file drive class. On the PicoBoy (rp2350/T64Drive.cpp) only .t64:
// the programs are read straight out of the archive, without the temporary
// files Frodo's own version makes.
class ArchDrive : public Drive {
public:
	ArchDrive(IEC *iec, const std::string & filepath);
	virtual ~ArchDrive();

	uint8_t Open(int channel, const uint8_t *name, int name_len) override;
	uint8_t Close(int channel) override;
	uint8_t Read(int channel, uint8_t &byte) override;
	uint8_t Write(int channel, uint8_t byte, bool eoi) override;
	void Reset() override;

	/* LOAD from the tape (device 1) out of the archive: no name, the file
	 * after the last one loaded; else the first whose name begins so. SA 0
	 * puts it at `xy`. False if there is none; else the end in *end. */
	bool TapeLoad(uint8_t *ram, bool verify, uint8_t sa, uint16_t xy,
	              const uint8_t *name, int name_len, uint16_t *end);

private:
	bool find_first_file(const uint8_t *pattern, int pattern_len, int &num);
	int last;				// the file loaded last (the tape's position)
	void make_directory();

	FILE *the_file;			// The archive
	std::vector<c64_dir_entry> file_info;	// Its programs (offset, size, load address)
	char dir_title[16];		// Directory title

	struct channel_t {
		bool open, dir;		// a program, or the directory listing
		int entry;
		uint32_t pos, len;	// bytes handed out / to hand out (with the load address)
		uint8_t buf[64];	// read-ahead from the archive
		uint32_t buf_at;
		int buf_len;
	} chan[15];
};


/*
 *  Functions
 */

// Check whether file with given header (64 bytes) and size looks like one
// of the file types supported by this module
extern bool IsArchFile(const std::string & path, const uint8_t *header, long size);

// Read directory of archive file into (empty) c64_dir_entry vector
extern bool ReadArchDirectory(const std::string & path, std::vector<c64_dir_entry> &vec);


#endif // ndef C1541T64_H
