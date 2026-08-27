/* 
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "palette.h"

#include "file.h"
#include "keyboard.h"
#include "nortsong.h"
#include "opentyr.h"
#include "video.h"

#include <assert.h>

static Uint32 rgb_to_yuv(int r, int g, int b);

#define PALETTE_COUNT 23

Palette *palettes;  /* Flash-Archiv, siehe palette.h */
int palette_count;

static Palette palette;
Uint32 rgb_palette[256], yuv_palette[256];

Palette colors;

void JE_loadPals(void)
{
	FILE *f = dir_fopen_die(data_dir(), "palette.dat", "rb");

	/*
	 * PicoBoy-Aenderung: kopierfrei. Die Streckung von 6 auf 8 Bit je
	 * Farbanteil hat tools/mkdata.py bereits erledigt, die Datei enthaelt also
	 * fertige SDL_Color-Werte.
	 */
	palette_count = ftell_eof(f) / sizeof(Palette);
	assert(palette_count == PALETTE_COUNT);

	palettes = (Palette *)xipfs_inplace(f, sizeof(Palette) * (size_t)palette_count);
	if (palettes == NULL)
	{
		fprintf(stderr, "error: palette data not memory-mapped\n");
		exit(EXIT_FAILURE);
	}

	fclose(f);
}

void set_palette(Palette colors, unsigned int first_color, unsigned int last_color)
{
	for (uint i = first_color; i <= last_color; ++i)
	{
		palette[i] = colors[i];
		rgb_palette[i] = SDL_MapRGB(main_window_tex_format, palette[i].r, palette[i].g, palette[i].b);
		yuv_palette[i] = rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b);
	}
}

void set_colors(SDL_Color color, unsigned int first_color, unsigned int last_color)
{
	for (uint i = first_color; i <= last_color; ++i)
	{
		palette[i] = color;
		rgb_palette[i] = SDL_MapRGB(main_window_tex_format, palette[i].r, palette[i].g, palette[i].b);
		yuv_palette[i] = rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b);
	}
}

void init_step_fade_palette(int diff[256][3], Palette colors, unsigned int first_color, unsigned int last_color)
{
	for (unsigned int i = first_color; i <= last_color; i++)
	{
		diff[i][0] = (int)colors[i].r - palette[i].r;
		diff[i][1] = (int)colors[i].g - palette[i].g;
		diff[i][2] = (int)colors[i].b - palette[i].b;
	}
}

void init_step_fade_solid(int diff[256][3], SDL_Color color, unsigned int first_color, unsigned int last_color)
{
	for (unsigned int i = first_color; i <= last_color; i++)
	{
		diff[i][0] = (int)color.r - palette[i].r;
		diff[i][1] = (int)color.g - palette[i].g;
		diff[i][2] = (int)color.b - palette[i].b;
	}
}

void step_fade_palette(int diff[256][3], int steps, unsigned int first_color, unsigned int last_color)
{
	assert(steps > 0);
	
	for (unsigned int i = first_color; i <= last_color; i++)
	{
		const int delta[3] = { diff[i][0] / steps, diff[i][1] / steps, diff[i][2] / steps };
		
		diff[i][0] -= delta[0];
		diff[i][1] -= delta[1];
		diff[i][2] -= delta[2];
		
		palette[i].r += delta[0];
		palette[i].g += delta[1];
		palette[i].b += delta[2];
		
		rgb_palette[i] = SDL_MapRGB(main_window_tex_format, palette[i].r, palette[i].g, palette[i].b);
		yuv_palette[i] = rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b);
	}
}

void fade_palette(Palette colors, int steps, unsigned int first_color, unsigned int last_color)
{
	assert(steps > 0);
	
	static int diff[256][3];
	init_step_fade_palette(diff, colors, first_color, last_color);
	
	for (; steps > 0; steps--)
	{
		setFrameCount(1);
		
		step_fade_palette(diff, steps, first_color, last_color);
		
		JE_showVGA();
		
		waitUntilElapsed();
	}

	// Discard input during fade.
	keyboardClearInput();
	mouseClearInput();
}

void fade_solid(SDL_Color color, int steps, unsigned int first_color, unsigned int last_color)
{
	assert(steps > 0);
	
	static int diff[256][3];
	init_step_fade_solid(diff, color, first_color, last_color);
	
	for (; steps > 0; steps--)
	{
		setFrameCount(1);
		
		step_fade_palette(diff, steps, first_color, last_color);
		
		JE_showVGA();
		
		waitUntilElapsed();
	}

	// Discard input during fade.
	keyboardClearInput();
	mouseClearInput();
}

void fade_black(int steps)
{
	SDL_Color black = { 0, 0, 0 };
	fade_solid(black, steps, 0, 255);
}

void fade_white(int steps)
{
	SDL_Color white = { 255, 255, 255 };
	fade_solid(white, steps, 0, 255);
}

static Uint32 rgb_to_yuv(int r, int g, int b)
{
	int y = (r + g + b) >> 2,
	    u = 128 + ((r - b) >> 2),
	    v = 128 + ((-r + 2 * g - b) >> 3);
	return (y << 16) + (u << 8) + v;
}
