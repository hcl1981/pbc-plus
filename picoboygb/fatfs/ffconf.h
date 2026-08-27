/*---------------------------------------------------------------------------/
/  FatFs configuration for PicoBoyGB (SDK build)
/---------------------------------------------------------------------------/
/  ChaN FatFs R0.15 (vendored from the pico-sdk TinyUSB bundle).
/
/  The FAT volume lives in a reserved flash region of the RP2350 and is
/  exported over USB (mass storage) so .gb ROMs can be dropped onto it.
/  One FatFs sector == one 4096-byte flash sector, so writes map directly
/  to erase+program with no read-modify-write.
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	80286	/* Revision ID (must match ff.h) */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY	0	/* Read/write (we need to write .gb files + format). */
#define FF_FS_MINIMIZE	0	/* Full API. */
#define FF_USE_FIND		1	/* f_findfirst / f_findnext for the *.gb scan. */
#define FF_USE_MKFS		1	/* f_mkfs to format an empty/new partition. */
#define FF_USE_FASTSEEK	0
#define FF_USE_EXPAND	0
#define FF_USE_CHMOD	0
#define FF_USE_LABEL	1	/* volume label ("PICOBOYGB"). */
#define FF_USE_FORWARD	0
#define FF_USE_STRFUNC	1	/* f_puts / f_printf for the README file. */
#define FF_PRINT_LLI	0
#define FF_PRINT_FLOAT	0
#define FF_STRF_ENCODE	3	/* strfunc I/O uses UTF-8. */

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE	437	/* U.S. (OEM). Only this table is compiled from ffunicode.c. */
#define FF_USE_LFN		1	/* Long file names, static work buffer (not thread safe - fine, single task). */
#define FF_MAX_LFN		255
#define FF_LFN_UNICODE	0	/* ANSI/OEM API (char*). */
#define FF_LFN_BUF		255
#define FF_SFN_BUF		12
#define FF_FS_RPATH		0

/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES		1
#define FF_STR_VOLUME_ID	0
#define FF_MULTI_PARTITION	0
#define FF_MIN_SS		4096	/* fixed 4096-byte sectors == flash erase unit */
#define FF_MAX_SS		4096
#define FF_LBA64		0
#define FF_MIN_GPT		0x10000000
#define FF_USE_TRIM		0

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY		0
#define FF_FS_EXFAT		0
#define FF_FS_NORTC		1	/* No RTC on the board -> use a fixed timestamp. */
#define FF_NORTC_MON	1
#define FF_NORTC_MDAY	1
#define FF_NORTC_YEAR	2025
#define FF_FS_NOFSINFO	0
#define FF_FS_LOCK		0
#define FF_FS_REENTRANT	0	/* Single-threaded access (core0 only). */
#define FF_FS_TIMEOUT	1000
#define FF_SYNC_t		HANDLE
