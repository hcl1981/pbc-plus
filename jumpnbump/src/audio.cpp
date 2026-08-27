// ============================================================================
//  Tonausgabe.
//
//  Der Lautsprecher haengt an GP15. Ein PWM-Kanal laeuft mit 10 Bit Aufloesung
//  als Traeger (rund 146 kHz, weit oberhalb des Hoerbereichs); ein DMA-Kanalpaar
//  schiebt die fertigen Abtastwerte im Ping-Pong-Betrieb mit 22050 Hz hinein.
//  Nachgefuellt wird im DMA-Interrupt - dadurch laeuft der Ton auch waehrend der
//  17 ms, in denen das Bild ueber SPI zum Display geschoben wird, ohne Aussetzer.
//
//  Gemischt werden acht Effektstimmen (die Originaldaten aus den *.smp-Dateien,
//  8 Bit vorzeichenbehaftet) und vier Kanaele des ProTracker-Moduls bump.mod.
// ============================================================================
#include "jnb.h"
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>

#define SR 22050
#define ABUF 256 // Abtastwerte je Puffer -> 11,6 ms
#define PWM_TOP 1023
#define SOFT_KNEE 400   // ab hier weiche Begrenzung (max. Auslenkung ist 511)
#define MUSIC_VOL 48    // Musiklautstaerke 0..64 (Original blendet auf 30 hoch)
#define MOD_PERIOD_MIN 54   // erweiterter ProTracker-Umfang
#define MOD_PERIOD_MAX 3424

// ---------------------------------------------------------------------------
// Effektstimmen
// ---------------------------------------------------------------------------
#define NUM_VOICES 8

typedef struct {
  const int8_t *data;
  uint32_t pos;  // 16.16
  uint32_t step; // 16.16
  uint32_t len;  // in Abtastwerten
  uint16_t gain; // Aussteuerungsausgleich, 8.8 (256 = unveraendert)
  uint8_t vol;   // 0..64
  bool loop;
  volatile bool active;
} voice_t;

static voice_t voice[NUM_VOICES];

static const int8_t *sfxData[5] = {sfx_jump, sfx_death, sfx_spring, sfx_splash, sfx_fly};
static const uint32_t sfxLen[5] = {SFXLEN_JUMP, SFXLEN_DEATH, SFXLEN_SPRING, SFXLEN_SPLASH,
                                   SFXLEN_FLY};
// Die Originaldateien sind sehr unterschiedlich ausgesteuert: jump.smp ist ein
// Rechteck mit nur +-30, death.smp geht bis +-127. Beim Start wird je Datei der
// Spitzenwert gesucht und daraus ein Ausgleichsfaktor gebildet, damit alle
// Effekte gleich laut kommen und den Wertebereich wirklich ausnutzen.
static uint16_t sfxGain[5];

void sfxPlay(uint8_t num, uint16_t freq, uint8_t volume, int8_t channel) {
  if (num >= 5) return;
  int slot = channel;
  if (slot < 0) {
    // Die Fliegenstimme (Kanal SFX_FLY) ist reserviert und wird nie verdraengt.
    for (slot = 0; slot < NUM_VOICES; slot++)
      if (slot != SFX_FLY && !voice[slot].active) break;
    if (slot >= NUM_VOICES) {
      for (slot = 0; slot < NUM_VOICES; slot++)
        if (slot != SFX_FLY) break;
    }
  }
  if (slot >= NUM_VOICES) return;
  voice_t &s = voice[slot];
  s.active = false; // erst stumm schalten, dann umbauen
  s.data = sfxData[num];
  s.pos = 0;
  s.step = (uint32_t)(((uint64_t)freq << 16) / SR);
  s.len = sfxLen[num];
  s.gain = sfxGain[num];
  s.vol = volume > 64 ? 64 : volume;
  s.loop = (num == SFX_FLY);
  // Erst wenn alle Felder stehen, die Stimme fuer den Interrupt freigeben.
  __asm__ volatile("" ::: "memory");
  s.active = true;
}

void sfxChannelVolume(uint8_t channel, uint8_t volume) {
  if (channel < NUM_VOICES) voice[channel].vol = volume > 64 ? 64 : volume;
}

// ---------------------------------------------------------------------------
// ProTracker-Modul (4 Kanaele, 31 Instrumente)
// ---------------------------------------------------------------------------
typedef struct {
  uint32_t offset; // Beginn der Abtastwerte im Modul
  uint32_t length;
  uint32_t repeat, replen;
  int8_t finetune;
  uint8_t volume;
} modsmp_t;

typedef struct {
  const int8_t *data;
  uint32_t pos, step, len, rep, replen;
  uint16_t period, wantPeriod;
  uint8_t sample, volume;
  uint8_t effect, param;
  int8_t vibPos, vibSpeed, vibDepth;
  uint8_t portaSpeed;
  bool playing;
} modchan_t;

static modsmp_t msmp[31];
static modchan_t mch[4];
static uint8_t songLength, orderTab[128], numPatterns;
static const uint8_t *patData;
static uint8_t modSpeed = 6, modTick = 0, modRow = 0, modOrder = 0;
static uint16_t modBpm = 125;
static int32_t tickSamples = 0, tickCounter = 0;
static uint8_t modVolume = 0; // 0..64, wie dj_set_mod_volume
static bool modPlaying = false;
static uint8_t patDelay = 0;
static int16_t breakRow = -1, jumpOrder = -1;

// Periodentabelle (ProTracker, Oktaven 1..3, ohne Feinstimmung)
static const uint16_t periodTab[36] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113};

static const uint8_t vibTab[32] = {0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212,
                                  224, 235, 244, 250, 253, 255, 253, 250, 244, 235, 224,
                                  212, 197, 180, 161, 141, 120, 97,  74,  49,  24};

static uint32_t periodToStep(uint16_t period) {
  if (!period) return 0;
  // Amiga-PAL: Abtastrate = 7093789,2 / (2 * Periode)
  float rate = 7093789.2f / (2.0f * (float)period);
  return (uint32_t)(rate * 65536.0f / (float)SR);
}

static void modParse(void) {
  const uint8_t *m = jnb_mod;
  for (int i = 0; i < 31; i++) {
    const uint8_t *s = m + 20 + i * 30;
    msmp[i].length = ((s[22] << 8) | s[23]) * 2u;
    msmp[i].finetune = (int8_t)(s[24] & 0x0F);
    if (msmp[i].finetune > 7) msmp[i].finetune -= 16;
    msmp[i].volume = s[25] > 64 ? 64 : s[25];
    msmp[i].repeat = ((s[26] << 8) | s[27]) * 2u;
    msmp[i].replen = ((s[28] << 8) | s[29]) * 2u;
  }
  songLength = m[950];
  memcpy(orderTab, m + 952, 128);
  numPatterns = 0;
  for (int i = 0; i < 128; i++)
    if (orderTab[i] >= numPatterns) numPatterns = orderTab[i] + 1;
  patData = m + 1084;
  uint32_t ofs = 1084u + (uint32_t)numPatterns * 1024u;
  for (int i = 0; i < 31; i++) {
    msmp[i].offset = ofs;
    ofs += msmp[i].length;
    if (msmp[i].replen <= 2) msmp[i].replen = 0; // kein Schleifenbereich
  }
}

static void modTrigger(int c, uint8_t sample, uint16_t period, bool retrig) {
  modchan_t &ch = mch[c];
  if (sample) {
    ch.sample = sample;
    const modsmp_t &s = msmp[sample - 1];
    ch.data = (const int8_t *)(jnb_mod + s.offset);
    ch.len = s.length;
    ch.rep = s.repeat;
    ch.replen = s.replen;
    ch.volume = s.volume;
  }
  if (period) {
    ch.period = period;
    ch.step = periodToStep(period);
    if (retrig) {
      ch.pos = 0;
      ch.playing = (ch.len > 0);
      ch.vibPos = 0;
    }
  }
}

static void modRowStart(void) {
  const uint8_t *row = patData + (uint32_t)orderTab[modOrder] * 1024u + (uint32_t)modRow * 16u;
  for (int c = 0; c < 4; c++) {
    const uint8_t *n = row + c * 4;
    uint16_t period = ((n[0] & 0x0F) << 8) | n[1];
    uint8_t sample = (n[0] & 0xF0) | (n[2] >> 4);
    uint8_t eff = n[2] & 0x0F;
    uint8_t par = n[3];
    modchan_t &ch = mch[c];
    ch.effect = eff;
    ch.param = par;

    if (sample && sample <= 31) {
      ch.sample = sample;
      ch.volume = msmp[sample - 1].volume;
    }
    if (period) {
      if (eff == 0x3 || eff == 0x5) {
        ch.wantPeriod = period; // Tonhoehengleiten: Ziel merken, nicht neu anschlagen
      } else {
        uint8_t off = 0;
        if (eff == 0x9) off = par;
        modTrigger(c, sample, period, true);
        if (off) {
          uint32_t o = (uint32_t)off * 256u;
          if (o < ch.len) ch.pos = o << 16;
        }
      }
    } else if (sample) {
      modTrigger(c, sample, 0, false);
    }

    switch (eff) {
      case 0x3:
        if (par) ch.portaSpeed = par;
        break;
      case 0x4:
        if (par >> 4) ch.vibSpeed = par >> 4;
        if (par & 0x0F) ch.vibDepth = par & 0x0F;
        break;
      case 0xB: jumpOrder = par; break;
      case 0xC: ch.volume = par > 64 ? 64 : par; break;
      case 0xD: breakRow = (par >> 4) * 10 + (par & 0x0F); break;
      case 0xE:
        switch (par >> 4) {
          case 0x1: ch.period -= (par & 0x0F); ch.step = periodToStep(ch.period); break;
          case 0x2: ch.period += (par & 0x0F); ch.step = periodToStep(ch.period); break;
          case 0xA: ch.volume = (ch.volume + (par & 0x0F) > 64) ? 64 : ch.volume + (par & 0x0F); break;
          case 0xB: ch.volume = (ch.volume < (par & 0x0F)) ? 0 : ch.volume - (par & 0x0F); break;
          case 0xE: patDelay = par & 0x0F; break;
          default: break;
        }
        break;
      case 0xF:
        if (par < 0x20) {
          if (par) modSpeed = par;
        } else {
          modBpm = par;
          tickSamples = (SR * 5) / (modBpm * 2);
        }
        break;
      default: break;
    }
  }
}

static inline void volSlide(modchan_t &ch, uint8_t p) {
  if (p >> 4) ch.volume = (ch.volume + (p >> 4) > 64) ? 64 : ch.volume + (p >> 4);
  else if (p & 0x0F) ch.volume = (ch.volume < (p & 0x0F)) ? 0 : ch.volume - (p & 0x0F);
}

static void modTickEffects(void) {
  for (int c = 0; c < 4; c++) {
    modchan_t &ch = mch[c];
    uint8_t p = ch.param;
    switch (ch.effect) {
      case 0x0: // Arpeggio
        if (p) {
          int n = modTick % 3;
          int semi = (n == 0) ? 0 : ((n == 1) ? (p >> 4) : (p & 0x0F));
          int idx = -1;
          for (int i = 0; i < 36; i++)
            if (periodTab[i] == ch.period) { idx = i; break; }
          if (idx >= 0 && idx + semi < 36) ch.step = periodToStep(periodTab[idx + semi]);
          else ch.step = periodToStep(ch.period);
        }
        break;
      // bump.mod nutzt den erweiterten Tonumfang (Perioden 63..2280), deshalb
      // nicht auf die Standardtabelle 113..856 begrenzen.
      case 0x1:
        if (ch.period > p + MOD_PERIOD_MIN) ch.period -= p; else ch.period = MOD_PERIOD_MIN;
        ch.step = periodToStep(ch.period);
        break;
      case 0x2:
        ch.period += p;
        if (ch.period > MOD_PERIOD_MAX) ch.period = MOD_PERIOD_MAX;
        ch.step = periodToStep(ch.period);
        break;
      case 0x5:
      case 0x3: { // Tonhoehengleiten
        if (ch.wantPeriod) {
          if (ch.period < ch.wantPeriod) {
            ch.period += ch.portaSpeed;
            if (ch.period > ch.wantPeriod) ch.period = ch.wantPeriod;
          } else if (ch.period > ch.wantPeriod) {
            ch.period = (ch.period < ch.portaSpeed) ? ch.wantPeriod : ch.period - ch.portaSpeed;
            if (ch.period < ch.wantPeriod) ch.period = ch.wantPeriod;
          }
          ch.step = periodToStep(ch.period);
        }
        if (ch.effect == 0x5) volSlide(ch, p);
        break;
      }
      case 0x6:
      case 0x4: { // Vibrato
        int v = (vibTab[ch.vibPos & 31] * ch.vibDepth) / 128;
        uint16_t per = (ch.vibPos & 32) ? (ch.period - v) : (ch.period + v);
        if (per < MOD_PERIOD_MIN) per = MOD_PERIOD_MIN;
        ch.step = periodToStep(per);
        ch.vibPos = (int8_t)((ch.vibPos + ch.vibSpeed) & 63);
        if (ch.effect == 0x6) volSlide(ch, p);
        break;
      }
      case 0xA:
        volSlide(ch, p);
        break;
      case 0xE:
        if ((p >> 4) == 0x9 && (p & 0x0F) && (modTick % (p & 0x0F)) == 0) {
          ch.pos = 0;
          ch.playing = (ch.len > 0);
        } else if ((p >> 4) == 0xC && modTick == (p & 0x0F)) {
          ch.volume = 0;
        }
        break;
      default: break;
    }
  }
}

static void modAdvance(void) {
  modTick++;
  if (modTick >= modSpeed) {
    modTick = 0;
    if (patDelay) {
      patDelay--;
      return;
    }
    modRow++;
    if (breakRow >= 0 || jumpOrder >= 0 || modRow >= 64) {
      if (jumpOrder >= 0) {
        modOrder = (uint8_t)jumpOrder;
        modRow = (breakRow >= 0) ? (uint8_t)breakRow : 0;
      } else if (breakRow >= 0) {
        modOrder++;
        modRow = (uint8_t)breakRow;
      } else {
        modOrder++;
        modRow = 0;
      }
      breakRow = -1;
      jumpOrder = -1;
      if (modOrder >= songLength) modOrder = 0;
      if (modRow >= 64) modRow = 0;
    }
    modRowStart();
  } else {
    modTickEffects();
  }
}

void musicStart(void) {
  for (int c = 0; c < 4; c++) memset(&mch[c], 0, sizeof(modchan_t));
  modSpeed = 6;
  modBpm = 125;
  modTick = 0;
  modRow = 0;
  modOrder = 0;
  patDelay = 0;
  breakRow = jumpOrder = -1;
  tickSamples = (SR * 5) / (modBpm * 2);
  tickCounter = 0;
  modRowStart();
  modPlaying = true;
}
void musicStop(void) { modPlaying = false; }
void musicVolume(uint8_t v) { modVolume = v > 64 ? 64 : v; }
uint8_t musicMaxVolume(void) { return MUSIC_VOL; }

// ---------------------------------------------------------------------------
// Mischer + DMA
// ---------------------------------------------------------------------------
static uint16_t abuf[2][ABUF];
static int dmaA = -1, dmaB = -1;
static uint dmaTimer = 0;

static void fillBuffer(uint16_t *out) {
  int done = 0;
  while (done < ABUF) {
    int n = ABUF - done;
    if (modPlaying) {
      int left = (int)(tickSamples - tickCounter);
      if (left <= 0) {
        modAdvance();
        tickCounter = 0;
        left = tickSamples;
      }
      if (left < n) n = left;
      tickCounter += n;
    }

    for (int i = 0; i < n; i++) {
      int32_t accSfx = 0, accMod = 0;

      for (int v = 0; v < NUM_VOICES; v++) {
        voice_t &s = voice[v];
        if (!s.active) continue;
        uint32_t idx = s.pos >> 16;
        if (idx >= s.len) {
          if (s.loop) {
            s.pos = 0;
            idx = 0;
          } else {
            s.active = false;
            continue;
          }
        }
        accSfx += ((int32_t)s.data[idx] * (int32_t)s.vol * (int32_t)s.gain) >> 8;
        s.pos += s.step;
      }

      if (modPlaying && modVolume) {
        for (int c = 0; c < 4; c++) {
          modchan_t &ch = mch[c];
          if (!ch.playing || !ch.step) continue;
          uint32_t idx = ch.pos >> 16;
          if (idx >= ch.len) {
            if (ch.replen > 2) {
              ch.pos = (uint32_t)ch.rep << 16;
              idx = ch.rep;
            } else {
              ch.playing = false;
              continue;
            }
          } else if (ch.replen > 2 && idx >= ch.rep + ch.replen) {
            ch.pos -= (uint32_t)ch.replen << 16;
            idx = ch.pos >> 16;
          }
          if (idx >= ch.len) { ch.playing = false; continue; }
          accMod += ((int32_t)ch.data[idx] * (int32_t)ch.volume * (int32_t)modVolume) >> 6;
          ch.pos += ch.step;
        }
      }

      // Effekte und Musik getrennt skalieren; beide erreichen bei voller
      // Aussteuerung etwa +-500, also den ganzen Wertebereich.
      int32_t v = (accSfx >> 4) + (accMod >> 5);

      // Weiche Begrenzung oberhalb von SOFT_KNEE. Die Kennlinie naehert sich
      // dem Anschlag nur an und erreicht ihn nie, deshalb entsteht auch bei
      // gleichzeitiger Musik und mehreren Effekten kein hartes Abschneiden -
      // laute Stellen werden gestaucht statt zu klirren. Dadurch kann der
      // Mischpegel deutlich hoeher liegen als bei reiner Begrenzung.
      const int32_t range = 511 - SOFT_KNEE;
      if (v > SOFT_KNEE) {
        int32_t e = v - SOFT_KNEE;
        v = SOFT_KNEE + (range * e) / (e + range);
      } else if (v < -SOFT_KNEE) {
        int32_t e = -v - SOFT_KNEE;
        v = -SOFT_KNEE - (range * e) / (e + range);
      }
      out[done + i] = (uint16_t)(512 + v);
    }
    done += n;
  }
}

// Nach dem Ende einer Uebertragung stehen Leseadresse UND Zaehler auf 0. Beides
// muss neu gesetzt werden, sonst laeuft der verkettete Kanal mit Laenge 0 los.
static void audioIrq(void) {
  if (dma_channel_get_irq1_status(dmaA)) {
    dma_channel_acknowledge_irq1(dmaA);
    fillBuffer(abuf[0]);
    dma_channel_set_read_addr(dmaA, abuf[0], false);
    dma_channel_set_trans_count(dmaA, ABUF, false);
  }
  if (dma_channel_get_irq1_status(dmaB)) {
    dma_channel_acknowledge_irq1(dmaB);
    fillBuffer(abuf[1]);
    dma_channel_set_read_addr(dmaB, abuf[1], false);
    dma_channel_set_trans_count(dmaB, ABUF, false);
  }
}

void audioInit(void) {
  memset(voice, 0, sizeof(voice));
  modParse();

  // Aussteuerungsausgleich je Effekt bestimmen (einmalig, rund 60 kB lesen).
  for (int i = 0; i < 5; i++) {
    int peak = 1;
    for (uint32_t k = 0; k < sfxLen[i]; k++) {
      int a = sfxData[i][k];
      if (a < 0) a = -a;
      if (a > peak) peak = a;
    }
    uint32_t g = (127u * 256u) / (uint32_t)peak;
    if (g > 1024) g = 1024; // hoechstens vierfach anheben
    sfxGain[i] = (uint16_t)g;
  }

  uint slice = pwm_gpio_to_slice_num(SPEAKER);
  uint chan = pwm_gpio_to_channel(SPEAKER);
  gpio_set_function(SPEAKER, GPIO_FUNC_PWM);
  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv(&cfg, 1.0f);
  pwm_config_set_wrap(&cfg, PWM_TOP);
  pwm_init(slice, &cfg, true);
  pwm_set_chan_level(slice, chan, 512);

  for (int i = 0; i < ABUF; i++) abuf[0][i] = abuf[1][i] = 512;

  // DMA-Zeitgeber auf die Abtastrate stellen (Bruchteil des Systemtakts).
  dmaTimer = dma_claim_unused_timer(true);
  uint32_t den = clock_get_hz(clk_sys) / SR;
  if (den < 1) den = 1;
  if (den > 65535) den = 65535;
  dma_timer_set_fraction(dmaTimer, 1, (uint16_t)den);

  // Zielregister: die 16 oberen Bits von CC gehoeren zu Kanal B, die unteren
  // zu Kanal A - deshalb je nach Kanal um 2 Byte versetzt schreiben.
  volatile void *dst = (volatile void *)((uintptr_t)&pwm_hw->slice[slice].cc + (chan ? 2 : 0));

  dmaA = dma_claim_unused_channel(true);
  dmaB = dma_claim_unused_channel(true);

  dma_channel_config ca = dma_channel_get_default_config(dmaA);
  channel_config_set_transfer_data_size(&ca, DMA_SIZE_16);
  channel_config_set_read_increment(&ca, true);
  channel_config_set_write_increment(&ca, false);
  channel_config_set_dreq(&ca, dma_get_timer_dreq(dmaTimer));
  channel_config_set_chain_to(&ca, dmaB);
  dma_channel_configure(dmaA, &ca, dst, abuf[0], ABUF, false);

  dma_channel_config cb = dma_channel_get_default_config(dmaB);
  channel_config_set_transfer_data_size(&cb, DMA_SIZE_16);
  channel_config_set_read_increment(&cb, true);
  channel_config_set_write_increment(&cb, false);
  channel_config_set_dreq(&cb, dma_get_timer_dreq(dmaTimer));
  channel_config_set_chain_to(&cb, dmaA);
  dma_channel_configure(dmaB, &cb, dst, abuf[1], ABUF, false);

  // DMA_IRQ_1 verwenden: IRQ 0 nutzen andere Bibliotheken haeufig exklusiv.
  dma_channel_set_irq1_enabled(dmaA, true);
  dma_channel_set_irq1_enabled(dmaB, true);
  irq_add_shared_handler(DMA_IRQ_1, audioIrq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  // WICHTIG: Der Mischinterrupt muss NIEDRIGER stehen als der USB-Interrupt.
  // Er laeuft rund 100 us am Stueck (256 Abtastwerte ueber zwoelf Stimmen, dazu
  // Lesezugriffe ins Flash) und wuerde die USB-Bedienung sonst genau so lange
  // aufhalten. Auf dieser ohnehin grenzwertigen Direktverbindung reicht das,
  // um gelegentlich einen Transfer zu zerreissen. Umgekehrt ist es harmlos:
  // der Tonpuffer hat 11,6 ms Vorlauf und vertraegt jede USB-Unterbrechung.
  irq_set_priority(DMA_IRQ_1, PICO_LOWEST_IRQ_PRIORITY);
  irq_set_enabled(DMA_IRQ_1, true);

  dma_channel_start(dmaA);
}
