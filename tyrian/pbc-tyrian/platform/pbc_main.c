/*
 * pbc_main -- Startpunkt auf dem Geraet, plus die letzten SDL-Kleinigkeiten.
 *
 * OpenTyrians main() wird beim Uebersetzen zu opentyrian_main() umbenannt
 * (-Dmain=opentyrian_main in CMakeLists.txt), damit opentyr.c unveraendert
 * bleiben kann. Hier davor steht, was auf einem Geraet ohne Betriebssystem
 * zuerst passieren muss: Takt, Bildschirm, Knoepfe, Datenarchiv.
 *
 * GPLv2, wie OpenTyrian.
 */

#include "SDL.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"

#include "pbc_config.h"
#include "pbc_display.h"
#include "pbc_input.h"
#include "pbc_link.h"
#include "pbc_audio.h"
#include "pbc_sys.h"
#include "xipfs.h"

/* aus OpenTyrians loudness.c */
extern bool music_disabled;

extern int opentyrian_main(int argc, char *argv[]);

/* ------------------------------------------------------------- Start */

int main(void)
{
	/*
	 * 150 MHz ist der Nennwert des RP2350 und das, womit beide anderen
	 * Portierungen auf dieser Hardware laufen. Hoeher zu takten waere
	 * verlockend -- der Bildaufbau haengt aber am SPI-Takt, nicht am Kern,
	 * und ein uebertakteter Kern verschiebt nur die Flanken im
	 * Multiplayer-Link.
	 */
	/*
	 * Kein "required": bei true haelt die SDK das Programm mit panic() an,
	 * wenn sich der Takt nicht exakt einstellen laesst. Das ist eine harte
	 * Reaktion auf etwas, das gar nicht schlimm ist -- der RP2350 startet
	 * ohnehin mit 150 MHz. Schlaegt es fehl, bleibt er einfach dabei.
	 */
	set_sys_clock_khz(150000, false);

	stdio_init_all();

	pbc_display_init();
	pbc_input_init();

	if (!xipfs_mount())
	{
		/*
		 * Das haeufigste Problem beim ersten Start: die Firmware ist geflasht,
		 * das Datenarchiv nicht. Ohne Meldung sieht das aus wie ein toter
		 * Bildschirm, und man sucht den Fehler in der Firmware.
		 */
		pbc_display_message("KEINE DATEN", "TYRIAN-DATA.UF2", "FLASHEN");
		for (;;)
			tight_loop_contents();
	}

	/*
	 * Taste B beim Einschalten: Farbtestbild statt Spiel. Es zeigt reine
	 * Balken mit Beschriftung und beantwortet damit ohne Umweg, ob die
	 * Farbreihenfolge des Panels stimmt -- ueber das Spiel ist das nur
	 * indirekt zu erschliessen.
	 */
	if (pbc_input_button_held(PBC_CHOICE_B))
	{
		pbc_display_test_pattern();
		for (;;)
			tight_loop_contents();
	}

	pbc_save_init();
	pbc_audio_init();

	/*
	 * Notausstieg fuer die Musik.
	 *
	 * Sie kostet wenig Speicher (rund 20 KB fuer die OPL-Tabellen), aber
	 * spuerbar Rechenzeit: der OPL-Chip wird in Software nachgebildet, und das
	 * laeuft 22050-mal je Sekunde in der Tonunterbrechung. Wenn die Bildrate
	 * nicht reicht, ist das der erste Hebel -- die Klaenge (Schuesse,
	 * Explosionen) bleiben dabei erhalten, weil sie den Mischer kaum belasten.
	 *
	 * Mit -DPBC_NO_MUSIC=ON beim Konfigurieren.
	 */
#ifdef PBC_NO_MUSIC
	music_disabled = true;
#endif

	static char *argv[] = { (char *)"tyrian", NULL };
	return opentyrian_main(1, argv);
}

/* ------------------------------------------------------------- Ende */

/*
 * exit() und abort() werden beim Binden hierher umgelenkt
 * (-Wl,--wrap=exit / --wrap=abort in CMakeLists.txt). Der Umweg ueber den
 * Binder statt ueber ein Makro ist Absicht: ein Makro namens "exit" bricht
 * jede spaetere Deklaration in einer Systemkopfdatei, und zwar mit einer
 * Fehlermeldung, die auf die falsche Zeile zeigt.
 */
void __wrap_exit(int status)
{
	pbc_die(status == 0 ? "SPIEL BEENDET" : "FEHLER");
}

void __wrap_abort(void)
{
	pbc_die("ABBRUCH");
}

void pbc_die(const char *what)
{
	/*
	 * exit() und abort() landen hier (umgebogen in pbc_prelude.h). Auf einem
	 * Geraet ohne Betriebssystem gibt es kein "zurueck"; stehenbleiben mit
	 * lesbarer Meldung ist mehr wert als ein Neustart, nach dem niemand mehr
	 * weiss, was passiert ist.
	 */
	pbc_link_stop();
	pbc_audio_stop();
	pbc_display_message(what ? what : "FEHLER", "", "BITTE NEU STARTEN");

	for (;;)
		tight_loop_contents();
}

/* ------------------------------------------------------- Absturzmeldung */

/*
 * Ein Speicherfehler endet auf diesem Geraet sonst in einer Endlosschleife:
 * schwarzer Bildschirm, Hintergrundlicht an, keinerlei Hinweis. Genau so sah
 * der Stapelueberlauf aus, der diesen Port zuerst lahmgelegt hat.
 *
 * Gezeigt werden Adresse und Ursache. Die Adresse laesst sich zuordnen:
 *
 *     arm-none-eabi-addr2line -f -e build/tyrian.elf 0x100xxxxx
 */

/*
 * Eigene Hexausgabe statt snprintf. Zwei Gruende: erstens laeuft das hier in
 * einer Ausnahme, und die Formatierung der C-Bibliothek ist dort nicht
 * verlaesslich (Sperren, Wiedereintritt); zweitens hat ein falscher
 * Formatbezeichner beim ersten Versuch statt der Adresse den Bezeichner selbst
 * ausgegeben -- was den Zweck der ganzen Meldung zunichte gemacht hat.
 */
static void hex8(char *out, uint32_t v)
{
	static const char digits[] = "0123456789ABCDEF";
	for (int i = 7; i >= 0; --i)
	{
		out[i] = digits[v & 0xF];
		v >>= 4;
	}
	out[8] = '\0';
}

/*
 * Kurzform der Ursache aus dem Configurable Fault Status Register. Nur die
 * Faelle, die hier ueberhaupt vorkommen koennen.
 */
static const char *fault_cause(uint32_t cfsr)
{
	if (cfsr & (1u << 4))  return "STAPEL VOLL";      /* MSTKERR   */
	if (cfsr & (1u << 3))  return "STAPEL LEER";      /* MUNSTKERR */
	if (cfsr & (1u << 1))  return "SCHREIBSPERRE";    /* DACCVIOL  */
	if (cfsr & (1u << 0))  return "CODE GESPERRT";    /* IACCVIOL  */
	if (cfsr & (1u << 9))  return "BUSFEHLER";        /* PRECISERR */
	if (cfsr & (1u << 24)) return "BEFEHL UNGUELTIG"; /* UNDEFINSTR*/
	if (cfsr & (1u << 25)) return "FALSCHER MODUS";   /* INVSTATE  */
	if (cfsr & (1u << 24 | 1u << 26)) return "SPRUNG INS LEERE";
	if (cfsr & (1u << 28)) return "UNAUSGERICHTET";   /* UNALIGNED */
	return "UNBEKANNT";
}

extern char __StackBottom;

/*
 * panic() der SDK abfangen (-Wl,--wrap=panic).
 *
 * Ohne das endet ein panic in einem Haltepunkt, der ohne angeschlossenen
 * Debugger als Speicherfehler erscheint -- die Meldung zeigt dann auf _exit
 * und sagt nichts darueber, was schiefging. Die SDK gibt panic aber einen
 * Text mit, und der benennt die Ursache meist direkt ("System clock of %u
 * kHz cannot be exactly achieved").
 *
 * Angezeigt werden der Anfang dieses Textes und die Ruecksprungadresse, also
 * die Stelle, die panic gerufen hat.
 */
void __wrap_panic(const char *fmt, ...) __attribute__((noreturn));

void __wrap_panic(const char *fmt, ...)
{
	const uint32_t caller = (uint32_t)(uintptr_t)__builtin_return_address(0);

	char where[16] = "AB ";
	hex8(where + 3, caller);

	/* Nur den Anfang des Textes -- eine Zeile fasst rund 20 Zeichen. */
	char what[21];
	unsigned i = 0;
	if (fmt != NULL)
		for (; i < sizeof what - 1 && fmt[i] != '\0' && fmt[i] != '\n'; ++i)
			what[i] = fmt[i];
	what[i] = '\0';

	pbc_display_message("SDK MELDET PANIC", what, where);

	for (;;)
		tight_loop_contents();
}

/*
 * hard_assert() der SDK abfangen (-Wl,--wrap=hard_assertion_failure).
 *
 * Sonst zeigt die Meldung nur "hard assert" und die Adresse innerhalb der
 * Meldefunktion selbst -- also nichts darueber, WELCHE Zusicherung gescheitert
 * ist. Mit dem Wrapper steht dort die Ruecksprungadresse des eigentlichen
 * Aufrufers, und die benennt ueber addr2line die genaue Zeile.
 */
void __wrap_hard_assertion_failure(void) __attribute__((noreturn));

void __wrap_hard_assertion_failure(void)
{
	const uint32_t caller = (uint32_t)(uintptr_t)__builtin_return_address(0);

	char where[16] = "AB ";
	hex8(where + 3, caller);

	pbc_display_message("SDK-ZUSICHERUNG", where, "ADDR2LINE HILFT");

	for (;;)
		tight_loop_contents();
}

void pbc_fault_report(uint32_t pc, uint32_t sp) __attribute__((noreturn, used));

void pbc_fault_report(uint32_t pc, uint32_t sp)
{
	/* SCB->CFSR liegt bei 0xE000ED28. */
	const uint32_t cfsr = *(volatile uint32_t *)0xE000ED28u;

	char line_pc[16] = "PC ";
	hex8(line_pc + 3, pc);

	/*
	 * Der Stapelzeiger sagt es zuverlaessiger als das Statusregister: liegt er
	 * unter dem Ende des Stapels, war es ein Ueberlauf -- egal, als welche
	 * Ausnahme das gemeldet wurde. Genau dieser Fall trat hier auf, und das
	 * Statusregister meldete dabei "unbekannt".
	 */
	const char *cause = (sp < (uint32_t)(uintptr_t)&__StackBottom)
	                  ? "STAPEL VOLL"
	                  : fault_cause(cfsr);

	pbc_display_message("SPEICHERFEHLER", line_pc, cause);

	for (;;)
		tight_loop_contents();
}

/*
 * Holt den gesicherten Programmzaehler aus dem Ausnahmerahmen. Bit 2 von LR
 * sagt, ob der Rahmen auf dem Haupt- oder dem Prozessstapel liegt.
 *
 * Alle vier Ausnahmen landen hier: der Stapelwaechter loest je nach
 * Einstellung als MemManage aus, und ohne freigeschaltete MemManage-Ausnahme
 * wird daraus ein HardFault.
 */
#define PBC_FAULT_ENTRY(name)                       \
	void __attribute__((naked)) name(void)          \
	{                                               \
		__asm volatile (                            \
			"movs r0, #4          \n"               \
			"mov  r1, lr          \n"               \
			"tst  r0, r1          \n"               \
			"beq  1f              \n"               \
			"mrs  r0, psp         \n"               \
			"b    2f              \n"               \
			"1:                   \n"               \
			"mrs  r0, msp         \n"               \
			"2:                   \n"               \
			"mov  r1, r0          \n"               \
			"ldr  r0, [r0, #24]   \n"               \
			"b    pbc_fault_report\n"               \
		);                                          \
	}

PBC_FAULT_ENTRY(isr_hardfault)
PBC_FAULT_ENTRY(isr_memmanage)
PBC_FAULT_ENTRY(isr_busfault)
PBC_FAULT_ENTRY(isr_usagefault)

/* --------------------------------------------------- SDL-Kleinigkeiten */

Uint32 SDL_GetTicks(void)
{
	return (Uint32)(time_us_64() / 1000u);
}

void SDL_Delay(Uint32 ms)
{
	/*
	 * Bewusst ueber den Link-Warteschritt und nicht ueber sleep_ms: OpenTyrian
	 * ruft SDL_Delay auch in Ladepausen und Zwischenbildern, und genau dort
	 * reisst eine blinde Wartezeit die Verbindung. Ohne laufendes Netzspiel
	 * verhaelt sich pbc_link_delay wie ein gewoehnliches Warten.
	 */
	pbc_link_delay(ms);
}

int SDL_Init(Uint32 flags) { (void)flags; return 0; }
int SDL_InitSubSystem(Uint32 flags) { (void)flags; return 0; }
void SDL_QuitSubSystem(Uint32 flags) { (void)flags; }
void SDL_Quit(void) { }

Uint32 SDL_WasInit(Uint32 flags) { (void)flags; return 0; }

const char *SDL_GetError(void) { return ""; }

size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen)
{
	size_t srclen = strlen(src);
	if (maxlen > 0)
	{
		size_t n = srclen < maxlen - 1 ? srclen : maxlen - 1;
		memcpy(dst, src, n);
		dst[n] = '\0';
	}
	return srclen;
}
