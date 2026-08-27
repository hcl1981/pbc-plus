// Erzeugt Bildschirmfotos aus dem echten Renderer (src/render.cpp) fuer
// verschiedene Spielsituationen.  Aufruf:  make shots
#include "jnb.h"
#include <Adafruit_GFX.h>
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
void sfxPlay(uint8_t, uint16_t, uint8_t, int8_t) {}
void sfxChannelVolume(uint8_t, uint8_t) {}
void musicStart(void) {}
void musicStop(void) {}
void musicVolume(uint8_t) {}

// ---------------------------------------------------------------------------
static void writePNG(const char *fn, const uint16_t *fb, int w, int h, int scale) {
  std::vector<uint8_t> raw;
  for (int y = 0; y < h * scale; y++) {
    raw.push_back(0);
    for (int x = 0; x < w * scale; x++) {
      uint16_t c = fb[(y / scale) * DISPLAY_WIDTH + (x / scale)];
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
  int ow = w * scale, oh = h * scale;
  uint8_t ihdr[13] = {(uint8_t)(ow >> 24), (uint8_t)(ow >> 16), (uint8_t)(ow >> 8), (uint8_t)ow,
                      (uint8_t)(oh >> 24), (uint8_t)(oh >> 16), (uint8_t)(oh >> 8), (uint8_t)oh,
                      8, 2, 0, 0, 0};
  chunk("IHDR", ihdr, 13);
  chunk("IDAT", comp.data(), clen);
  chunk("IEND", nullptr, 0);
  fclose(f);
  printf("  %s (%dx%d)\n", fn, ow, oh);
}

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
  player[p].direction = 0;
  player[p].action_left = player[p].action_right = player[p].action_up = 0;
  player[p].enabled = 1;
}
static void tick(int n) {
  for (int i = 0; i < n; i++) {
    g_ms += 16;
    steer_players();
    collision_check();
    update_objects();
    update_flies(1);
  }
}
// Kamera einschwingen lassen (sie folgt mit hoechstens 6 Pixeln je Bild).
static void settleCam(int who) {
  for (int i = 0; i < 80; i++) renderFrame(who);
}

int main() {
  renderInit();
  gameInit();

  // ---- 1: Normale Sicht des Hosts, beide Hasen ----
  place(0, 60, 96);
  place(1, 110, 96);
  player[0].bumps = 0;
  player[1].bumps = 0;
  tick(60);
  settleCam(0);
  writePNG("shot1_spiel.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 2: Sicht des Gasts auf denselben Moment (eigener Bildausschnitt) ----
  place(0, 60, 96);
  place(1, 300, 60);
  tick(40);
  settleCam(1);
  writePNG("shot2_gastsicht.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 3: Tod - Fellfetzen und Fleisch im Flug, mit Blutspur ----
  gameInit();
  place(0, 150, 150);
  place(1, 150, 158);
  player[0].y_add = 100000;
  player[0].bumps = 6;
  player[1].bumps = 3;
  collision_check();
  tick(14);
  settleCam(0);
  writePNG("shot3_tod.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 4: kurz danach - die Spur zieht sich durchs Bild ----
  tick(16);
  renderFrame(0);
  writePNG("shot4_blutspur.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 5: alles gelandet - dauerhafte Blutflecken im Hintergrund ----
  tick(400);
  place(0, 150, 150);
  place(1, 190, 150);
  tick(40);
  renderFrame(0);
  writePNG("shot5_blutflecken.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 6: Wasser - Schwimmen an der Oberflaeche ----
  gameInit();
  place(0, 60, 150);
  place(1, 100, 150);
  tick(150); // beide fallen ins Wasser und treiben an die Oberflaeche
  settleCam(0);
  writePNG("shot6_wasser.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 7: Eis - Hase rutscht ueber die Eisflaeche rechts unten ----
  gameInit();
  place(0, 262, 192);
  place(1, 300, 100);
  tick(60);
  for (int i = 0; i < 25; i++) {
    player[0].action_right = 1;
    tick(1);
  }
  player[0].action_right = 0;
  tick(6);
  settleCam(0);
  writePNG("shot7_eis.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 8: Punkteleiste mit Punktestand ----
  player[0].bumps = 7;
  player[1].bumps = 12;
  renderFrame(0);
  uint16_t *fb = canvas.getBuffer();
  std::vector<uint16_t> hud(DISPLAY_WIDTH * HUD_H);
  for (int y = 0; y < HUD_H; y++)
    for (int x = 0; x < DISPLAY_WIDTH; x++) hud[y * DISPLAY_WIDTH + x] = fb[(HUD_Y + y) * DISPLAY_WIDTH + x];
  writePNG("shot8_punkte.png", hud.data(), DISPLAY_WIDTH, HUD_H, 4);

  // ---- 9: Startmenue mit Titelbild ----
  drawRoleMenu(0);
  writePNG("shot10_menue_host.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);
  drawRoleMenu(1);
  writePNG("shot11_menue_join.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);
  drawInfoScreen("HOSTING", "WARTE AUF GAST");
  writePNG("shot12_warten.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  // ---- 10: Vordergrundmaske - Hase hinter dem Baumstamm ----
  gameInit();
  place(0, 168, 226);
  place(1, 320, 60);
  tick(30);
  settleCam(0);
  writePNG("shot9_maske.png", canvas.getBuffer(), VIEW_W, DISPLAY_HEIGHT, 2);

  return 0;
}
