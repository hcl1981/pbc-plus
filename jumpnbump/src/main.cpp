// ============================================================================
//  Jump 'n Bump fuer zwei PicoBoy Color Plus, gekoppelt ueber USB (D+/D-).
//
//  Rollen: Ein Geraet ist HOST (nativer USB-Host), das andere JOIN (USB-Device
//  mit CDC). Die Rolle wird beim Start im Menue gewaehlt - es ist EINE Firmware.
//  Verkabelung: USB-C <-> USB-C, D+/D-/GND verbinden, VBUS NICHT (beide Geraete
//  sind eigenversorgt).
//
//  Aufgabenteilung im Netz:
//    Der HOST rechnet die vollstaendige Spielmechanik (Physik, Zusammenstoesse,
//    Tode, Punkte) und schickt 18 Byte Zustand je Bild. Der Gast schickt seine
//    drei Tasten. Alles Schmueckende - Rauch, Fellfetzen, Fleisch, Blutspur,
//    Wasserfontaenen, Schmetterlinge, Fliegenschwarm - laesst der Gast lokal
//    laufen und wird nur durch Ereignisbits im Zustandspaket angestossen. So
//    bleibt das Paket winzig und ein verlorenes Bild heilt sich von selbst.
//
//  Beide Geraete scrollen unabhaengig auf den EIGENEN Hasen.
// ============================================================================
#include "jnb.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_TinyUSB.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <SPI.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/resets.h>
#include <hardware/structs/usb.h>
#include <hardware/watchdog.h>

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);

enum Role { ROLE_HOST, ROLE_GUEST };
static Role myRole;
uint8_t myPlayer = 0;

// ---- Link-Rahmenformat (unveraendert aus der Spielesammlung uebernommen) ----
#define LINK_SYNC0 0xABu
#define LINK_SYNC1 0xCDu
#define LINK_T_DATA 0x44u   // Host  -> Gast
#define LINK_T_INPUT 0x49u  // Gast  -> Host (2 Byte Eingabe)
#define LINK_T_PING 0x50u   // Lebenszeichen
#define LINK_T_RHELLO 0x52u // "bei mir kommt nichts mehr an"
#define LINK_T_RACK 0x41u   // Bestaetigung des Rueckwegs
#define LINK_DEAD_MS 3000
#define LINK_MAXPL 250

// ---- Anwendungsprotokoll ---------------------------------------------------
#define JNB_HELLO 0xC0
#define JNB_STATE 0x53
#define JNB_RESTART 0xBE
#define JNB_STATE_LEN 18

Adafruit_USBH_Host USBHost;
Adafruit_USBH_CDC SerialHost;

static volatile uint8_t guestInputByte = 0;
static volatile uint8_t guestFireCnt = 0;
static volatile bool helloSeen = false; // Gast: Host hat den Start freigegeben
static void onLinkData(const uint8_t *d, uint8_t n); // Gast: Paket sofort anwenden
static void drawReconnecting(void);

// ============================================================================
class LinkClass {
public:
  bool _started = false;
  bool _isHost = false;
  volatile unsigned long _lastRx = 0;
  bool _wasMounted = false;
  bool _gameActive = false;
  volatile bool _peerRecovering = false;
  volatile bool _ackDue = false;
  uint8_t _reattachStage = 0;
  volatile uint8_t _lastInput[2] = {0, 0};
  volatile bool _haveInput = false;
  volatile uint32_t bytesIn = 0, framesIn = 0, crcErr = 0, txDrop = 0;
  volatile uint16_t reattachCnt = 0;
  uint8_t stallWhy = 0;
  // Diagnose: wie oft war die Verbindung gestoert, wie lange hat das eigene
  // Programm am Stueck nicht hingeschaut, und was war die laengste Pause.
  volatile uint16_t stallCnt = 0;
  volatile uint32_t blindMs = 0;
  volatile uint16_t blindMax = 0;

  bool isHost() const { return _isHost; }
  unsigned long lastRx() const { return _lastRx; }
  void gameStarted() {
    _gameActive = true;
    _lastRx = millis();
    _lastPump = millis(); // Uhr beim Spielstart neu stellen
    rp2040.wdt_begin(5000);
  }

  void startRole(bool host) {
    _isHost = host;
    _started = true;
    if (host) {
      tud_deinit(0);
      USBHost.begin(0);
      SerialHost.begin(115200);
    } else {
      // Selbstversorgtes Geraet ohne Host-VBUS: Erkennung erzwingen, damit das
      // anschliessende Anmelden greift.
      usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
      Serial.begin(115200);
      delay(50);
      tud_connect();
    }
  }

  // ---- Senden ----
  void sendData(const uint8_t *d, uint8_t n) {
    if (!_isHost || !SerialHost.mounted()) return;
    sendFrame(LINK_T_DATA, d, n);
  }
  void sendInputNow(uint8_t a, uint8_t b) {
    uint8_t d[2] = {a, b};
    sendFrame(LINK_T_INPUT, d, 2);
  }

  // ---- Zuletzt empfangene Gasteingabe ----
  void readGuestInput(uint8_t *dir, uint8_t *cnt) {
    *dir = _lastInput[0];
    *cnt = _lastInput[1];
  }

  bool stalled() const {
    return _gameActive && ((millis() - _lastRx) > LINK_DEAD_MS || _peerRecovering);
  }

  void service() {
    if (_gameActive) rp2040.wdt_reset();
    if (!_started) return;
    pumpUSB();
    if (stalled()) handleStall();
  }

  // Nur Empfangen/Senden, ohne Stoerungsbehandlung (siehe linkPump).
  void pumpOnly() {
    if (!_started) return;
    if (_gameActive) rp2040.wdt_reset();
    pumpUSB();
  }

private:
  unsigned long _lastPump = 0;

  // Hat das eigene Programm laenger als 100 ms nicht nach dem Link gesehen
  // (langer Bildaufbau, Levelaufbau, Wartebild), dann darf diese Zeit NICHT
  // der Gegenstelle als Ausfall angerechnet werden - sonst reisst die
  // Verbindung genau in solchen Momenten scheinbar ab. Die Blindzeit wird der
  // Empfangsuhr gutgeschrieben, hoechstens aber bis "jetzt".
  void creditBlindTime(unsigned long now) {
    if (_lastPump) {
      unsigned long gap = now - _lastPump;
      if (gap > 100) {
        blindMs += gap;
        if (gap > blindMax) blindMax = (uint16_t)(gap > 65535 ? 65535 : gap);
        unsigned long adjusted = _lastRx + gap;
        _lastRx = ((long)(adjusted - now) > 0) ? now : adjusted;
      }
    }
    _lastPump = now;
  }

  void pumpUSB(bool send = true) {
    creditBlindTime(millis());
    if (_isHost) {
      USBHost.task(0);
      bool m = SerialHost.mounted();
      if (m && !_wasMounted) {
        _st = 0;
        _pcnt = 0;
        _haveInput = false;
      }
      _wasMounted = m;
      int guard = 1024;
      while (m && SerialHost.available() && guard-- > 0) parseByte((uint8_t)SerialHost.read());
      if (_ackDue) {
        _ackDue = false;
        sendFrame(LINK_T_RACK, nullptr, 0);
      }
      static unsigned long lastPing = 0;
      if (send && (millis() - lastPing) >= 250) {
        lastPing = millis();
        sendFrame(LINK_T_PING, nullptr, 0);
      }
    } else {
      yield(); // bedient tud_task
      int guard = 1024;
      while (Serial.available() && guard-- > 0) parseByte((uint8_t)Serial.read());
      if (_ackDue) {
        _ackDue = false;
        sendFrame(LINK_T_RACK, nullptr, 0);
      }
      static unsigned long lastTx = 0;
      if (send && (millis() - lastTx) >= 12) {
        lastTx = millis();
        sendInputNow(guestInputByte, guestFireCnt);
      }
    }
  }

  void reattach() {
    _st = 0;
    _pcnt = 0;
    _reattachStage++;
    if (_reattachStage < 3) {
      if (_isHost) _wasMounted = false;
      else {
        tud_disconnect();
        delay(120);
        tud_connect();
      }
      return;
    }
    _reattachStage = 0;
    usbHardRestart();
  }

  void usbHardRestart() {
    irq_set_enabled(USBCTRL_IRQ, false);
    if (_isHost) {
      SerialHost.end();
      tuh_deinit(0);
    } else {
      tud_disconnect();
      tud_deinit(0);
    }
    reset_unreset_block_num_wait_blocking(RESET_USBCTRL);
    if (_isHost) {
      USBHost.begin(0);
      SerialHost.begin(115200);
      _wasMounted = false;
    } else {
      usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
      tud_init(0);
      tud_connect();
    }
    irq_set_enabled(USBCTRL_IRQ, true);
  }

  // Beide Seiten halten gemeinsam an und bauen die Verbindung neu auf; das
  // Spiel laeuft danach unveraendert aus dem Arbeitsspeicher weiter.
  void handleStall() {
    stallWhy = _peerRecovering ? 2 : 1;
    stallCnt++;
    const unsigned long period = _isHost ? 4000 : 2500;
    unsigned long lastTry = _isHost ? millis() : (millis() - period);
    unsigned long lastDraw = 0, lastHello = 0, escSince = 0, lastRejoin = 0;
    const unsigned long began = millis();
    while (stalled()) {
      rp2040.wdt_reset();
      pumpUSB(false);
      if (millis() - lastHello >= 250) {
        lastHello = millis();
        sendFrame(LINK_T_RHELLO, nullptr, 0);
      }
      // Der Gast koennte neu gestartet sein und auf das Begruessungsbyte warten.
      if (_isHost && millis() - lastRejoin >= 400) {
        lastRejoin = millis();
        uint8_t hs[1] = {JNB_HELLO};
        sendFrame(LINK_T_DATA, hs, 1);
      }
      if (millis() - lastDraw >= 250) {
        lastDraw = millis();
        drawReconnecting();
      }
      if (millis() - lastTry >= period) {
        lastTry = millis();
        reattachCnt++;
        reattach();
      }
      if (digitalRead(KEY_A) == LOW && digitalRead(KEY_B) == LOW) {
        if (escSince == 0) escSince = millis();
        else if (millis() - escSince > 1500) rp2040.reboot();
      } else escSince = 0;
      if (millis() - began > 30000) rp2040.reboot();
    }
    _st = 0;
    _pcnt = 0;
    _haveInput = false;
  }

  static uint8_t crc8(uint8_t type, uint8_t len, const uint8_t *d, uint16_t n) {
    uint8_t c = 0;
    uint8_t h[2] = {type, len};
    for (uint8_t i = 0; i < 2; i++) {
      c ^= h[i];
      for (uint8_t b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    for (uint16_t i = 0; i < n; i++) {
      c ^= d[i];
      for (uint8_t b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    return c;
  }

  void sendFrame(uint8_t type, const uint8_t *d, uint16_t n) {
    if (n > LINK_MAXPL) n = LINK_MAXPL;
    uint8_t f[4 + LINK_MAXPL + 1];
    f[0] = LINK_SYNC0;
    f[1] = LINK_SYNC1;
    f[2] = type;
    f[3] = (uint8_t)n;
    for (uint16_t i = 0; i < n; i++) f[4 + i] = d[i];
    f[4 + n] = crc8(type, (uint8_t)n, d, n);
    streamWrite(f, (uint16_t)(4 + n + 1));
  }

  // Nie unbegrenzt blockieren: stockt der Link, wuerde sonst die ganze
  // Spielschleife stehen bleiben. Ein verworfener Frame wird im naechsten
  // Bild ohnehin durch einen neuen ersetzt.
  void streamWrite(const uint8_t *d, uint16_t n) {
    if (_isHost) {
      if (!SerialHost.mounted()) {
        txDrop++;
        return;
      }
      unsigned long t = millis();
      while (SerialHost.mounted() && (uint16_t)SerialHost.availableForWrite() < n &&
             (millis() - t) < 5)
        USBHost.task(0);
      if (SerialHost.mounted() && (uint16_t)SerialHost.availableForWrite() >= n) {
        SerialHost.write(d, n);
        SerialHost.flush();
      } else txDrop++;
    } else {
      // tud_cdc_write() umgeht das DTR-Tor von Serial.write() - DTR wird auf
      // dieser Verbindung nie gesetzt.
      if (tud_cdc_write_available() >= n) {
        tud_cdc_write(d, n);
        tud_cdc_write_flush();
      } else txDrop++;
    }
  }

  uint8_t _st = 0, _ptype = 0, _plen = 0, _pcnt = 0;
  uint8_t _pbuf[LINK_MAXPL];
  unsigned long _lastByte = 0;
  void parseByte(uint8_t b) {
    bytesIn++;
    unsigned long now = millis();
    if (_st != 0 && (now - _lastByte) > 50) _st = 0; // Luecke -> Rest verwerfen
    _lastByte = now;
    switch (_st) {
      case 0: _st = (b == LINK_SYNC0) ? 1 : 0; break;
      case 1: _st = (b == LINK_SYNC1) ? 2 : ((b == LINK_SYNC0) ? 1 : 0); break;
      case 2: _ptype = b; _st = 3; break;
      case 3: _plen = b; _pcnt = 0; _st = (b == 0) ? 5 : 4; break;
      case 4: _pbuf[_pcnt++] = b; if (_pcnt >= _plen) _st = 5; break;
      case 5:
        if (crc8(_ptype, _plen, _pbuf, _plen) == b) deliver(_ptype, _pbuf, _plen);
        else crcErr++;
        _st = 0;
        break;
    }
  }

  void deliver(uint8_t type, const uint8_t *d, uint8_t n) {
    framesIn++;
    if (type == LINK_T_RHELLO) {
      _peerRecovering = true;
      _ackDue = true;
      return;
    }
    _lastRx = millis();
    _peerRecovering = false;
    if (type == LINK_T_RACK) return;
    if (_isHost) {
      if (type == LINK_T_INPUT) {
        _lastInput[0] = (n > 0) ? d[0] : 0;
        _lastInput[1] = (n > 1) ? d[1] : 0;
        _haveInput = true;
      }
    } else {
      // Sofort anwenden statt zwischenzupuffern: kommen zwei Pakete in einem
      // Durchlauf an, ginge sonst das erste verloren.
      if (type == LINK_T_DATA) onLinkData(d, n);
    }
  }
};

LinkClass Link;

extern "C" void tuh_cdc_mount_cb(uint8_t idx) { SerialHost.mount(idx); }
extern "C" void tuh_cdc_umount_cb(uint8_t idx) { SerialHost.umount(idx); }

static void linkDelay(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) Link.service();
}

// Wird aus dem Renderer zwischen den Bildstreifen aufgerufen. Bewusst OHNE die
// Stoerungsbehandlung: mitten in einer laufenden SPI-Uebertragung darf kein
// Banner gezeichnet werden. Erkannt wird die Stoerung dann im naechsten
// Link.service() der Spielschleife.
void linkPump(void) { Link.pumpOnly(); }

// ============================================================================
//  Eingabe und Bildausgabe
// ============================================================================
#define IN_LEFT 1
#define IN_RIGHT 2
#define IN_UP 4

static uint8_t readMyInput(void) {
  uint8_t b = 0;
  if (digitalRead(KEY_LEFT) == LOW) b |= IN_LEFT;
  if (digitalRead(KEY_RIGHT) == LOW) b |= IN_RIGHT;
  // Springen: Steuerkreuz nach oben oder eine der beiden Tasten.
  if (digitalRead(KEY_UP) == LOW || digitalRead(KEY_A) == LOW || digitalRead(KEY_B) == LOW)
    b |= IN_UP;
  return b;
}
static bool aOrB(void) { return digitalRead(KEY_A) == LOW || digitalRead(KEY_B) == LOW; }

static void pushCanvas(void) {
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
}

static void drawCenteredText(const char *t, int by, uint16_t c) {
  int16_t x0, y0;
  uint16_t w, h;
  canvas.getTextBounds(t, 0, by, &x0, &y0, &w, &h);
  canvas.setTextColor(c);
  canvas.setCursor((DISPLAY_WIDTH - (int)w) / 2 - x0, by);
  canvas.print(t);
}

static void drawInfo(const char *a, const char *b) {
  drawInfoScreen(a, b);
  pushCanvas();
}

// Banner ueber dem eingefrorenen Bild, waehrend die Verbindung neu aufgebaut
// wird. Nur hier stehen Messwerte - im Spiel selbst wird nichts eingeblendet.
//   why  TIMEOUT = hier kam nichts mehr an | PEER = Gegenstelle meldete Ausfall
//   mnt  USB-Verbindung besteht
//   F/C  empfangene Rahmen / Pruefsummenfehler
//   txd  verworfene Sendeframes      try  Neuverbindungsversuche
//   st   bisherige Stoerungen        bl   eigene Blindzeit, laengste Pause
static void drawReconnecting(void) {
  const int bw = 220, bh = 100;
  const int bx = (DISPLAY_WIDTH - bw) / 2, by = (DISPLAY_HEIGHT - bh) / 2;
  canvas.fillRect(bx, by, bw, bh, 0x0000);
  canvas.drawRect(bx, by, bw, bh, 0xFFE0);
  canvas.setFont(&FreeSansBold12pt7b);
  drawCenteredText("Reconnecting...", by + 26, 0xFFE0);
  canvas.setFont(NULL);
  canvas.setTextSize(1);
  char ln[40];
  bool mnt = Link.isHost() ? SerialHost.mounted() : (tud_mounted() != 0);
  canvas.setTextColor(0xFFFF);
  snprintf(ln, sizeof(ln), "%s  why:%s  mnt:%d", Link.isHost() ? "HOST" : "JOIN",
           (Link.stallWhy == 2) ? "PEER" : "TIMEOUT", mnt ? 1 : 0);
  canvas.setCursor(bx + 8, by + 44);
  canvas.print(ln);
  snprintf(ln, sizeof(ln), "F:%lu C:%lu txd:%lu try:%u", (unsigned long)Link.framesIn,
           (unsigned long)Link.crcErr, (unsigned long)Link.txDrop, (unsigned)Link.reattachCnt);
  canvas.setCursor(bx + 8, by + 56);
  canvas.print(ln);
  snprintf(ln, sizeof(ln), "st:%u bl:%lu max:%u", (unsigned)Link.stallCnt,
           (unsigned long)Link.blindMs, (unsigned)Link.blindMax);
  canvas.setCursor(bx + 8, by + 68);
  canvas.print(ln);
  canvas.setTextColor(0xC618);
  canvas.setCursor(bx + 8, by + 82);
  canvas.print("A+B lang = Neustart");
  pushCanvas();
}

// ============================================================================
//  Spielzustand / Netzpaket
// ============================================================================
static uint8_t matchStatus = 0; // 0 laeuft, 1 = Hase 1 gewinnt, 2 = Hase 2
static uint8_t lastDeaths[JNB_MAX_PLAYERS] = {0, 0};
static uint8_t modVol = 0;

// Klaenge und Schmuckobjekte zu den Ereignisbits eines Spielers.
// spawnObjects=true nur beim Gast: dort entstehen Rauch und Fontaene lokal,
// beim Host sind sie schon in steer_players() erzeugt worden.
static void applyPlayerFx(int i, uint8_t flags, bool spawnObjects) {
  if (flags & SF_JUMP)
    sfxPlay(SFX_JUMP, (uint16_t)(SFX_JUMP_FREQ + rnd(2000) - 1000), 64, -1);
  if (flags & SF_SPRING) {
    sfxPlay(SFX_SPRING, (uint16_t)(SFX_SPRING_FREQ + rnd(2000) - 1000), 64, -1);
    if (spawnObjects) {
      int s1 = (player[i].x >> 16), s2 = (player[i].y >> 16);
      for (int c2 = 0; c2 < NUM_OBJECTS; c2++) {
        if (objects[c2].used == 1 && objects[c2].type == OBJ_SPRING &&
            (objects[c2].y >> 20) == ((s2 + 15) >> 4) &&
            abs((objects[c2].x >> 20) - ((s1 + 8) >> 4)) <= 1) {
          objects[c2].frame = 0;
          objects[c2].ticks = 3;
          objects[c2].image = 0;
          break;
        }
      }
    }
  }
  if (flags & SF_SPLASH) {
    sfxPlay(SFX_SPLASH, (uint16_t)(SFX_SPLASH_FREQ + rnd(2000) - 1000), 64, -1);
    if (spawnObjects)
      add_object(OBJ_SPLASH, (player[i].x >> 16) + 8, ((player[i].y >> 16) & 0xfff0) + 15, 0, 0,
                 OBJ_ANIM_SPLASH, 0);
  }
  if ((flags & SF_SMOKE) && spawnObjects)
    add_object(OBJ_SMOKE, (player[i].x >> 16) + 2 + rnd(9), (player[i].y >> 16) + 13 + rnd(5), 0,
               -16384 - rnd(8192), OBJ_ANIM_SMOKE, 0);
}

// Ereignisbits eines ganzen Bildes. Laufen mehrere Rechenschritte je Bild,
// duerfen die Bits des ersten Schritts nicht verlorengehen.
static uint8_t frameFx[JNB_MAX_PLAYERS] = {0, 0};

static void buildState(uint8_t *b) {
  b[0] = JNB_STATE;
  b[1] = matchStatus;
  b[2] = (uint8_t)player[0].bumps;
  b[3] = (uint8_t)player[1].bumps;
  int o = 4;
  for (int i = 0; i < JNB_MAX_PLAYERS; i++) {
    int16_t px = (int16_t)(player[i].x >> 16);
    int16_t py = (int16_t)(player[i].y >> 16);
    b[o++] = (uint8_t)(px & 0xFF);
    b[o++] = (uint8_t)((px >> 8) & 0xFF);
    b[o++] = (uint8_t)(py & 0xFF);
    b[o++] = (uint8_t)((py >> 8) & 0xFF);
    b[o++] = (uint8_t)player[i].image;
    b[o++] = (uint8_t)((player[i].dead_flag ? 1 : 0) | (player[i].in_water ? 2 : 0) |
                       ((frameFx[i] & 0x0F) << 4));
    b[o++] = player[i].deaths;
  }
}

static void applyState(const uint8_t *b, uint8_t n) {
  if (n < JNB_STATE_LEN) return;
  matchStatus = b[1];
  player[0].bumps = b[2];
  player[1].bumps = b[3];
  int o = 4;
  for (int i = 0; i < JNB_MAX_PLAYERS; i++) {
    int16_t px = (int16_t)(b[o] | (b[o + 1] << 8));
    int16_t py = (int16_t)(b[o + 2] | (b[o + 3] << 8));
    o += 4;
    uint8_t img = b[o++];
    uint8_t fl = b[o++];
    uint8_t deaths = b[o++];
    player[i].enabled = 1;
    player[i].x = (int)px << 16;
    player[i].y = (int)py << 16;
    player[i].image = img;
    player[i].dead_flag = (fl & 1) ? 1 : 0;
    player[i].in_water = (fl & 2) ? 1 : 0;
    // Neuer Tod -> die gleichen Gedaerme und den Todesklang lokal erzeugen.
    if (deaths != lastDeaths[i]) {
      lastDeaths[i] = deaths;
      spawn_gore(i, player[i].x, player[i].y);
    }
    applyPlayerFx(i, (uint8_t)(fl >> 4), true);
  }
}

// Der Neustart baut Objektliste und Hintergrund komplett um. Das darf NICHT
// aus dem Empfangspfad heraus geschehen, denn der laeuft auch mitten im
// Bildaufbau (linkPump zwischen den Bildstreifen) - der Renderer wuerde dann
// ueber eine Liste laufen, die sich unter ihm veraendert. Also nur vormerken
// und in der Spielschleife ausfuehren.
static volatile bool restartPending = false;

static void onLinkData(const uint8_t *d, uint8_t n) {
  if (n < 1) return;
  if (d[0] == JNB_HELLO) helloSeen = true;
  else if (d[0] == JNB_STATE) applyState(d, n);
  else if (d[0] == JNB_RESTART) restartPending = true;
}

// ============================================================================
//  Menue
// ============================================================================
static void flushPress(void) {
  while (aOrB()) linkDelay(10);
  linkDelay(80);
}

static void selectRole(void) {
  int sel = 0;
  bool pU = false, pD = false, pAB = false, redraw = true;
  while (true) {
    bool U = digitalRead(KEY_UP) == LOW, D = digitalRead(KEY_DOWN) == LOW;
    bool AB = aOrB();
    if (U && !pU && sel != 0) { sel = 0; redraw = true; }
    if (D && !pD && sel != 1) { sel = 1; redraw = true; }
    if (AB && !pAB) {
      myRole = (sel == 0) ? ROLE_HOST : ROLE_GUEST;
      myPlayer = (sel == 0) ? 0 : 1;
      flushPress();
      return;
    }
    pU = U; pD = D; pAB = AB;
    if (redraw) {
      drawRoleMenu(sel);
      pushCanvas();
      redraw = false;
    }
    delay(30);
  }
}

static void connectAsHost(void) {
  drawInfo("HOSTING", "WARTE AUF GAST");
  uint8_t dir = 0, cnt = 0;
  unsigned long lastHello = 0;
  unsigned long t0 = millis();
  uint32_t f0 = Link.framesIn;
  while (true) {
    Link.service();
    if (SerialHost.mounted()) {
      Link.readGuestInput(&dir, &cnt);
      if (dir == JNB_HELLO) break; // der Gast meldet sich mit 0xC0
      // Nur der Host wurde neu gestartet: der Gast laeuft laengst und schickt
      // echte Tasten statt der Begruessung. Dann an seinem Datenstrom erkennen.
      if ((Link.framesIn - f0) > 40 && (millis() - t0) > 1500) break;
    }
    // Begruessung schon waehrend des Wartens senden, damit ein bereits
    // wartender Gast in jeder Einschaltreihenfolge weiterkommt.
    if (millis() - lastHello >= 400) {
      lastHello = millis();
      uint8_t hs[1] = {JNB_HELLO};
      Link.sendData(hs, 1);
    }
    linkDelay(5);
  }
  for (int i = 0; i < 5; i++) {
    uint8_t hs[1] = {JNB_HELLO};
    Link.sendData(hs, 1);
    linkDelay(30);
  }
}

static void connectAsGuest(void) {
  guestInputByte = JNB_HELLO; // Erkennungszeichen fuer den Host
  drawInfo("JOIN", "WARTE AUF HOST");
  while (true) {
    if (helloSeen) break;
    linkDelay(5);
  }
  guestInputByte = 0;
}

// ============================================================================
//  Spielschleifen
// ============================================================================
// Musik beim Spielstart hochblenden (im Original bis 30 von 64; hier lauter,
// siehe MUSIC_VOL in audio.cpp).
static void musicFade(void) {
  if (modVol < musicMaxVolume()) {
    modVol++;
    musicVolume(modVol);
  }
}

static void drawWinBanner(void) { drawWinnerBanner(matchStatus); }

static void hostMain(void) {
  gameInit();
  matchStatus = 0;
  lastDeaths[0] = lastDeaths[1] = 0;
  musicStart();
  unsigned long lastUs = micros();
  int32_t acc = 0;
  bool pAB = false;
  uint8_t prevDir = 0, prevFire = 0;
  Link.readGuestInput(&prevDir, &prevFire);

  while (true) {
    Link.service();

    unsigned long nowUs = micros();
    int32_t dt = (int32_t)(nowUs - lastUs);
    lastUs = nowUs;
    if (dt < 0 || dt > 200000) dt = 16667;
    acc += dt;
    int steps = 0;
    while (acc >= 16667 && steps < 4) { // Originaltakt: 60 Hz
      acc -= 16667;
      steps++;

      uint8_t gDir = 0, gCnt = 0;
      Link.readGuestInput(&gDir, &gCnt);
      uint8_t hDir = readMyInput();

      for (int i = 0; i < JNB_MAX_PLAYERS; i++) player[i].sfxflags = 0;
      if (matchStatus == 0) {
        player[0].action_left = (hDir & IN_LEFT) ? 1 : 0;
        player[0].action_right = (hDir & IN_RIGHT) ? 1 : 0;
        player[0].action_up = (hDir & IN_UP) ? 1 : 0;
        player[1].action_left = (gDir & IN_LEFT) ? 1 : 0;
        player[1].action_right = (gDir & IN_RIGHT) ? 1 : 0;
        player[1].action_up = (gDir & IN_UP) ? 1 : 0;
        steer_players();
        collision_check();
        if (player[0].bumps >= JNB_END_SCORE) matchStatus = 1;
        else if (player[1].bumps >= JNB_END_SCORE) matchStatus = 2;
      }
      update_objects();
      if (flies_enabled) update_flies(1);
      for (int i = 0; i < JNB_MAX_PLAYERS; i++) {
        if (player[i].sfxflags) applyPlayerFx(i, player[i].sfxflags, false);
        frameFx[i] |= player[i].sfxflags;
      }
      musicFade();
    }

    uint8_t pkt[JNB_STATE_LEN];
    buildState(pkt);
    Link.sendData(pkt, JNB_STATE_LEN);
    frameFx[0] = frameFx[1] = 0;

    renderFrame(myPlayer);
    if (matchStatus != 0) {
      drawWinBanner();
      pushCanvas();
    }

    // Neustart ist von beiden Seiten aus moeglich
    bool AB = aOrB();
    uint8_t gDir, gCnt;
    Link.readGuestInput(&gDir, &gCnt);
    bool guestWants = (gCnt != prevFire);
    prevFire = gCnt;
    if (matchStatus != 0 && ((AB && !pAB) || guestWants)) {
      for (int i = 0; i < 5; i++) {
        uint8_t r[1] = {JNB_RESTART};
        Link.sendData(r, 1);
        linkDelay(40);
      }
      gameInit();
      matchStatus = 0;
      lastDeaths[0] = lastDeaths[1] = 0;
      pAB = false;
      // Zaehler und Zeitbasis nach der Sendepause neu aufsetzen, sonst loest
      // ein waehrenddessen weitergezaehlter Tastendruck des Gasts sofort den
      // naechsten Neustart aus.
      Link.readGuestInput(&prevDir, &prevFire);
      lastUs = micros();
      acc = 0;
      continue;
    }
    pAB = AB;
  }
}

static void guestMain(void) {
  gameInit();
  matchStatus = 0;
  lastDeaths[0] = lastDeaths[1] = 0;
  musicStart();
  unsigned long lastUs = micros();
  int32_t acc = 0;
  bool pF = false;

  while (true) {
    Link.service();
    guestInputByte = readMyInput();
    bool cF = aOrB();
    if (cF && !pF && matchStatus != 0) guestFireCnt++;
    pF = cF;

    unsigned long nowUs = micros();
    int32_t dt = (int32_t)(nowUs - lastUs);
    lastUs = nowUs;
    if (dt < 0 || dt > 200000) dt = 16667;
    acc += dt;
    int steps = 0;
    while (acc >= 16667 && steps < 4) {
      acc -= 16667;
      steps++;
      // Nur das Schmueckende: Partikel, Falter, Fliegen. Die Hasen kommen
      // vollstaendig aus dem Zustandspaket des Hosts.
      update_objects();
      if (flies_enabled) update_flies(1);
      musicFade();
    }

    if (restartPending) {
      restartPending = false;
      gameInit();
      matchStatus = 0;
      lastDeaths[0] = lastDeaths[1] = 0;
    }

    renderFrame(myPlayer);
    if (matchStatus != 0) {
      drawWinBanner();
      pushCanvas();
    }
  }
}

// ============================================================================
void setup() {
  tft.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  tft.setSPISpeed(62500000);
  pinMode(BACKLIGHT, OUTPUT);
  analogWrite(BACKLIGHT, 255);
  pinMode(KEY_RIGHT, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_LEFT, INPUT_PULLUP);
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_CENTER, INPUT_PULLUP);
  pinMode(KEY_A, INPUT_PULLUP);
  pinMode(KEY_B, INPUT_PULLUP);
  tft.fillScreen(0x0000);

  // Beim Booten USB-unsichtbar bleiben: erst mit der Rollenwahl anmelden.
  tud_disconnect();

  if (!renderInit()) {
    // Sollte nicht vorkommen (90 kB von 520 kB), aber lieber eine klare
    // Meldung als ein schwarzes Bild.
    canvas.fillScreen(0x0000);
    canvas.setFont(&FreeSansBold12pt7b);
    drawCenteredText("Out of memory", 140, 0xF800);
    pushCanvas();
    while (true) delay(1000);
  }
  audioInit();

  selectRole();

  Link.startRole(myRole == ROLE_HOST);
  if (myRole == ROLE_HOST) {
    connectAsHost();
    linkDelay(400);
    Link.gameStarted();
    hostMain();
  } else {
    connectAsGuest();
    linkDelay(400);
    Link.gameStarted();
    guestMain();
  }
}

void loop() {}
