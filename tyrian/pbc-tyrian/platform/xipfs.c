/*
 * xipfs -- Umsetzung. Siehe xipfs.h fuer das Archivformat und die Begruendung.
 *
 * GPLv2, wie OpenTyrian.
 */

#include "xipfs.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PBC_HOST
#  include "pico/stdlib.h"
#  include "hardware/flash.h"
#  include "hardware/sync.h"
#endif

#define XIPFS_MAGIC "PBCTYR01"
#define NAME_MAX_LEN 16

typedef struct
{
	char name[NAME_MAX_LEN];
	uint32_t offset;
	uint32_t size;
} __attribute__((packed)) xipfs_entry;

typedef struct
{
	char magic[8];
	uint32_t count;
	uint32_t total_size;
} __attribute__((packed)) xipfs_header;

/*
 * Auf dem Geraet steht das Archiv an einer festen Adresse. Im Rechnertest
 * (PBC_HOST) wird es in den Speicher geladen, deshalb ist der Anfang eine
 * Variable und keine Konstante.
 */
#ifdef PBC_HOST
static const uint8_t *xipfs_base = NULL;
void xipfs_set_base(const uint8_t *p) { xipfs_base = p; }
#else
static const uint8_t *const xipfs_base = (const uint8_t *)PBC_XIPFS_ADDR;
#endif

static const xipfs_header *hdr;
static const xipfs_entry  *dir;

/* ------------------------------------------------------------------ mount */

bool xipfs_mount(void)
{
	hdr = NULL;
	dir = NULL;

	if (xipfs_base == NULL)
		return false;

	const xipfs_header *h = (const xipfs_header *)xipfs_base;
	if (memcmp(h->magic, XIPFS_MAGIC, 8) != 0)
		return false;

	/*
	 * Ein leeres Flash liest sich als lauter 0xFF, ein halb geschriebenes als
	 * Unsinn. Die Kennung faengt beides ab; die Plausibilitaetspruefung
	 * darunter faengt ein Archiv ab, dessen Verzeichnis laenger ist als das
	 * Archiv selbst -- das kaeme nur bei einem abgebrochenen Flash-Vorgang vor,
	 * wuerde aber beim Suchen ins Leere greifen.
	 */
	if (h->count == 0 || h->count > 4096)
		return false;
	if (sizeof(xipfs_header) + (uint64_t)h->count * sizeof(xipfs_entry) > h->total_size)
		return false;

	hdr = h;
	dir = (const xipfs_entry *)(xipfs_base + sizeof(xipfs_header));
	return true;
}

unsigned xipfs_count(void) { return hdr ? hdr->count : 0; }
uint32_t xipfs_bytes(void) { return hdr ? hdr->total_size : 0; }

/* Nur den Namensteil hinter dem letzten '/' oder '\'. */
static const char *basename_of(const char *path)
{
	const char *b = path;
	for (const char *p = path; *p; ++p)
		if (*p == '/' || *p == '\\')
			b = p + 1;
	return b;
}

static bool name_matches(const char *entry, const char *want)
{
	/* entry ist auf NAME_MAX_LEN mit Nullen aufgefuellt und bereits klein. */
	unsigned i = 0;
	for (; i < NAME_MAX_LEN; ++i)
	{
		char w = want[i];
		if (w == '\0')
			return entry[i] == '\0';
		if (entry[i] != (char)tolower((unsigned char)w))
			return false;
	}
	return want[NAME_MAX_LEN] == '\0';
}

const uint8_t *xipfs_find(const char *name, uint32_t *out_size)
{
	if (!hdr)
		return NULL;

	const char *want = basename_of(name);

	for (uint32_t i = 0; i < hdr->count; ++i)
	{
		if (!name_matches(dir[i].name, want))
			continue;
		if (out_size)
			*out_size = dir[i].size;
		return xipfs_base + dir[i].offset;
	}
	return NULL;
}

/* ------------------------------------------------------------ Deskriptoren */

/*
 * OpenTyrian hat nie mehr als zwei Dateien gleichzeitig offen (eine Quelle plus
 * gelegentlich der Spielstand). Acht Plaetze sind grosszuegig und kosten nichts.
 */
#define MAX_OPEN 8

typedef struct
{
	bool used;
	bool writable;
	const uint8_t *base;   /* Flash oder RAM-Puffer */
	uint32_t size;
	uint32_t pos;
	/* nur fuer Schreibdateien */
	int slot;
	uint8_t *wbuf;
	uint32_t wcap;
} xfile;

static xfile files[MAX_OPEN];

static xfile *as_xfile(FILE *f)
{
	xfile *x = (xfile *)(void *)f;
	if (x < files || x >= files + MAX_OPEN || !x->used)
		return NULL;
	return x;
}

/* --------------------------------------------------------- Spielstaende */

/*
 * Vier feste Plaetze zu je 3 KB, im RAM gespiegelt und beim Schliessen einer
 * Schreibdatei komplett zurueck ins Flash gebrannt. Zur Aufteilung siehe
 * xipfs.h.
 *
 * Der ganze Bereich wird auf einmal geschrieben, nicht der einzelne Platz:
 * das kostet einen Loeschzyklus je Speichervorgang statt einen je Platz und
 * ist bei einer Handvoll Speichervorgaengen je Spielabend voellig unkritisch.
 * Ein Stromausfall mittendrin verliert die Einstellungen, nicht das Flash.
 */

typedef struct
{
	char magic[4];      /* "TSAV" */
	uint32_t version;
	struct
	{
		char name[NAME_MAX_LEN];
		uint32_t len;
	} slot[PBC_SAVE_SLOTS];
} __attribute__((packed)) save_header;

#define SAVE_TOTAL (sizeof(save_header) + PBC_SAVE_AREA)

#ifndef PBC_HOST
/*
 * Geloescht wird in ganzen Sektoren. Der aufgerundete Bereich muss vollstaendig
 * im Flash liegen -- sonst bricht flash_range_erase mit einer Zusicherung ab,
 * und zwar erst beim ersten Speichern, also lange nach dem Uebersetzen.
 */
#define SAVE_ERASE_BYTES \
	((SAVE_TOTAL + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1u))

_Static_assert((PBC_SAVE_ADDR - 0x10000000u) % FLASH_SECTOR_SIZE == 0,
               "Spielstandsbereich beginnt nicht auf einer Sektorgrenze");
_Static_assert((PBC_SAVE_ADDR - 0x10000000u) + SAVE_ERASE_BYTES <= 16u * 1024u * 1024u,
               "Spielstandsbereich reicht ueber das Flash-Ende hinaus");

/*
 * Die letzten 16 KB bleiben frei: dort liegt der Zusatzblock, den die
 * SDK ans Ende der Firmware-UF2 setzt (0x10FFFF00). Ihn mitzuloeschen waere
 * ein Fehler, den man erst beim naechsten Flashen bemerkt.
 */
_Static_assert((PBC_SAVE_ADDR - 0x10000000u) + SAVE_ERASE_BYTES <= 16u * 1024u * 1024u - 16u * 1024u,
               "Spielstandsbereich reicht in die obersten 16 KB des Flash");
#endif

static uint8_t save_image[SAVE_TOTAL] __attribute__((aligned(4)));
static bool    save_dirty;

static save_header *save_hdr(void) { return (save_header *)save_image; }

static uint8_t *save_slot_data(unsigned i)
{
	return save_image + sizeof(save_header) + (size_t)i * PBC_SAVE_SLOT_BYTES;
}

void pbc_save_init(void)
{
	memset(save_image, 0, sizeof save_image);
	memcpy(save_hdr()->magic, "TSAV", 4);
	save_hdr()->version = 1;
	save_dirty = false;

#ifndef PBC_HOST
	const save_header *sh = (const save_header *)PBC_SAVE_ADDR;
	if (memcmp(sh->magic, "TSAV", 4) == 0 && sh->version == 1)
	{
		/*
		 * Vor dem Uebernehmen pruefen, ob die Laengen plausibel sind. Ein
		 * halb geschriebener Sektor traegt sonst eine Laenge, mit der spaeter
		 * ueber den Platz hinaus gelesen wuerde.
		 */
		bool sane = true;
		for (unsigned i = 0; i < PBC_SAVE_SLOTS; ++i)
			if (sh->slot[i].len > PBC_SAVE_SLOT_BYTES)
				sane = false;

		if (sane)
			memcpy(save_image, (const uint8_t *)PBC_SAVE_ADDR, SAVE_TOTAL);
	}
#endif
}

static void save_commit(void)
{
#ifndef PBC_HOST
	if (!save_dirty)
		return;

	uint32_t rounded = (SAVE_TOTAL + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1u);

	/*
	 * Waehrenddessen darf nichts aus dem Flash gelesen oder ausgefuehrt
	 * werden. Der Aufrufer hat die Bildausgabe bereits abgewartet; hier
	 * bleiben die Unterbrechungen aus.
	 */
	uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(PBC_SAVE_ADDR - 0x10000000u, SAVE_ERASE_BYTES);
	flash_range_program(PBC_SAVE_ADDR - 0x10000000u, save_image, rounded);
	restore_interrupts(ints);
#endif
	save_dirty = false;
}

/* Platz mit diesem Namen, oder -1. */
static int save_find(const char *name)
{
	for (unsigned i = 0; i < PBC_SAVE_SLOTS; ++i)
		if (save_hdr()->slot[i].name[0] != '\0' &&
		    name_matches(save_hdr()->slot[i].name, name))
			return (int)i;
	return -1;
}

/* Platz zum Beschreiben: erst der gleichnamige, sonst ein freier. */
static int save_claim(const char *name)
{
	int i = save_find(name);
	if (i >= 0)
		return i;

	for (unsigned k = 0; k < PBC_SAVE_SLOTS; ++k)
		if (save_hdr()->slot[k].name[0] == '\0')
		{
			strncpy(save_hdr()->slot[k].name, name, NAME_MAX_LEN - 1);
			save_hdr()->slot[k].name[NAME_MAX_LEN - 1] = '\0';
			for (char *c = save_hdr()->slot[k].name; *c; ++c)
				*c = (char)tolower((unsigned char)*c);
			return (int)k;
		}

	return -1;
}

/* ------------------------------------------------------------------ stdio */

FILE *pbc_fopen(const char *path, const char *mode)
{
	const bool want_write = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL);

	xfile *x = NULL;
	for (unsigned i = 0; i < MAX_OPEN; ++i)
		if (!files[i].used) { x = &files[i]; break; }
	if (!x)
		return NULL;

	memset(x, 0, sizeof *x);
	const char *name = basename_of(path);

	if (want_write)
	{
		int slot = save_claim(name);
		if (slot < 0)
			return NULL;   /* alle Plaetze belegt -- siehe PBC_SAVE_SLOTS */

		save_hdr()->slot[slot].len = 0;
		save_dirty = true;

		x->used = true;
		x->writable = true;
		x->slot = slot;
		x->wbuf = save_slot_data((unsigned)slot);
		x->wcap = PBC_SAVE_SLOT_BYTES;
		x->base = x->wbuf;
		x->size = 0;
		x->pos = 0;
		return (FILE *)(void *)x;
	}

	/* Lesen: erst die Spielstaende im RAM, dann das Archiv im Flash. */
	{
		int slot = save_find(name);
		if (slot >= 0)
		{
			x->used = true;
			x->slot = slot;
			x->base = save_slot_data((unsigned)slot);
			x->size = save_hdr()->slot[slot].len;
			x->pos = 0;
			return (FILE *)(void *)x;
		}
	}

	uint32_t size = 0;
	const uint8_t *p = xipfs_find(name, &size);
	if (!p)
		return NULL;

	x->used = true;
	x->base = p;
	x->size = size;
	x->pos = 0;
	return (FILE *)(void *)x;
}

int pbc_fclose(FILE *f)
{
	xfile *x = as_xfile(f);
	if (!x)
		return -1;

	if (x->writable)
	{
		save_hdr()->slot[x->slot].len = x->size;
		save_commit();
	}

	x->used = false;
	return 0;
}

size_t pbc_fread(void *ptr, size_t size, size_t nmemb, FILE *f)
{
	xfile *x = as_xfile(f);
	if (!x || size == 0)
		return 0;

	size_t want = size * nmemb;
	size_t avail = x->size - x->pos;
	if (want > avail)
		want = avail;

	memcpy(ptr, x->base + x->pos, want);
	x->pos += want;
	return want / size;
}

size_t pbc_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f)
{
	xfile *x = as_xfile(f);
	if (!x || !x->writable || size == 0)
		return 0;

	size_t want = size * nmemb;
	if (x->pos + want > x->wcap)
		want = x->wcap - x->pos;

	memcpy(x->wbuf + x->pos, ptr, want);
	x->pos += want;
	if (x->pos > x->size)
		x->size = x->pos;
	save_dirty = true;
	return want / size;
}

int pbc_fseek(FILE *f, long offset, int whence)
{
	xfile *x = as_xfile(f);
	if (!x)
		return -1;

	long base;
	switch (whence)
	{
		case SEEK_SET: base = 0;             break;
		case SEEK_CUR: base = (long)x->pos;  break;
		case SEEK_END: base = (long)x->size; break;
		default: return -1;
	}

	long np = base + offset;
	if (np < 0 || np > (long)x->size)
		return -1;

	x->pos = (uint32_t)np;
	return 0;
}

long pbc_ftell(FILE *f)
{
	xfile *x = as_xfile(f);
	return x ? (long)x->pos : -1L;
}

void pbc_rewind(FILE *f)
{
	xfile *x = as_xfile(f);
	if (x)
		x->pos = 0;
}

int pbc_fgetc(FILE *f)
{
	xfile *x = as_xfile(f);
	if (!x || x->pos >= x->size)
		return EOF;
	return x->base[x->pos++];
}

char *pbc_fgets(char *s, int size, FILE *f)
{
	xfile *x = as_xfile(f);
	if (!x || size <= 1 || x->pos >= x->size)
		return NULL;

	int n = 0;
	while (n < size - 1 && x->pos < x->size)
	{
		char c = (char)x->base[x->pos++];
		s[n++] = c;
		if (c == '\n')
			break;
	}
	s[n] = '\0';
	return s;
}

int pbc_feof(FILE *f)
{
	xfile *x = as_xfile(f);
	return (!x || x->pos >= x->size) ? 1 : 0;
}

int pbc_fflush(FILE *f)
{
	xfile *x = as_xfile(f);
	if (x && x->writable)
	{
		save_hdr()->slot[x->slot].len = x->size;
		save_commit();
	}
	return 0;
}

int pbc_fprintf(FILE *f, const char *fmt, ...)
{
	/*
	 * Sämtliche fprintf-Aufrufe in OpenTyrian gehen nach stderr oder stdout
	 * und sind reine Diagnose. Auf dem Geraet landet das auf der seriellen
	 * Schnittstelle -- sofern die nicht gerade dem Multiplayer-Link gehoert,
	 * dann verschwindet es folgenlos.
	 */
	(void)f;
	va_list ap;
	va_start(ap, fmt);
	int n = vprintf(fmt, ap);
	va_end(ap);
	return n;
}

int pbc_fileno(FILE *f)
{
	/*
	 * Es gibt keine Dateideskriptoren. Der einzige Aufrufer reicht das
	 * Ergebnis an fsync weiter, und das ist hier ohnehin gegenstandslos --
	 * geschrieben wird beim Schliessen, in einem Zug.
	 */
	(void)f;
	return 0;
}

/*
 * Zwei POSIX-Funktionen, die OpenTyrian beim Speichern benutzt und die es auf
 * diesem Geraet nicht geben kann:
 *
 *   mkdir  Es gibt keine Verzeichnisse. Der Spielstand liegt in einem festen
 *          Flash-Sektor, nicht in einem Benutzerverzeichnis.
 *   fsync  Es gibt keinen Schreibpuffer, den man erzwingen muesste. pbc_fclose
 *          brennt den Sektor in einem Zug; danach steht alles im Flash.
 *
 * Beide melden Erfolg, weil beides in der Sache erfuellt ist.
 */
int mkdir(const char *path, mode_t mode)
{
	(void)path; (void)mode;
	return 0;
}

int fsync(int fd)
{
	(void)fd;
	return 0;
}

bool xipfs_owns(const void *p)
{
	if (!hdr || p == NULL)
		return false;
	const uint8_t *q = (const uint8_t *)p;
	return q >= xipfs_base && q < xipfs_base + hdr->total_size;
}

const uint8_t *xipfs_inplace(FILE *f, size_t len)
{
	xfile *x = as_xfile(f);
	if (!x || x->writable)
		return NULL;
	if (x->pos + len > x->size)
		return NULL;

	const uint8_t *p = x->base + x->pos;
	x->pos += len;
	return p;
}
