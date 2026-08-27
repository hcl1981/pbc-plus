/*
 * xipfs -- ein winziges, schreibgeschuetztes Archiv, das direkt aus dem
 * gemappten Flash gelesen wird.
 *
 * Der RP2350 blendet sein Flash ab 0x10000000 in den Adressraum ein. Die
 * Tyrian-Daten (rund 10 MB) liegen dort als ein zusammenhaengendes Archiv;
 * "eine Datei oeffnen" heisst deshalb nichts weiter, als einen Zeiger auf die
 * richtige Stelle zu setzen. Es wird nichts ins RAM kopiert, solange der
 * Aufrufer es nicht ausdruecklich verlangt -- bei 512 KB RAM und 10 MB Daten
 * ist das keine Feinheit, sondern die Voraussetzung dafuer, dass es ueberhaupt
 * geht.
 *
 * OpenTyrian selbst bleibt unveraendert: pbc_prelude.h biegt fopen/fread/...
 * per Makro hierher um, sodass der vorhandene Ladecode weiterlaeuft.
 *
 * Archivformat (alles little endian, wie der Prozessor):
 *
 *     Offset 0   char     magic[8]    "PBCTYR01"
 *     Offset 8   uint32   count       Anzahl Eintraege
 *     Offset 12  uint32   total_size  Gesamtgroesse des Archivs
 *     Offset 16  entry[count]         je 24 Byte:
 *                  char   name[16]    kleingeschrieben, mit 0 aufgefuellt
 *                  uint32 offset      ab Archivanfang
 *                  uint32 size
 *                ... Nutzdaten ...
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_XIPFS_H
#define PBC_XIPFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Adresse des Archivs im Adressraum.
 *
 * 0x10000000 ist der Anfang des Flash-Fensters. Die Firmware bekommt das erste
 * Megabyte; das Archiv beginnt eine Sektorgrenze dahinter. Muss mit dem Wert
 * in tools/mkdata.py und dem Ladeziel des Daten-UF2 uebereinstimmen.
 */
#ifndef PBC_XIPFS_ADDR
#define PBC_XIPFS_ADDR 0x10100000u
#endif

/* Meldet, ob an PBC_XIPFS_ADDR ein gueltiges Archiv steht. Wird beim Start
   geprueft, damit ein fehlendes Daten-UF2 eine klare Meldung ergibt statt
   irgendwo im Ladecode zu stranden. */
bool xipfs_mount(void);

/* Anzahl Eintraege bzw. Gesamtgroesse -- nur fuer die Diagnoseanzeige. */
unsigned xipfs_count(void);
uint32_t xipfs_bytes(void);

/*
 * Zeiger auf den Dateiinhalt im Flash, oder NULL. Der Name darf ein
 * Verzeichnispraefix tragen (alles bis zum letzten '/' wird verworfen) und
 * wird ohne Ruecksicht auf Gross-/Kleinschreibung gesucht.
 *
 * Das ist der direkte Weg fuer alles, was ohne Kopie auskommt.
 */
const uint8_t *xipfs_find(const char *name, uint32_t *out_size);

/* ------------------------------------------------ stdio-Ersatz -------- */

/*
 * Die Ersatzfunktionen geben und nehmen FILE*, damit die Makros in
 * pbc_prelude.h die OpenTyrian-Quellen nicht anfassen muessen. Dahinter steckt
 * kein newlib-FILE, sondern ein eigener Deskriptor -- die Zeiger duerfen
 * deshalb NIE an eine echte libc-Funktion geraten.
 */

FILE *pbc_fopen(const char *path, const char *mode);
int   pbc_fclose(FILE *f);
size_t pbc_fread(void *ptr, size_t size, size_t nmemb, FILE *f);
size_t pbc_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);
int   pbc_fseek(FILE *f, long offset, int whence);
long  pbc_ftell(FILE *f);
void  pbc_rewind(FILE *f);
int   pbc_fgetc(FILE *f);
char *pbc_fgets(char *s, int size, FILE *f);
int   pbc_feof(FILE *f);
int   pbc_fflush(FILE *f);
int   pbc_fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Siehe pbc_prelude.h: darf niemals newlibs fileno erreichen. */
int   pbc_fileno(FILE *f);

/*
 * Zeiger auf die aktuelle Leseposition im Flash -- der Schluessel zum
 * kopierfreien Laden.
 *
 * OpenTyrian liest Sprite-Bloecke nach dem Muster "malloc(n); fread(n)". Auf
 * diesem Geraet waeren das mehrere hundert Kilobyte RAM fuer Daten, die bereits
 * gelesen im Adressraum stehen. sprite.c ist deshalb an drei Stellen so
 * geaendert, dass es stattdessen hierher zeigt und den Lesezeiger weitersetzt.
 *
 * Gibt NULL zurueck, wenn die Datei im RAM liegt (Spielstaende) oder weniger
 * als len Bytes uebrig sind.
 */
const uint8_t *xipfs_inplace(FILE *f, size_t len);

/*
 * Zeigt p ins Archiv (und darf deshalb NICHT freigegeben werden)?
 *
 * Noetig, weil OpenTyrian seine Sprite-Tabellen mit free() aufraeumt. Nach dem
 * kopierfreien Laden zeigen die Zeiger ins Flash; sie an free() zu geben waere
 * ein Absturz beim naechsten Episodenwechsel -- also genau die Art Fehler, die
 * erst nach zwanzig Minuten Spielzeit auftritt.
 */
bool xipfs_owns(const void *p);

/* ------------------------------------------------ Spielstaende -------- */

/*
 * Konfiguration und Spielstaende sind das Einzige, was geschrieben wird.
 * Sie liegen in einem eigenen Flash-Sektor am oberen Ende und werden
 * vollstaendig neu geschrieben, wenn OpenTyrian die Datei schliesst -- bei
 * wenigen Kilobyte je Speichervorgang ist Feineres nicht noetig.
 */
/*
 * Anfang des Spielstandsbereichs.
 *
 * VIER Sektoren (16 KB) vom Flash-Ende, nicht drei. Gebraucht werden nur
 * 12.376 Byte -- aber geloescht wird in Sektoren zu 4 KB, und 12.376
 * aufgerundet sind 16.384. Bei drei Sektoren Abstand haette der Loeschbefehl
 * 4 KB hinter dem Flash-Ende geendet, und die SDK bricht das mit einer
 * Zusicherung ab. Das passiert beim allerersten Speichern -- also gleich beim
 * ersten Start, wenn die Konfiguration angelegt wird.
 *
 * NICHT ganz ans Ende: die vom SDK erzeugte Firmware-UF2 enthaelt einen
 * Zusatzblock bei 0x10FFFF00, also in den letzten 256 Byte des Flash. Laege
 * der Spielstandsbereich dort, wuerde jeder Speichervorgang ihn mitloeschen --
 * und umgekehrt jedes Flashen der Firmware den Spielstand anschneiden. Der
 * Bereich beginnt deshalb 32 KB vor dem Ende und laesst die obersten 16 KB
 * frei.
 *
 * xipfs.c prueft beim Uebersetzen, dass er sektorbuendig liegt und innerhalb
 * des Flash bleibt.
 */
#ifndef PBC_SAVE_ADDR
#define PBC_SAVE_ADDR 0x10FF8000u   /* 32 KB vom Flash-Ende, siehe unten */
#endif

/*
 * Geschrieben werden DREI Dateien, nicht eine:
 *   opentyrian.cfg  Einstellungen (Text)
 *   tyrian.cfg      DOS-Einstellungen (28 Byte)
 *   tyrian.sav      22 Spielstaende plus Bestenliste (~2,6 KB)
 *
 * Deshalb mehrere feste Plaetze statt eines einzigen Puffers. Ein einzelner
 * haette scheinbar funktioniert und beim Speichern des Spielstands still die
 * Einstellungen geloescht -- ein Fehler, der erst beim naechsten Start
 * auffaellt und dann nach einem Hardwareproblem aussieht.
 *
 * Feste Plaetze statt variabler Saetze, weil es hier nichts zu optimieren
 * gibt: die Anzahl der Dateien steht fest und aendert sich nie.
 */
#define PBC_SAVE_SLOTS 4
#define PBC_SAVE_SLOT_BYTES 3072u
#define PBC_SAVE_AREA (PBC_SAVE_SLOTS * PBC_SAVE_SLOT_BYTES)

void pbc_save_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PBC_XIPFS_H */
