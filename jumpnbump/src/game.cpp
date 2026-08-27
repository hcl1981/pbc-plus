// ============================================================================
//  Spielmechanik - direkte Portierung von main.c / network.c des Originals.
//  Alle Konstanten, Reihenfolgen und Sonderfaelle sind uebernommen: Festkomma
//  16.16, Beschleunigung 16384 bzw. 12288, Eis mit 768/1024, Wasserauftrieb
//  1536, Sprungfeder -400000, Schwerkraft 12288 usw.
// ============================================================================
#include "jnb.h"

player_t player[JNB_MAX_PLAYERS];
object_t objects[NUM_OBJECTS];
fly_t flies[NUM_FLIES];
int flies_enabled = 1;

// Die Kachelkarte kommt aus levelmap.txt des Originals (siehe read_level()) und
// steht in assets_level.cpp: 0 leer, 1 fest, 2 Wasser, 3 Eis, 4 Sprungfeder.
// Eine Kachel ist 16x16 Pixel.
#define ban_map jnb_ban_map

// clang-format off
// Animationen der Hasen (player_anim_data aus init_program).
// Reihenfolge: stehen, laufen, springen, fallen, ins Wasser, schwimmen, sterben.
static const int16_t player_anim_data[7][10] = {
	{1, 0, 0, 0x7fff, 0, 0, 0, 0, 0, 0},
	{4, 0, 0, 4, 1, 4, 2, 4, 3, 4},
	{1, 0, 4, 0x7fff, 0, 0, 0, 0, 0, 0},
	{4, 2, 5, 8, 6, 10, 7, 3, 6, 3},
	{1, 0, 6, 0x7fff, 0, 0, 0, 0, 0, 0},
	{2, 1, 5, 8, 4, 0x7fff, 0, 0, 0, 0},
	{1, 0, 8, 5, 0, 0, 0, 0, 0, 0}};

// Objektanimationen (object_anims aus main.c).
static const struct {
	uint8_t num_frames;
	uint8_t restart_frame;
	struct { uint8_t image; int16_t ticks; } frame[10];
} object_anims[8] = {
	{6, 0, {{0,3},{1,3},{2,3},{3,3},{4,3},{5,3},{0,0},{0,0},{0,0},{0,0}}},
	{9, 0, {{6,2},{7,2},{8,2},{9,2},{10,2},{11,2},{12,2},{13,2},{14,2},{0,0}}},
	{5, 0, {{15,3},{16,3},{16,3},{17,3},{18,3},{19,3},{0,0},{0,0},{0,0},{0,0}}},
	{10,0, {{20,2},{21,2},{22,2},{23,2},{24,2},{25,2},{24,2},{23,2},{22,2},{21,2}}},
	{10,0, {{26,2},{27,2},{28,2},{29,2},{30,2},{31,2},{30,2},{29,2},{28,2},{27,2}}},
	{10,0, {{32,2},{33,2},{34,2},{35,2},{36,2},{37,2},{36,2},{35,2},{34,2},{33,2}}},
	{10,0, {{38,2},{39,2},{40,2},{41,2},{42,2},{43,2},{42,2},{41,2},{40,2},{39,2}}},
	{4, 0, {{76,4},{77,4},{78,4},{79,4},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}}}};
// clang-format on

// ---------------------------------------------------------------------------
// Zufall. Das Original nutzt rand()%max; hier ein eigener Generator, damit
// beide Geraete unabhaengig voneinander, aber gleich "lebendig" wuerfeln.
static uint32_t rndState = 0x2545F491u;
unsigned short rnd(unsigned short max) {
  rndState ^= rndState << 13;
  rndState ^= rndState >> 17;
  rndState ^= rndState << 5;
  if (max == 0) return 0;
  return (unsigned short)((rndState >> 8) % max);
}
void rndSeed(uint32_t s) { rndState = s ? s : 1; }

// GET_BAN_MAP_XY mit Bereichsschutz. Das Original liest ausserhalb des Feldes,
// wenn ein Hase ueber den oberen Rand hinausfliegt; hier ist "oberhalb" leerer
// Himmel und "unterhalb" fester Boden - das entspricht dem sichtbaren Level.
int ban(int x, int y) {
  int tx = x >> 4, ty = y >> 4;
  if (ty < 0) return BAN_VOID;
  if (ty > 16) ty = 16;
  if (tx < 0) tx = 0;
  if (tx > 21) tx = 21;
  return (int)ban_map[ty][tx];
}
#define GET_BAN_MAP_XY(x, y) ban((x), (y))
#define GET_BAN_MAP_IN_WATER(s1, s2)                                            \
  ((GET_BAN_MAP_XY((s1), ((s2) + 7)) == BAN_VOID ||                             \
    GET_BAN_MAP_XY(((s1) + 15), ((s2) + 7)) == BAN_VOID) &&                     \
   (GET_BAN_MAP_XY((s1), ((s2) + 8)) == BAN_WATER ||                            \
    GET_BAN_MAP_XY(((s1) + 15), ((s2) + 8)) == BAN_WATER))

static inline void set_anim(int c1, int a) {
  player[c1].anim = a;
  player[c1].frame = 0;
  player[c1].frame_tick = 0;
  player[c1].image = player_anim_data[a][2] + player[c1].direction * 9;
}

// ---------------------------------------------------------------------------
void add_object(int type, int x, int y, int x_add, int y_add, int anim, int frame) {
  for (int c1 = 0; c1 < NUM_OBJECTS; c1++) {
    if (objects[c1].used == 0) {
      objects[c1].used = 1;
      objects[c1].type = type;
      objects[c1].x = (int)((uint32_t)x << 16);
      objects[c1].y = (int)((uint32_t)y << 16);
      objects[c1].x_add = x_add;
      objects[c1].y_add = y_add;
      objects[c1].x_acc = 0;
      objects[c1].y_acc = 0;
      objects[c1].anim = anim;
      objects[c1].frame = frame;
      // Fell und Fleisch tragen die Bildnummer direkt im Feld "frame" (so auch
      // im Original) - dann darf die Animationstabelle nicht befragt werden.
      if (frame < object_anims[anim].num_frames) {
        objects[c1].ticks = object_anims[anim].frame[frame].ticks;
        objects[c1].image = object_anims[anim].frame[frame].image;
      } else {
        objects[c1].ticks = 1;
        objects[c1].image = frame;
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Original: player_action_left / player_action_right.
// Enthaelt das Rutschen auf Eis (kleine Beschleunigung, kein Bremsen) und die
// Staubwolken beim Abbremsen auf festem Boden.
static void player_action_left(int c1) {
  int s1 = (player[c1].x >> 16);
  int s2 = (player[c1].y >> 16);
  int below_left = GET_BAN_MAP_XY(s1, s2 + 16);
  int below = GET_BAN_MAP_XY(s1 + 8, s2 + 16);
  int below_right = GET_BAN_MAP_XY(s1 + 15, s2 + 16);

  if (below == BAN_ICE) {
    if (player[c1].x_add > 0) player[c1].x_add -= 1024;
    else player[c1].x_add -= 768;
  } else if ((below_left != BAN_SOLID && below_right == BAN_ICE) ||
             (below_left == BAN_ICE && below_right != BAN_SOLID)) {
    if (player[c1].x_add > 0) player[c1].x_add -= 1024;
    else player[c1].x_add -= 768;
  } else {
    if (player[c1].x_add > 0) {
      player[c1].x_add -= 16384;
      if (player[c1].x_add > -98304L && player[c1].in_water == 0 && below == BAN_SOLID) {
        add_object(OBJ_SMOKE, (player[c1].x >> 16) + 2 + rnd(9), (player[c1].y >> 16) + 13 + rnd(5),
                   0, -16384 - rnd(8192), OBJ_ANIM_SMOKE, 0);
        player[c1].sfxflags |= SF_SMOKE;
      }
    } else
      player[c1].x_add -= 12288;
  }
  if (player[c1].x_add < -98304L) player[c1].x_add = -98304L;
  player[c1].direction = 1;
  if (player[c1].anim == 0) set_anim(c1, 1);
}

static void player_action_right(int c1) {
  int s1 = (player[c1].x >> 16);
  int s2 = (player[c1].y >> 16);
  int below_left = GET_BAN_MAP_XY(s1, s2 + 16);
  int below = GET_BAN_MAP_XY(s1 + 8, s2 + 16);
  int below_right = GET_BAN_MAP_XY(s1 + 15, s2 + 16);

  if (below == BAN_ICE) {
    if (player[c1].x_add < 0) player[c1].x_add += 1024;
    else player[c1].x_add += 768;
  } else if ((below_left != BAN_SOLID && below_right == BAN_ICE) ||
             (below_left == BAN_ICE && below_right != BAN_SOLID)) {
    if (player[c1].x_add > 0) player[c1].x_add += 1024;
    else player[c1].x_add += 768;
  } else {
    if (player[c1].x_add < 0) {
      player[c1].x_add += 16384;
      if (player[c1].x_add < 98304L && player[c1].in_water == 0 && below == BAN_SOLID) {
        add_object(OBJ_SMOKE, (player[c1].x >> 16) + 2 + rnd(9), (player[c1].y >> 16) + 13 + rnd(5),
                   0, -16384 - rnd(8192), OBJ_ANIM_SMOKE, 0);
        player[c1].sfxflags |= SF_SMOKE;
      }
    } else
      player[c1].x_add += 12288;
  }
  if (player[c1].x_add > 98304L) player[c1].x_add = 98304L;
  player[c1].direction = 0;
  if (player[c1].anim == 0) set_anim(c1, 1);
}

// ---------------------------------------------------------------------------
void steer_players(void) {
  int s1 = 0, s2 = 0;

  for (int c1 = 0; c1 < JNB_MAX_PLAYERS; c1++) {
    if (player[c1].enabled != 1) continue;

    if (player[c1].dead_flag == 0) {
      if (player[c1].action_left && player[c1].action_right) {
        if (player[c1].direction == 0) player_action_right(c1);
        else player_action_left(c1);
      } else if (player[c1].action_left) {
        player_action_left(c1);
      } else if (player[c1].action_right) {
        player_action_right(c1);
      } else {
        s1 = (player[c1].x >> 16);
        s2 = (player[c1].y >> 16);
        int below_left = GET_BAN_MAP_XY(s1, s2 + 16);
        int below = GET_BAN_MAP_XY(s1 + 8, s2 + 16);
        int below_right = GET_BAN_MAP_XY(s1 + 15, s2 + 16);
        if (below == BAN_SOLID || below == BAN_SPRING ||
            (((below_left == BAN_SOLID || below_left == BAN_SPRING) && below_right != BAN_ICE) ||
             (below_left != BAN_ICE && (below_right == BAN_SOLID || below_right == BAN_SPRING)))) {
          if (player[c1].x_add < 0) {
            player[c1].x_add += 16384;
            if (player[c1].x_add > 0) player[c1].x_add = 0;
          } else {
            player[c1].x_add -= 16384;
            if (player[c1].x_add < 0) player[c1].x_add = 0;
          }
          if (player[c1].x_add != 0 && GET_BAN_MAP_XY((s1 + 8), (s2 + 16)) == BAN_SOLID) {
            add_object(OBJ_SMOKE, (player[c1].x >> 16) + 2 + rnd(9), (player[c1].y >> 16) + 13 + rnd(5),
                       0, -16384 - rnd(8192), OBJ_ANIM_SMOKE, 0);
            player[c1].sfxflags |= SF_SMOKE;
          }
        }
        if (player[c1].anim == 1) set_anim(c1, 0);
      }

      // Springen
      if (player[c1].jump_ready == 1 && player[c1].action_up) {
        s1 = (player[c1].x >> 16);
        s2 = (player[c1].y >> 16);
        if (s2 < -16) s2 = -16;
        if (GET_BAN_MAP_XY(s1, (s2 + 16)) == BAN_SOLID || GET_BAN_MAP_XY(s1, (s2 + 16)) == BAN_ICE ||
            GET_BAN_MAP_XY((s1 + 15), (s2 + 16)) == BAN_SOLID ||
            GET_BAN_MAP_XY((s1 + 15), (s2 + 16)) == BAN_ICE) {
          player[c1].y_add = -280000L;
          set_anim(c1, 2);
          player[c1].jump_ready = 0;
          player[c1].jump_abort = 1;
          player[c1].sfxflags |= SF_JUMP;
        }
        // Sprung aus dem Wasser heraus
        if (GET_BAN_MAP_IN_WATER(s1, s2)) {
          player[c1].y_add = -196608L;
          player[c1].in_water = 0;
          set_anim(c1, 2);
          player[c1].jump_ready = 0;
          player[c1].jump_abort = 1;
          player[c1].sfxflags |= SF_JUMP;
        }
      }
      // Sprungtaste losgelassen -> Sprung abbrechen (kurzer Hupf)
      if (!player[c1].action_up) {
        player[c1].jump_ready = 1;
        if (player[c1].in_water == 0 && player[c1].y_add < 0 && player[c1].jump_abort == 1) {
          player[c1].y_add += 32768;
          if (player[c1].y_add > 0) player[c1].y_add = 0;
        }
      }

      player[c1].x += player[c1].x_add;
      if ((player[c1].x >> 16) < 0) {
        player[c1].x = 0;
        player[c1].x_add = 0;
      }
      if ((player[c1].x >> 16) + 15 > 351) {
        player[c1].x = 336L << 16;
        player[c1].x_add = 0;
      }
      {
        if (player[c1].y > 0) s2 = (player[c1].y >> 16);
        else s2 = 0;

        s1 = (player[c1].x >> 16);
        if (GET_BAN_MAP_XY(s1, s2) == BAN_SOLID || GET_BAN_MAP_XY(s1, s2) == BAN_ICE ||
            GET_BAN_MAP_XY(s1, s2) == BAN_SPRING || GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_SOLID ||
            GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_ICE ||
            GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_SPRING) {
          player[c1].x = (((s1 + 16) & 0xfff0)) << 16;
          player[c1].x_add = 0;
        }

        s1 = (player[c1].x >> 16);
        if (GET_BAN_MAP_XY((s1 + 15), s2) == BAN_SOLID ||
            GET_BAN_MAP_XY((s1 + 15), s2) == BAN_ICE ||
            GET_BAN_MAP_XY((s1 + 15), s2) == BAN_SPRING ||
            GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_SOLID ||
            GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_ICE ||
            GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_SPRING) {
          player[c1].x = (((s1 + 16) & 0xfff0) - 16) << 16;
          player[c1].x_add = 0;
        }
      }

      player[c1].y += player[c1].y_add;

      // Sprungfeder
      s1 = (player[c1].x >> 16);
      s2 = (player[c1].y >> 16);
      if (s2 < 0) s2 = 0;
      if (GET_BAN_MAP_XY((s1 + 8), (s2 + 15)) == BAN_SPRING ||
          ((GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_SPRING &&
            GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) != BAN_SOLID) ||
           (GET_BAN_MAP_XY(s1, (s2 + 15)) != BAN_SOLID &&
            GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_SPRING))) {
        player[c1].y = ((player[c1].y >> 16) & 0xfff0) << 16;
        player[c1].y_add = -400000L;
        set_anim(c1, 2);
        player[c1].jump_ready = 0;
        player[c1].jump_abort = 0;
        for (int c2 = 0; c2 < NUM_OBJECTS; c2++) {
          if (objects[c2].used == 1 && objects[c2].type == OBJ_SPRING) {
            int hit = 0;
            if (GET_BAN_MAP_XY((s1 + 8), (s2 + 15)) == BAN_SPRING) {
              hit = ((objects[c2].x >> 20) == ((s1 + 8) >> 4) &&
                     (objects[c2].y >> 20) == ((s2 + 15) >> 4));
            } else if (GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_SPRING) {
              hit = ((objects[c2].x >> 20) == (s1 >> 4) &&
                     (objects[c2].y >> 20) == ((s2 + 15) >> 4));
            } else if (GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_SPRING) {
              hit = ((objects[c2].x >> 20) == ((s1 + 15) >> 4) &&
                     (objects[c2].y >> 20) == ((s2 + 15) >> 4));
            }
            if (hit) {
              objects[c2].frame = 0;
              objects[c2].ticks = object_anims[objects[c2].anim].frame[0].ticks;
              objects[c2].image = object_anims[objects[c2].anim].frame[0].image;
              break;
            }
          }
        }
        player[c1].sfxflags |= SF_SPRING;
      }

      // Decke
      s1 = (player[c1].x >> 16);
      s2 = (player[c1].y >> 16);
      if (s2 < 0) s2 = 0;
      if (GET_BAN_MAP_XY(s1, s2) == BAN_SOLID || GET_BAN_MAP_XY(s1, s2) == BAN_ICE ||
          GET_BAN_MAP_XY(s1, s2) == BAN_SPRING || GET_BAN_MAP_XY((s1 + 15), s2) == BAN_SOLID ||
          GET_BAN_MAP_XY((s1 + 15), s2) == BAN_ICE ||
          GET_BAN_MAP_XY((s1 + 15), s2) == BAN_SPRING) {
        player[c1].y = (((s2 + 16) & 0xfff0)) << 16;
        player[c1].y_add = 0;
        set_anim(c1, 0);
      }

      // Wasser / Boden / freier Fall
      s1 = (player[c1].x >> 16);
      s2 = (player[c1].y >> 16);
      if (s2 < 0) s2 = 0;
      if (GET_BAN_MAP_XY((s1 + 8), (s2 + 8)) == BAN_WATER) {
        if (player[c1].in_water == 0) {
          player[c1].in_water = 1;
          set_anim(c1, 4);
          if (player[c1].y_add >= 32768) {
            add_object(OBJ_SPLASH, (player[c1].x >> 16) + 8,
                       ((player[c1].y >> 16) & 0xfff0) + 15, 0, 0, OBJ_ANIM_SPLASH, 0);
            player[c1].sfxflags |= SF_SPLASH;
          }
        }
        // langsam an die Wasseroberflaeche treiben
        player[c1].y_add -= 1536;
        if (player[c1].y_add < 0 && player[c1].anim != 5) set_anim(c1, 5);
        if (player[c1].y_add < -65536L) player[c1].y_add = -65536L;
        if (player[c1].y_add > 65535L) player[c1].y_add = 65535L;
        if (GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_SOLID ||
            GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_ICE ||
            GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_SOLID ||
            GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_ICE) {
          player[c1].y = (((s2 + 16) & 0xfff0) - 16) << 16;
          player[c1].y_add = 0;
        }
      } else if (GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_SOLID ||
                 GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_ICE ||
                 GET_BAN_MAP_XY(s1, (s2 + 15)) == BAN_SPRING ||
                 GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_SOLID ||
                 GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_ICE ||
                 GET_BAN_MAP_XY((s1 + 15), (s2 + 15)) == BAN_SPRING) {
        player[c1].in_water = 0;
        player[c1].y = (((s2 + 16) & 0xfff0) - 16) << 16;
        player[c1].y_add = 0;
        if (player[c1].anim != 0 && player[c1].anim != 1) set_anim(c1, 0);
      } else {
        if (player[c1].in_water == 0) {
          player[c1].y_add += 12288;
          if (player[c1].y_add > 327680L) player[c1].y_add = 327680L;
        } else {
          player[c1].y = (int)(((uint32_t)player[c1].y & 0xFFFF0000u) + 0x10000u);
          player[c1].y_add = 0;
        }
        player[c1].in_water = 0;
      }
      if (player[c1].y_add > 36864 && player[c1].anim != 3 && player[c1].in_water == 0)
        set_anim(c1, 3);
    }

    // Animation weiterschalten
    player[c1].frame_tick++;
    if (player[c1].frame_tick >= player_anim_data[player[c1].anim][player[c1].frame * 2 + 3]) {
      player[c1].frame++;
      if (player[c1].frame >= player_anim_data[player[c1].anim][0]) {
        if (player[c1].anim != 6) player[c1].frame = player_anim_data[player[c1].anim][1];
        else position_player(c1);
      }
      player[c1].frame_tick = 0;
    }
    player[c1].image =
        player_anim_data[player[c1].anim][player[c1].frame * 2 + 2] + player[c1].direction * 9;
  }
}

// ---------------------------------------------------------------------------
void position_player(int player_num) {
  int c1, s1, s2;

  while (1) {
    while (1) {
      s1 = rnd(22);
      s2 = rnd(16);
      if (ban_map[s2][s1] == BAN_VOID &&
          (ban_map[s2 + 1][s1] == BAN_SOLID || ban_map[s2 + 1][s1] == BAN_ICE))
        break;
    }
    for (c1 = 0; c1 < JNB_MAX_PLAYERS; c1++) {
      if (c1 != player_num && player[c1].enabled == 1) {
        if (abs((s1 << 4) - (player[c1].x >> 16)) < 32 && abs((s2 << 4) - (player[c1].y >> 16)) < 32)
          break;
      }
    }
    if (c1 == JNB_MAX_PLAYERS) {
      player[player_num].x = (int)((uint32_t)s1 << 20);
      player[player_num].y = (int)((uint32_t)s2 << 20);
      player[player_num].x_add = player[player_num].y_add = 0;
      player[player_num].direction = 0;
      player[player_num].jump_ready = 1;
      player[player_num].in_water = 0;
      player[player_num].anim = 0;
      player[player_num].frame = 0;
      player[player_num].frame_tick = 0;
      player[player_num].image = player_anim_data[0][2];
      player[player_num].dead_flag = 0;
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Original: processKillPacket - Fell, Fleisch, Todesklang, Punktestand.
void spawn_gore(int victim, int x, int y) {
  for (int c4 = 0; c4 < 6; c4++)
    add_object(OBJ_FUR, (x >> 16) + 6 + rnd(5), (y >> 16) + 6 + rnd(5),
               ((int)rnd(65535) - 32768) * 3, ((int)rnd(65535) - 32768) * 3, 0, 44 + victim * 8);
  for (int c4 = 0; c4 < 6; c4++)
    add_object(OBJ_FLESH, (x >> 16) + 6 + rnd(5), (y >> 16) + 6 + rnd(5),
               ((int)rnd(65535) - 32768) * 3, ((int)rnd(65535) - 32768) * 3, 0, 76);
  for (int c4 = 0; c4 < 6; c4++)
    add_object(OBJ_FLESH, (x >> 16) + 6 + rnd(5), (y >> 16) + 6 + rnd(5),
               ((int)rnd(65535) - 32768) * 3, ((int)rnd(65535) - 32768) * 3, 0, 77);
  for (int c4 = 0; c4 < 8; c4++)
    add_object(OBJ_FLESH, (x >> 16) + 6 + rnd(5), (y >> 16) + 6 + rnd(5),
               ((int)rnd(65535) - 32768) * 3, ((int)rnd(65535) - 32768) * 3, 0, 78);
  for (int c4 = 0; c4 < 10; c4++)
    add_object(OBJ_FLESH, (x >> 16) + 6 + rnd(5), (y >> 16) + 6 + rnd(5),
               ((int)rnd(65535) - 32768) * 3, ((int)rnd(65535) - 32768) * 3, 0, 79);
  sfxPlay(SFX_DEATH, (uint16_t)(SFX_DEATH_FREQ + rnd(2000) - 1000), 64, -1);
}

// Nur der Host ruft das auf (autoritative Simulation).
void player_kill(int c1, int c2) {
  player[c1].y_add = -player[c1].y_add;
  if (player[c1].y_add > -262144L) player[c1].y_add = -262144L;
  player[c1].jump_abort = 1;
  player[c2].dead_flag = 1;
  if (player[c2].anim != 6) {
    set_anim(c2, 6);
    spawn_gore(c2, player[c2].x, player[c2].y);
    player[c2].deaths++;
    player[c1].bumps++;
  }
}

void collision_check(void) {
  int c1 = 0, c2 = 1;
  if (player[c1].enabled != 1 || player[c2].enabled != 1) return;
  if (labs((long)player[c1].x - player[c2].x) < (12L << 16) &&
      labs((long)player[c1].y - player[c2].y) < (12L << 16)) {
    if ((labs((long)player[c1].y - player[c2].y) >> 16) > 5) {
      // Der obere zerquetscht den unteren.
      if (player[c1].y < player[c2].y) {
        if (player[c1].y_add >= 0) player_kill(c1, c2);
        else if (player[c2].y_add < 0) player[c2].y_add = 0;
      } else {
        if (player[c2].y_add >= 0) player_kill(c2, c1);
        else if (player[c1].y_add < 0) player[c1].y_add = 0;
      }
    } else {
      // Seitlicher Zusammenstoss: Impulstausch.
      int l1;
      if (player[c1].x < player[c2].x) {
        if (player[c1].x_add > 0) player[c1].x = player[c2].x - (12L << 16);
        else if (player[c2].x_add < 0) player[c2].x = player[c1].x + (12L << 16);
        else {
          player[c1].x -= player[c1].x_add;
          player[c2].x -= player[c2].x_add;
        }
        l1 = player[c2].x_add;
        player[c2].x_add = player[c1].x_add;
        player[c1].x_add = l1;
        if (player[c1].x_add > 0) player[c1].x_add = -player[c1].x_add;
        if (player[c2].x_add < 0) player[c2].x_add = -player[c2].x_add;
      } else {
        if (player[c1].x_add > 0) player[c2].x = player[c1].x - (12L << 16);
        else if (player[c2].x_add < 0) player[c1].x = player[c2].x + (12L << 16);
        else {
          player[c1].x -= player[c1].x_add;
          player[c2].x -= player[c2].x_add;
        }
        l1 = player[c2].x_add;
        player[c2].x_add = player[c1].x_add;
        player[c1].x_add = l1;
        if (player[c1].x_add < 0) player[c1].x_add = -player[c1].x_add;
        if (player[c2].x_add > 0) player[c2].x_add = -player[c2].x_add;
      }
    }
  }
}

// ---------------------------------------------------------------------------
void update_objects(void) {
  for (int c1 = 0; c1 < NUM_OBJECTS; c1++) {
    if (objects[c1].used != 1) continue;
    switch (objects[c1].type) {
      case OBJ_SPRING:
        objects[c1].ticks--;
        if (objects[c1].ticks <= 0) {
          objects[c1].frame++;
          if (objects[c1].frame >= object_anims[objects[c1].anim].num_frames) {
            objects[c1].frame--;
            objects[c1].ticks = object_anims[objects[c1].anim].frame[objects[c1].frame].ticks;
          } else {
            objects[c1].ticks = object_anims[objects[c1].anim].frame[objects[c1].frame].ticks;
            objects[c1].image = object_anims[objects[c1].anim].frame[objects[c1].frame].image;
          }
        }
        break;

      case OBJ_SPLASH:
      case OBJ_FLESH_TRACE:
        objects[c1].ticks--;
        if (objects[c1].ticks <= 0) {
          objects[c1].frame++;
          if (objects[c1].frame >= object_anims[objects[c1].anim].num_frames)
            objects[c1].used = 0;
          else {
            objects[c1].ticks = object_anims[objects[c1].anim].frame[objects[c1].frame].ticks;
            objects[c1].image = object_anims[objects[c1].anim].frame[objects[c1].frame].image;
          }
        }
        break;

      case OBJ_SMOKE:
        objects[c1].x += objects[c1].x_add;
        objects[c1].y += objects[c1].y_add;
        objects[c1].ticks--;
        if (objects[c1].ticks <= 0) {
          objects[c1].frame++;
          if (objects[c1].frame >= object_anims[objects[c1].anim].num_frames)
            objects[c1].used = 0;
          else {
            objects[c1].ticks = object_anims[objects[c1].anim].frame[objects[c1].frame].ticks;
            objects[c1].image = object_anims[objects[c1].anim].frame[objects[c1].frame].image;
          }
        }
        break;

      case OBJ_YEL_BUTFLY:
      case OBJ_PINK_BUTFLY: {
        objects[c1].x_acc += (int)rnd(128) - 64;
        if (objects[c1].x_acc < -1024) objects[c1].x_acc = -1024;
        if (objects[c1].x_acc > 1024) objects[c1].x_acc = 1024;
        objects[c1].x_add += objects[c1].x_acc;
        if (objects[c1].x_add < -32768) objects[c1].x_add = -32768;
        if (objects[c1].x_add > 32768) objects[c1].x_add = 32768;
        objects[c1].x += objects[c1].x_add;
        if ((objects[c1].x >> 16) < 16) {
          objects[c1].x = 16 << 16;
          objects[c1].x_add = -objects[c1].x_add >> 2;
          objects[c1].x_acc = 0;
        } else if ((objects[c1].x >> 16) > 350) {
          objects[c1].x = 350 << 16;
          objects[c1].x_add = -objects[c1].x_add >> 2;
          objects[c1].x_acc = 0;
        }
        if (ban(objects[c1].x >> 16, objects[c1].y >> 16) != 0) {
          if (objects[c1].x_add < 0)
            objects[c1].x = (((objects[c1].x >> 16) + 16) & 0xfff0) << 16;
          else
            objects[c1].x = ((((objects[c1].x >> 16) - 16) & 0xfff0) + 15) << 16;
          objects[c1].x_add = -objects[c1].x_add >> 2;
          objects[c1].x_acc = 0;
        }
        objects[c1].y_acc += (int)rnd(64) - 32;
        if (objects[c1].y_acc < -1024) objects[c1].y_acc = -1024;
        if (objects[c1].y_acc > 1024) objects[c1].y_acc = 1024;
        objects[c1].y_add += objects[c1].y_acc;
        if (objects[c1].y_add < -32768) objects[c1].y_add = -32768;
        if (objects[c1].y_add > 32768) objects[c1].y_add = 32768;
        objects[c1].y += objects[c1].y_add;
        if ((objects[c1].y >> 16) < 0) {
          objects[c1].y = 0;
          objects[c1].y_add = -objects[c1].y_add >> 2;
          objects[c1].y_acc = 0;
        } else if ((objects[c1].y >> 16) > 255) {
          objects[c1].y = 255 << 16;
          objects[c1].y_add = -objects[c1].y_add >> 2;
          objects[c1].y_acc = 0;
        }
        if (ban(objects[c1].x >> 16, objects[c1].y >> 16) != 0) {
          if (objects[c1].y_add < 0)
            objects[c1].y = (((objects[c1].y >> 16) + 16) & 0xfff0) << 16;
          else
            objects[c1].y = ((((objects[c1].y >> 16) - 16) & 0xfff0) + 15) << 16;
          objects[c1].y_add = -objects[c1].y_add >> 2;
          objects[c1].y_acc = 0;
        }
        int wantL = (objects[c1].type == OBJ_YEL_BUTFLY) ? OBJ_ANIM_YEL_BUTFLY_LEFT
                                                         : OBJ_ANIM_PINK_BUTFLY_LEFT;
        int wantR = (objects[c1].type == OBJ_YEL_BUTFLY) ? OBJ_ANIM_YEL_BUTFLY_RIGHT
                                                         : OBJ_ANIM_PINK_BUTFLY_RIGHT;
        if (objects[c1].x_add < 0 && objects[c1].anim != wantL) {
          objects[c1].anim = wantL;
          objects[c1].frame = 0;
          objects[c1].ticks = object_anims[objects[c1].anim].frame[0].ticks;
          objects[c1].image = object_anims[objects[c1].anim].frame[0].image;
        } else if (objects[c1].x_add > 0 && objects[c1].anim != wantR) {
          objects[c1].anim = wantR;
          objects[c1].frame = 0;
          objects[c1].ticks = object_anims[objects[c1].anim].frame[0].ticks;
          objects[c1].image = object_anims[objects[c1].anim].frame[0].image;
        }
        objects[c1].ticks--;
        if (objects[c1].ticks <= 0) {
          objects[c1].frame++;
          if (objects[c1].frame >= object_anims[objects[c1].anim].num_frames)
            objects[c1].frame = object_anims[objects[c1].anim].restart_frame;
          else {
            objects[c1].ticks = object_anims[objects[c1].anim].frame[objects[c1].frame].ticks;
            objects[c1].image = object_anims[objects[c1].anim].frame[objects[c1].frame].image;
          }
        }
        break;
      }

      case OBJ_FUR:
      case OBJ_FLESH: {
        const int isFlesh = (objects[c1].type == OBJ_FLESH);
        // Blutspur hinterlassen
        if (rnd(100) < 30) {
          if (!isFlesh)
            add_object(OBJ_FLESH_TRACE, objects[c1].x >> 16, objects[c1].y >> 16, 0, 0,
                       OBJ_ANIM_FLESH_TRACE, 0);
          else if (objects[c1].frame >= 76 && objects[c1].frame <= 78)
            add_object(OBJ_FLESH_TRACE, objects[c1].x >> 16, objects[c1].y >> 16, 0, 0,
                       OBJ_ANIM_FLESH_TRACE, objects[c1].frame - 75);
        }
        int tile = ban(objects[c1].x >> 16, objects[c1].y >> 16);
        if (tile == 0) {
          objects[c1].y_add += 3072;
          if (objects[c1].y_add > 196608L) objects[c1].y_add = 196608L;
        } else if (tile == 2) { // im Wasser abbremsen und absinken
          if (objects[c1].x_add < 0) {
            if (objects[c1].x_add < -65536L) objects[c1].x_add = -65536L;
            objects[c1].x_add += 1024;
            if (objects[c1].x_add > 0) objects[c1].x_add = 0;
          } else {
            if (objects[c1].x_add > 65536L) objects[c1].x_add = 65536L;
            objects[c1].x_add -= 1024;
            if (objects[c1].x_add < 0) objects[c1].x_add = 0;
          }
          objects[c1].y_add += 1024;
          if (objects[c1].y_add < -65536L) objects[c1].y_add = -65536L;
          if (objects[c1].y_add > 65536L) objects[c1].y_add = 65536L;
        }
        objects[c1].x += objects[c1].x_add;
        tile = ban(objects[c1].x >> 16, objects[c1].y >> 16);
        if ((objects[c1].y >> 16) > 0 && (tile == 1 || tile == 3)) {
          if (objects[c1].x_add < 0) {
            objects[c1].x = (((objects[c1].x >> 16) + 16) & 0xfff0) << 16;
            objects[c1].x_add = -objects[c1].x_add >> 2;
          } else {
            objects[c1].x = ((((objects[c1].x >> 16) - 16) & 0xfff0) + 15) << 16;
            objects[c1].x_add = -objects[c1].x_add >> 2;
          }
        }
        objects[c1].y += objects[c1].y_add;
        if ((objects[c1].x >> 16) < -5 || (objects[c1].x >> 16) > 405 ||
            (objects[c1].y >> 16) > 260)
          objects[c1].used = 0;
        tile = ban(objects[c1].x >> 16, objects[c1].y >> 16);
        if (objects[c1].used && (objects[c1].y >> 16) > 0 && tile != 0) {
          if (objects[c1].y_add < 0) {
            if (tile != 2) {
              objects[c1].y = (((objects[c1].y >> 16) + 16) & 0xfff0) << 16;
              objects[c1].x_add >>= 2;
              objects[c1].y_add = -objects[c1].y_add >> 2;
            }
          } else {
            if (tile == 1) {
              if (objects[c1].y_add > 131072L) {
                objects[c1].y = ((((objects[c1].y >> 16) - 16) & 0xfff0) + 15) << 16;
                objects[c1].x_add >>= 2;
                objects[c1].y_add = -objects[c1].y_add >> 2;
              } else {
                // Liegengebliebenes Fleisch wird zum dauerhaften Blutfleck.
                if (isFlesh && rnd(100) < 10) {
                  int s1 = (int)rnd(4) - 2;
                  addStain(objects[c1].x >> 16, (objects[c1].y >> 16) + s1, objects[c1].frame);
                }
                objects[c1].used = 0;
              }
            } else if (tile == 3) {
              objects[c1].y = ((((objects[c1].y >> 16) - 16) & 0xfff0) + 15) << 16;
              if (objects[c1].y_add > 131072L) objects[c1].y_add = -objects[c1].y_add >> 2;
              else objects[c1].y_add = 0;
            }
          }
        }
        if (objects[c1].x_add < 0 && objects[c1].x_add > -16384) objects[c1].x_add = -16384;
        if (objects[c1].x_add > 0 && objects[c1].x_add < 16384) objects[c1].x_add = 16384;
        break;
      }
    }
  }
}

// ---------------------------------------------------------------------------
static void get_closest_player_to_point(int x, int y, int *dist, int *closest_player) {
  *dist = 0x7fff;
  *closest_player = 0;
  for (int c1 = 0; c1 < JNB_MAX_PLAYERS; c1++) {
    if (player[c1].enabled != 1) continue;
    int dx = x - ((player[c1].x >> 16) + 8);
    int dy = y - ((player[c1].y >> 16) + 8);
    int cur = (int)sqrtf((float)(dx * dx + dy * dy));
    if (cur < *dist) {
      *closest_player = c1;
      *dist = cur;
    }
  }
}

// Der Fliegenschwarm: haelt zusammen, weicht Spielern aus, bleibt in Hohlraeumen.
void update_flies(int with_sound) {
  int s1 = 0, s2 = 0, s3, s4;
  int closest_player = 0, dist = 0;

  for (int c1 = 0; c1 < NUM_FLIES; c1++) {
    s1 += flies[c1].x;
    s2 += flies[c1].y;
  }
  s1 /= NUM_FLIES;
  s2 /= NUM_FLIES;

  if (with_sound) {
    get_closest_player_to_point(s1, s2, &dist, &closest_player);
    s3 = 32 - dist / 3;
    if (s3 < 0) s3 = 0;
    sfxChannelVolume(SFX_FLY, (uint8_t)s3);
  }

  for (int c1 = 0; c1 < NUM_FLIES; c1++) {
    get_closest_player_to_point(flies[c1].x, flies[c1].y, &dist, &closest_player);
    s3 = 0;
    if ((s1 - flies[c1].x) > 30) s3 += 1;
    else if ((s1 - flies[c1].x) < -30) s3 -= 1;
    if (dist < 30) {
      if (((player[closest_player].x >> 16) + 8) > flies[c1].x) s3 -= 1;
      else s3 += 1;
    }
    s4 = (int)rnd(3) - 1 + s3;
    if ((flies[c1].x + s4) < 16) s4 = 0;
    if ((flies[c1].x + s4) > 351) s4 = 0;
    if (ban(flies[c1].x + s4, flies[c1].y) != BAN_VOID) s4 = 0;
    flies[c1].x += s4;

    s3 = 0;
    if ((s2 - flies[c1].y) > 30) s3 += 1;
    else if ((s2 - flies[c1].y) < -30) s3 -= 1;
    if (dist < 30) {
      if (((player[closest_player].y >> 16) + 8) > flies[c1].y) s3 -= 1;
      else s3 += 1;
    }
    s4 = (int)rnd(3) - 1 + s3;
    if ((flies[c1].y + s4) < 0) s4 = 0;
    if ((flies[c1].y + s4) > 239) s4 = 0;
    if (ban(flies[c1].x, flies[c1].y + s4) != BAN_VOID) s4 = 0;
    flies[c1].y += s4;
  }
}

// ---------------------------------------------------------------------------
// Original: init_level - Spieler setzen, Sprungfedern aufstellen, vier Falter
// und den Fliegenschwarm aussetzen.
void gameInit(void) {
  rndSeed(micros() ^ 0x9E3779B9u);
  stainsClear();

  for (int c1 = 0; c1 < NUM_OBJECTS; c1++) objects[c1].used = 0;

  for (int c1 = 0; c1 < JNB_MAX_PLAYERS; c1++) {
    memset(&player[c1], 0, sizeof(player_t));
    player[c1].enabled = 1;
    player[c1].jump_ready = 1;
    position_player(c1);
  }

  for (int c1 = 0; c1 < 16; c1++)
    for (int c2 = 0; c2 < 22; c2++)
      if (ban_map[c1][c2] == BAN_SPRING)
        add_object(OBJ_SPRING, c2 << 4, c1 << 4, 0, 0, OBJ_ANIM_SPRING, 5);

  for (int n = 0; n < 4; n++) {
    int type = (n < 2) ? OBJ_YEL_BUTFLY : OBJ_PINK_BUTFLY;
    while (1) {
      int s1 = rnd(22), s2 = rnd(16);
      if (ban_map[s2][s1] == BAN_VOID) {
        add_object(type, (s1 << 4) + 8, (s2 << 4) + 8, ((int)rnd(65535) - 32768) * 2,
                   ((int)rnd(65535) - 32768) * 2, 0, 0);
        break;
      }
    }
  }

  if (flies_enabled) {
    int s1 = rnd(250) + 50;
    int s2 = rnd(150) + 50;
    for (int c1 = 0; c1 < NUM_FLIES; c1++) {
      while (1) {
        flies[c1].x = (int16_t)(s1 + (int)rnd(101) - 50);
        flies[c1].y = (int16_t)(s2 + (int)rnd(101) - 50);
        if (flies[c1].x >= 16 && flies[c1].x <= 351 && flies[c1].y >= 0 && flies[c1].y <= 239 &&
            ban(flies[c1].x, flies[c1].y) == BAN_VOID)
          break;
      }
    }
    sfxPlay(SFX_FLY, SFX_FLY_FREQ, 0, SFX_FLY);
  }
}
