/* Automatisch erzeugt von tools/convert_assets.py - nicht von Hand aendern.
 * Inhalt: Originaldaten von Jump 'n Bump (c) 1998 Brainchild Design, GPL2+.
 */
#pragma once
#include <stdint.h>

typedef struct {
  uint8_t  w, h;
  int8_t   hs_x, hs_y;
  uint32_t ofs;
} JnbGobEntry;

#define JNB_NUM_RABBIT  72
#define JNB_NUM_OBJECTS 80
#define JNB_NUM_NUMBERS 10
#define JNB_NUM_FONT    81

extern const uint8_t rabbit_pixels[15824];
extern const JnbGobEntry rabbit_gob[72];
extern const uint8_t objects_pixels[4492];
extern const JnbGobEntry objects_gob[80];
extern const uint8_t numbers_pixels[3520];
extern const JnbGobEntry numbers_gob[10];
extern const uint8_t font_pixels[5319];
extern const JnbGobEntry font_gob[81];
