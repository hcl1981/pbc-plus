// ============================================================================
//  Darstellung.
//
//  Der Levelhintergrund liegt als Palettenbild im Arbeitsspeicher (352x256),
//  damit Blutflecken dauerhaft hineingebrannt werden koennen - genau wie im
//  Original, wo die "leftovers" in die Bildseiten gezeichnet werden.
//
//  Sichtfenster: 240x256 Originalpixel, horizontal auf den eigenen Hasen
//  gescrollt. Die Levelhoehe von 256 Pixeln passt genau auf das Display, es
//  wird also nur seitlich gescrollt und nie skaliert.
//
//  Sprites werden - wie put_pob() im Original - nur dort gezeichnet, wo die
//  Maske 0 ist. So verschwinden die Hasen korrekt hinter Grasbuescheln und
//  Vordergrundkanten.
// ============================================================================
#include "jnb.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

extern Adafruit_ST7789 tft;
extern GFXcanvas16 canvas;

static uint8_t *bgwork = nullptr; // 352x256 Palettenindizes, mit Blutflecken
static uint16_t pal[256];         // Palette im RAM (schneller als aus dem Flash)
static int camx = 0;
static int lastBumps[JNB_MAX_PLAYERS] = {-1, -1};
static bool hudDirty = true;

#define BGW JNB_PLAY_W

// Das Display hat abgerundete Ecken: in der Punkteleiste zu beiden Seiten
// Abstand halten, sonst werden die aeusseren Ziffern angeschnitten.
#define HUD_MARGIN 20
#define HUD_BLOCK 50 // Hasenkopf + Luecke + zwei Ziffern
#define HUD_BG 0x630C // 0x606060 in RGB565

static inline bool maskAt(int x, int y) {
  if ((unsigned)x >= JNB_WIDTH || (unsigned)y >= JNB_HEIGHT) return true;
  return (jnb_maskbits[y * JNB_MASK_STRIDE + (x >> 3)] >> (7 - (x & 7))) & 1;
}

bool renderInit(void) {
  for (int i = 0; i < 256; i++) pal[i] = jnb_palette[i];
  if (!bgwork) bgwork = (uint8_t *)malloc(BGW * JNB_HEIGHT);
  if (!bgwork) return false;
  stainsClear();
  return true;
}

void stainsClear(void) {
  if (!bgwork) return;
  for (int y = 0; y < JNB_HEIGHT; y++)
    memcpy(bgwork + y * BGW, jnb_level + y * JNB_WIDTH, BGW);
  hudDirty = true;
  lastBumps[0] = lastBumps[1] = -1;
}

// Dauerhafter Blutfleck: das Fleischsprite wird in den Hintergrund gebrannt.
void addStain(int x, int y, int image) {
  if (!bgwork || image < 0 || image >= JNB_NUM_OBJECTS) return;
  const JnbGobEntry &g = objects_gob[image];
  const uint8_t *src = objects_pixels + g.ofs;
  int x0 = x - g.hs_x, y0 = y - g.hs_y;
  for (int row = 0; row < g.h; row++) {
    int py = y0 + row;
    if (py < 0 || py >= JNB_HEIGHT) continue;
    for (int col = 0; col < g.w; col++) {
      int px = x0 + col;
      if (px < 0 || px >= BGW) continue;
      uint8_t c = src[row * g.w + col];
      if (c && !maskAt(px, py)) bgwork[py * BGW + px] = c;
    }
  }
}

// ---------------------------------------------------------------------------
// Ein Sprite ins Bild setzen (Entsprechung zu put_pob mit Maske).
static void putPob(int x, int y, int image, const JnbGobEntry *gob, const uint8_t *pixels,
                   int num) {
  if (image < 0 || image >= num) return;
  const JnbGobEntry &g = gob[image];
  const uint8_t *src = pixels + g.ofs;
  int x0 = x - g.hs_x, y0 = y - g.hs_y;
  uint16_t *fb = canvas.getBuffer();

  for (int row = 0; row < g.h; row++) {
    int py = y0 + row;
    if (py < 0 || py >= VIEW_H) continue;
    uint16_t *dst = fb + py * DISPLAY_WIDTH;
    for (int col = 0; col < g.w; col++) {
      int px = x0 + col;
      if (px < 0 || px >= BGW) continue;
      uint8_t c = src[row * g.w + col];
      if (!c) continue;
      if (maskAt(px, py)) continue; // liegt hinter dem Vordergrund
      int dx = px - camx;
      if (dx < 0 || dx >= VIEW_W) continue;
      dst[dx] = pal[c];
    }
  }
}

// Zwei Farben mischen; t = 0..255 Anteil von b.
uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r = (ar * (255 - t) + br * t) / 255;
  int g = (ag * (255 - t) + bg * t) / 255;
  int bl = (ab * (255 - t) + bb * t) / 255;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// ---------------------------------------------------------------------------
static void drawHud(void) {
  uint16_t *fb = canvas.getBuffer();
  for (int y = HUD_Y; y < DISPLAY_HEIGHT; y++)
    for (int x = 0; x < DISPLAY_WIDTH; x++) fb[y * DISPLAY_WIDTH + x] = HUD_BG;

  // Schriftzug mittig zwischen die beiden Punktestaende
  {
    const int lx = (DISPLAY_WIDTH - JNB_LOGO_W) / 2;
    const int ly = HUD_Y + (HUD_H - JNB_LOGO_H) / 2;
    for (int row = 0; row < JNB_LOGO_H; row++) {
      uint16_t *d = fb + (ly + row) * DISPLAY_WIDTH + lx;
      const uint16_t *s = jnb_logo_rgb + row * JNB_LOGO_W;
      const uint8_t *a = jnb_logo_a + row * JNB_LOGO_W;
      for (int col = 0; col < JNB_LOGO_W; col++)
        if (a[col]) d[col] = blend565(d[col], s[col], a[col]);
    }
  }

  for (int p = 0; p < JNB_MAX_PLAYERS; p++) {
    int base = (p == 0) ? HUD_MARGIN : (DISPLAY_WIDTH - HUD_MARGIN - HUD_BLOCK);
    // Hasenkopf als Spielerkennung
    const JnbGobEntry &g = rabbit_gob[p * 18];
    const uint8_t *src = rabbit_pixels + g.ofs;
    for (int row = 0; row < g.h && row < HUD_H; row++)
      for (int col = 0; col < g.w; col++) {
        uint8_t c = src[row * g.w + col];
        if (c) fb[(HUD_Y + row + 1) * DISPLAY_WIDTH + base + col] = pal[c];
      }
    // zweistelliger Punktestand mit den Originalziffern
    int s = player[p].bumps % 100;
    for (int d = 0; d < 2; d++) {
      int digit = (d == 0) ? (s / 10) : (s % 10);
      const JnbGobEntry &n = numbers_gob[digit];
      const uint8_t *ns = numbers_pixels + n.ofs;
      int nx = base + 18 + d * 16;
      for (int row = 0; row < n.h; row++) {
        int py = HUD_Y + 1 + row;
        if (py >= DISPLAY_HEIGHT) break;
        for (int col = 0; col < n.w; col++) {
          uint8_t c = ns[row * n.w + col];
          if (c) fb[py * DISPLAY_WIDTH + nx + col] = pal[c];
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Textausgabe mit der Originalschrift des Spiels (font.pcx, 81 Zeichen).
// Die Zeichenzuordnung stammt aus put_text() des Originals.
// ---------------------------------------------------------------------------
static int fontImage(unsigned char t) {
  if (t >= 33 && t <= 34) return t - 33;
  if (t >= 39 && t <= 41) return t - 37;
  if (t >= 44 && t <= 59) return t - 39;
  if (t >= 64 && t <= 90) return t - 43;
  if (t >= 97 && t <= 122) return t - 49;
  return -1;
}

int gameTextWidth(const char *s, int scale) {
  int w = 0;
  for (; *s; s++) {
    if (*s == ' ') { w += 5; continue; }
    int img = fontImage((unsigned char)*s);
    if (img >= 0 && img < JNB_NUM_FONT) w += font_gob[img].w + 1;
  }
  return w * scale;
}

// Zeichnet ohne Rand; jedes Quellpixel wird zu einem scale x scale grossen Block.
static void gameTextPlain(int x, int y, const char *s, int scale, uint16_t col) {
  uint16_t *fb = canvas.getBuffer();
  int cur = x;
  for (; *s; s++) {
    if (*s == ' ') { cur += 5 * scale; continue; }
    int img = fontImage((unsigned char)*s);
    if (img < 0 || img >= JNB_NUM_FONT) continue;
    const JnbGobEntry &g = font_gob[img];
    const uint8_t *src = font_pixels + g.ofs;
    for (int row = 0; row < g.h; row++)
      for (int c = 0; c < g.w; c++) {
        if (!src[row * g.w + c]) continue;
        for (int sy = 0; sy < scale; sy++) {
          int py = y + row * scale + sy;
          if ((unsigned)py >= DISPLAY_HEIGHT) continue;
          uint16_t *d = fb + py * DISPLAY_WIDTH;
          for (int sx = 0; sx < scale; sx++) {
            int px = cur + c * scale + sx;
            if ((unsigned)px < DISPLAY_WIDTH) d[px] = col;
          }
        }
      }
    cur += (g.w + 1) * scale;
  }
}

// Weiss mit schwarzem Rand: acht versetzte schwarze Durchgaenge, dann die Fuellung.
void gameText(int x, int y, const char *s, int scale, uint16_t col) {
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
      if (dx || dy) gameTextPlain(x + dx * scale, y + dy * scale, s, scale, 0x0000);
  gameTextPlain(x, y, s, scale, col);
}

void gameTextCentered(int cx, int y, const char *s, int scale, uint16_t col) {
  gameText(cx - gameTextWidth(s, scale) / 2, y, s, scale, col);
}

// Groesste ganzzahlige Vergroesserung waehlen, die noch in maxW passt. Damit
// laeuft kein Text ueber den Rand, egal wie lang er ist.
int gameTextFit(int cx, int y, const char *s, int maxW, int maxScale, uint16_t col) {
  int scale = maxScale;
  while (scale > 1 && gameTextWidth(s, scale) > maxW) scale--;
  gameTextCentered(cx, y, s, scale, col);
  return scale;
}

// ---------------------------------------------------------------------------
// Titelbild und die Bilder davor/danach
// ---------------------------------------------------------------------------
void drawTitleBackdrop(void) {
  memcpy(canvas.getBuffer(), jnb_title, (size_t)JNB_TITLE_W * JNB_TITLE_H * 2);
}

// Weisse, halbdurchsichtige Box mit hellerem Rand.
void drawGlassBox(int x, int y, int w, int h, uint8_t opacity) {
  uint16_t *fb = canvas.getBuffer();
  for (int yy = y; yy < y + h; yy++) {
    if ((unsigned)yy >= DISPLAY_HEIGHT) break;
    uint16_t *d = fb + yy * DISPLAY_WIDTH;
    for (int xx = x; xx < x + w; xx++) {
      if ((unsigned)xx >= DISPLAY_WIDTH) break;
      bool edge = (yy == y || yy == y + h - 1 || xx == x || xx == x + w - 1);
      d[xx] = blend565(d[xx], 0xFFFF, edge ? 230 : opacity);
    }
  }
}

// Einen Hasen ganzzahlig vergroessert zeichnen (fuer das Menue).
static void drawRabbitScaled(int x, int y, int who, int scale, bool pale) {
  const JnbGobEntry &g = rabbit_gob[who * 18];
  const uint8_t *src = rabbit_pixels + g.ofs;
  uint16_t *fb = canvas.getBuffer();
  for (int row = 0; row < g.h; row++)
    for (int c = 0; c < g.w; c++) {
      uint8_t v = src[row * g.w + c];
      if (!v) continue;
      uint16_t col = pal[v];
      if (pale) col = blend565(col, 0x8410, 150);
      for (int sy = 0; sy < scale; sy++) {
        int py = y + row * scale + sy;
        if ((unsigned)py >= DISPLAY_HEIGHT) continue;
        for (int sx = 0; sx < scale; sx++) {
          int px = x + c * scale + sx;
          if ((unsigned)px < DISPLAY_WIDTH) fb[py * DISPLAY_WIDTH + px] = col;
        }
      }
    }
}

// Startmenue: Titelbild, darauf die Box mit HOST und JOIN. Je Zeile steht der
// Hase davor, den man in dieser Rolle spielt.
#define MENU_SCALE 2   // Vergroesserung der Schrift in der Box
#define MENU_BOX_Y 112 // Oberkante der Box

void drawRoleMenu(int sel) {
  drawTitleBackdrop();
  const int rowH = rabbit_gob[0].h * 2;      // der Hase ist die hoehere Zeile
  const int rowStep = rowH + 12;
  const int bh = 2 * rowH + 12 + 20;

  // Beide Zeilen buendig setzen: gemeinsame linke Kante aus der breiteren.
  const char *label[2] = {"HOST", "JOIN"};
  int rw = rabbit_gob[0].w * 2;
  int tw = gameTextWidth(label[0], MENU_SCALE);
  if (gameTextWidth(label[1], MENU_SCALE) > tw) tw = gameTextWidth(label[1], MENU_SCALE);
  const int content = rw + 12 + tw;
  const int gx = (DISPLAY_WIDTH - content) / 2;

  // Box am Inhalt ausrichten, aber nie breiter als der sichere Bereich.
  int bw = content + 56;
  if (bw > DISPLAY_WIDTH - 40) bw = DISPLAY_WIDTH - 40;
  const int bx = (DISPLAY_WIDTH - bw) / 2;
  drawGlassBox(bx, MENU_BOX_Y, bw, bh, 130);

  for (int i = 0; i < 2; i++) {
    int ty = MENU_BOX_Y + 10 + i * rowStep;
    // Der Hase behaelt immer seine Farbe - er zeigt, welchen man spielt.
    drawRabbitScaled(gx, ty, i, 2, false);
    // Schrift mittig zum Hasen ausrichten (10 Pixel Zeichenhoehe mal Faktor)
    gameText(gx + rw + 12, ty + (rowH - 10 * MENU_SCALE) / 2, label[i], MENU_SCALE,
             sel == i ? 0xFFFF : 0x9CD3);
  }
}

void drawInfoScreen(const char *a, const char *b) {
  drawTitleBackdrop();
  // gleiche Lage wie die Box im Startmenue, damit beim Wechsel nichts springt
  const int bw = 200, bx = (DISPLAY_WIDTH - bw) / 2;
  const int inner = bw - 16;
  drawGlassBox(bx, MENU_BOX_Y, bw, b ? 72 : 44, 140);
  int s1 = gameTextFit(DISPLAY_WIDTH / 2, MENU_BOX_Y + 12, a, inner, MENU_SCALE, 0xFFFF);
  if (b) gameTextFit(DISPLAY_WIDTH / 2, MENU_BOX_Y + 16 + s1 * 10 + 6, b, inner, 2, 0xFFFF);
}

void drawWinnerBanner(int status) {
  const int bw = 200, bh = 74;
  const int bx = (DISPLAY_WIDTH - bw) / 2, by = 90;
  drawGlassBox(bx, by, bw, bh, 150);
  gameTextFit(DISPLAY_WIDTH / 2, by + 12, status == 1 ? "HASE 1" : "HASE 2", bw - 16, 3, 0xFFFF);
  gameTextFit(DISPLAY_WIDTH / 2, by + 46, "GEWINNT!", bw - 16, 2, 0xFFFF);
}

// ---------------------------------------------------------------------------
void renderFrame(int me) {
  if (!bgwork) return;

  // Kamera sanft dem eigenen Hasen nachfuehren (hoechstens 6 Pixel je Bild,
  // damit das Scrollen ruhig bleibt und nicht zittert).
  int tx = (player[me].x >> 16) + 8 - VIEW_W / 2;
  if (tx < 0) tx = 0;
  if (tx > BGW - VIEW_W) tx = BGW - VIEW_W;
  const int step = 6;
  if (camx < tx) camx += (tx - camx > step) ? step : (tx - camx);
  else if (camx > tx) camx -= (camx - tx > step) ? step : (camx - tx);

  // ---- Hintergrund ----
  uint16_t *fb = canvas.getBuffer();
  for (int y = 0; y < VIEW_H; y++) {
    const uint8_t *s = bgwork + y * BGW + camx;
    uint16_t *d = fb + y * DISPLAY_WIDTH;
    for (int x = 0; x < VIEW_W; x++) d[x] = pal[s[x]];
  }

  // ---- Objekte (Reihenfolge wie draw_pobs: zuletzt erzeugtes zuerst) ----
  for (int c1 = NUM_OBJECTS - 1; c1 >= 0; c1--) {
    if (objects[c1].used != 1) continue;
    int ox = objects[c1].x >> 16, oy = objects[c1].y >> 16;
    int img;
    if (objects[c1].type == OBJ_FUR) {
      // Fellfetzen drehen sich in Flugrichtung (8 Stufen, wie im Original)
      int s1 = (int)(atan2f((float)objects[c1].y_add, (float)objects[c1].x_add) * 4.0f / (float)M_PI);
      if (s1 < 0) s1 += 8;
      if (s1 < 0) s1 = 0;
      if (s1 > 7) s1 = 7;
      img = objects[c1].frame + s1;
    } else if (objects[c1].type == OBJ_FLESH) {
      img = objects[c1].frame;
    } else {
      img = objects[c1].image;
    }
    putPob(ox, oy, img, objects_gob, objects_pixels, JNB_NUM_OBJECTS);
  }

  // ---- Hasen ----
  for (int i = 0; i < JNB_MAX_PLAYERS; i++) {
    if (player[i].enabled != 1) continue;
    putPob(player[i].x >> 16, player[i].y >> 16, player[i].image + i * 18, rabbit_gob,
           rabbit_pixels, JNB_NUM_RABBIT);
  }

  // ---- Fliegen: einzelne schwarze Pixel, nie vor dem Vordergrund ----
  if (flies_enabled) {
    for (int i = 0; i < NUM_FLIES; i++) {
      int fx = flies[i].x, fy = flies[i].y;
      if (maskAt(fx, fy)) continue;
      int dx = fx - camx;
      if (dx < 0 || fy < 0 || dx >= VIEW_W || fy >= VIEW_H) continue;
      fb[fy * DISPLAY_WIDTH + dx] = pal[0];
    }
  }

  // ---- Punkteleiste nur bei Aenderung neu zeichnen ----
  int rows = VIEW_H; // sonst reicht der Spielbereich - spart Uebertragungszeit
  if (player[0].bumps != lastBumps[0] || player[1].bumps != lastBumps[1] || hudDirty) {
    lastBumps[0] = player[0].bumps;
    lastBumps[1] = player[1].bumps;
    hudDirty = false;
    drawHud();
    rows = DISPLAY_HEIGHT;
  }

  // In vier Streifen uebertragen und dazwischen den USB-Link bedienen: die
  // Uebertragung dauert rund 16 ms, so lange darf der Stack nicht ruhen.
  tft.startWrite();
  tft.setAddrWindow(0, 0, DISPLAY_WIDTH, rows);
  const int slice = rows / 4;
  for (int i = 0; i < 4; i++) {
    int n = (i == 3) ? (rows - 3 * slice) : slice;
    tft.writePixels(fb + (size_t)i * slice * DISPLAY_WIDTH, (uint32_t)n * DISPLAY_WIDTH);
    linkPump();
  }
  tft.endWrite();
}
