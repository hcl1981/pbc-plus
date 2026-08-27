// Rendert Musik und Effekte in WAV-Dateien und prueft sie statistisch.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

// Die zu pruefende Quelldatei direkt einbinden, um an die internen
// Mischfunktionen zu kommen.
#include "audio.cpp"

pwm_hw_t pwm_hw_inst;
pwm_hw_t *pwm_hw = &pwm_hw_inst;

static void writeWAV(const char *fn, const std::vector<uint16_t> &s) {
  FILE *f = fopen(fn, "wb");
  uint32_t n = s.size() * 2, rate = SR;
  fwrite("RIFF", 1, 4, f);
  uint32_t v = 36 + n;
  fwrite(&v, 4, 1, f);
  fwrite("WAVEfmt ", 1, 8, f);
  v = 16; fwrite(&v, 4, 1, f);
  uint16_t w = 1; fwrite(&w, 2, 1, f);
  w = 1; fwrite(&w, 2, 1, f);
  fwrite(&rate, 4, 1, f);
  v = rate * 2; fwrite(&v, 4, 1, f);
  w = 2; fwrite(&w, 2, 1, f);
  w = 16; fwrite(&w, 2, 1, f);
  fwrite("data", 1, 4, f);
  fwrite(&n, 4, 1, f);
  for (uint16_t x : s) {
    int16_t o = (int16_t)(((int)x - 512) * 32); // 10 Bit -> 16 Bit
    fwrite(&o, 2, 1, f);
  }
  fclose(f);
}

struct Stats { int lo, hi; double rms; int clipLo, clipHi, silent; };
static Stats analyse(const std::vector<uint16_t> &s) {
  Stats st = {9999, -9999, 0, 0, 0, 0};
  double acc = 0;
  for (uint16_t x : s) {
    int d = (int)x - 512;
    if (d < st.lo) st.lo = d;
    if (d > st.hi) st.hi = d;
    acc += (double)d * d;
    if (x == 0) st.clipLo++;
    if (x == PWM_TOP) st.clipHi++;
    if (d == 0) st.silent++;
  }
  st.rms = s.empty() ? 0 : sqrt(acc / s.size());
  return st;
}

int main() {
  audioInit();

  // ---- 1: Nur Musik, 20 Sekunden ----
  musicStart();
  musicVolume(musicMaxVolume());
  std::vector<uint16_t> mus;
  uint16_t buf[ABUF];
  int rowsSeen = 0, lastRow = -1, orderMax = 0;
  int nbuf = (SR * 20) / ABUF;
  for (int i = 0; i < nbuf; i++) {
    fillBuffer(buf);
    mus.insert(mus.end(), buf, buf + ABUF);
    if (modRow != lastRow) { rowsSeen++; lastRow = modRow; }
    if (modOrder > orderMax) orderMax = modOrder;
  }
  Stats m = analyse(mus);
  printf("Musik 20 s: Pegel %d..%d (max +-511)  RMS %.1f  Uebersteuerung %d  Stille %.1f%%\n",
         m.lo, m.hi, m.rms, m.clipLo + m.clipHi, 100.0 * m.silent / mus.size());
  printf("            Zeilenwechsel %d, hoechstes Muster %d von %d, Tempo %u BPM, Speed %u\n",
         rowsSeen, orderMax, songLength - 1, modBpm, modSpeed);
  double rowsPerSec = rowsSeen / 20.0;
  double expect = (double)modBpm * 2.0 / 5.0 / modSpeed;
  printf("            Zeilen/s: gemessen %.2f, erwartet %.2f -> %s\n", rowsPerSec, expect,
         (rowsPerSec > expect * 0.9 && rowsPerSec < expect * 1.1) ? "OK" : "FEHLER");
  writeWAV("music.wav", mus);

  // ---- 2: Nur Effekte ----
  musicStop();
  memset(voice, 0, sizeof(voice));
  std::vector<uint16_t> fx;
  struct { const char *n; int id; int freq; } list[] = {
      {"Sprung", SFX_JUMP, SFX_JUMP_FREQ},
      {"Tod", SFX_DEATH, SFX_DEATH_FREQ},
      {"Feder", SFX_SPRING, SFX_SPRING_FREQ},
      {"Platsch", SFX_SPLASH, SFX_SPLASH_FREQ}};
  for (auto &e : list) {
    size_t before = fx.size();
    sfxPlay(e.id, e.freq, 64, -1);
    for (int i = 0; i < (SR * 3) / 4 / ABUF; i++) {
      fillBuffer(buf);
      fx.insert(fx.end(), buf, buf + ABUF);
    }
    std::vector<uint16_t> one(fx.begin() + before, fx.end());
    Stats s = analyse(one);
    // Laenge des Klangs in Millisekunden bestimmen
    int last = 0;
    for (size_t i = 0; i < one.size(); i++)
      if ((int)one[i] - 512 != 0) last = i;
    printf("Effekt %-8s Pegel %4d..%4d  RMS %6.1f  Dauer %5.0f ms  Uebersteuerung %d\n", e.n, s.lo,
           s.hi, s.rms, 1000.0 * last / SR, s.clipLo + s.clipHi);
  }
  writeWAV("sfx.wav", fx);

  // ---- 3: Fliegen-Dauerschleife ----
  memset(voice, 0, sizeof(voice));
  sfxPlay(SFX_FLY, SFX_FLY_FREQ, 32, SFX_FLY);
  std::vector<uint16_t> fly;
  for (int i = 0; i < (SR * 5) / ABUF; i++) {
    fillBuffer(buf);
    fly.insert(fly.end(), buf, buf + ABUF);
  }
  Stats fs = analyse(fly);
  // laeuft die Schleife durch? Zweite Haelfte darf nicht still sein.
  std::vector<uint16_t> half(fly.begin() + fly.size() / 2, fly.end());
  Stats hs = analyse(half);
  printf("Fliegen 5 s (Datei ist 2,7 s lang): RMS gesamt %.1f, zweite Haelfte %.1f -> %s\n", fs.rms,
         hs.rms, hs.rms > 1.0 ? "Schleife laeuft" : "FEHLER: verstummt");

  // ---- 4: Alles zusammen - Uebersteuerung? ----
  musicStart();
  musicVolume(musicMaxVolume());
  memset(voice, 0, sizeof(voice));
  sfxPlay(SFX_FLY, SFX_FLY_FREQ, 32, SFX_FLY);
  std::vector<uint16_t> all;
  for (int i = 0; i < (SR * 6) / ABUF; i++) {
    if (i % 20 == 0) sfxPlay(SFX_DEATH, SFX_DEATH_FREQ, 64, -1);
    if (i % 13 == 0) sfxPlay(SFX_JUMP, SFX_JUMP_FREQ, 64, -1);
    if (i % 31 == 0) sfxPlay(SFX_SPRING, SFX_SPRING_FREQ, 64, -1);
    fillBuffer(buf);
    all.insert(all.end(), buf, buf + ABUF);
  }
  Stats as = analyse(all);
  printf("Musik + 4 Effekte gleichzeitig: Pegel %d..%d  RMS %.1f  Uebersteuerung %d von %zu "
         "(%.3f%%) -> %s\n",
         as.lo, as.hi, as.rms, as.clipLo + as.clipHi, all.size(),
         100.0 * (as.clipLo + as.clipHi) / all.size(),
         (as.clipLo + as.clipHi) * 1000 < (int)all.size() ? "OK" : "zu laut");
  writeWAV("mixed.wav", all);
  return 0;
}
