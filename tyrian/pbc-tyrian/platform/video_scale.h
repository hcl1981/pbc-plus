/*
 * video_scale.h -- Attrappe.
 *
 * OpenTyrians Skalierer vergroessern das 320x200-Bild auf ein Desktop-Fenster.
 * Auf einem fest verbauten Panel gibt es dafuer keine Entsprechung: die
 * Bildaufteilung steht fest (siehe pbc_config.h). Der eine noch benoetigte
 * Rest ist die Liste der Skalierernamen -- config.c speichert den zuletzt
 * gewaehlten in der Konfiguration, und OpenTyrians Optionsmenue zeigt ihn an.
 *
 * Die Umsetzung steht in platform/pbc_video.c.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef VIDEO_SCALE_H
#define VIDEO_SCALE_H

#include "opentyr.h"

#include "SDL.h"

typedef void (*ScalerFunction)(SDL_Surface *src, SDL_Texture *dst);

struct Scalers
{
	int width, height;
	ScalerFunction scaler16, scaler32;
	const char *name;
};

extern uint scaler;
extern const struct Scalers scalers[];
extern const uint scalers_count;

void set_scaler_by_name(const char *name);

#endif /* VIDEO_SCALE_H */
