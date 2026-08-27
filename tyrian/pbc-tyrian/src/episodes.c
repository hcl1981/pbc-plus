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
#include "episodes.h"

#include <string.h>

#include "config.h"
#include "file.h"
#include "lvllib.h"
#include "lvlmast.h"
#include "opentyr.h"

/* MAIN Weapons Data */
JE_WeaponPortType weaponPort;
const JE_WeaponType *weapons;  /* zeigt ins Flash-Archiv, siehe episodes.h */

/* Items */
JE_PowerType   powerSys;
JE_ShipType    ships;
JE_OptionType  options[OPTION_NUM + 1]; /* [0..optionnum] */
JE_ShieldType  shields;
JE_SpecialType special;

/* Enemy data */
const JE_EnemyDatEntry *enemyDat;   /* zeigt ins Flash-Archiv, siehe episodes.h */
JE_EnemyDatEntry enemyDat_scratch;  /* Eintrag 0, beschreibbare Vorlage */

/* EPISODE variables */
JE_byte    initial_episode_num, episodeNum = 0;
JE_boolean episodeAvail[EPISODE_MAX]; /* [1..episodemax] */
char       episode_file[13], cube_file[13];

JE_longint episode1DataLoc;

/* Tells the game whether the level currently loaded is a bonus level. */
JE_boolean bonusLevel;

/* Tells if the game jumped back to Episode 1 */
JE_boolean jumpBackToEpisode1;

void JE_loadItemDat(void)
{
	FILE *f = NULL;
	
	if (episodeNum <= 3)
	{
		f = dir_fopen_die(data_dir(), "tyrian.hdt", "rb");
		fread_s32_die(&episode1DataLoc, 1, f);
		fseek(f, episode1DataLoc, SEEK_SET);
	}
	else
	{
		// episode 4 stores item data in the level file
		f = dir_fopen_die(data_dir(), levelFile, "rb");
		fseek(f, lvlPos[lvlNum-1], SEEK_SET);
	}

	JE_word itemNum[7]; /* [1..7] */
	fread_u16_die(itemNum, 7, f);

	/*
	 * PicoBoy-Aenderung: kopierfrei statt Feld fuer Feld.
	 *
	 * Der Test unten ist die Zusicherung, auf der das beruht: 80 Byte sind
	 * genau die Summe der Felder. Faenge irgendwann ein Uebersetzer an, zu
	 * polstern, oder aendere jemand die Struktur, schlaegt das hier fehl statt
	 * sich als verschobene Waffenwerte im Spiel zu zeigen.
	 *
	 * Unausgerichtete Zugriffe sind unkritisch: der Cortex-M33 erlaubt sie fuer
	 * 16- und 32-Bit-Zugriffe, und die Struktur verlangt ohnehin nur
	 * 2-Byte-Ausrichtung.
	 */
	_Static_assert(sizeof(JE_WeaponType) == 80, "JE_WeaponType hat Polsterung -- kopierfreies Laden nicht moeglich");

	weapons = (const JE_WeaponType *)xipfs_inplace(f, sizeof(JE_WeaponType) * (WEAP_NUM + 1));
	if (weapons == NULL)
	{
		fprintf(stderr, "error: weapon data not memory-mapped\n");
		exit(EXIT_FAILURE);
	}
	
	for (int i = 0; i < PORT_NUM + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                   1, f);
		fread_die(    &weaponPort[i].name,    1, 30, f);
		weaponPort[i].name[MIN(nameLen, 30)] = '\0';
		fread_u8_die( &weaponPort[i].opnum,       1, f);
		fread_u16_die( weaponPort[i].op[0],      11, f);
		fread_u16_die( weaponPort[i].op[1],      11, f);
		fread_u16_die(&weaponPort[i].cost,        1, f);
		fread_u16_die(&weaponPort[i].itemgraphic, 1, f);
		fread_u16_die(&weaponPort[i].poweruse,    1, f);
	}

	for (int i = 0; i < SPECIAL_NUM + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                1, f);
		fread_die(    &special[i].name,    1, 30, f);
		special[i].name[MIN(nameLen, 30)] = '\0';
		fread_u16_die(&special[i].itemgraphic, 1, f);
		fread_u8_die( &special[i].pwr,         1, f);
		fread_u8_die( &special[i].stype,       1, f);
		fread_u16_die(&special[i].wpn,         1, f);
	}

	for (int i = 0; i < POWER_NUM + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                 1, f);
		fread_die(    &powerSys[i].name,    1, 30, f);
		powerSys[i].name[MIN(nameLen, 30)] = '\0';
		fread_u16_die(&powerSys[i].itemgraphic, 1, f);
		fread_u8_die( &powerSys[i].power,       1, f);
		fread_s8_die( &powerSys[i].speed,       1, f);
		fread_u16_die(&powerSys[i].cost,        1, f);
	}

	for (int i = 0; i < SHIP_NUM + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                 1, f);
		fread_die(    &ships[i].name,       1, 30, f);
		ships[i].name[MIN(nameLen, 30)] = '\0';
		fread_u16_die(&ships[i].shipgraphic,    1, f);
		fread_u16_die(&ships[i].itemgraphic,    1, f);
		fread_u8_die( &ships[i].ani,            1, f);
		fread_s8_die( &ships[i].spd,            1, f);
		fread_u8_die( &ships[i].dmg,            1, f);
		fread_u16_die(&ships[i].cost,           1, f);
		fread_u8_die( &ships[i].bigshipgraphic, 1, f);
	}

	for (int i = 0; i < OPTION_NUM + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die(  &nameLen,                1, f);
		fread_die(     &options[i].name,    1, 30, f);
		options[i].name[MIN(nameLen, 30)] = '\0';
		fread_u8_die(  &options[i].pwr,         1, f);
		fread_u16_die( &options[i].itemgraphic, 1, f);
		fread_u16_die( &options[i].cost,        1, f);
		fread_u8_die(  &options[i].tr,          1, f);
		fread_u8_die(  &options[i].option,      1, f);
		fread_s8_die(  &options[i].opspd,       1, f);
		fread_u8_die(  &options[i].ani,         1, f);
		fread_u16_die(  options[i].gr,         20, f);
		fread_u8_die(  &options[i].wport,       1, f);
		fread_u16_die( &options[i].wpnum,       1, f);
		fread_u8_die(  &options[i].ammo,        1, f);
		fread_bool_die(&options[i].stop,           f);
		fread_u8_die(  &options[i].icongr,      1, f);
	}

	for (int i = 0; i < SHIELD_NUM + 1; ++i)
	{
		Uint8 nameLen;
		fread_u8_die( &nameLen,                1, f);
		fread_die(    &shields[i].name,    1, 30, f);
		shields[i].name[MIN(nameLen, 30)] = '\0';
		fread_u8_die( &shields[i].tpwr,        1, f);
		fread_u8_die( &shields[i].mpwr,        1, f);
		fread_u16_die(&shields[i].itemgraphic, 1, f);
		fread_u16_die(&shields[i].cost,        1, f);
	}
	
	/*
	 * PicoBoy-Aenderung: kopierfrei, wie bei den Waffen. Die Zusicherung
	 * unten ist der Grund, warum das sicher ist -- 76 Byte sind genau die
	 * Feldsumme, also enthaelt die Datei bereits das Speicherabbild.
	 */
	_Static_assert(sizeof(JE_EnemyDatEntry) == 77, "JE_EnemyDatEntry hat Polsterung -- kopierfreies Laden nicht moeglich");

	enemyDat = (const JE_EnemyDatEntry *)xipfs_inplace(f, sizeof(JE_EnemyDatEntry) * (ENEMY_NUM + 1));
	if (enemyDat == NULL)
	{
		fprintf(stderr, "error: enemy data not memory-mapped\n");
		exit(EXIT_FAILURE);
	}

	/* Eintrag 0 ins RAM spiegeln -- er wird als Vorlage beschrieben. */
	memcpy(&enemyDat_scratch, &enemyDat[0], sizeof enemyDat_scratch);
	
	fclose(f);
}

void JE_initEpisode(JE_byte newEpisode)
{
	if (newEpisode == episodeNum)
		return;
	
	episodeNum = newEpisode;
	
	snprintf(levelFile,    sizeof(levelFile),    "tyrian%d.lvl",  episodeNum);
	snprintf(cube_file,    sizeof(cube_file),    "cubetxt%d.dat", episodeNum);
	snprintf(episode_file, sizeof(episode_file), "levels%d.dat",  episodeNum);
	
	JE_analyzeLevel();
	JE_loadItemDat();
}

void JE_scanForEpisodes(void)
{
	for (int i = 0; i < EPISODE_MAX; ++i)
	{
		char ep_file[20];
		snprintf(ep_file, sizeof(ep_file), "tyrian%d.lvl", i + 1);
		episodeAvail[i] = dir_file_exists(data_dir(), ep_file);
	}
}

unsigned int JE_findNextEpisode(void)
{
	unsigned int newEpisode = episodeNum;
	
	jumpBackToEpisode1 = false;
	
	while (true)
	{
		newEpisode++;
		
		if (newEpisode > EPISODE_MAX)
		{
			newEpisode = 1;
			jumpBackToEpisode1 = true;
			gameHasRepeated = true;
		}
		
		if (episodeAvail[newEpisode-1] || newEpisode == episodeNum)
		{
			break;
		}
	}
	
	return newEpisode;
}
