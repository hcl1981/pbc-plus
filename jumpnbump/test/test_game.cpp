// Prueft die portierte Spielmechanik und den Renderer auf dem Entwicklungsrechner.
#include "jnb.h"
#include <Adafruit_GFX.h>
#include <stdarg.h>
#include <Adafruit_ST7789.h>
#include <stdio.h>
#include <vector>
#include <zlib.h>

Adafruit_ST7789 tft(0, 0, 0);
GFXcanvas16 canvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);
uint8_t myPlayer = 0;

static unsigned long g_ms = 0;
unsigned long millis(void) { return g_ms; }
unsigned long micros(void) { return g_ms * 1000; }
void linkPump(void) {}

int sfxCount[8] = {0};
void sfxPlay(uint8_t num, uint16_t, uint8_t, int8_t) { if (num < 8) sfxCount[num]++; }
void sfxChannelVolume(uint8_t, uint8_t) {}
void musicStart(void) {}
void musicStop(void) {}
void musicVolume(uint8_t) {}

static int fails = 0;
static void check(const char *name, bool ok, const char *fmt, ...) {
  va_list ap;
  char buf[300];
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  printf("%-26s %-70s %s\n", name, buf, ok ? "OK" : "FEHLER");
  if (!ok) fails++;
}

static void writePNG(const char *fn, const uint16_t *fb, int w, int h) {
  std::vector<uint8_t> raw;
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    for (int x = 0; x < w; x++) {
      uint16_t c = fb[y * DISPLAY_WIDTH + x];
      raw.push_back(((c >> 11) & 0x1F) << 3);
      raw.push_back(((c >> 5) & 0x3F) << 2);
      raw.push_back((c & 0x1F) << 3);
    }
  }
  uLongf clen = compressBound(raw.size());
  std::vector<uint8_t> comp(clen);
  compress(comp.data(), &clen, raw.data(), raw.size());
  FILE *f = fopen(fn, "wb");
  const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  fwrite(sig, 1, 8, f);
  auto chunk = [&](const char *t, const uint8_t *d, uint32_t n) {
    uint8_t lb[4] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n};
    fwrite(lb, 1, 4, f);
    std::vector<uint8_t> buf(4 + n);
    memcpy(buf.data(), t, 4);
    if (n) memcpy(buf.data() + 4, d, n);
    fwrite(buf.data(), 1, buf.size(), f);
    uint32_t c = crc32(0, buf.data(), buf.size());
    uint8_t cb[4] = {(uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c};
    fwrite(cb, 1, 4, f);
  };
  uint8_t ihdr[13] = {(uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w,
                      (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h,
                      8, 2, 0, 0, 0};
  chunk("IHDR", ihdr, 13);
  chunk("IDAT", comp.data(), clen);
  chunk("IEND", nullptr, 0);
  fclose(f);
}

static const char *tileName(int t) {
  static const char *n[] = {"leer", "fest", "wasser", "eis", "feder"};
  return (t >= 0 && t <= 4) ? n[t] : "?";
}

// Position in PIXELN setzen (nicht in Kacheln).
static void place(int p, int px, int py) {
  player[p].x = px << 16;
  player[p].y = py << 16;
  player[p].x_add = player[p].y_add = 0;
  player[p].dead_flag = 0;
  player[p].anim = player[p].frame = player[p].frame_tick = 0;
  player[p].in_water = 0;
  player[p].jump_ready = 1;
  player[p].jump_abort = 0;
  player[p].sfxflags = 0;
  player[p].action_left = player[p].action_right = player[p].action_up = 0;
}
static void tick(void) {
  g_ms += 16;
  steer_players();
  update_objects();
  update_flies(1);
}
static void idle(int p, int n) {
  for (int i = 0; i < n; i++) {
    player[p].action_left = player[p].action_right = player[p].action_up = 0;
    tick();
    player[p].sfxflags = 0;
  }
}

int main() {
  if (!renderInit()) { printf("renderInit fehlgeschlagen\n"); return 1; }
  gameInit();
  player[1].enabled = 0; // Tests zunaechst mit einem Hasen

  // ---- 1: Landen auf festem Boden, Position stabil ----
  place(0, 48, 96); // Spalte 3, faellt auf die Plattform in Zeile 11
  idle(0, 90);
  int y1 = player[0].y >> 16;
  bool stable = true;
  for (int i = 0; i < 20; i++) { idle(0, 1); if ((player[0].y >> 16) != y1) stable = false; }
  int below = ban((player[0].x >> 16) + 8, y1 + 16);
  check("1 Landung", stable && below == BAN_SOLID && y1 == 160,
        "y=%d konstant=%d Kachel darunter=%s", y1, stable, tileName(below));

  // ---- 2: Laufen bis zur Hoechstgeschwindigkeit ----
  int startx = player[0].x >> 16;
  for (int t = 0; t < 40; t++) {
    player[0].action_right = 1;
    player[0].action_left = player[0].action_up = 0;
    tick();
  }
  check("2 Laufen", player[0].x_add == 98304, "dx=%d px  x_add=%d (Grenze 98304)",
        (player[0].x >> 16) - startx, player[0].x_add);

  // ---- 3: Sprunghoehe ----
  place(0, 48, 96);
  idle(0, 90);
  int gy0 = player[0].y >> 16, peak = gy0;
  for (int t = 0; t < 70; t++) {
    player[0].action_up = 1;
    player[0].action_left = player[0].action_right = 0;
    tick();
    if ((player[0].y >> 16) < peak) peak = player[0].y >> 16;
    player[0].sfxflags = 0;
  }
  int jh = gy0 - peak;
  check("3 Sprung", jh >= 45 && jh <= 55, "Boden y=%d Scheitel y=%d Hoehe=%d px", gy0, peak, jh);

  // ---- 4: Wasser - eintauchen, Fontaene, an der Oberflaeche schwimmen ----
  place(0, 112, 144); // Spalte 7, freier Fall bis Zeile 14 (Wasser)
  bool splash = false;
  int swim = 0;
  for (int t = 0; t < 200; t++) {
    player[0].action_left = player[0].action_right = player[0].action_up = 0;
    tick();
    if (player[0].sfxflags & SF_SPLASH) splash = true;
    if (player[0].in_water) swim++;
    player[0].sfxflags = 0;
  }
  int wy = player[0].y >> 16;
  check("4 Wasser", splash && swim > 80 && wy >= 208 && wy <= 220 && player[0].anim == 5,
        "Fontaene=%d Ticks_im_Wasser=%d y=%d anim=%d(5=schwimmen)", splash, swim, wy,
        player[0].anim);

  // ---- 4b: aus dem Wasser springen ----
  int wy0 = player[0].y >> 16;
  int wpeak = wy0;
  for (int t = 0; t < 60; t++) {
    player[0].action_up = 1;
    tick();
    if ((player[0].y >> 16) < wpeak) wpeak = player[0].y >> 16;
    player[0].sfxflags = 0;
  }
  check("4b Sprung aus Wasser", wy0 - wpeak > 15, "aus y=%d auf y=%d (%d px)", wy0, wpeak,
        wy0 - wpeak);

  // ---- 5: Eis - kaum Beschleunigung, langes Gleiten ----
  place(0, 272, 192); // Spalten 16..18 in Zeile 14 sind Eis
  idle(0, 60);
  int iceTile = ban((player[0].x >> 16) + 8, (player[0].y >> 16) + 16);
  for (int t = 0; t < 20; t++) { player[0].action_right = 1; tick(); }
  int iceSpeed = player[0].x_add;
  int glide = 0;
  for (int t = 0; t < 90; t++) {
    player[0].action_right = 0;
    tick();
    if (player[0].x_add != 0) glide++;
  }
  check("5 Eis", iceTile == BAN_ICE && iceSpeed > 0 && iceSpeed < 30000 && glide > 60,
        "Kachel=%s x_add nach 20 Ticks=%d (fester Boden: 98304) Gleiten=%d Ticks", tileName(iceTile),
        iceSpeed, glide);

  // Zum Vergleich: auf festem Boden bremst der Hase sofort ab.
  place(0, 48, 96);
  idle(0, 90);
  for (int t = 0; t < 20; t++) { player[0].action_right = 1; tick(); }
  int glideSolid = 0;
  for (int t = 0; t < 90; t++) {
    player[0].action_right = 0;
    tick();
    if (player[0].x_add != 0) glideSolid++;
  }
  check("5b Vergleich fester Boden", glideSolid < 10, "Nachlauf nur %d Ticks (Eis: %d)", glideSolid,
        glide);

  // ---- 6: Sprungfeder (Zeile 14, Spalte 9) ----
  place(0, 144, 192);
  bool springSeen = false;
  int spPeak = 9999;
  for (int t = 0; t < 120; t++) {
    player[0].action_left = player[0].action_right = player[0].action_up = 0;
    tick();
    if (player[0].sfxflags & SF_SPRING) springSeen = true;
    if (springSeen && (player[0].y >> 16) < spPeak) spPeak = player[0].y >> 16;
    player[0].sfxflags = 0;
  }
  int springObj = 0;
  for (int i = 0; i < NUM_OBJECTS; i++)
    if (objects[i].used && objects[i].type == OBJ_SPRING) springObj++;
  check("6 Sprungfeder", springSeen && springObj == 1 && spPeak < 200,
        "ausgeloest=%d Federobjekte=%d Scheitel y=%d", springSeen, springObj, spPeak);

  // ---- 7: Tod von oben - Gedaerme, Punkte, Todesklang ----
  player[1].enabled = 1;
  int deathBefore = sfxCount[SFX_DEATH];
  place(0, 80, 96);
  place(1, 80, 104); // 8 px darunter: >5 und <12 -> Zerquetschen
  player[0].y_add = 100000;
  player[0].bumps = 0;
  collision_check();
  int fur = 0, flesh = 0;
  for (int i = 0; i < NUM_OBJECTS; i++) {
    if (!objects[i].used) continue;
    if (objects[i].type == OBJ_FUR) fur++;
    if (objects[i].type == OBJ_FLESH) flesh++;
  }
  check("7 Tod", player[1].dead_flag == 1 && fur == 6 && flesh == 30 && player[0].bumps == 1 &&
                     sfxCount[SFX_DEATH] == deathBefore + 1 && player[1].deaths == 1,
        "tot=%d deaths=%u Punkte=%d Fell=%d Fleisch=%d Klang=%d", player[1].dead_flag,
        player[1].deaths, player[0].bumps, fur, flesh, sfxCount[SFX_DEATH] - deathBefore);

  // ---- 8: Blutspur waehrend des Flugs, Blutflecken am Ende ----
  int traceMax = 0;
  for (int t = 0; t < 500; t++) {
    player[0].action_left = player[0].action_right = player[0].action_up = 0;
    player[1].action_left = player[1].action_right = player[1].action_up = 0;
    tick();
    int tr = 0;
    for (int i = 0; i < NUM_OBJECTS; i++)
      if (objects[i].used && objects[i].type == OBJ_FLESH_TRACE) tr++;
    if (tr > traceMax) traceMax = tr;
  }
  int leftGore = 0;
  for (int i = 0; i < NUM_OBJECTS; i++)
    if (objects[i].used && (objects[i].type == OBJ_FLESH || objects[i].type == OBJ_FUR)) leftGore++;
  check("8 Blutspur", traceMax >= 4 && leftGore == 0,
        "max. %d Spurstuecke gleichzeitig, danach %d Fleischreste uebrig", traceMax, leftGore);

  // ---- 9: Seitlicher Zusammenstoss prallt ab statt zu toeten ----
  place(0, 100, 160);
  place(1, 108, 160); // gleiche Hoehe -> Abprallen
  player[0].x_add = 50000;
  player[1].dead_flag = 0;
  int deaths9 = player[1].deaths;
  collision_check();
  check("9 Seitenstoss", player[1].deaths == (uint8_t)deaths9 && player[1].dead_flag == 0 &&
                             player[1].x_add > 0,
        "kein Tod, Impuls uebertragen: P1 x_add=%d P2 x_add=%d", player[0].x_add, player[1].x_add);

  // ---- 10: Fliegen bleiben in Hohlraeumen ----
  for (int t = 0; t < 400; t++) { g_ms += 16; update_flies(1); }
  int badFly = 0, flyMinX = 9999;
  for (int i = 0; i < NUM_FLIES; i++) {
    if (ban(flies[i].x, flies[i].y) != BAN_VOID) badFly++;
    if (flies[i].x < flyMinX) flyMinX = flies[i].x;
  }
  check("10 Fliegen", badFly == 0 && flyMinX >= 16, "%d von %d in Waenden, kleinstes x=%d", badFly,
        NUM_FLIES, flyMinX);

  // ---- 11: Falter bleiben im Bild ----
  int badBf = 0, bf = 0;
  for (int t = 0; t < 800; t++) { g_ms += 16; update_objects(); }
  for (int i = 0; i < NUM_OBJECTS; i++) {
    if (!objects[i].used) continue;
    if (objects[i].type != OBJ_YEL_BUTFLY && objects[i].type != OBJ_PINK_BUTFLY) continue;
    bf++;
    int x = objects[i].x >> 16, y = objects[i].y >> 16;
    if (x < 16 || x > 351 || y < 0 || y > 255) badBf++;
  }
  check("11 Falter", bf == 4 && badBf == 0, "%d vorhanden, %d ausserhalb", bf, badBf);

  // ---- 12: Spieler bleiben im Spielfeld ----
  place(0, 0, 96);
  for (int t = 0; t < 300; t++) { player[0].action_left = 1; tick(); }
  int minx = player[0].x >> 16;
  place(0, 300, 96);
  for (int t = 0; t < 300; t++) { player[0].action_left = 0; player[0].action_right = 1; tick(); }
  int maxx = player[0].x >> 16;
  check("12 Spielfeldrand", minx >= 0 && maxx <= 336, "linke Grenze x=%d, rechte Grenze x=%d", minx,
        maxx);

  // ---- 13: Wiedereinsetzen landet immer auf begehbarem Grund ----
  int badSpawn = 0;
  for (int i = 0; i < 400; i++) {
    position_player(0);
    int tx = player[0].x >> 20, ty = player[0].y >> 20;
    int here = ban(tx << 4, ty << 4), under = ban(tx << 4, (ty + 1) << 4);
    if (here != BAN_VOID || (under != BAN_SOLID && under != BAN_ICE)) badSpawn++;
  }
  check("13 Wiedereinstieg", badSpawn == 0, "%d von 400 Startplaetzen unpassend", badSpawn);

  // ---- Bilder ausgeben ----
  gameInit();
  player[0].enabled = player[1].enabled = 1;
  place(0, 60, 96);
  place(1, 100, 96);
  for (int t = 0; t < 80; t++) { tick(); }
  // ein paar Blutflecken einbacken, damit sie im Bild zu sehen sind
  for (int i = 0; i < 12; i++) addStain(70 + i * 6, 168 + (i % 3), 76 + (i % 4));
  myPlayer = 0;
  for (int i = 0; i < 60; i++) renderFrame(0); // Kamera nachfuehren lassen
  writePNG("view_1x.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT);


  // ---- Vordergrundmaske pruefen: Hase hinter dem Busch bei (173,238) ----
  place(0, 168, 226);
  place(1, 300, 96);
  for (int i = 0; i < 200; i++) renderFrame(0); // Kamera einschwingen lassen
  renderFrame(0);
  writePNG("view_mask.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT);
  printf("\n%s (%d Fehler)\n", fails ? "TESTS FEHLGESCHLAGEN" : "alle Tests bestanden", fails);
  return fails ? 1 : 0;
}
