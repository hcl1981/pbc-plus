/*
 * pb_input.h
 *
 * Strukt zur Speicherung des aktuellen Eingabezustands. Lebt extra in einem
 * Header (und nicht in der .ino), weil der Arduino-Autoprototype-Generator
 * sonst Forward-Declarations fuer Funktionen wie read_buttons() VOR die
 * Strukt-Definition setzt - was dann zu "'PbInput' does not name a type"
 * fuehrt. Aus einem Header geladen ist der Typ vor allen Forward-Decls da.
 */

#ifndef PB_INPUT_H
#define PB_INPUT_H

#include <stdbool.h>

struct PbInput {
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    bool btn_a;
    bool btn_b;
    bool btn_start;
    bool btn_select;
};

#endif // PB_INPUT_H
