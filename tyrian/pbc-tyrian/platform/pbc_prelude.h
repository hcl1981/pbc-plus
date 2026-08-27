/*
 * pbc_prelude.h -- wird per -include VOR jede OpenTyrian-Quelldatei gezogen.
 *
 * Zweck: die Dateizugriffe des Spiels auf das Flash-Archiv umbiegen, ohne die
 * Quellen anzufassen. OpenTyrian oeffnet seine Daten mit fopen und liest sie
 * mit fread/fseek/ftell -- alles davon gibt es hier nicht, weil es kein
 * Dateisystem gibt. Die Makros unten lenken diese Aufrufe auf xipfs um, das
 * direkt aus dem gemappten Flash liest.
 *
 * Warum Makros und keine Umbenennung in den Quellen: es sind knapp 30 Dateien
 * mit rund 70 Aufrufstellen. Jede einzeln zu aendern hiesse, den Abgleich mit
 * dem OpenTyrian-Upstream fuer immer aufzugeben. So bleiben die Quellen bis auf
 * eine Handvoll bewusst gesetzter Aenderungen (siehe PATCHES.md) identisch.
 *
 * Die FILE*-Zeiger, die hier herauskommen, sind KEINE newlib-Zeiger. Sie
 * duerfen nie an eine echte libc-Funktion geraten -- deshalb sind hier
 * saemtliche stdio-Funktionen abgefangen, die OpenTyrian benutzt, nicht nur
 * die auffaelligen.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_PRELUDE_H
#define PBC_PRELUDE_H

/* Zuerst die echten Kopfdateien -- danach duerfen die Namen umgebogen werden.
   Umgekehrt wuerde das Makro die Deklarationen in stdio.h zerschiessen. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "xipfs.h"

/* ------------------------------------------------------- Dateizugriffe */

/*
 * newlib definiert einige davon selbst als Makro (feof, getc, ...), damit sie
 * ohne Funktionsaufruf auskommen. Die muessen erst weg, sonst streitet der
 * Praeprozessor -- und im schlimmsten Fall bliebe newlibs Fassung stehen und
 * griffe in einen Zeiger, der gar kein newlib-FILE ist.
 */
#undef fopen
#undef fclose
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef rewind
#undef fgetc
#undef getc
#undef fgets
#undef feof
#undef ferror
#undef fflush
#undef fprintf
#undef fileno

#define fopen   pbc_fopen
#define fclose  pbc_fclose
#define fread   pbc_fread
#define fwrite  pbc_fwrite
#define fseek   pbc_fseek
#define ftell   pbc_ftell
#define rewind  pbc_rewind
#define fgetc   pbc_fgetc
#define getc    pbc_fgetc
#define fgets   pbc_fgets
#define feof    pbc_feof
#define fflush  pbc_fflush
#define fprintf pbc_fprintf

/*
 * fileno ist der gefaehrlichste der Namen hier: config.c ruft
 * fsync(fileno(file)) auf, und newlibs fileno greift IN die Struktur hinter
 * dem Zeiger. Der Zeiger ist aber keiner von newlib, sondern einer von xipfs.
 * Ohne diese Umleitung waere das kein Uebersetzungsfehler, sondern ein
 * Absturz zur Laufzeit -- und zwar erst beim Speichern.
 */
#define fileno  pbc_fileno

/* ------------------------------------------------------- Programmende */

/*
 * OpenTyrian beendet sich bei fehlenden Daten oder Ladefehlern mit exit().
 * Auf einem Geraet ohne Betriebssystem gibt es nichts, wohin man
 * zurueckkehren koennte -- pbc_die zeigt den Grund auf dem Bildschirm an und
 * bleibt stehen, statt in einen Reset zu laufen, bei dem niemand mehr sieht,
 * was los war.
 *
 * Hier steht bewusst KEIN Makro. exit() als Makro zu kapern kollidiert mit
 * jeder spaeteren Deklaration von exit in einer Systemkopfdatei -- und die
 * Fehlermeldung zeigt dann auf diese Zeile statt auf die Ursache. Stattdessen
 * fangen --wrap=exit und --wrap=abort den Aufruf beim Binden ab (siehe
 * CMakeLists.txt); die Ersatzfunktionen stehen in pbc_main.c.
 */
void pbc_die(const char *what) __attribute__((noreturn));

/* ------------------------------------------------------- Fliesskomma */

/*
 * Anmerkung, kein Eingriff: der RP2350 hat eine FPU nur fuer einfache
 * Genauigkeit, double wird in Software gerechnet. OpenTyrian benutzt double in
 * Palettenverlaeufen, in destruct.c (Winkel) und bei der Lautstaerke -- alles
 * Stellen, die hoechstens einmal je Bild laufen. Eine pauschale Umleitung auf
 * die float-Fassungen per Makro waere hier riskanter als nuetzlich: sie trifft
 * auch die Deklarationen in math.h und veraendert stillschweigend die
 * Genauigkeit von Vergleichen. Falls das Profil spaeter double zeigt, gehoert
 * die Aenderung an die betroffene Stelle, nicht hierher.
 */

#endif /* PBC_PRELUDE_H */
