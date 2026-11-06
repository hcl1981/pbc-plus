/*
 *  T64Drive.cpp - .t64 archives as drive 8 (the ArchDrive of 1541t64.h)
 *
 *  Written for the PicoBoy Color Plus (GPL-2.0-or-later, as Frodo).
 *
 *  A .t64 is no tape image: the C64S emulator kept programs in it, each as
 *  it lies in memory, with a directory in front. Mounted as drive 8, LOAD"*"
 *  gets the first program and a game that loads more parts finds them by
 *  name, as on a disk. The programs are read straight from the archive (a
 *  small read-ahead per channel); Frodo's own version copied each into a
 *  temporary file, which the PicoBoy does not have. Read-only.
 *
 *  Layout: 64-byte header ("C64..." signature, 0x22 directory size, 0x24
 *  entries used, 0x28 title), then 32-byte entries: 0 type (1 = file), 1 the
 *  1541 file type, 2/3 load address, 4/5 end address, 8..11 offset in the
 *  archive, 16..31 the name, padded with spaces. Many archives carry a wrong
 *  end address; the size comes from the next entry's offset (or the end of
 *  the archive) as well, and the smaller one counts.
 */

#include "../sysdeps.h"
#include "../1541t64.h"
#include "../IEC.h"

#include <cstring>

static const int HEADER = 64, ENTRY = 32;

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

bool IsArchFile(const std::string &path, const uint8_t *header, long size)
{
	(void)path;
	return size >= HEADER + ENTRY && memcmp(header, "C64", 3) == 0;
}

/* the archive's programs; false if it is no .t64 */
static bool read_t64(FILE *f, std::vector<c64_dir_entry> &vec, char *title)
{
	uint8_t h[HEADER];
	vec.clear();
	if (fseek(f, 0, SEEK_END) != 0)
		return false;
	long fsize = ftell(f);
	if (fseek(f, 0, SEEK_SET) != 0 || fread(h, 1, HEADER, f) != (size_t)HEADER || memcmp(h, "C64", 3) != 0)
		return false;
	if (title) {
		memcpy(title, h + 0x28, 16);
		for (int i = 0; i < 16; i++)
			if (title[i] == 0)
				title[i] = ' ';
	}
	int max = le16(h + 0x22);
	if (max <= 0 || max > 256)
		max = 32;
	for (int i = 0; i < max; i++) {
		uint8_t e[ENTRY];
		if (fread(e, 1, ENTRY, f) != (size_t)ENTRY)
			break;
		if (e[0] == 0)
			continue;                   // free slot
		uint16_t start = le16(e + 2), end = le16(e + 4);
		uint32_t off = le32(e + 8);
		if (off < (uint32_t)HEADER || (long)off >= fsize)
			continue;
		uint8_t name[17];
		memcpy(name, e + 16, 16);
		int n = 16;
		while (n > 0 && (name[n - 1] == 0x20 || name[n - 1] == 0xa0 || name[n - 1] == 0))
			n--;
		name[n] = 0;
		size_t size = end > start ? (size_t)(end - start) : 0;
		vec.emplace_back(name, FTYPE_PRG, false, false, size, (off_t)off, (uint8_t)(start & 0xff), (uint8_t)(start >> 8));
	}
	/* the size from the offsets: the next program's start, or the end */
	for (auto &d : vec) {
		uint32_t next = (uint32_t)fsize;
		for (auto &o : vec)
			if ((uint32_t)o.offset > (uint32_t)d.offset && (uint32_t)o.offset < next)
				next = (uint32_t)o.offset;
		size_t room = next - (uint32_t)d.offset;
		if (d.size == 0 || d.size > room)
			d.size = room;
	}
	return !vec.empty();
}

bool ReadArchDirectory(const std::string &path, std::vector<c64_dir_entry> &vec)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	bool ok = read_t64(f, vec, nullptr);
	fclose(f);
	return ok;
}

/* the mounted archive, for LOAD from the tape (CPUC64.cpp) */
static ArchDrive *mounted;

bool t64_tape_load(uint8_t *ram, bool verify, uint8_t sa, uint16_t xy,
                   const uint8_t *name, int name_len, uint16_t *end)
{
	return mounted && mounted->TapeLoad(ram, verify, sa, xy, name, name_len, end);
}

ArchDrive::ArchDrive(IEC *iec, const std::string &filepath) : Drive(iec), the_file(nullptr), last(-1)
{
	memset(chan, 0, sizeof chan);
	memset(dir_title, ' ', sizeof dir_title);
	the_file = fopen(filepath.c_str(), "rb");
	if (the_file && read_t64(the_file, file_info, dir_title)) {
		Ready = true;
		mounted = this;
		Reset();
	} else {
		if (the_file)
			fclose(the_file);
		the_file = nullptr;
		Ready = false;
	}
}

ArchDrive::~ArchDrive()
{
	if (mounted == this)
		mounted = nullptr;
	if (the_file)
		fclose(the_file);
}

bool ArchDrive::TapeLoad(uint8_t *ram, bool verify, uint8_t sa, uint16_t xy,
                         const uint8_t *name, int name_len, uint16_t *end)
{
	int n = (int)file_info.size(), num = -1;
	if (!the_file || n == 0)
		return false;
	if (name_len == 0) {
		num = last + 1;                 // the next one on the tape
		if (num >= n)
			return false;
	} else {
		for (int k = 1; k <= n && num < 0; k++) {   // from the tape's position on, round
			int i = (last + k) % n;
			const uint8_t *e = file_info[i].name;
			int l = 0;
			while (l < name_len && e[l] && (name[l] == e[l] || name[l] == '?'))
				l++;
			if (l == name_len || (l < name_len && name[l] == '*'))
				num = i;
		}
		if (num < 0)
			return false;
	}
	const c64_dir_entry &d = file_info[num];
	uint32_t start = sa == 0 ? xy : (uint32_t)(d.sa_lo | d.sa_hi << 8);
	uint32_t size = (uint32_t)d.size;
	if (start + size > 0x10000)
		size = 0x10000 - start;
	if (!verify) {
		if (fseek(the_file, (long)d.offset, SEEK_SET) != 0 || fread(ram + start, 1, size, the_file) != size)
			return false;
	}
	last = num;
	*end = (uint16_t)(start + size);
	return true;
}

void ArchDrive::Reset()
{
	for (auto &c : chan)
		c.open = false;
	set_error(ERR_STARTUP);
}

/* '*' the rest, '?' one character; the names are PETSCII on both sides */
static bool match(const uint8_t *p, int p_len, const uint8_t *n)
{
	int i = 0;
	for (; i < p_len; i++) {
		if (p[i] == '*')
			return true;
		if (!n[i] || (p[i] != '?' && p[i] != n[i]))
			return false;
	}
	return n[i] == 0;
}

bool ArchDrive::find_first_file(const uint8_t *pattern, int pattern_len, int &num)
{
	for (size_t i = 0; i < file_info.size(); i++)
		if (match(pattern, pattern_len, file_info[i].name)) {
			num = (int)i;
			return true;
		}
	return false;
}

/* "$": the directory as a BASIC program, as a 1541 sends it */
static uint8_t dir_buf[40 * 34 + 64];
static int dir_n;

static void dir_put(uint8_t b)
{
	if (dir_n < (int)sizeof dir_buf)
		dir_buf[dir_n++] = b;
}

static void dir_line(int number, const char *text, int len)
{
	dir_put(0x01);                      // link: anything not 0
	dir_put(0x01);
	dir_put((uint8_t)(number & 0xff));
	dir_put((uint8_t)(number >> 8));
	for (int i = 0; i < len; i++)
		dir_put((uint8_t)text[i]);
	dir_put(0);
}

void ArchDrive::make_directory()
{
	dir_n = 0;
	auto line = dir_line;
	dir_put(0x01);                      // load address $0401
	dir_put(0x04);
	char t[40];
	t[0] = 0x12;                        // reverse on
	t[1] = '"';
	memcpy(t + 2, dir_title, 16);
	memcpy(t + 18, "\" T64 2A", 8);
	line(0, t, 26);
	for (auto &d : file_info) {
		if (dir_n > (int)sizeof dir_buf - 40)
			break;                      // as many as fit
		int blocks = (int)((d.size + 253) / 254);
		int nlen = (int)strlen((const char *)d.name);
		int k = 0;
		if (blocks < 10) t[k++] = ' ';
		if (blocks < 100) t[k++] = ' ';
		t[k++] = '"';
		memcpy(t + k, d.name, nlen);
		k += nlen;
		t[k++] = '"';
		for (int i = nlen; i < 16; i++)
			t[k++] = ' ';
		memcpy(t + k, " PRG", 4);
		k += 4;
		line(blocks, t, k);
	}
	line(0, "BLOCKS FREE.", 12);
	dir_put(0);
	dir_put(0);
}

uint8_t ArchDrive::Open(int channel, const uint8_t *name, int name_len)
{
	set_error(ERR_OK);
	if (channel == 15) {
		execute_cmd(name, name_len);
		return ST_OK;
	}
	channel_t &c = chan[channel];
	c.open = false;
	if (!the_file) {
		set_error(ERR_NOTREADY);
		return ST_OK;
	}
	if (name_len > 0 && name[0] == '$') {
		make_directory();
		c.open = c.dir = true;
		c.pos = 0;
		c.len = (uint32_t)dir_n;
		return ST_OK;
	}
	uint8_t plain[NAMEBUF_LENGTH];
	int plain_len, mode = FMODE_READ, type = FTYPE_DEL, rec_len = 0;
	parse_file_name(name, name_len, plain, plain_len, mode, type, rec_len);
	if (mode != FMODE_READ || channel == 1) {
		set_error(ERR_WRITEPROTECT);    // an archive is read-only
		return ST_OK;
	}
	int num;
	bool found = plain_len > 0 && find_first_file(plain, plain_len, num);
#ifdef T64_TRACE
	fprintf(stderr, "T64 open ch %d \"%.*s\" -> %s\n", channel, plain_len, (const char *)plain,
	        found ? (const char *)file_info[num].name : "not found");
#endif
	if (!found) {
		set_error(ERR_FILENOTFOUND);
		return ST_OK;
	}
	c.open = true;
	c.dir = false;
	c.entry = num;
	last = num;                         // the tape goes on from here
	c.pos = 0;
	c.len = (uint32_t)file_info[num].size + 2;
	c.buf_len = 0;
	return ST_OK;
}

uint8_t ArchDrive::Close(int channel)
{
	if (channel == 15) {
		for (auto &c : chan)
			c.open = false;
	} else {
		chan[channel].open = false;
	}
	return ST_OK;
}

uint8_t ArchDrive::Read(int channel, uint8_t &byte)
{
	if (channel == 15) {                // the error channel
		byte = (uint8_t)*error_ptr++;
		if (--error_len)
			return ST_OK;
		set_error(ERR_OK);
		return ST_EOF;
	}
	channel_t &c = chan[channel];
	if (!c.open || c.pos >= c.len)
		return ST_READ_TIMEOUT;
	if (c.dir) {
		byte = dir_buf[c.pos];
	} else {
		const c64_dir_entry &d = file_info[c.entry];
		if (c.pos < 2) {
			byte = c.pos ? d.sa_hi : d.sa_lo;   // the load address first
		} else {
			uint32_t at = c.pos - 2;
			if (c.buf_len == 0 || at < c.buf_at || at >= c.buf_at + (uint32_t)c.buf_len) {
				c.buf_at = at;
				uint32_t want = (uint32_t)d.size - at;
				if (want > sizeof c.buf)
					want = sizeof c.buf;
				if (fseek(the_file, (long)(d.offset + at), SEEK_SET) != 0 ||
				    fread(c.buf, 1, want, the_file) != want) {
					c.open = false;
					return ST_READ_TIMEOUT;
				}
				c.buf_len = (int)want;
			}
			byte = c.buf[at - c.buf_at];
		}
	}
	return ++c.pos >= c.len ? ST_EOF : ST_OK;
}

uint8_t ArchDrive::Write(int channel, uint8_t byte, bool eoi)
{
	(void)byte;
	(void)eoi;
	if (channel == 15)
		return ST_OK;                   // commands: nothing to do on an archive
	set_error(ERR_WRITEPROTECT);
	return ST_TIMEOUT;
}
