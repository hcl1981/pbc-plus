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
#ifndef EPISODES_H
#define EPISODES_H

#include "opentyr.h"

#include "lvlmast.h"

/* Episodes and general data */

#define FIRST_LEVEL 1
#define EPISODE_MAX 5
#define EPISODE_AVAILABLE 4

typedef struct
{
	JE_word     drain;
	JE_byte     shotrepeat;
	JE_byte     multi;
	JE_word     weapani;
	JE_byte     max;
	JE_byte     tx, ty, aim;
	JE_byte     attack[8], del[8]; /* [1..8] */
	JE_shortint sx[8], sy[8]; /* [1..8] */
	JE_shortint bx[8], by[8]; /* [1..8] */
	JE_word     sg[8]; /* [1..8] */
	JE_shortint acceleration, accelerationx;
	JE_byte     circlesize;
	JE_byte     sound;
	JE_byte     trail;
	JE_byte     shipblastfilter;
}
/*
 * PicoBoy-Aenderung: packed.
 *
 * Nicht wegen der Groesse -- die Struktur hat ohnehin keine Polsterung --,
 * sondern wegen der AUSRICHTUNG. Sie liegt im Flash an der Stelle, an der
 * tyrian.hdt sie enthaelt, und das ist eine ungerade Adresse. Ohne packed
 * nimmt der Uebersetzer 2-Byte-Ausrichtung an und darf dann Befehle waehlen,
 * die unausgerichtet nicht arbeiten (LDRD, LDM) -- das ergibt einen
 * Speicherfehler beim Laden der Waffendaten, also genau dann, wenn ein Level
 * beginnt.
 *
 * Mit packed erzeugt der Uebersetzer ausrichtungssichere Zugriffe. Die kosten
 * ein paar Takte, aber die Waffentabelle wird beim Levelstart gelesen, nicht
 * je Bildpunkt.
 */
__attribute__((packed))
JE_WeaponType;

typedef struct
{
	char    name[31]; /* string [30] */
	JE_byte opnum;
	JE_word op[2][11]; /* [1..2, 1..11] */
	JE_word cost;
	JE_word itemgraphic;
	JE_word poweruse;
} JE_WeaponPortType[PORT_NUM + 1]; /* [0..portnum] */

typedef struct
{
	char        name[31]; /* string [30] */
	JE_word     itemgraphic;
	JE_byte     power;
	JE_shortint speed;
	JE_word     cost;
} JE_PowerType[POWER_NUM + 1]; /* [0..powernum] */

typedef struct
{
	char    name[31]; /* string [30] */
	JE_word itemgraphic;
	JE_byte pwr;
	JE_byte stype;
	JE_word wpn;
} JE_SpecialType[SPECIAL_NUM + 1]; /* [0..specialnum] */

typedef struct
{
	char        name[31]; /* string [30] */
	JE_byte     pwr;
	JE_word     itemgraphic;
	JE_word     cost;
	JE_byte     tr, option;
	JE_shortint opspd;
	JE_byte     ani;
	JE_word     gr[20]; /* [1..20] */
	JE_byte     wport;
	JE_word     wpnum;
	JE_byte     ammo;
	JE_boolean  stop;
	JE_byte     icongr;
} JE_OptionType;

typedef struct
{
	char    name[31]; /* string [30] */
	JE_byte tpwr;
	JE_byte mpwr;
	JE_word itemgraphic;
	JE_word cost;
} JE_ShieldType[SHIELD_NUM + 1]; /* [0..shieldnum] */

typedef struct
{
	char        name[31]; /* string [30] */
	JE_word     shipgraphic;
	JE_word     itemgraphic;
	JE_byte     ani;
	JE_shortint spd;
	JE_byte     dmg;
	JE_word     cost;
	JE_byte     bigshipgraphic;
} JE_ShipType[SHIP_NUM + 1]; /* [0..shipnum] */

/* EnemyData */
typedef struct
{
	JE_byte     ani;
	JE_byte     tur[3]; /* [1..3] */
	JE_byte     freq[3]; /* [1..3] */
	JE_shortint xmove;
	JE_shortint ymove;
	JE_shortint xaccel;
	JE_shortint yaccel;
	JE_shortint xcaccel;
	JE_shortint ycaccel;
	JE_integer  startx;
	JE_integer  starty;
	JE_shortint startxc;
	JE_shortint startyc;
	JE_byte     armor;
	JE_byte     esize;
	JE_word     egraphic[20];  /* [1..20] */
	JE_byte     explosiontype;
	JE_byte     animate;       /* 0:Not Yet   1:Always   2:When Firing Only */
	JE_byte     shapebank;     /* See LEVELMAK.DOC */
	JE_shortint xrev, yrev;
	JE_word     dgr;
	JE_shortint dlevel;
	JE_shortint dani;
	JE_byte     elaunchfreq;
	JE_word     elaunchtype;
	JE_integer  value;
	JE_word     eenemydie;
}
/*
 * PicoBoy-Aenderung: packed.
 *
 * Ohne Packung legt der Uebersetzer vor "startx" ein Fuellbyte ein (das Feld
 * beginnt bei Byte 13 und will gerade stehen); die Struktur ist dann 80 statt
 * 77 Byte und passt NICHT mehr auf die Datei. Gepackt sind Dateiinhalt und
 * Speicherabbild identisch, und die 68 KB koennen im Flash bleiben.
 *
 * Unausgerichtete 16-Bit-Zugriffe sind auf dem Cortex-M33 erlaubt und kosten
 * hoechstens einen Takt -- die Struktur wird beim Erzeugen eines Gegners
 * gelesen, nicht je Bildpunkt.
 */
__attribute__((packed))
JE_EnemyDatEntry;

typedef JE_EnemyDatEntry JE_EnemyDatType[ENEMY_NUM + 1]; /* [0..enemynum] */

extern JE_WeaponPortType weaponPort;
/*
 * PicoBoy-Aenderung: Zeiger statt Feld.
 *
 * Die 781 Waffendatensaetze sind 62 KB und rein lesend. JE_WeaponType hat
 * ausschliesslich 8- und 16-Bit-Felder und ist mit 80 Byte exakt so gross wie
 * die Summe seiner Felder -- es gibt keinen Verschnitt. Weil die
 * Deklarationsreihenfolge zudem der Lesereihenfolge in JE_loadItemDat
 * entspricht, ist der Dateiinhalt Byte fuer Byte bereits das Speicherabbild.
 * Der Zeiger kann deshalb direkt ins gemappte Flash zeigen.
 *
 * Die Schreibweise weapons[i].feld bleibt unveraendert gueltig.
 */
extern const JE_WeaponType *weapons; /* [0..weapnum], zeigt ins Flash-Archiv */
extern JE_PowerType powerSys;
extern JE_ShipType ships;
extern JE_OptionType options[OPTION_NUM + 1]; /* [0..optionnum] */
extern JE_ShieldType shields;
extern JE_SpecialType special;
/*
 * PicoBoy-Aenderung: Zeiger ins Flash-Archiv statt Feld im RAM (68 KB).
 *
 * Ausnahme ist Eintrag 0: die Ereignisse 49..52 benutzen ihn als VORLAGE und
 * schreiben Panzerung und Grafik hinein, bevor sie den Gegner erzeugen. Er
 * liegt deshalb als Kopie im RAM (enemyDat_scratch) und wird in JE_makeEnemy
 * anstelle des Flash-Eintrags herangezogen.
 *
 * Nur dort ist das noetig: die uebrigen Lesestellen greifen auf startyc, esize
 * und value zu, und die veraendert die Vorlage nicht.
 */
extern const JE_EnemyDatEntry *enemyDat;
extern JE_EnemyDatEntry enemyDat_scratch;
extern JE_byte initial_episode_num, episodeNum;
extern JE_boolean episodeAvail[EPISODE_MAX];

extern char episode_file[13], cube_file[13];

extern JE_longint episode1DataLoc;
extern JE_boolean bonusLevel;
extern JE_boolean jumpBackToEpisode1;

void JE_loadItemDat(void);
void JE_initEpisode(JE_byte newEpisode);
unsigned int JE_findNextEpisode(void);
void JE_scanForEpisodes(void);

#endif /* EPISODES_H */
