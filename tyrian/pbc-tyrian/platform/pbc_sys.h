/*
 * pbc_sys -- was das Programm ausserhalb von Bild, Ton und Eingabe braucht.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_SYS_H
#define PBC_SYS_H

/*
 * Endstation mit lesbarer Meldung. Hier landen exit() und abort() der
 * OpenTyrian-Quellen (ueber --wrap beim Binden) sowie alles, was der Port
 * selbst als nicht behebbar einstuft -- allen voran ein fehlendes Datenarchiv.
 *
 * Kehrt nicht zurueck und startet ausdruecklich NICHT neu: nach einem Neustart
 * saehe niemand mehr, was passiert ist.
 */
void pbc_die(const char *what) __attribute__((noreturn));

#endif /* PBC_SYS_H */
