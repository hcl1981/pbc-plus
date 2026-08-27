#include <Arduino.h>
// =============================================================================
// PicoBoy Color - Multiplayer-Sammlung ueber USB (D+/D-)
//   - TRON, PONG, DUELL, RENNEN ...
// Kommunikation laeuft ueber die native USB-Schnittstelle (D+/D-):
//   HOST-Firmware  = nativer USB-Host,  JOIN-Firmware = nativer USB-Device.
// Verkabelung: USB-C <-> USB-C, D+/D-/GND verbinden, VBUS NICHT verbinden.
// (Frueher: I2C ueber SDA=GP20/SCL=GP21 - jetzt ersetzt durch LinkClass.)
// =============================================================================
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>   // ersetzt <Link.h> - Kommunikation jetzt ueber USB (D+/D-)
#include <math.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define TFT_CS 10
#define TFT_RST 9
#define TFT_DC 8
#define KEY_RIGHT 1
#define KEY_DOWN 2
#define KEY_LEFT 3
#define KEY_UP 4
#define KEY_CENTER 0
#define KEY_A 27
#define KEY_B 28
#define BACKLIGHT 26
#define SPEAKER 15
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 280
// Die Verbindung laeuft ueber USB (D+/D-), nicht mehr ueber I2C. Die alten
// Pins SDA=GP20/SCL=GP21 werden nicht mehr belegt und sind deshalb entfernt.
// LinkClass behaelt bewusst die I2C-artigen Methodennamen
// (beginTransmission/requestFrom/write), damit die Spielelogik unveraendert
// bleiben konnte - I2C_ADDR ist jetzt schlicht die Kennung im USB-Protokoll.
#define I2C_ADDR 0x42

#define COL_BG 0x0000
#define COL_FRAME 0x18E3
// Spielerfarben - wirken ueberall: Figuren in Bombing Bob, Spuren in Lightcycle,
// Schlaeger, Schiffe, Panzer, Steine, Punktestaende.
#define COL_P1 0xFA48   // Rot  (255, 72, 65)
#define COL_P2 0x551F   // Blau ( 82,161,255)
#define COL_HEAD 0xFFFF
#define COL_TEXT 0xFFFF
#define COL_DIM 0x7BEF
#define COL_BALL 0xFFE0
#define COL_SHOT_P1 0xAFFF
#define COL_SHOT_P2 0xFE40
#define COL_TRACK 0x4208
#define COL_GRASS 0x0320

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);

enum Role { ROLE_HOST, ROLE_GUEST };
enum GameId { GAME_TRON = 1, GAME_PONG = 2, GAME_DUEL = 3, GAME_ARTY = 4, GAME_BOMBER = 5, GAME_CHESS = 6, GAME_WNR = 7, GAME_C4 = 8, GAME_DOTS = 9 };
Role myRole;
uint8_t selectedGame = GAME_TRON;

volatile uint8_t guestInputByte = 0;
volatile uint8_t guestFireCnt = 0;
volatile uint8_t recvBuf[256];
volatile uint16_t recvLen = 0;
volatile bool recvNew = false;

// =============================================================================
//  USB-Link  -  ersetzt die bisherige I2C/Wire-Kommunikation
//  EINE Firmware, Rolle wird ZUR LAUFZEIT im Menue gewaehlt:
//    Menue "HOST"  -> nativer USB-Host   (Rootport 0)
//    Menue "JOIN"  -> nativer USB-Device (CDC / Serial)
//  Moeglich durch eigene Dual-Role tusb_config (Device + Host gleichzeitig
//  einkompiliert); bei HOST wird der Device-Stack per tud_deinit(0)
//  abgebaut und mit tuh_init(0)/USBHost.begin(0) der Host-Stack gestartet.
//  Verbindung: USB-C <-> USB-C, Datenkommunikation ueber D+/D-.
//  WICHTIG: VBUS-Ader NICHT verbinden (beide Geraete eigenversorgt!), GND JA.
//  Die LinkClass bildet die frueher genutzte Wire-API nach, damit der
//  restliche Spielcode praktisch unveraendert bleibt.
// =============================================================================
#include <hardware/structs/usb.h>        // fuer VBUS-Detect-Override (RP2350)
#include <hardware/watchdog.h>           // Absturz-Aufzeichnung ueber Neustart hinweg
#include <hardware/pwm.h>                // eigener, nicht blockierender Tongenerator
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/resets.h>             // USB-Block gezielt zuruecksetzen

// ---- "Blackbox": ueberlebt einen Neustart -------------------------------
// Die Watchdog-Scratch-Register bleiben bei einem Reset erhalten (nur ein
// echter Power-Off loescht sie). Waehrend des Spiels wird dort laufend der
// Link-Zustand abgelegt; nach einem Neustart zeigt das Menue, WAS zuletzt los
// war. So sieht man auch dann noch die Ursache, wenn man nicht danebensteht.
#define BB_MAGIC 0x4C494E4Bu             // 'LINK'
#define BBOX_MAGIC watchdog_hw->scratch[0]
#define BBOX_FLAGS watchdog_hw->scratch[1]
#define BBOX_FRAMES watchdog_hw->scratch[2]
#define BBOX_TIME  watchdog_hw->scratch[3]
char crashInfo[4][34] = { "", "", "", "" };  // aufbereitete Anzeige nach dem Neustart
bool haveCrashInfo = false;
bool autoResume = false;                 // nach Watchdog-Neustart Spiel selbst fortsetzen
// Wo war das Programm zuletzt? Zeigt nach einem Neustart, ob es im Spiel, in der
// Reconnect-Anzeige oder MITTEN im Neuverbinden stehen geblieben ist.
volatile uint8_t linkPhase = 0;          // 0 Spiel, 1 Reconnect-Anzeige, 2 im Neuverbinden
void beepService();                      // schaltet den Ton ab (Definition weiter unten)
void pushCanvas();                       // Bild zum Display (Definition weiter unten)

// ---- Positionsmarker: WO steht das Programm gerade? ---------------------
// Wird laufend (ein Registerzugriff) in die Blackbox geschrieben. Bleibt das
// Board haengen, verraet der zuletzt gesetzte Wert nach dem Neustart die
// genaue Stelle - damit muss nicht mehr geraten werden.
#define POS_GAME    1   // Spiel-/Zeichenlogik (ausserhalb der Link-Schicht)
#define POS_USBTASK 2   // USBHost.task / yield (USB-Stack)
#define POS_RX      3   // Empfangsschleife
#define POS_TX      4   // Senden
#define POS_BEEP    5   // Tonausgabe
#define POS_DISPLAY 6   // Bilduebertragung zum Display (SPI)
#define POS_STALL   7   // Reconnect-Anzeige
#define POS_REATT   8   // USB-Neuverbindung
// Interrupt-Zaehler aus dem gepatchten USB-Treiber (siehe patches/rp2040_usb.c).
// Entscheidend fuer die Diagnose: rast der Zaehler, wird die Hauptschleife von
// einem staendig neu ausgeloesten Interrupt ausgehungert; steht er still, haengt
// wirklich Code fest. Von aussen sieht beides identisch aus.
extern "C" volatile uint32_t usbcollection_irq_count;
uint8_t irqRate = 0;                     // Interrupts je 10 ms, gedeckelt bei 255
volatile uint8_t gPos = 0;
volatile uint8_t gS1 = 0, gS2 = 0;       // aktueller Punktestand (fuer Fortsetzen)
uint8_t crashCount = 0;                  // Abstuerze seit dem Einschalten
uint8_t resumeS1 = 0, resumeS2 = 0;      // Punktestand vor dem Absturz
char crashTag[16] = "";                  // Kurzhinweis, dauerhaft im Spiel sichtbar

// Speicherbereich, den die Startroutine NICHT zurueckstellt (.uninitialized_data,
// im Linkerskript als NOLOAD angelegt). Ein Watchdog-Neustart loescht den
// Arbeitsspeicher nicht - so ueberlebt der komplette Spielzustand einen
// Absturz und es kann exakt an derselben Stelle weitergehen.
#define PERSIST __attribute__((section(".uninitialized_data")))
PERSIST uint32_t persistMagic;           // gueltig nur bei genau diesem Wert
PERSIST uint8_t  persistGame;            // fuer welches Spiel der Zustand gilt
#define PERSIST_MAGIC 0x42424F42u        // 'BBOB'
// true = nach einem Watchdog-Neustart liegt ein gueltiger Zustand dieses Spiels
// im Speicher und darf weiterverwendet werden (statt neu zu initialisieren).
bool persistUsable(uint8_t game) {
  return autoResume && persistMagic == PERSIST_MAGIC && persistGame == game;
}
void persistArm(uint8_t game) { persistGame = game; persistMagic = PERSIST_MAGIC; }
static inline void markPos(uint8_t p) {
  gPos = p;
  BBOX_FRAMES = (BBOX_FRAMES & 0xFF00FFFFu) | ((uint32_t)p << 16);
}
// Jedes Spiel meldet hier seinen Punktestand, damit er einen Absturz uebersteht.
static inline void reportScore(uint8_t a, uint8_t b) { gS1 = a; gS2 = b; }

#define LINK_SYNC0   0xABu
#define LINK_SYNC1   0xCDu
#define LINK_T_DATA  0x44u   // 'D'  Host  -> Guest (Spielzustand/Befehle)
#define LINK_T_INPUT 0x49u   // 'I'  Guest -> Host  (2 Byte Eingabe)
#define LINK_T_PING  0x50u   // 'P'  Keepalive (nur Lebendigkeit, kein Inhalt)
#define LINK_T_RHELLO 0x52u  // 'R'  Recovery-Hello: "bei MIR kommt nichts mehr an"
#define LINK_T_RACK   0x41u  // 'A'  Recovery-Ack: Antwort auf RHELLO (beweist den Rueckweg)
#define LINK_DEAD_MS 3000    // ms ohne Frame -> USB-Verbindung neu aufbauen (kein Reboot)
#define LINK_MAXPL   250     // < 256: das Laengenfeld ist 1 Byte; 256 wuerde als 0 gesendet

Adafruit_USBH_Host USBHost;    // immer vorhanden (CFG_TUH_ENABLED=1)
Adafruit_USBH_CDC  SerialHost;

class LinkClass {
public:
  // ---- Registrierte Callbacks (nur Guest genutzt) ----
  void (*_onRecv)(int) = nullptr;
  void (*_onReq)(void)  = nullptr;
  void onReceive(void (*cb)(int)) { _onRecv = cb; }
  void onRequest(void (*cb)(void)) { _onReq = cb; }

  // ---- Kompatibilitaets-No-Ops (frueher I2C-Pin/Clock-Setup) ----
  void setSDA(int) {}
  void setSCL(int) {}
  void setClock(uint32_t) {}

  // ---- Rolle ----
  bool _started = false;
  bool _isHost  = false;

  // ---- Link-Tod-Erkennung / Auto-Recovery ----
  // Auf dem marginalen (nicht USB-konformen) Link kann die Verbindung
  // gelegentlich hart abreissen (Endpoint-Stall). Dann bekommt eine Seite
  // keine Frames mehr und ihr Bild friert ein. Statt manuellem Reset:
  // wenn waehrend des Spiels laenger kein gueltiger Frame ankommt, neu
  // starten -> beide landen wieder im Menue und koennen neu verbinden.
  volatile unsigned long _lastRx = 0;   // millis() des letzten gueltigen Frames
  bool _wasMounted = false;             // Flanke fuer Host-Re-Mount (Parser-Reset)
  bool _gameActive = false;             // erst nach erfolgreichem Connect scharf
  volatile bool _peerRecovering = false; // Gegenstelle meldet per RHELLO: Link tot
  volatile bool _ackDue = false;         // RHELLO empfangen -> RACK zurueckschicken
  uint8_t _reattachStage = 0;            // 0..2 sanft, dann harter USB-Reset
  // Ab Spielstart laeuft ein Hardware-Watchdog: bleibt die Hauptschleife wirklich
  // stehen (Absturz/Endlosschleife), startet das Board nach ~5 s selbst neu und
  // landet im Menue - statt fuer immer mit eingefrorenem Bild dazustehen.
  // Gefuettert wird in service() UND in der Reconnect-Pause, d.h. ein normaler
  // Verbindungsausfall loest KEINEN Neustart aus (Banner bleibt bedienbar).
  void gameStarted() { _gameActive = true; _lastRx = millis(); rp2040.wdt_begin(5000); }

  // Beim Boot ist (durch den Core) der Device-Stack aktiv. Die endgueltige
  // Rolle wird nach der Menue-Auswahl durch startRole() festgelegt.
  void startRole(bool host) {
    _isHost   = host;
    _started  = true;
    if (host) {
      // Vom Boot aktiver Device-Stack -> abbauen, dann nativen Host starten.
      tud_deinit(0);
      USBHost.begin(0);               // tuh_init auf nativem Rootport 0
      SerialHost.begin(115200);
    } else {
      // Selbstversorgtes Device ohne Host-VBUS: VBUS-Erkennung ZUERST erzwingen,
      // damit das darauffolgende Connect (in Serial.begin) sauber greift.
      usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS |
                    USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
      // Das Board war seit dem Boot USB-unsichtbar (tud_disconnect in setup).
      // Serial.begin() registriert jetzt die CDC-Schnittstelle, dann melden
      // wir uns FRISCH mit CDC an. Der Host (egal ob schon wartend oder erst
      // spaeter) enumeriert damit ein sauberes CDC-Geraet -> reihenfolge-
      // unabhaengig.
      Serial.begin(115200);
      delay(50);
      tud_connect();
    }
  }

  // ---- Wire-Kompatibilitaet: begin() ist No-Op (Rolle via startRole) ----
  void begin()        {}
  void begin(uint8_t) {}

  // ---- Sende-Aufbau (Host-Datenpaket ODER Guest-Antwort) ----
  uint8_t  _txbuf[LINK_MAXPL];
  uint16_t _txlen = 0;
  void beginTransmission(uint8_t) { _txlen = 0; }
  size_t write(uint8_t b) { if (_txlen < LINK_MAXPL) _txbuf[_txlen++] = b; return 1; }
  uint8_t endTransmission() {
    if (_isHost) {
      if (!SerialHost.mounted()) return 4;  // Peer-CDC noch nicht gemountet
      if (_txlen) sendFrame(LINK_T_DATA, _txbuf, _txlen);
      service();
      return 0;
    }
    return 0;   // Guest sendet keine Datenpakete
  }

  // ---- Empfangs-Lesepuffer (Host: Eingabe-Antwort / Guest: Daten-Payload) ----
  uint8_t  _rx[LINK_MAXPL];
  uint16_t _rxlen = 0, _rxpos = 0;
  int available() { return (int)_rxlen - (int)_rxpos; }
  int read() { return (_rxpos < _rxlen) ? _rx[_rxpos++] : -1; }

  // ---- Host: zuletzt empfangene Gast-Eingabe (2 Byte) ----
  volatile uint8_t _lastInput[2] = {0, 0};
  volatile bool    _haveInput = false;

  // ---- Diagnose-Zaehler ----
  volatile uint32_t bytesIn = 0;    // gesehene Roh-Bytes vom USB-Strom
  volatile uint32_t framesIn = 0;   // gueltige Frames (CRC ok)
  volatile uint32_t crcErr  = 0;    // Frames mit falscher Pruefsumme (Leitungsfehler)
  volatile uint32_t txDrop  = 0;    // verworfene Sendeframes (FIFO voll / nicht gemountet)
  volatile uint16_t reattachCnt = 0;// Neuverbindungs-Versuche
  uint8_t  stallWhy = 0;            // 1 = hier kam nichts an, 2 = Gegenstelle meldete Ausfall
  uint32_t _sBytes = 0, _sFrames = 0, _sCrc = 0;   // Staende beim Start der Stoerung
  bool isHost() const { return _isHost; }

  // Link.requestFrom(addr, n): liefert die zuletzt empfangenen Eingabe-Bytes.
  uint8_t requestFrom(uint8_t, uint8_t n) {
    service();
    if (_isHost && !_haveInput) {             // Handshake: kurz auf erstes Paket warten
      unsigned long t = millis();
      while (!_haveInput && (millis() - t) < 60) service();
    }
    if (n > LINK_MAXPL) n = LINK_MAXPL;
    _rxlen = n; _rxpos = 0;
    for (uint8_t i = 0; i < n; i++) _rx[i] = (i < 2) ? _lastInput[i] : 0;
    return n;
  }

  // ---- zyklische Bedienung: MUSS in jeder Schleife laufen (ersetzt I2C-ISR) ----
  // true = seit LINK_DEAD_MS kein Frame mehr -> Verbindung gilt als gestoert.
  // Gestoert ist der Link, wenn hier nichts mehr ankommt ODER die Gegenstelle
  // per RHELLO meldet, dass bei IHR nichts mehr ankommt. Der zweite Fall ist
  // entscheidend: sonst laeuft eine Seite munter weiter (ihre Empfangsrichtung
  // ist ja intakt), waehrend die andere ewig im Banner haengt - und da nur der
  // Gast neu enumerieren kann, kaeme die Verbindung nie zurueck.
  bool stalled() const {
    return _gameActive && ((millis() - _lastRx) > LINK_DEAD_MS || _peerRecovering);
  }

  // Laufend den Zustand in die Blackbox schreiben (billig: 4 Registerzugriffe).
  void blackbox() {
    unsigned long age = millis() - _lastRx;
    uint32_t f = 0;
    f |= (_isHost ? 1u : 0u);
    f |= (_isHost ? (SerialHost.mounted() ? 2u : 0u) : (tud_mounted() ? 2u : 0u));
    f |= (stalled() ? 4u : 0u);
    f |= (stallWhy == 2 ? 8u : 0u);
    f |= ((uint32_t)(selectedGame & 0x0F) << 4);
    f |= ((uint32_t)(linkPhase & 3) << 30);    // wo stand das Programm zuletzt
    f |= ((uint32_t)(reattachCnt > 255 ? 255 : reattachCnt) << 8);
    f |= ((uint32_t)(txDrop > 255 ? 255 : txDrop) << 16);
    f |= ((uint32_t)(irqRate > 63 ? 63 : irqRate) << 24); // 6 Bit (24..29): Interruptrate
    BBOX_FLAGS  = f;
    BBOX_FRAMES = (framesIn & 0xFFFFu) | ((uint32_t)gPos << 16)
                | ((uint32_t)(gS1 & 0x0F) << 24) | ((uint32_t)(gS2 & 0x0F) << 28);
    uint32_t a10 = (age > 25500UL) ? 255u : (uint32_t)(age / 100);
    BBOX_TIME   = (uint32_t)((millis() / 1000) & 0xFFFF)
                | (a10 << 16) | ((uint32_t)crashCount << 24);
    BBOX_MAGIC  = BB_MAGIC;
  }

  void service() {
    markPos(POS_BEEP);
    beepService();                             // Ton zeitgesteuert beenden
    if (_gameActive) {
      rp2040.wdt_reset();                      // Watchdog fuettern: Schleife lebt
      static unsigned long lastBb = 0;
      static uint32_t lastIrq = 0;
      if (millis() - lastBb >= 100) {
        lastBb = millis();
        uint32_t now = usbcollection_irq_count;
        uint32_t d = (now - lastIrq) / 10;      // pro 10 ms
        lastIrq = now;
        irqRate = (d > 255) ? 255 : (uint8_t)d;
        blackbox();
      }
    }
    if (!_started) { markPos(POS_GAME); return; }
    pumpUSB();
    markPos(POS_GAME);                         // zurueck in die Spiel-/Zeichenlogik
    // Bei Stoerung NICHT weiterrechnen: hier blockieren, "Reconnecting..." zeigen
    // und die USB-Verbindung neu aufbauen. Da ALLE Spiel-Loops service() aufrufen,
    // pausieren so beide Seiten gemeinsam und laufen nach dem Reconnect weiter.
    if (stalled()) handleStall();
  }

private:
  void sendInput() {
    if (!_onReq) return;
    _txlen = 0;
    _onReq();                                  // schreibt via write() 2 Byte in _txbuf
    sendFrame(LINK_T_INPUT, _txbuf, _txlen);
  }

  void sendPing() {                            // Keepalive-Frame (0 Byte Nutzlast)
    sendFrame(LINK_T_PING, _txbuf, 0);
  }
  void sendHello() { sendFrame(LINK_T_RHELLO, _txbuf, 0); }   // "mein Link ist tot"
  void sendAck()   { sendFrame(LINK_T_RACK,   _txbuf, 0); }   // Antwort darauf

  // Nicht-blockierendes Bedienen der USB-Verbindung (Empfang + periodisches
  // Senden). Wird in service() und waehrend der Reconnect-Pause aufgerufen.
  // send=false: nur empfangen/USB-Stack bedienen, NICHT senden. Waehrend der
  // Reconnect-Pause genutzt, damit die stille Seite die Gegenseite ebenfalls
  // verhungern laesst -> beide pausieren gemeinsam (Banner auf beiden Geraeten).
  void pumpUSB(bool send = true) {
    markPos(POS_USBTASK);
    if (_isHost) {
      USBHost.task(0);                         // nicht blockierend
      bool m = SerialHost.mounted();
      // Beim (Re-)Mount nur den Parser zuruecksetzen - NICHT _lastRx auffrischen.
      // Sonst haelt ein blosses Re-Mount (z.B. waehrend der Gast staendig
      // tud_disconnect/connect versucht) den Host faelschlich "am Leben", er
      // stallt nie und zeigt nie das Banner -> das einseitige "Reconnecting..".
      // Lebendigkeit kommt ausschliesslich aus echt empfangenen Frames (deliver()).
      if (m && !_wasMounted) { _st = 0; _pcnt = 0; _haveInput = false; }  // frisch (re)gemountet
      _wasMounted = m;
      // BEGRENZT leeren: meldet available() durch einen Treiberfehler dauerhaft
      // Daten, wuerde eine ungebremste Schleife das Programm hier festnageln
      // (Bild eingefroren, USB stirbt) - genau das Symptom auf der Gast-Seite.
      markPos(POS_RX);
      int guard = 1024;
      while (m && SerialHost.available() && guard-- > 0)
        parseByte((uint8_t)SerialHost.read());
      if (_ackDue) { _ackDue = false; sendAck(); }   // RACK IMMER senden (auch in der Pause)
      static unsigned long lastPing = 0;       // Keepalive alle 250 ms
      if (send && (millis() - lastPing) >= 250) { lastPing = millis(); sendPing(); }
    } else {
      yield();                                 // bedient TinyUSB-Device (tud_task)
      markPos(POS_RX);                         // begrenzt leeren (siehe Host-Zweig)
      int guard = 1024;
      while (Serial.available() && guard-- > 0) parseByte((uint8_t)Serial.read());
      // Eingabe periodisch pushen. 12 ms (~83 Hz) reicht dem schnellsten Spiel
      // (Pong-Tick 25 ms) locker und entlastet die marginale Leitung deutlich
      // (vorher 3 ms/330 Hz = unnoetig viele Kleinst-Transfers -> Wedge-Risiko).
      static unsigned long lastTx = 0;
      if (_ackDue) { _ackDue = false; sendAck(); }   // RACK IMMER senden (auch in der Pause)
      if (send && (millis() - lastTx) >= 12) { lastTx = millis(); sendInput(); }
    }
  }

  // USB-Verbindung neu aufbauen statt Reboot. Der Gast meldet sich frisch an
  // (tud_disconnect -> tud_connect); der Host erzwingt nichts - er erkennt das
  // Ab-/Anstecken ueber USBHost.task und re-mountet automatisch (Flanke in pumpUSB).
  // NUR die USB-Einheit neu starten - nicht das ganze Geraet. Das Spiel laeuft
  // im Arbeitsspeicher unangetastet weiter, es gibt keinen Neustart und keinen
  // Verlust. Stufe 1 meldet sich lediglich neu an; hilft das nicht, setzt Stufe 2
  // den USB-Baustein per Reset-Controller komplett zurueck und baut den Stack neu
  // auf. (Frueher scheiterte das an Haengern im Treiber - die sind inzwischen
  // behoben, siehe patches/.)
  void reattach() {
    _st = 0; _pcnt = 0;                        // halb empfangenen Frame verwerfen
    _reattachStage++;
    if (_reattachStage < 3) {
      // Stufe 1: sanft - nur neu anmelden bzw. Mount-Zustand verwerfen.
      if (_isHost) _wasMounted = false;
      else { tud_disconnect(); delay(120); tud_connect(); }
      return;
    }
    _reattachStage = 0;
    // Stufe 2: USB-Baustein hart zuruecksetzen und Stack neu aufsetzen.
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
    // Hardware-Reset des USB-Blocks (aus dem Reset holen und warten)
    reset_unreset_block_num_wait_blocking(RESET_USBCTRL);
    if (_isHost) {
      USBHost.begin(0);
      SerialHost.begin(115200);
      _wasMounted = false;
    } else {
      usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS |
                    USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
      tud_init(0);
      tud_connect();
    }
    irq_set_enabled(USBCTRL_IRQ, true);
  }

  // Reconnect-Pause: beide Seiten halten hier an, zeigen "Reconnecting..." und
  // bauen die Verbindung neu auf. Rueckkehr erst, wenn wieder Frames ankommen
  // (_lastRx frisch) -> das Spiel laeuft dann nahtlos aus dem RAM weiter. Kommt
  // die Leitung nie zurueck, bleibt es hier stehen (mit Info) und versucht es
  // alle LINK_DEAD_MS erneut - kein Rueckschritt gegenueber dem alten Einfrieren.
  void handleStall() {
    // Reconnect-Versuche STAFFELN, sonst sabotieren sich beide Seiten:
    // Eine USB-Enumeration dauert mehrere hundert ms. Wuerden Host und Gast im
    // gleichen Takt (und sofort) neu ansetzen, risse der Host den Bus genau dann
    // ab, wenn der Gast mitten in seiner Neuanmeldung steckt - jedes Mal.
    // Darum: Gast sofort und alle 2500 ms (leichtes disconnect/connect), Host
    // erst nach 4000 ms und dann alle 4000 ms (schwerer Stack-Neuaufbau).
    // Ungleiche Perioden lassen die Phasen driften -> es entsteht zwangslaeufig
    // ein ungestoertes Fenster fuer eine saubere Enumeration.
    // Ursache und Zaehlerstaende beim Start festhalten (fuer die Diagnose-Anzeige):
    // 2 = die Gegenstelle hat den Ausfall gemeldet (dort brach es zuerst ab),
    // 1 = hier kam selbst nichts mehr an.
    stallWhy = _peerRecovering ? 2 : 1;
    _sBytes = bytesIn; _sFrames = framesIn; _sCrc = crcErr;
    const unsigned long period = _isHost ? 4000 : 2500;
    unsigned long lastTry = _isHost ? millis() : (millis() - period);
    unsigned long lastDraw = 0, lastHello = 0, escSince = 0, lastRejoin = 0;
    const unsigned long stallBegan = millis();
    while (stalled()) {
      // Waehrend der Pause KEINE normalen Frames (die wuerden der Gegenseite
      // "alles gut" vorgaukeln), sondern alle 250 ms ein RHELLO: damit weiss die
      // Gegenseite, dass sie ebenfalls in die Recovery muss (Banner auf BEIDEN,
      // und der Gast meldet sich neu an - nur er kann re-enumerieren).
      // RACKs gehen unabhaengig davon raus (siehe pumpUSB).
      rp2040.wdt_reset();                 // in der Pause weiterfuettern (kein Neustart)
      beepService();
      markPos(POS_STALL);
      linkPhase = 1;                      // "BANNER"
      blackbox();                         // auch hier aufzeichnen (sonst friert der Stand ein)
      pumpUSB(false);
      if (millis() - lastHello >= 250)    { lastHello = millis(); sendHello(); }
      // Der Gast koennte inzwischen NEU GESTARTET sein und in seinem
      // Verbindungsaufbau auf das Handshake-Byte 0xC0 warten - das sendet sonst
      // nur connectAsHost, das hier nie wieder erreicht wird. Ohne das warten
      // beide fuer immer aufeinander (Gast: "Waiting for host", Host: Banner).
      if (_isHost && millis() - lastRejoin >= 400) {
        lastRejoin = millis();
        uint8_t hs[2] = { 0xC0, selectedGame };
        sendFrame(LINK_T_DATA, hs, 2);
      }
      if (millis() - lastDraw >= 250)     { lastDraw = millis(); drawReconnecting(); }
      // Phase 2 wird VOR dem Neuverbinden festgehalten: bleibt das Board hier
      // stehen, zeigt die Blackbox nach dem Neustart "IN-RECONNECT" - dann
      // haengt der USB-Neuaufbau selbst und nicht das Spiel.
      if (millis() - lastTry >= period) {
        lastTry = millis(); reattachCnt++;
        linkPhase = 2; markPos(POS_REATT); blackbox();
        reattach();
        linkPhase = 1; markPos(POS_STALL); blackbox();
      }
      // Notausstieg: A und B zusammen ~1,5 s halten -> Neustart ins Menue.
      // Damit haengt niemand fest, wenn die Leitung gar nicht mehr zurueckkommt.
      if (digitalRead(KEY_A) == LOW && digitalRead(KEY_B) == LOW) {
        if (escSince == 0) escSince = millis();
        else if (millis() - escSince > 1500) rp2040.reboot();
      } else escSince = 0;
      // Letzte Rettung: Nach 12 s ohne Erfolg neu starten. Beide Seiten laufen
      // dann durch den normalen Verbindungsaufbau und finden wieder zusammen;
      // der Spielstand bleibt erhalten (siehe autoResume/PERSIST).
      // Erst nach 30 s aufgeben. Bis dahin hat der reine USB-Neustart mehrere
      // Versuche gehabt (sanft, dann Hardware-Reset des USB-Blocks) - der laesst
      // das Spiel unangetastet weiterlaufen. Ein Geraeteneustart ist die
      // allerletzte Rueckfallebene.
      if (millis() - stallBegan > 30000) rp2040.reboot();
    }
    // Nach dem Reconnect ALLE veralteten Empfangs-/Eingabezustaende verwerfen,
    // damit keine Teilframes oder alten Eingaben von vor dem Ausfall nachwirken.
    _st = 0; _pcnt = 0; _rxlen = _rxpos = 0; _haveInput = false; recvNew = false;
    linkPhase = 0;                      // wieder im Spiel
  }

  // "Reconnecting..."-Banner MIT Diagnose ueber das eingefrorene Bild legen.
  // Die Zahlen sagen, WO es klemmt:
  //   why TIMEOUT = hier kam nichts mehr an | PEER = Gegenstelle meldete Ausfall
  //   mnt         = USB-Verbindung besteht (1) oder ist weg (0)
  //   +B          = seit der Stoerung neu empfangene Bytes  (0 = Funkstille)
  //   +F          = davon gueltige Frames                   (0 bei +B>0 = Datenmuell)
  //   +C          = Pruefsummenfehler seit der Stoerung     (>0 = verfaelschte Bytes)
  //   txd         = verworfene Sendeframes (gesamt)
  //   try         = Neuverbindungs-Versuche
  void drawReconnecting() {
    const int bw = 224, bh = 118;
    const int bx = (DISPLAY_WIDTH - bw) / 2, by = (DISPLAY_HEIGHT - bh) / 2;
    canvas.fillRect(bx, by, bw, bh, COL_BG);
    canvas.drawRect(bx, by, bw, bh, 0xFFE0);
    canvas.drawRect(bx + 1, by + 1, bw - 2, bh - 2, 0xFFE0);
    canvas.setFont(&FreeSansBold12pt7b);
    canvas.setTextColor(0xFFE0);
    int16_t x0, y0; uint16_t w, h;
    canvas.getTextBounds("Reconnecting...", 0, 0, &x0, &y0, &w, &h);
    canvas.setCursor(bx + (bw - (int)w) / 2 - x0, by + 24);
    canvas.print("Reconnecting...");

    bool mnt = _isHost ? SerialHost.mounted() : (tud_mounted() != 0);
    char ln[40];
    canvas.setFont(NULL);                    // kleine 6x8-Schrift fuer die Details
    canvas.setTextSize(1);
    canvas.setTextColor(0xFFFF);
    int ty = by + 38;
    snprintf(ln, sizeof(ln), "%s  why:%s  mnt:%d",
             _isHost ? "HOST" : "JOIN",
             (stallWhy == 2) ? "PEER" : "TIMEOUT", mnt ? 1 : 0);
    canvas.setCursor(bx + 8, ty); canvas.print(ln); ty += 11;
    snprintf(ln, sizeof(ln), "+B:%lu  +F:%lu  +C:%lu",
             (unsigned long)(bytesIn - _sBytes), (unsigned long)(framesIn - _sFrames),
             (unsigned long)(crcErr - _sCrc));
    canvas.setCursor(bx + 8, ty); canvas.print(ln); ty += 11;
    snprintf(ln, sizeof(ln), "txd:%lu  try:%u  age:%lus",
             (unsigned long)txDrop, (unsigned)reattachCnt,
             (unsigned long)((millis() - _lastRx) / 1000));
    canvas.setCursor(bx + 8, ty); canvas.print(ln); ty += 11;
    snprintf(ln, sizeof(ln), "tot B:%lu F:%lu C:%lu",
             (unsigned long)bytesIn, (unsigned long)framesIn, (unsigned long)crcErr);
    canvas.setCursor(bx + 8, ty); canvas.print(ln); ty += 13;
    canvas.setTextColor(0xC618);
    canvas.setCursor(bx + 8, ty); canvas.print("A+B lang = zurueck ins Menue");
    pushCanvas();
    canvas.setFont(&FreeSans9pt7b);   // kleine Systemschrift nicht aktiv lassen
  }

  static uint8_t crc8(uint8_t type, uint8_t len, const uint8_t* d, uint16_t n) {
    uint8_t c = 0; uint8_t h[2] = { type, len };
    for (uint8_t i = 0; i < 2; i++) { c ^= h[i];
      for (uint8_t b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1); }
    for (uint16_t i = 0; i < n; i++) { c ^= d[i];
      for (uint8_t b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1); }
    return c;
  }

  void sendFrame(uint8_t type, const uint8_t* d, uint16_t n) {
    if (n > LINK_MAXPL) n = LINK_MAXPL;
    uint8_t f[4 + LINK_MAXPL + 1];
    f[0] = LINK_SYNC0; f[1] = LINK_SYNC1; f[2] = type; f[3] = (uint8_t)n;
    for (uint16_t i = 0; i < n; i++) f[4 + i] = d[i];
    f[4 + n] = crc8(type, (uint8_t)n, d, n);
    streamWrite(f, (uint16_t)(4 + n + 1));
  }

  void streamWrite(const uint8_t* d, uint16_t n) {
    markPos(POS_TX);
    if (_isHost) {
      // WICHTIG: SerialHost.write() blockiert intern, BIS die TX-FIFO Platz
      // fuer alle Bytes hat (while remain && mounted). Stockt der Link (ein
      // Transfer-Stall/Backpressure auf dieser nicht-USB-konformen Verbindung
      // kommt gelegentlich vor), haengt sonst der GANZE Host-Loop - und da der
      // Host die komplette Spiellogik rechnet, frieren BEIDE Spieler ein.
      // Deshalb NIE unbegrenzt blockieren: kurz (max ~5 ms) auf Platz warten,
      // sonst den Frame verwerfen. Der Spielzustand wird jeden Tick neu
      // gesendet (idempotent), ein verworfener Frame wird gleich ersetzt.
      if (!SerialHost.mounted()) { txDrop++; return; }
      unsigned long t = millis();
      while (SerialHost.mounted() &&
             (uint16_t)SerialHost.availableForWrite() < n &&
             (millis() - t) < 5) {
        USBHost.task(0);
      }
      if (SerialHost.mounted() && (uint16_t)SerialHost.availableForWrite() >= n) {
        SerialHost.write(d, n);   // passt komplett -> blockiert nicht
        SerialHost.flush();
      } else txDrop++;            // kein Platz in der Zeit -> Frame verworfen
    } else {
      // WICHTIG: NICHT Serial.write() nehmen - Adafruit_USBD_CDC::write()
      // sendet nur solange tud_cdc_connected() (DTR vom Host). Wir haben die
      // Line-Control-Transfers bei der Enumeration abgeschaltet, also ist DTR
      // nie gesetzt und Serial.write() wuerde ALLE Eingabe-Frames verwerfen.
      // Direktes tud_cdc_write() umgeht das DTR-Gate; die IN-Uebertragung zum
      // Host laeuft unabhaengig von DTR.
      // Nur senden, wenn der GANZE Frame in die TX-FIFO passt - sonst wuerde
      // tud_cdc_write() nur einen Teil schreiben und den Byte-Strom zerstoeren.
      // Ein verworfener Eingabe-Frame ist unkritisch (naechster folgt in ~3 ms).
      if (tud_cdc_write_available() >= n) {
        tud_cdc_write(d, n);
        tud_cdc_write_flush();
      } else txDrop++;            // TX-FIFO voll -> Frame verworfen
    }
  }

  // ---- Frame-Parser: Byte-Strom -> [SYNC0][SYNC1][type][len][payload][crc8] ----
  uint8_t _st = 0, _ptype = 0, _plen = 0, _pcnt = 0;
  uint8_t _pbuf[LINK_MAXPL];
  unsigned long _lastByte = 0;
  void parseByte(uint8_t b) {
    bytesIn++;
    // Resync-Schutz: ein Frame kommt am Stueck (<1 ms). Klafft mitten im Frame
    // eine groessere Luecke, ging ein Byte verloren -> Parser verwirft den Rest
    // sofort, statt bis zu 250 Folgebytes als "Payload" zu fressen.
    unsigned long now = millis();
    if (_st != 0 && (now - _lastByte) > 50) _st = 0;
    _lastByte = now;
    switch (_st) {
      case 0: _st = (b == LINK_SYNC0) ? 1 : 0; break;
      case 1: _st = (b == LINK_SYNC1) ? 2 : ((b == LINK_SYNC0) ? 1 : 0); break;
      case 2: _ptype = b; _st = 3; break;
      case 3: _plen = b; _pcnt = 0; _st = (b == 0) ? 5 : 4; break;
      case 4: _pbuf[_pcnt++] = b; if (_pcnt >= _plen) _st = 5; break;
      case 5: if (crc8(_ptype, _plen, _pbuf, _plen) == b) deliver(_ptype, _pbuf, _plen);
              else crcErr++;                 // Daten kamen an, waren aber verfaelscht
              _st = 0; break;
    }
  }

  void deliver(uint8_t type, const uint8_t* d, uint8_t n) {
    framesIn++;
    if (type == LINK_T_RHELLO) {
      // Die Gegenstelle empfaengt nichts mehr. Das ist KEIN Lebenszeichen
      // (_lastRx bleibt alt!), sondern der Anlass, selbst in die Recovery zu
      // gehen. Rueckweg per RACK bestaetigen (verzoegert in pumpUSB gesendet,
      // damit hier keine Rekursion ueber streamWrite/USBHost.task entsteht).
      _peerRecovering = true;
      _ackDue = true;
      return;
    }
    // Ab hier: echtes Lebenszeichen (DATA/INPUT/PING/RACK).
    _lastRx = millis();
    _peerRecovering = false;     // Gegenstelle ist wieder da
    if (type == LINK_T_RACK) return;   // reine Rueckweg-Bestaetigung
    if (_isHost) {
      if (type == LINK_T_INPUT) {              // Gast-Eingabe zwischenspeichern
        _lastInput[0] = (n > 0) ? d[0] : 0;
        _lastInput[1] = (n > 1) ? d[1] : 0;
        _haveInput = true;
      }
    } else {
      if (type == LINK_T_DATA) {               // Datenpaket -> Link.onReceive nachbilden
        for (uint8_t i = 0; i < n; i++) _rx[i] = d[i];
        _rxlen = n; _rxpos = 0;
        if (_onRecv) _onRecv((int)n);
      }
    }
  }
};

LinkClass Link;

// Blockierendes Warten, das die USB-Verbindung WEITER bedient (statt sie wie
// delay() 60-240 ms lang komplett stillzulegen). MITTEN im Spiel fuer kurze
// Ton-/Effektpausen verwenden - sonst bleibt der Link in dieser Zeit haengen
// (genau das war die Ursache fuer Abbrueche bei Rochade/Schachmatt).
void linkDelay(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) Link.service();
}

// Einmalige Steuer-Ereignisse (Runden-/Match-Neustart) MEHRFACH senden. Geht ein
// einzelnes Frame auf der marginalen Leitung verloren, wuerde sonst nur der Host
// neu starten und der Gast nicht -> dauerhafter Desync, der sich NICHT selbst
// heilt (der Zustand ist idempotent, dieses Ereignis aber nicht). 5x ueber ~200 ms
// verteilt; der Gast reagiert auf das erste Frame, das ankommt, weitere sind
// unschaedlich (initMatch/initRound sind idempotent). Ersetzt das fruehere
// Einmal-Senden + linkDelay(200).
// Aktuellen Eingabe-Stand des Gasts FRISCH vom Link holen.
// Wichtig beim Fortsetzen nach einem Neustart: Die Zaehler des Gasts laufen
// weiter, waehrend der Host bei 0 beginnt. Ohne Abgleich wertet der Host den
// ersten Vergleich als Tastendruck - dann erscheint z.B. eine Bombe oder ein
// Schuss voellig ohne Zutun des Spielers.
struct GuestIn { uint8_t dir, cnt; };
GuestIn readGuestNow() {
  GuestIn g = { 0xFF, 0 };
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  if (Link.available() >= 1) {
    g.dir = (uint8_t)Link.read();
    if (Link.available()) g.cnt = (uint8_t)Link.read();
  }
  return g;
}

void sendRestart(uint8_t code) {
  for (int i = 0; i < 5; i++) {
    Link.beginTransmission(I2C_ADDR); Link.write(code); Link.endTransmission();
    linkDelay(40);
  }
}

// TinyUSB-Host-Callback: wird beim Mounten eines CDC-Geraets aufgerufen.
extern "C" void tuh_cdc_mount_cb(uint8_t idx)   { SerialHost.mount(idx); }
extern "C" void tuh_cdc_umount_cb(uint8_t idx)  { SerialHost.umount(idx); }

void onWireReceive(int n) {
  uint16_t i = 0;
  while (Link.available() && i < sizeof(recvBuf)) recvBuf[i++] = Link.read();
  recvLen = i; recvNew = true;
}
void onWireRequest() {
  Link.write((uint8_t)guestInputByte);
  Link.write((uint8_t)guestFireCnt);
}
// Bild zum Display schieben - mit Positionsmarker, damit ein Haenger in der
// SPI-Uebertragung nach einem Neustart erkennbar ist.
void pushCanvas() {
  // Absturz-Kurzhinweis dauerhaft einblenden (z.B. "!2 SENDEN"): Zahl = wievielter
  // Absturz, danach die Stelle. Bleibt sichtbar, bis das Board stromlos war -
  // so ist die Ursache auch ablesbar, wenn man beim Absturz nicht dabei war.
  if (crashTag[0]) {
    int w = (int)strlen(crashTag) * 6;
    canvas.setFont(NULL); canvas.setTextSize(1); canvas.setTextColor(0xFD20);
    canvas.setCursor((DISPLAY_WIDTH - w) / 2, DISPLAY_HEIGHT - 9);
    canvas.print(crashTag);
  }
  markPos(POS_DISPLAY);
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
  markPos(POS_GAME);
}

// ---- Eigener Tongenerator (ersetzt tone()) ------------------------------
// Der tone() des Cores nutzt PIO + Timer-Alarme und enthaelt mit
// pio_sm_put_blocking() einen BLOCKIERENDEN Aufruf. Zusaetzlich schaltet sein
// Alarm-Callback (_stopTonePIO) die PIO-Statemachine im INTERRUPT ab - faellt
// das zwischen die Pruefung und das Beschreiben der FIFO, kann tone() dort
// haengen bleiben und legt das ganze Board still (Bild eingefroren, USB tot).
// Genau dieses Bild zeigte die Blackbox: Gast haengt "IM SPIEL" bei intakter
// Verbindung. Bombing Bob piept am haeufigsten (jede Explosion) -> faellt dort
// zuerst auf. Diese Version nutzt direkt die PWM-Hardware: kein Blockieren,
// keine Alarme, kein Interrupt - das Ausschalten erledigt beepService().
volatile unsigned long beepUntil = 0;
volatile bool beepOn = false;

void beepStop() {
  if (!beepOn) return;
  beepOn = false;
  pwm_set_enabled(pwm_gpio_to_slice_num(SPEAKER), false);
  gpio_set_function(SPEAKER, GPIO_FUNC_SIO);
  gpio_put(SPEAKER, 0);
}
void beep(int f, int d) {
  if (f <= 0 || d <= 0) { beepStop(); return; }
  uint32_t fcpu = clock_get_hz(clk_sys);
  float div = (float)fcpu / ((float)f * 4096.0f);
  if (div < 1.0f) div = 1.0f;
  if (div > 255.0f) div = 255.0f;
  uint32_t wrap = (uint32_t)((float)fcpu / (div * (float)f));
  if (wrap < 2) wrap = 2;
  if (wrap > 65535) wrap = 65535;
  uint slice = pwm_gpio_to_slice_num(SPEAKER);
  gpio_set_function(SPEAKER, GPIO_FUNC_PWM);
  pwm_set_clkdiv(slice, div);
  pwm_set_wrap(slice, (uint16_t)(wrap - 1));
  pwm_set_chan_level(slice, pwm_gpio_to_channel(SPEAKER), (uint16_t)(wrap / 2));
  pwm_set_enabled(slice, true);
  beepOn = true;
  beepUntil = millis() + (unsigned long)d;
}
// Muss zyklisch laufen (steckt in Link.service()) - schaltet den Ton ab.
void beepService() {
  if (beepOn && (long)(millis() - beepUntil) >= 0) beepStop();
}
bool aOrB() { return digitalRead(KEY_A) == LOW || digitalRead(KEY_B) == LOW; }

void drawCenteredText(const char* t, int bx, int bw, int by, uint16_t c) {
  int16_t x0, y0; uint16_t w, h;
  canvas.getTextBounds(t, 0, by, &x0, &y0, &w, &h);
  canvas.setTextColor(c);
  canvas.setCursor(bx + (bw - (int)w) / 2 - x0, by);
  canvas.print(t);
}
void drawInfo(const char* a, const char* b) {
  canvas.fillScreen(COL_BG);
  canvas.drawRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COL_FRAME);
  canvas.setFont(&FreeSansBold12pt7b);
  drawCenteredText(a, 0, DISPLAY_WIDTH, 135, COL_TEXT);
  if (b) { canvas.setFont(&FreeSans9pt7b); drawCenteredText(b, 0, DISPLAY_WIDTH, 165, COL_DIM); }
  pushCanvas();
}
void flushPress() { while (aOrB()) linkDelay(10); linkDelay(80); }

// === Restart-Wunsch des Gasts pollen ===
// Im End-Zustand eines Spiels prueft der Host periodisch, ob der Gast
// guestFireCnt erhoeht hat (= Gast hat A oder B gedrueckt). Damit kann
// der Restart von beiden Seiten ausgeloest werden.
//
// Nutzung im Host: vor Eintritt in den End-Zustand snapshotGuestRestart()
// einmal aufrufen, dann pollGuestRestart() alle paar Ticks fragen.
uint8_t restartFireSnap = 0;
void snapshotGuestRestart() {
  // Zwei-Byte-Antwort lesen: [inputByte, fireCnt]
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  if (Link.available() >= 1) {
    Link.read();    // inputByte verwerfen
    if (Link.available()) restartFireSnap = Link.read();
    else                  restartFireSnap = 0;
  }
}
bool pollGuestRestart() {
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t cnt = restartFireSnap;
  if (Link.available() >= 1) {
    Link.read();
    if (Link.available()) cnt = Link.read();
  }
  if (cnt != restartFireSnap) {
    restartFireSnap = cnt;
    return true;
  }
  return false;
}

// === Restart-Wunsch des Gasts senden ===
// Auf Gast-Seite im End-Zustand A/B mit Edge-Erkennung -> guestFireCnt++.
// Helper haelt den vorherigen A/B-Status.
bool restartABprev = false;
void resetGuestRestartEdge() { restartABprev = aOrB(); }
bool guestPressedRestart() {
  bool now = aOrB();
  bool edge = (now && !restartABprev);
  restartABprev = now;
  return edge;
}

// Blackbox nach dem Booten auswerten: Warum lief das Board zuletzt neu an?
void readCrashInfo() {
  if (BBOX_MAGIC != BB_MAGIC) return;          // frisch eingeschaltet -> nichts zu zeigen
  uint32_t f = BBOX_FLAGS, t = BBOX_TIME;
  bool host = f & 1, mnt = f & 2, stall = f & 4, peer = f & 8;
  uint8_t g = (f >> 4) & 0x0F, tries = (f >> 8) & 0xFF;
  uint8_t txd = (f >> 16) & 0xFF, irq = (f >> 24) & 0x3F, phase = (f >> 30) & 3;
  uint16_t up = t & 0xFFFF;
  uint8_t age = (t >> 16) & 0xFF;              // in 0,1-s-Schritten
  crashCount = (uint8_t)(((t >> 24) & 0xFF) + 1);   // Zaehler fortschreiben
  resumeS1 = (BBOX_FRAMES >> 24) & 0x0F;
  resumeS2 = (BBOX_FRAMES >> 28) & 0x0F;
  const char* gn = (g == GAME_TRON) ? "LIGHTCYC" : (g == GAME_PONG) ? "TENNIS" :
                   (g == GAME_DUEL) ? "DUEL"     : (g == GAME_ARTY) ? "TANKS"  :
                   (g == GAME_BOMBER) ? "BOMBOB" : (g == GAME_CHESS) ? "CHESS" :
                   (g == GAME_WNR) ? "WNR"       : (g == GAME_C4) ? "QUADLNK" : "DOTS";
  const char* ph = (phase == 2) ? "IN-RECONNECT" : (phase == 1) ? "BANNER" : "IM SPIEL";
  snprintf(crashInfo[0], sizeof(crashInfo[0]), "%s  %s  %s",
           watchdog_caused_reboot() ? "WATCHDOG" : "RESET",
           host ? "HOST" : "JOIN", gn);
  snprintf(crashInfo[1], sizeof(crashInfo[1]), "%s  mnt%d  %s  up%us",
           ph, mnt ? 1 : 0, stall ? (peer ? "stPEER" : "stTMO") : "run", up);
  uint8_t pos = (BBOX_FRAMES >> 16) & 0xFF;
  const char* pn = (pos == POS_GAME) ? "SPIEL/ZEICHNEN" : (pos == POS_USBTASK) ? "USB-STACK" :
                   (pos == POS_RX) ? "EMPFANG"   : (pos == POS_TX) ? "SENDEN" :
                   (pos == POS_BEEP) ? "TON"     : (pos == POS_DISPLAY) ? "DISPLAY-SPI" :
                   (pos == POS_STALL) ? "BANNER" : (pos == POS_REATT) ? "NEUVERBINDEN" : "?";
  snprintf(crashInfo[2], sizeof(crashInfo[2]), "bei:%s F%lu age%u.%u",
           pn, (unsigned long)(BBOX_FRAMES & 0xFFFFu), age / 10, age % 10);
  snprintf(crashInfo[3], sizeof(crashInfo[3]), "Nr%u try%u txd%u irq%u",
           crashCount, tries, txd, irq);
  // Kurzhinweis, der WAEHREND des Spiels dauerhaft eingeblendet bleibt - damit
  // die Ursache auch dann noch ablesbar ist, wenn man beim Absturz nicht
  // danebensteht und die Startanzeige laengst weg ist.
  snprintf(crashTag, sizeof(crashTag), "!%u %s i%u", crashCount, pn, irq);
  haveCrashInfo = true;
  // Automatisch fortsetzen: Hat der Watchdog waehrend eines laufenden Spiels
  // zugeschlagen, wieder DASSELBE Spiel in DERSELBEN Rolle starten, statt den
  // Spieler ins Menue zu werfen. Beide Boards tun das unabhaengig voneinander,
  // sind also nach wenigen Sekunden von allein wieder verbunden.
  if (watchdog_caused_reboot() && g != 0) {
    selectedGame = g;
    myRole = host ? ROLE_HOST : ROLE_GUEST;
    autoResume = true;
  }
  BBOX_MAGIC = 0;                              // nur einmal melden
}

void selectGame() {
  int sel = 0;
  bool pU = false, pD = false, pAB = false, redraw = true;
  while (true) {
    Link.service();
    bool U = digitalRead(KEY_UP) == LOW, D = digitalRead(KEY_DOWN) == LOW;
    bool AB = aOrB();
    if (U && !pU && sel > 0) { sel--; redraw = true; beep(500, 20); }
    if (D && !pD && sel < 8) { sel++; redraw = true; beep(500, 20); }
    if (AB && !pAB) {
      const uint8_t m[9] = { GAME_TRON, GAME_PONG, GAME_DUEL, GAME_ARTY, GAME_BOMBER, GAME_CHESS, GAME_WNR, GAME_C4, GAME_DOTS };
      selectedGame = m[sel]; beep(900, 60); flushPress(); return;
    }
    pU = U; pD = D; pAB = AB;
    if (redraw) {
      canvas.fillScreen(COL_BG);
      canvas.drawRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COL_FRAME);
      canvas.setFont(&FreeSansBold12pt7b);
      drawCenteredText("USBCollection", 0, DISPLAY_WIDTH, 32, COL_TEXT);
      canvas.drawLine(40, 42, DISPLAY_WIDTH - 40, 42, COL_FRAME);
      canvas.setFont(&FreeSansBold9pt7b);
      const char* labels[9] = { "LIGHTCYCLE", "TENNIS42", "VECTOR DUEL", "POCKET TANKS", "BOMBING BOB", "CHESS2000", "W'N'R", "QUADLINK", "DOTS'N'BOXES" };
      for (int i = 0; i < 9; i++) {
        drawCenteredText(labels[i], 0, DISPLAY_WIDTH, 70 + i * 22,
                         sel == i ? COL_P1 : COL_DIM);
      }
      // Blackbox: was war beim letzten Neustart los? (nach Power-On leer)
      // MITTIG und mit Abstand zum Rand - das Display hat runde Ecken.
      if (haveCrashInfo) {
        canvas.setFont(NULL); canvas.setTextSize(1);
        for (int i = 0; i < 4; i++) {
          int w = (int)strlen(crashInfo[i]) * 6;          // 6 px pro Zeichen
          canvas.setTextColor(i == 0 ? 0xFD20 : 0xC618);
          canvas.setCursor((DISPLAY_WIDTH - w) / 2, DISPLAY_HEIGHT - 50 + i * 11);
          canvas.print(crashInfo[i]);
        }
        canvas.setFont(&FreeSansBold9pt7b);
      }
      pushCanvas();
      redraw = false;
    }
    delay(30);
  }
}
void selectRole() {
  int sel = 0;
  bool pU = false, pD = false, pAB = false, redraw = true;
  while (true) {
    Link.service();
    bool U = digitalRead(KEY_UP) == LOW, D = digitalRead(KEY_DOWN) == LOW;
    bool AB = aOrB();
    if (U && !pU && sel != 0) { sel = 0; redraw = true; beep(500, 20); }
    if (D && !pD && sel != 1) { sel = 1; redraw = true; beep(500, 20); }
    if (AB && !pAB) {
      myRole = (sel == 0) ? ROLE_HOST : ROLE_GUEST;
      beep(900, 60); flushPress(); return;
    }
    pU = U; pD = D; pAB = AB;
    if (redraw) {
      canvas.fillScreen(COL_BG);
      canvas.drawRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COL_FRAME);
      canvas.setFont(&FreeSansBold12pt7b);
      const char* t = (selectedGame == GAME_TRON) ? "Lightcycle"
                    : (selectedGame == GAME_PONG) ? "Tennis42"
                    : (selectedGame == GAME_DUEL) ? "Vector Duel"
                    : (selectedGame == GAME_ARTY) ? "Pocket Tanks"
                    : (selectedGame == GAME_BOMBER)  ? "Bombing Bob"
                    : (selectedGame == GAME_CHESS) ? "CHESS2000"
                    : (selectedGame == GAME_WNR)  ? "W'n'R"
                    : (selectedGame == GAME_C4)   ? "Quadlink" : "Dots'n'Boxes";
      drawCenteredText(t, 0, DISPLAY_WIDTH, 80, COL_P1);
      canvas.drawLine(40, 105, DISPLAY_WIDTH - 40, 105, COL_FRAME);
      drawCenteredText("HOST", 0, DISPLAY_WIDTH, 165, sel == 0 ? COL_P1 : COL_DIM);
      drawCenteredText("JOIN", 0, DISPLAY_WIDTH, 215, sel == 1 ? COL_P2 : COL_DIM);
      pushCanvas();
      redraw = false;
    }
    delay(30);
  }
}
// Bei Spielwahl-Konflikt: Nachricht zeigen, auf Taste warten, dann
// den Pico neu starten - dadurch landet der Spieler wieder im Startmenue
// und I2C wird komplett zurueckgesetzt.
void mismatchAndReboot() {
  canvas.fillScreen(COL_BG);
  canvas.drawRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COL_FRAME);
  canvas.setFont(&FreeSansBold12pt7b);
  drawCenteredText("Game choice",  0, DISPLAY_WIDTH, 120, COL_TEXT);
  drawCenteredText("differs!",     0, DISPLAY_WIDTH, 150, COL_P2);
  canvas.setFont(&FreeSans9pt7b);
  drawCenteredText("Press a button", 0, DISPLAY_WIDTH, 195, COL_DIM);
  drawCenteredText("to restart",     0, DISPLAY_WIDTH, 215, COL_DIM);
  pushCanvas();
  beep(200, 200); delay(220); beep(150, 300);
  // Auf Tastendruck warten
  while (!aOrB() &&
         digitalRead(KEY_UP)     != LOW &&
         digitalRead(KEY_DOWN)   != LOW &&
         digitalRead(KEY_LEFT)   != LOW &&
         digitalRead(KEY_RIGHT)  != LOW &&
         digitalRead(KEY_CENTER) != LOW) {
    delay(50);
  }
  delay(150);
  rp2040.reboot();
}

void connectAsHost() {
  Link.begin();
  uint8_t guestGame = 0;
  drawInfo("Hosting...", "Waiting for guest");
  // Auf einen ECHTEN Joiner warten: erst wenn ein gueltiges Spielwahl-Byte
  // vom Gast kommt (das sendet nur die JOIN-Spiellogik). Noch KEIN 0xC0
  // senden, sonst wechselt der Gast sofort ins Spiel und guestInputByte ist
  // dann die Bewegungsrichtung (Race).
  while (guestGame == 0) {
    Link.service();
    if (SerialHost.mounted()) {
      Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
      if (Link.available() >= 1) {
        uint8_t g = Link.read();
        if (Link.available()) Link.read();
        if (g != 0) guestGame = g;      // echter Joiner hat geantwortet
      }
    }
    delay(5);
  }
  // Phase 2: Gast ins Spiel entlassen (0xC0 + eigene Spielwahl). Mehrfach
  // senden, damit es sicher ankommt, waehrend der Gast wechselt.
  for (int i = 0; i < 5; i++) {
    Link.beginTransmission(I2C_ADDR);
    Link.write(0xC0); Link.write(selectedGame);
    Link.endTransmission();
    Link.service();
    delay(30);
  }
  // Der Gast erkennt einen Mismatch selbst am 0xC0-Byte und rebootet dann.
  if (guestGame != selectedGame) {
    mismatchAndReboot();
  }
  beep(1200, 70); linkDelay(90); beep(1600, 90);
}
void connectAsGuest() {
  // VOR der Kommunikation unsere Spielwahl als Antwort hinterlegen,
  // damit der Host sie lesen kann (siehe connectAsHost).
  guestInputByte = selectedGame;
  Link.begin(I2C_ADDR);
  Link.onReceive(onWireReceive);
  Link.onRequest(onWireRequest);
  drawInfo("Ready", "Waiting for host");
  while (true) {
    Link.service();
    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l >= 2 && b[0] == 0xC0) {
        if (b[1] != selectedGame) {
          mismatchAndReboot();
        }
        break;
      }
    }
    delay(5);
  }
  beep(1200, 70); linkDelay(90); beep(1600, 90);
}

// ========================= TRON =========================
namespace Tron {
// Spielfeld: 230x230 zentriert, da das Display abgerundete Ecken hat.
// Gitter 57x57 = 228x228 px; Rahmen ist 230x230 = ein Pixel um den Spielraum.
#define TR_CELL 4
#define TR_GW 57
#define TR_GH 57
#define TR_PLAY_W (TR_GW * TR_CELL)               // 228
#define TR_PLAY_H (TR_GH * TR_CELL)               // 228
#define TR_OFF_X ((DISPLAY_WIDTH  - TR_PLAY_W) / 2)   //  6
#define TR_OFF_Y ((DISPLAY_HEIGHT - TR_PLAY_H) / 2)   // 26
#define TR_FRAME_X (TR_OFF_X - 1)                 //  5
#define TR_FRAME_Y (TR_OFF_Y - 1)                 // 25
#define TR_FRAME_W (TR_PLAY_W + 2)                // 230
#define TR_FRAME_H (TR_PLAY_H + 2)                // 230
#define TR_UP 0
#define TR_RT 1
#define TR_DN 2
#define TR_LF 3
#define TR_STATE 0xA5
#define TR_RESTART 0xFE

PERSIST uint8_t grid[TR_GW][TR_GH];      // PERSIST: ueberlebt einen Absturz
PERSIST int p1x, p1y, p2x, p2y;
PERSIST uint8_t p1d, p2d, status, s1, s2;

void initRound() {
  for (int x = 0; x < TR_GW; x++) for (int y = 0; y < TR_GH; y++) grid[x][y] = 0;
  p1x = 8; p1y = TR_GH / 2; p1d = TR_RT;
  p2x = TR_GW - 9; p2y = TR_GH / 2; p2d = TR_LF;
  grid[p1x][p1y] = 1; grid[p2x][p2y] = 2;
  status = 0; guestInputByte = TR_LF;
}
uint8_t readDir(uint8_t c) {
  if (digitalRead(KEY_UP) == LOW && c != TR_DN) return TR_UP;
  if (digitalRead(KEY_DOWN) == LOW && c != TR_UP) return TR_DN;
  if (digitalRead(KEY_LEFT) == LOW && c != TR_RT) return TR_LF;
  if (digitalRead(KEY_RIGHT) == LOW && c != TR_LF) return TR_RT;
  return c;
}
void fillLine(int x0, int y0, int x1, int y1, uint8_t v) {
  if (x1 < 0 || x1 >= TR_GW || y1 < 0 || y1 >= TR_GH) return;
  if (x0 == x1) {
    int f = y0; if (f < 0) f = 0; if (f >= TR_GH) f = TR_GH - 1;
    int s = (y1 >= f) ? 1 : -1;
    for (int y = f;; y += s) { grid[x0][y] = v; if (y == y1) break; }
  } else if (y0 == y1) {
    int f = x0; if (f < 0) f = 0; if (f >= TR_GW) f = TR_GW - 1;
    int s = (x1 >= f) ? 1 : -1;
    for (int x = f;; x += s) { grid[x][y0] = v; if (x == x1) break; }
  } else grid[x1][y1] = v;
}
void draw() {
  canvas.fillScreen(COL_BG);
  // Rahmen direkt um das Spielfeld - zeigt klar an, wo das Feld endet.
  canvas.drawRect(TR_FRAME_X, TR_FRAME_Y, TR_FRAME_W, TR_FRAME_H, COL_FRAME);
  for (int x = 0; x < TR_GW; x++) for (int y = 0; y < TR_GH; y++) {
    uint8_t v = grid[x][y];
    if (v == 1) canvas.fillRect(TR_OFF_X + x * TR_CELL, TR_OFF_Y + y * TR_CELL, TR_CELL, TR_CELL, COL_P1);
    else if (v == 2) canvas.fillRect(TR_OFF_X + x * TR_CELL, TR_OFF_Y + y * TR_CELL, TR_CELL, TR_CELL, COL_P2);
  }
  if (status == 0) {
    canvas.fillRect(TR_OFF_X + p1x * TR_CELL, TR_OFF_Y + p1y * TR_CELL, TR_CELL, TR_CELL, COL_HEAD);
    canvas.fillRect(TR_OFF_X + p2x * TR_CELL, TR_OFF_Y + p2y * TR_CELL, TR_CELL, TR_CELL, COL_HEAD);
  }
  if (status != 0) {
    int bx = 20, by = 105, bw = DISPLAY_WIDTH - 40, bh = 90;
    canvas.fillRect(bx, by, bw, bh, COL_BG);
    canvas.drawRect(bx, by, bw, bh, COL_TEXT);
    canvas.setFont(&FreeSansBold9pt7b);
    if (status == 1) drawCenteredText("P1 WINS", bx, bw, by + 25, COL_P1);
    else if (status == 2) drawCenteredText("P2 WINS", bx, bw, by + 25, COL_P2);
    else drawCenteredText("DRAW", bx, bw, by + 25, COL_TEXT);
    char sb[16]; snprintf(sb, sizeof(sb), "%u  :  %u", s1, s2);
    drawCenteredText(sb, bx, bw, by + 52, COL_TEXT);
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Press a button", bx, bw, by + 78, COL_DIM);
  }
  pushCanvas();
}
void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(TR_STATE);
  Link.write((uint8_t)p1x); Link.write((uint8_t)p1y);
  Link.write((uint8_t)p2x); Link.write((uint8_t)p2y);
  Link.write(status); Link.write(s1); Link.write(s2);
  Link.endTransmission();
}
bool opp(uint8_t a, uint8_t b) {
  return (a == TR_UP && b == TR_DN) || (a == TR_DN && b == TR_UP) ||
         (a == TR_LF && b == TR_RT) || (a == TR_RT && b == TR_LF);
}
void hostTick() {
  static uint8_t r = 0;
  if (status != 0) { if (++r >= 5) { r = 0; sendState(); } return; }
  r = 0;
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  if (Link.available() >= 1) {
    uint8_t d = Link.read();
    if (Link.available()) Link.read();
    if (d <= TR_LF && !opp(d, p2d)) p2d = d;
  }
  int n1x = p1x, n1y = p1y, n2x = p2x, n2y = p2y;
  if (p1d == TR_UP) n1y--; else if (p1d == TR_DN) n1y++;
  else if (p1d == TR_LF) n1x--; else n1x++;
  if (p2d == TR_UP) n2y--; else if (p2d == TR_DN) n2y++;
  else if (p2d == TR_LF) n2x--; else n2x++;
  bool d1 = false, d2 = false;
  if (n1x < 0 || n1x >= TR_GW || n1y < 0 || n1y >= TR_GH) d1 = true;
  else if (grid[n1x][n1y] != 0) d1 = true;
  if (n2x < 0 || n2x >= TR_GW || n2y < 0 || n2y >= TR_GH) d2 = true;
  else if (grid[n2x][n2y] != 0) d2 = true;
  if (!d1 && !d2 && n1x == n2x && n1y == n2y) { d1 = d2 = true; }
  if (!d1 && !d2 && n1x == p2x && n1y == p2y && n2x == p1x && n2y == p1y) { d1 = d2 = true; }
  if (d1 && d2) status = 3; else if (d1) status = 2; else if (d2) status = 1;
  if (status == 0) {
    p1x = n1x; p1y = n1y; p2x = n2x; p2y = n2y;
    grid[p1x][p1y] = 1; grid[p2x][p2y] = 2;
  } else {
    if (status == 1) s1++; else if (status == 2) s2++;
    beep(120, 250);
  }
  sendState();
}
// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool stateSane() {
  if (p1x < 0 || p1x >= TR_GW || p2x < 0 || p2x >= TR_GW) return false;
  if (p1y < 0 || p1y >= TR_GH || p2y < 0 || p2y >= TR_GH) return false;
  if (p1d > 3 || p2d > 3 || status > 3) return false;
  for (int x = 0; x < TR_GW; x++)
    for (int y = 0; y < TR_GH; y++) if (grid[x][y] > 2) return false;
  return true;
}

void hostMain() {
  // Nach einem Absturz exakt weiterspielen (Zustand liegt im PERSIST-Speicher).
  if (persistUsable(GAME_TRON) && stateSane()) { /* Spielfeld/Spuren/Punkte stehen noch */ }
  else { s1 = 0; s2 = 0; status = 0; initRound(); }
  persistArm(GAME_TRON);
  unsigned long lt = millis();
  unsigned long lp = 0;
  bool pAB = false;
  uint8_t prevStatus = 0;
  draw();
  while (true) {
    Link.service();
    if (status == 0) p1d = readDir(p1d);
    if (millis() - lt >= 80) { lt = millis(); hostTick(); draw(); }
    if (prevStatus == 0 && status != 0) { snapshotGuestRestart(); lp = millis(); }
    prevStatus = status;
    bool AB = aOrB();
    bool guestWants = false;
    if (status != 0 && millis() - lp >= 100) {
      lp = millis();
      guestWants = pollGuestRestart();
    }
    if (status != 0 && ((AB && !pAB) || guestWants)) {
      sendRestart(TR_RESTART); initRound(); draw(); beep(900, 60);
      pAB = false;
      continue;
    }
    pAB = AB;
  }
}
void guestMain() {
  s1 = 0; s2 = 0; status = 0;   // sonst Zufallswerte aus dem PERSIST-Speicher
  initRound();
  unsigned long ld = 0;
  draw();
  uint8_t prevStatus = 0;
  while (true) {
    Link.service();
    guestInputByte = readDir(p2d);
    // Im End-Zustand: A/B-Edge -> Restart anfragen
    if (prevStatus == 0 && status != 0) resetGuestRestartEdge();
    prevStatus = status;
    if (status != 0 && guestPressedRestart()) guestFireCnt++;
    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (b[0] == TR_RESTART) { initRound(); beep(900, 60); }
        else if (b[0] == TR_STATE && l >= 8) {
          int n1x = b[1], n1y = b[2], n2x = b[3], n2y = b[4];
          uint8_t ns = b[5]; s1 = b[6]; s2 = b[7];
          if (ns == 0) {
            if (n2x > p2x) p2d = TR_RT; else if (n2x < p2x) p2d = TR_LF;
            else if (n2y > p2y) p2d = TR_DN; else if (n2y < p2y) p2d = TR_UP;
            fillLine(p1x, p1y, n1x, n1y, 1);
            fillLine(p2x, p2y, n2x, n2y, 2);
            p1x = n1x; p1y = n1y; p2x = n2x; p2y = n2y;
          }
          uint8_t prev = status; status = ns;
          if (prev == 0 && ns != 0) beep(120, 250);
        }
      }
    }
    if (millis() - ld >= 33) { ld = millis(); draw(); }
  }
}
}

// ========================= PONG =========================
namespace Pong {
#define PG_PW 44
#define PG_PH 6
#define PG_BR 4
#define PG_TY 30
#define PG_BY (DISPLAY_HEIGHT-4)
#define PG_P1Y (PG_TY+8)
#define PG_P2Y (PG_BY-8-PG_PH)
#define PG_WIN 7
#define PG_START 4.0f
#define PG_MAX 9.0f
#define PG_DEFL 2.4f
#define PG_STATE 0xB1
#define PG_RESTART 0xBE

PERSIST float bX, bY, bVX, bVY;          // PERSIST: ueberlebt einen Absturz
PERSIST int p1X, p2X;
PERSIST uint8_t s1, s2, status, lastScorer;
unsigned long pauseUntil = 0;

void resetBall(int dir) {
  bX = DISPLAY_WIDTH / 2.0f; bY = (PG_TY + PG_BY) / 2.0f;
  float a = (random(-30, 31)) * 0.0174533f;
  bVX = sinf(a) * PG_START;
  bVY = (dir >= 0 ? 1.0f : -1.0f) * cosf(a) * PG_START;
}
void initRound(int d) {
  p1X = (DISPLAY_WIDTH - PG_PW) / 2; p2X = p1X;
  resetBall(d); status = 0;
}
void initMatch() {
  s1 = 0; s2 = 0; randomSeed(millis());
  initRound(random(0, 2) ? 1 : -1);
}
void readPad(int& x) {
  if (digitalRead(KEY_LEFT) == LOW) x -= 4;
  if (digitalRead(KEY_RIGHT) == LOW) x += 4;
  if (x < 4) x = 4;
  if (x > DISPLAY_WIDTH - 4 - PG_PW) x = DISPLAY_WIDTH - 4 - PG_PW;
}
void draw() {
  canvas.fillScreen(COL_BG);
  canvas.setFont(&FreeSansBold9pt7b);
  // Score: eigener UNTEN (eigene Seite), gegnerischer OBEN.
  uint8_t myScore = (myRole == ROLE_HOST) ? s1 : s2;
  uint8_t opScore = (myRole == ROLE_HOST) ? s2 : s1;
  uint16_t myCol = (myRole == ROLE_HOST) ? COL_P1 : COL_P2;
  uint16_t opCol = (myRole == ROLE_HOST) ? COL_P2 : COL_P1;
  char sb[16];
  // Spielstand immer oben, Host-Punktestand zuerst: "s1 - s2"
  snprintf(sb, sizeof(sb), "%u - %u", s1, s2);
  drawCenteredText(sb, 0, DISPLAY_WIDTH, 18, COL_TEXT);
  canvas.drawRect(0, PG_TY - 2, DISPLAY_WIDTH, DISPLAY_HEIGHT - PG_TY + 2, COL_FRAME);
  int my = (PG_TY + PG_BY) / 2;
  for (int x = 6; x < DISPLAY_WIDTH - 6; x += 10) canvas.drawFastHLine(x, my, 6, COL_DIM);
  // Host spiegelt das Spielfeld vertikal um die Spielfeld-Mitte, damit das
  // eigene Paddle unten erscheint. Der Gast zeichnet unveraendert.
  bool mirror = (myRole == ROLE_HOST);
  int p1Y    = mirror ? (PG_TY + PG_BY - PG_P1Y - PG_PH) : PG_P1Y;
  int p2Y    = mirror ? (PG_TY + PG_BY - PG_P2Y - PG_PH) : PG_P2Y;
  int bDispY = mirror ? (PG_TY + PG_BY - (int)bY)        : (int)bY;
  canvas.fillRect(p1X, p1Y, PG_PW, PG_PH, COL_P1);
  canvas.fillRect(p2X, p2Y, PG_PW, PG_PH, COL_P2);
  canvas.fillCircle((int)bX, bDispY, PG_BR, COL_BALL);
  if (status == 1 || status == 2) {
    int bx = 20, by = 110, bw = DISPLAY_WIDTH - 40, bh = 90;
    canvas.fillRect(bx, by, bw, bh, COL_BG);
    canvas.drawRect(bx, by, bw, bh, COL_TEXT);
    canvas.setFont(&FreeSansBold9pt7b);
    bool iWon = (status == 1 && myRole == ROLE_HOST) || (status == 2 && myRole == ROLE_GUEST);
    drawCenteredText(iWon ? "YOU WIN" : "OPPONENT WINS",
                     bx, bw, by + 25, iWon ? myCol : opCol);
    snprintf(sb, sizeof(sb), "%u  :  %u", myScore, opScore);
    drawCenteredText(sb, bx, bw, by + 52, COL_TEXT);
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Press a button", bx, bw, by + 78, COL_DIM);
  }
  pushCanvas();
}
void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(PG_STATE);
  int16_t bx_ = (int16_t)bX, by_ = (int16_t)bY;
  Link.write(p1X & 0xFF); Link.write((p1X >> 8) & 0xFF);
  Link.write(p2X & 0xFF); Link.write((p2X >> 8) & 0xFF);
  Link.write(bx_ & 0xFF); Link.write((bx_ >> 8) & 0xFF);
  Link.write(by_ & 0xFF); Link.write((by_ >> 8) & 0xFF);
  Link.write(s1); Link.write(s2);
  Link.write(status); Link.write(lastScorer);
  Link.endTransmission();
}
void hostTick() {
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gIn = 0;
  if (Link.available() >= 1) { gIn = Link.read(); if (Link.available()) Link.read(); }
  if (gIn & 0x01) p2X -= 4;
  if (gIn & 0x02) p2X += 4;
  if (p2X < 4) p2X = 4;
  if (p2X > DISPLAY_WIDTH - 4 - PG_PW) p2X = DISPLAY_WIDTH - 4 - PG_PW;
  readPad(p1X);
  if (status == 3) {
    if (millis() >= pauseUntil) {
      if (s1 >= PG_WIN) status = 1;
      else if (s2 >= PG_WIN) status = 2;
      else { status = 0; resetBall(lastScorer == 1 ? 1 : -1); }
    }
    sendState(); return;
  }
  if (status == 1 || status == 2) {
    static uint8_t r = 0; if (++r >= 4) { r = 0; sendState(); } return;
  }
  // 4 Substeps wegen hoher Anfangsgeschwindigkeit
  for (int i = 0; i < 4; i++) {
    bX += bVX / 4; bY += bVY / 4;
    if (bX < PG_BR + 1) { bX = PG_BR + 1; bVX = -bVX; beep(800, 15); }
    if (bX > DISPLAY_WIDTH - PG_BR - 1) { bX = DISPLAY_WIDTH - PG_BR - 1; bVX = -bVX; beep(800, 15); }
    if (bVY < 0 && bY - PG_BR <= PG_P1Y + PG_PH && bY - PG_BR >= PG_P1Y - 4 &&
        bX >= p1X && bX <= p1X + PG_PW) {
      bY = PG_P1Y + PG_PH + PG_BR + 1; bVY = -bVY;
      float r = ((bX - p1X) - PG_PW / 2.0f) / (PG_PW / 2.0f);
      bVX = r * PG_DEFL;
      float spd = sqrtf(bVX * bVX + bVY * bVY);
      float ns = min(spd * 1.05f, PG_MAX);
      float k = ns / spd; bVX *= k; bVY *= k;
      beep(1200, 20);
    }
    if (bVY > 0 && bY + PG_BR >= PG_P2Y && bY + PG_BR <= PG_P2Y + PG_PH + 4 &&
        bX >= p2X && bX <= p2X + PG_PW) {
      bY = PG_P2Y - PG_BR - 1; bVY = -bVY;
      float r = ((bX - p2X) - PG_PW / 2.0f) / (PG_PW / 2.0f);
      bVX = r * PG_DEFL;
      float spd = sqrtf(bVX * bVX + bVY * bVY);
      float ns = min(spd * 1.05f, PG_MAX);
      float k = ns / spd; bVX *= k; bVY *= k;
      beep(1200, 20);
    }
    if (bY < PG_TY) { s2++; lastScorer = 2; status = 3; pauseUntil = millis() + 800; beep(150, 200); break; }
    if (bY > PG_BY) { s1++; lastScorer = 1; status = 3; pauseUntil = millis() + 800; beep(150, 200); break; }
  }
  sendState();
}
// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool stateSane() {
  if (!(bX > -50 && bX < DISPLAY_WIDTH + 50)) return false;
  if (!(bY > -50 && bY < DISPLAY_HEIGHT + 50)) return false;
  if (!(bVX > -20 && bVX < 20) || !(bVY > -20 && bVY < 20)) return false;
  if (p1X < -PG_PW || p1X > DISPLAY_WIDTH || p2X < -PG_PW || p2X > DISPLAY_WIDTH) return false;
  if (status > 3 || lastScorer > 2) return false;
  return true;
}

void hostMain() {
  // Nach einem Absturz exakt weiterspielen (Zustand liegt im PERSIST-Speicher).
  if (persistUsable(GAME_PONG) && stateSane()) { pauseUntil = millis() + 500; }
  else { s1 = 0; s2 = 0; status = 0; lastScorer = 0; initMatch(); }
  persistArm(GAME_PONG);
  unsigned long lt = millis();
  unsigned long lp = 0;
  bool pAB = false;
  uint8_t prevWin = 0;
  draw();
  while (true) {
    Link.service();
    if (millis() - lt >= 25) { lt = millis(); hostTick(); draw(); }
    bool inWin = (status == 1 || status == 2);
    if (!prevWin && inWin) { snapshotGuestRestart(); lp = millis(); }
    prevWin = inWin;
    bool AB = aOrB();
    bool guestWants = false;
    if (inWin && millis() - lp >= 100) {
      lp = millis();
      guestWants = pollGuestRestart();
    }
    if (inWin && ((AB && !pAB) || guestWants)) {
      sendRestart(PG_RESTART); initMatch(); draw(); beep(900, 60);
      pAB = false;
      continue;
    }
    pAB = AB;
  }
}
void guestMain() {
  s1 = 0; s2 = 0; status = 0; lastScorer = 0;   // s.o.
  initRound(1);
  unsigned long ld = 0;
  draw();
  uint8_t prevStatus = 0;
  while (true) {
    Link.service();
    uint8_t in = 0;
    if (digitalRead(KEY_LEFT) == LOW) in |= 0x01;
    if (digitalRead(KEY_RIGHT) == LOW) in |= 0x02;
    guestInputByte = in;
    bool inWin = (status == 1 || status == 2);
    bool prevWin = (prevStatus == 1 || prevStatus == 2);
    if (!prevWin && inWin) resetGuestRestartEdge();
    prevStatus = status;
    if (inWin && guestPressedRestart()) guestFireCnt++;
    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (b[0] == PG_RESTART) { initMatch(); beep(900, 60); }
        else if (b[0] == PG_STATE && l >= 13) {
          p1X = (int16_t)(b[1] | (b[2] << 8));
          p2X = (int16_t)(b[3] | (b[4] << 8));
          bX = (int16_t)(b[5] | (b[6] << 8));
          bY = (int16_t)(b[7] | (b[8] << 8));
          s1 = b[9]; s2 = b[10];
          uint8_t prev = status;
          status = b[11]; lastScorer = b[12];
          if (prev != 3 && status == 3) beep(150, 200);
          if ((prev == 0 || prev == 3) && (status == 1 || status == 2)) beep(120, 300);
        }
      }
    }
    if (millis() - ld >= 33) { ld = millis(); draw(); }
  }
}
}
// ========================= DUELL =========================
namespace Duel {
#define DU_STATE 0xD1
#define DU_RESTART 0xDE
#define DU_WIN 5
#define DU_TY 32
#define DU_NS 4
#define DU_NST (DU_NS*2)
#define DU_TURN 0.18f
#define DU_THR 0.10f
#define DU_DRAG 0.9925f
#define DU_MAXV 4.375f
#define DU_SHV 4.5f
#define DU_SHL 55
#define DU_RES_MS 1200
#define DU_INV_MS 1500
#define DU_SH_MAX 90
#define DU_SH_REC 30
#define DU_SH_R 14
// Layout: hdr(1) + 2*Schiff(10) + Status(3) + Schuesse(8*3) = 48
// Schuss verlustfrei in 3 Byte gepackt: x(8 Bit, <240) | y(9 Bit, <280) |
// life(6 Bit, <=DU_SHL=55) | owner(1 Bit). Vorher 6 Byte -> das Paket passt
// jetzt in EIN USB-Paket (53 Byte Frame) statt in zwei.
#define DU_PKT 48

struct Ship {
  float x, y, vx, vy, ang;
  bool alive;
  unsigned long respawnAt, invulnUntil;
  uint8_t lastFireCnt, shieldCharge, shieldRegenAcc;
  bool shieldActive, shieldLocked;
};
struct Shot { float x, y, vx, vy; uint8_t life, owner; };
PERSIST Ship p1, p2;                     // PERSIST: ueberlebt einen Absturz
PERSIST Shot shots[DU_NST];
PERSIST uint8_t s1, s2, status;

void resetSh(Ship& s) {
  s.shieldCharge = DU_SH_MAX;
  s.shieldActive = false; s.shieldLocked = false; s.shieldRegenAcc = 0;
}
void respawn(Ship& s, bool isP1) {
  s.x = isP1 ? 60.0f : (DISPLAY_WIDTH - 60.0f);
  s.y = DISPLAY_HEIGHT / 2.0f;
  s.vx = s.vy = 0;
  s.ang = isP1 ? 0.0f : 3.14159f;
  s.alive = true; s.respawnAt = 0;
  s.invulnUntil = millis() + DU_INV_MS;
  resetSh(s);
}
void initMatch() {
  s1 = 0; s2 = 0; status = 0;
  for (int i = 0; i < DU_NST; i++) shots[i].life = 0;
  respawn(p1, true); respawn(p2, false);
  p1.lastFireCnt = guestFireCnt; p2.lastFireCnt = guestFireCnt;
}
void wrap(float& x, float& y) {
  if (x < 0) x += DISPLAY_WIDTH;
  if (x >= DISPLAY_WIDTH) x -= DISPLAY_WIDTH;
  if (y < DU_TY) y += (DISPLAY_HEIGHT - DU_TY);
  if (y >= DISPLAY_HEIGHT) y -= (DISPLAY_HEIGHT - DU_TY);
}
int findFree() {
  for (int i = 0; i < DU_NST; i++) if (shots[i].life == 0) return i;
  return -1;
}
void fire(Ship& s, uint8_t o) {
  int i = findFree(); if (i < 0) return;
  shots[i].x = s.x + cosf(s.ang) * 8;
  shots[i].y = s.y + sinf(s.ang) * 8;
  shots[i].vx = cosf(s.ang) * DU_SHV + s.vx * 0.5f;
  shots[i].vy = sinf(s.ang) * DU_SHV + s.vy * 0.5f;
  shots[i].life = DU_SHL; shots[i].owner = o;
  beep(1500, 15);
}
void drawShip(const Ship& s, uint16_t c) {
  if (!s.alive) return;
  if (millis() < s.invulnUntil && ((millis() / 80) & 1)) return;
  float ca = cosf(s.ang), sa = sinf(s.ang);
  float p[3][2] = { { 9, 0 }, { -6, -5 }, { -6, 5 } };
  int sx[3], sy[3];
  for (int i = 0; i < 3; i++) {
    sx[i] = (int)(s.x + p[i][0] * ca - p[i][1] * sa);
    sy[i] = (int)(s.y + p[i][0] * sa + p[i][1] * ca);
  }
  canvas.drawLine(sx[0], sy[0], sx[1], sy[1], c);
  canvas.drawLine(sx[1], sy[1], sx[2], sy[2], c);
  canvas.drawLine(sx[2], sy[2], sx[0], sy[0], c);
}
void drawShield(const Ship& s, uint16_t c) {
  if (!s.shieldActive || !s.alive) return;
  unsigned long t = millis();
  if ((t / 60) % 4 != 0) canvas.drawCircle((int)s.x, (int)s.y, 11, c);
  if ((t / 45) % 5 != 0) canvas.drawCircle((int)s.x, (int)s.y, 13, c);
}
void drawBar() {
  const Ship& m = (myRole == ROLE_HOST) ? p1 : p2;
  uint16_t c = (myRole == ROLE_HOST) ? COL_P1 : COL_P2;
  int bX = 20, bY = 4, bW = DISPLAY_WIDTH - 40, bH = 4;
  uint16_t fc = m.shieldLocked ? COL_DIM : c;
  canvas.drawRect(bX, bY, bW, bH, COL_FRAME);
  int fw = ((int)m.shieldCharge * (bW - 2)) / DU_SH_MAX;
  if (fw > 0) canvas.fillRect(bX + 1, bY + 1, fw, bH - 2, fc);
}
void draw() {
  canvas.fillScreen(COL_BG);
  drawBar();
  canvas.setFont(&FreeSansBold9pt7b);
  char sb[24]; snprintf(sb, sizeof(sb), "%u   :   %u", s1, s2);
  drawCenteredText(sb, 0, DISPLAY_WIDTH, 24, COL_TEXT);
  canvas.drawFastHLine(0, DU_TY - 2, DISPLAY_WIDTH, COL_FRAME);
  drawShip(p1, COL_P1); drawShip(p2, COL_P2);
  drawShield(p1, COL_P1); drawShield(p2, COL_P2);
  for (int i = 0; i < DU_NST; i++) {
    if (shots[i].life == 0) continue;
    uint16_t c = (shots[i].owner == 1) ? COL_SHOT_P1 : COL_SHOT_P2;
    canvas.fillRect((int)shots[i].x - 1, (int)shots[i].y - 1, 2, 2, c);
  }
  if (status == 1 || status == 2) {
    int bx = 20, by = 110, bw = DISPLAY_WIDTH - 40, bh = 90;
    canvas.fillRect(bx, by, bw, bh, COL_BG);
    canvas.drawRect(bx, by, bw, bh, COL_TEXT);
    canvas.setFont(&FreeSansBold9pt7b);
    drawCenteredText(status == 1 ? "P1 WINS" : "P2 WINS", bx, bw, by + 25, status == 1 ? COL_P1 : COL_P2);
    snprintf(sb, sizeof(sb), "%u  :  %u", s1, s2);
    drawCenteredText(sb, bx, bw, by + 52, COL_TEXT);
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Press a button", bx, bw, by + 78, COL_DIM);
  }
  pushCanvas();
}
uint8_t readMy() {
  uint8_t in = 0;
  if (digitalRead(KEY_LEFT) == LOW) in |= 0x01;
  if (digitalRead(KEY_RIGHT) == LOW) in |= 0x02;
  if (digitalRead(KEY_UP) == LOW) in |= 0x04;
  if (digitalRead(KEY_DOWN) == LOW) in |= 0x08;
  return in;
}
void applyIn(Ship& s, uint8_t in) {
  if (!s.alive) return;
  if (in & 0x01) s.ang -= DU_TURN;
  if (in & 0x02) s.ang += DU_TURN;
  if (in & 0x04) {
    s.vx += cosf(s.ang) * DU_THR;
    s.vy += sinf(s.ang) * DU_THR;
    float spd = sqrtf(s.vx * s.vx + s.vy * s.vy);
    if (spd > DU_MAXV) { float k = DU_MAXV / spd; s.vx *= k; s.vy *= k; }
  }
  bool want = (in & 0x08) != 0;
  if (s.shieldLocked && s.shieldCharge >= DU_SH_REC) s.shieldLocked = false;
  s.shieldActive = want && !s.shieldLocked && s.shieldCharge > 0;
}
void updSh(Ship& s) {
  if (!s.alive) return;
  if (s.shieldActive) {
    // Doppelt so schneller Verbrauch (vorher: -1 pro Tick)
    if (s.shieldCharge >= 2) s.shieldCharge -= 2;
    else                      s.shieldCharge = 0;
    if (s.shieldCharge == 0) { s.shieldActive = false; s.shieldLocked = true; }
  } else {
    s.shieldRegenAcc++;
    // Halb so schnelle Aufladung (vorher: +1 alle 2 Ticks)
    if (s.shieldRegenAcc >= 4) {
      s.shieldRegenAcc = 0;
      if (s.shieldCharge < DU_SH_MAX) s.shieldCharge++;
    }
  }
}
void integ(Ship& s) {
  if (!s.alive) {
    if (millis() >= s.respawnAt) respawn(s, &s == &p1);
    return;
  }
  s.vx *= DU_DRAG; s.vy *= DU_DRAG;
  s.x += s.vx; s.y += s.vy;
  wrap(s.x, s.y);
}
void kill(Ship& s) {
  s.alive = false;
  s.respawnAt = millis() + DU_RES_MS;
  beep(120, 200);
}
// Elastische Kollision zwischen den beiden Raumschiffen.
// Kein Schaden - nur Wegschubsen + Impulsaustausch entlang der Verbindungslinie.
void resolveShipCol(Ship& a, Ship& b) {
  if (!a.alive || !b.alive) return;
  float dx = b.x - a.x, dy = b.y - a.y;
  float d2 = dx * dx + dy * dy;
  const float r = 8.0f;       // Kollisionsradius pro Schiff
  const float md = 2 * r;
  if (d2 >= md * md) return;
  float d = sqrtf(d2);
  if (d < 0.001f) { dx = 1; dy = 0; d = 1; }
  float nx = dx / d, ny = dy / d;
  // Auseinanderschieben
  float ov = (md - d) * 0.5f + 0.1f;
  a.x -= nx * ov; a.y -= ny * ov;
  b.x += nx * ov; b.y += ny * ov;
  wrap(a.x, a.y);
  wrap(b.x, b.y);
  // Impulsaustausch (nur wenn sie sich aufeinander zu bewegen)
  float rv = (a.vx - b.vx) * nx + (a.vy - b.vy) * ny;
  if (rv > 0) {
    float imp = rv * 0.95f;
    a.vx -= nx * imp; a.vy -= ny * imp;
    b.vx += nx * imp; b.vy += ny * imp;
    beep(900, 30);
  }
}
void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(DU_STATE);
  int16_t v;
  v = (int16_t)p1.x; Link.write(v & 0xFF); Link.write((v >> 8) & 0xFF);
  v = (int16_t)p1.y; Link.write(v & 0xFF); Link.write((v >> 8) & 0xFF);
  v = (int16_t)(p1.ang * 1000); Link.write(v & 0xFF); Link.write((v >> 8) & 0xFF);
  Link.write(p1.alive ? 1 : 0);
  Link.write((millis() < p1.invulnUntil) ? 1 : 0);
  Link.write(p1.shieldActive ? 1 : 0);
  Link.write(p1.shieldCharge);
  v = (int16_t)p2.x; Link.write(v & 0xFF); Link.write((v >> 8) & 0xFF);
  v = (int16_t)p2.y; Link.write(v & 0xFF); Link.write((v >> 8) & 0xFF);
  v = (int16_t)(p2.ang * 1000); Link.write(v & 0xFF); Link.write((v >> 8) & 0xFF);
  Link.write(p2.alive ? 1 : 0);
  Link.write((millis() < p2.invulnUntil) ? 1 : 0);
  Link.write(p2.shieldActive ? 1 : 0);
  Link.write(p2.shieldCharge);
  Link.write(s1); Link.write(s2); Link.write(status);
  for (int i = 0; i < DU_NST; i++) {          // je Schuss 3 Byte (siehe DU_PKT)
    int sx = (int)shots[i].x; if (sx < 0) sx = 0; if (sx > 255) sx = 255;
    int sy = (int)shots[i].y; if (sy < 0) sy = 0; if (sy > 511) sy = 511;
    uint8_t lf = shots[i].life; if (lf > 63) lf = 63;
    Link.write((uint8_t)sx);
    Link.write((uint8_t)(sy & 0xFF));
    Link.write((uint8_t)(((sy >> 8) & 1) | (lf << 1) | ((shots[i].owner == 2) ? 0x80 : 0)));
  }
  Link.endTransmission();
}
void hostTick() {
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gIn = 0, gFc = 0;
  if (Link.available() >= 1) { gIn = Link.read(); if (Link.available()) gFc = Link.read(); }
  uint8_t hIn = readMy();
  if (status == 0) {
    applyIn(p1, hIn); applyIn(p2, gIn);
    static unsigned long fireBlock = 0;
    if (aOrB() && millis() > fireBlock && p1.alive && !p1.shieldActive) {
      fire(p1, 1); fireBlock = millis() + 220;
    }
    if (gFc != p2.lastFireCnt) {
      if (p2.alive && !p2.shieldActive) fire(p2, 2);
      p2.lastFireCnt = gFc;
    }
    integ(p1); integ(p2);
    updSh(p1); updSh(p2);
    resolveShipCol(p1, p2);
    for (int i = 0; i < DU_NST; i++) {
      if (shots[i].life == 0) continue;
      shots[i].x += shots[i].vx; shots[i].y += shots[i].vy;
      wrap(shots[i].x, shots[i].y);
      shots[i].life--;
      if (shots[i].life == 0) continue;
      Ship& t = (shots[i].owner == 1) ? p2 : p1;
      if (!t.alive || millis() < t.invulnUntil) continue;
      float dx = shots[i].x - t.x, dy = shots[i].y - t.y;
      float r2 = dx * dx + dy * dy;
      if (t.shieldActive && r2 < (float)DU_SH_R * DU_SH_R) {
        shots[i].life = 0; beep(1900, 25);
      } else if (r2 < 64) {
        shots[i].life = 0;
        if (shots[i].owner == 1) s1++; else s2++;
        kill(t);
        if (s1 >= DU_WIN) status = 1;
        else if (s2 >= DU_WIN) status = 2;
        if (status != 0) beep(120, 350);
      }
    }
  }
  sendState();
}
// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool sanePos(float x, float y) {
  return x > -100 && x < DISPLAY_WIDTH + 100 && y > -100 && y < DISPLAY_HEIGHT + 100;
}
bool stateSane() {
  if (!sanePos(p1.x, p1.y) || !sanePos(p2.x, p2.y)) return false;
  if (status > 3 || s1 > DU_WIN + 1 || s2 > DU_WIN + 1) return false;
  if (p1.shieldCharge > DU_SH_MAX || p2.shieldCharge > DU_SH_MAX) return false;
  for (int i = 0; i < DU_NST; i++)
    if (shots[i].life && (shots[i].life > DU_SHL || shots[i].owner < 1 || shots[i].owner > 2)) return false;
  return true;
}

void hostMain() {
  if (persistUsable(GAME_DUEL) && stateSane()) {
    // Zeitstempel beziehen sich auf die alte Laufzeit -> neu setzen
    p1.respawnAt = p1.invulnUntil = 0; p2.respawnAt = p2.invulnUntil = 0;
    p2.lastFireCnt = readGuestNow().cnt;   // sonst schiesst der Gast von allein
  } else { s1 = 0; s2 = 0; status = 0; initMatch(); }
  persistArm(GAME_DUEL);
  unsigned long lt = millis();
  unsigned long lp = 0;
  bool pAB = false;
  uint8_t prevStatus = 0;
  draw();
  while (true) {
    Link.service();
    if (millis() - lt >= 33) { lt = millis(); hostTick(); draw(); }
    bool inWin = (status == 1 || status == 2);
    bool prevWin = (prevStatus == 1 || prevStatus == 2);
    if (!prevWin && inWin) { snapshotGuestRestart(); lp = millis(); }
    prevStatus = status;
    bool AB = aOrB();
    bool guestWants = false;
    if (inWin && millis() - lp >= 100) {
      lp = millis();
      guestWants = pollGuestRestart();
    }
    if (inWin && ((AB && !pAB) || guestWants)) {
      sendRestart(DU_RESTART); initMatch(); draw(); beep(900, 60);
      pAB = false;
      continue;
    }
    pAB = AB;
  }
}
// Mindestlaenge = exakte Sendelaenge (48 Bytes, gepackte Schuesse - s. DU_PKT).
void applyState(const uint8_t* b, uint16_t l) {
  if (l < DU_PKT) return;
  const uint8_t* p = b + 1;
  int16_t v;
  v = (int16_t)(p[0] | (p[1] << 8)); p += 2; p1.x = v;
  v = (int16_t)(p[0] | (p[1] << 8)); p += 2; p1.y = v;
  v = (int16_t)(p[0] | (p[1] << 8)); p += 2; p1.ang = v / 1000.0f;
  p1.alive = *p++;
  bool inv1 = *p++;
  p1.shieldActive = *p++;
  p1.shieldCharge = *p++;
  p1.invulnUntil = inv1 ? (millis() + 80) : 0;
  v = (int16_t)(p[0] | (p[1] << 8)); p += 2; p2.x = v;
  v = (int16_t)(p[0] | (p[1] << 8)); p += 2; p2.y = v;
  v = (int16_t)(p[0] | (p[1] << 8)); p += 2; p2.ang = v / 1000.0f;
  p2.alive = *p++;
  bool inv2 = *p++;
  p2.shieldActive = *p++;
  p2.shieldCharge = *p++;
  p2.invulnUntil = inv2 ? (millis() + 80) : 0;
  s1 = *p++; s2 = *p++;
  uint8_t ns = *p++;
  for (int i = 0; i < DU_NST; i++) {          // je Schuss 3 Byte (siehe DU_PKT)
    uint8_t b0 = *p++, b1 = *p++, b2 = *p++;
    shots[i].x = (float)b0;
    shots[i].y = (float)(b1 | ((b2 & 1) << 8));
    shots[i].life = (uint8_t)((b2 >> 1) & 0x3F);
    shots[i].owner = (b2 & 0x80) ? 2 : 1;
  }
  uint8_t prev = status; status = ns;
  if (prev == 0 && (status == 1 || status == 2)) beep(120, 350);
}
void guestMain() {
  initMatch();
  unsigned long ld = 0;
  draw();
  bool pF = false;
  while (true) {
    Link.service();
    guestInputByte = readMy();
    bool cF = aOrB();
    if (cF && !pF) guestFireCnt++;
    pF = cF;
    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (b[0] == DU_RESTART) { initMatch(); beep(900, 60); }
        else if (b[0] == DU_STATE) applyState(b, l);
      }
    }
    if (millis() - ld >= 33) { ld = millis(); draw(); }
  }
}
}

// ========================= POCKET TANKS =========================
// Zwei Panzer auf zerstoerbarer Landschaft (echtes 2D-Pixelgitter). Spieler
// abwechselnd: Winkel + Power einstellen, Taste A wechselt die Waffe, Taste B
// feuert. Wind beeinflusst die Flugbahn. Treffer reissen ein Loch; ueberstehendes
// Erdreich faellt danach senkrecht nach (kein seitliches Rieseln). Das untere
// Grundgestein ist unzerstoerbar, damit kein Panzer aus dem Bild faellt.
//
// Waffen (kein Shop, feste Ausruestung pro Match):
//   Basic Shell   - Standard, beliebig oft.
//   Inferno Blast - 2x: doppelter Explosions-/Zerstoerungsdurchmesser, gleicher Schaden.
//   Shock Charge  - 2x: Radius wie Standard, hell-lila Explosion, doppelter Schaden.
//   Golden Dome   - 1x: kein Schuss; legt einen gelben Schutzschild um den eigenen
//                   Panzer, der einen ankommenden Schuss zur Explosion bringt und
//                   dabei zerstoert wird. Radius so, dass ein Standard- oder
//                   Shock-Treffer am Schild gerade keinen Schaden macht. Der Schild
//                   bleibt stehen, wo er gesetzt wurde (faellt nicht mit dem Panzer).
namespace Arty {
#define AR_STATE   0xA1
#define AR_RESTART 0xAE

#define AR_W 240
#define AR_H 280
#define AR_TER_TOP 100
#define AR_ROWS (AR_H - AR_TER_TOP)
#define AR_HUD_H 48
#define AR_MAX_HP 50
#define AR_WINS_NEEDED 3
#define AR_BEDROCK_Y 240               // ab hier abwaerts unzerstoerbar

#define AR_BLAST_R 14
#define AR_REPOSE 4

// Waffen
#define WP_BASIC   0
#define WP_INFERNO 1
#define WP_SHOCK   2
#define WP_DOME    3
#define AR_SHIELD_R (AR_BLAST_R + 8)   // 22: gerade kein Schaden bei Basic/Shock
#define AR_SHIELD_SEG 32               // Schild aus 32 Ringsegmenten (einzeln zerstoerbar)
#define AR_SHIELD_PTS 64               // Zeichenpunkte entlang des Rings
#define COL_SHOCK 0xCC1F               // hell-lila Explosion
#define COL_SHIELD 0xFFE0              // gelber Schild

// Explosionsanimation: konstante Dauer unabhaengig vom Radius.
#define AR_EXP_GROW 18
#define AR_EXP_PLAT 2
#define AR_EXP_SHR  18
#define AR_EXP_TOTAL (AR_EXP_GROW + AR_EXP_PLAT + AR_EXP_SHR)

#define AR_PH_AIM     0
#define AR_PH_FLY     1
#define AR_PH_EXPLODE 2
#define AR_PH_SETTLE  3
#define AR_PH_MATCH   4
#define AR_PH_ROUNDWAIT 5

PERSIST unsigned char terrain[AR_W];   // PERSIST: ueberlebt einen Absturz
PERSIST uint8_t solid[AR_ROWS][AR_W];
PERSIST int16_t tank1X, tank2X;
PERSIST int16_t tank1Y, tank2Y;
PERSIST uint8_t hp1, hp2;
PERSIST uint8_t wins1, wins2;
PERSIST uint8_t activePlayer;
PERSIST uint8_t phase;
PERSIST int8_t  wind;
PERSIST int16_t angle;
PERSIST uint8_t power;
PERSIST int16_t angleP[3];
PERSIST uint8_t powerP[3];
float prjX, prjY, prjVX, prjVY;
uint8_t prjOwner;
uint8_t prjWeapon;
int16_t expX, expY;
uint8_t expOwner;
uint8_t expWeapon;
uint8_t expFrame;
uint16_t settleFrames;
// Waffen-Inventar (Index 1/2)
PERSIST uint8_t selWeapon[3];
PERSIST uint8_t ammo[3][4];
uint8_t curWeapon, curAmmo;            // fuer HUD-Anzeige des aktiven Spielers
// Schilde (Index 1/2): Ring aus AR_SHIELD_SEG Segmenten; Bit gesetzt = intakt.
PERSIST uint32_t shieldMask[3];
PERSIST int16_t  shieldX[3], shieldY[3];
int8_t shieldDXo[AR_SHIELD_PTS], shieldDYo[AR_SHIELD_PTS];   // Ring aussen (R)
int8_t shieldDXi[AR_SHIELD_PTS], shieldDYi[AR_SHIELD_PTS];   // Ring innen (R-1)
// Eingabe-Edges
uint8_t hostPrevA, hostPrevB;
uint8_t guestPrevCnt;
uint8_t guestPrevCycle;
uint8_t guestCarved;
uint16_t guestSettle;

unsigned long lastAimRepeat = 0;
unsigned long terrainResyncAt = 0;
PERSIST uint32_t terrainSeed;   // gemeinsamer Seed: Host wuerfelt, Gast bekommt ihn per State -> gleiches Terrain
unsigned long aimReadyAt = 0;          // vor diesem Zeitpunkt wird nicht gefeuert
unsigned long roundWaitUntil = 0;

void sendFullTerrain();

// === Waffen-Parameter ===
int wpHoleR(uint8_t w)  { int base = AR_BLAST_R + 4; return (w == WP_INFERNO) ? base * 2 : base; }
int wpBlastR(uint8_t w) { return (w == WP_INFERNO) ? AR_BLAST_R * 2 : AR_BLAST_R; }
int wpDamage(uint8_t w) { return (w == WP_SHOCK) ? 60 : 30; }
uint16_t wpColor(uint8_t w, uint8_t owner) {
  if (w == WP_SHOCK) return COL_SHOCK;
  return (owner == 1) ? COL_P1 : COL_P2;
}
const char* wpName(uint8_t w) {
  switch (w) {
    case WP_INFERNO: return "Inferno Blast";
    case WP_SHOCK:   return "Shock Charge";
    case WP_DOME:    return "Golden Dome";
    default:         return "Basic Shell";
  }
}

inline bool isSolid(int x, int y) {
  if (x < 0 || x >= AR_W) return true;
  if (y >= AR_H) return true;
  if (y < AR_TER_TOP) return false;
  return solid[y - AR_TER_TOP][x] != 0;
}
inline void setSolid(int x, int y, bool v) {
  if (x < 0 || x >= AR_W || y < AR_TER_TOP || y >= AR_H) return;
  solid[y - AR_TER_TOP][x] = v ? 1 : 0;
}

void genTerrain() {
  // Deterministisch aus terrainSeed (lokaler PRNG, damit der globale random()-
  // Strom fuer Wind/Waffen unberuehrt bleibt). Host & Gast erzeugen bei gleichem
  // Seed exakt dasselbe Terrain -> die Hoehenkarte muss nie uebertragen werden.
  uint32_t s = terrainSeed ? terrainSeed : 1u;
  auto rnd10 = [&]() -> float { s = s * 1664525u + 1013904223u; return (float)((s >> 8) % 1000) / 100.0f; };
  float a = rnd10(), b = rnd10(), c = rnd10(), d = rnd10();
  for (int x = 0; x < AR_W; x++) {
    float t = x * 0.045f;
    int h = 110 + (int)(28 * sinf(t + a)
                        + 18 * sinf(2.0f*t + b)
                        + 10 * sinf(0.6f*t + c)
                        +  6 * sinf(4.0f*t + d));
    if (h < 50)  h = 50;
    if (h > 170) h = 170;
    terrain[x] = (unsigned char)h;
  }
}

bool slumpPass(bool rev) {
  bool moved = false;
  int xs = rev ? AR_W - 2 : 0;
  int xe = rev ? -1 : AR_W - 1;
  int dx = rev ? -1 : 1;
  for (int x = xs; x != xe; x += dx) {
    int diff = (int)terrain[x] - (int)terrain[x + 1];
    if (diff > AR_REPOSE) { terrain[x]--; terrain[x + 1]++; moved = true; }
    else if (diff < -AR_REPOSE) { terrain[x]++; terrain[x + 1]--; moved = true; }
  }
  return moved;
}
void settleTerrain() {
  bool rev = false;
  for (int i = 0; i < 600; i++) { if (!slumpPass(rev)) break; rev = !rev; }
}

void buildGrid() {
  for (int x = 0; x < AR_W; x++) {
    int top = AR_H - terrain[x];
    for (int gy = 0; gy < AR_ROWS; gy++) {
      int y = gy + AR_TER_TOP;
      solid[gy][x] = (y >= top) ? 1 : 0;
    }
  }
}
void gridToHeightmap() {
  for (int x = 0; x < AR_W; x++) {
    int top = AR_H;
    for (int gy = 0; gy < AR_ROWS; gy++) {
      if (solid[gy][x]) { top = gy + AR_TER_TOP; break; }
    }
    int h = AR_H - top;
    if (h < 0) h = 0;
    if (h > AR_ROWS) h = AR_ROWS;
    terrain[x] = (unsigned char)h;
  }
}

int16_t groundTop(int x) {
  if (x < 0) x = 0;
  if (x >= AR_W) x = AR_W - 1;
  for (int gy = 0; gy < AR_ROWS; gy++) if (solid[gy][x]) return gy + AR_TER_TOP;
  return AR_H;
}

void placeTanks() {
  tank1X = 25; tank2X = AR_W - 25;
  int h1 = 0, h2 = 0;
  for (int x = max(0, tank1X - 10); x <= min(AR_W - 1, tank1X + 10); x++)
    if (terrain[x] > h1) h1 = terrain[x];
  for (int x = max(0, tank2X - 10); x <= min(AR_W - 1, tank2X + 10); x++)
    if (terrain[x] > h2) h2 = terrain[x];
  for (int x = max(0, tank1X - 8); x <= min(AR_W - 1, tank1X + 8); x++) terrain[x] = h1;
  for (int x = max(0, tank2X - 8); x <= min(AR_W - 1, tank2X + 8); x++) terrain[x] = h2;
}

void saveActiveSettings() { angleP[activePlayer] = angle; powerP[activePlayer] = power; }
void loadActiveSettings() { angle = angleP[activePlayer]; power = powerP[activePlayer]; }

void resetTerrain() {
  // Nur der Host wuerfelt einen neuen Seed; der Gast nutzt den per State empfangenen.
  if (myRole == ROLE_HOST) terrainSeed = (uint32_t)micros() ^ (terrainSeed * 2654435761u + 12345u);
  genTerrain();
  settleTerrain();
  placeTanks();
  buildGrid();
  tank1Y = groundTop(tank1X);
  tank2Y = groundTop(tank2X);
}

void initAmmo() {
  for (int p = 1; p <= 2; p++) {
    ammo[p][WP_BASIC] = 0;
    ammo[p][WP_INFERNO] = 2;
    ammo[p][WP_SHOCK] = 2;
    ammo[p][WP_DOME] = 1;
    selWeapon[p] = WP_BASIC;
  }
}
void clearShields() {
  shieldMask[1] = shieldMask[2] = 0;
}
void initShieldTable() {
  for (int i = 0; i < AR_SHIELD_PTS; i++) {
    float th = (float)i / AR_SHIELD_PTS * 6.2831853f;
    shieldDXo[i] = (int8_t)lroundf(AR_SHIELD_R * cosf(th));
    shieldDYo[i] = (int8_t)lroundf(AR_SHIELD_R * sinf(th));
    shieldDXi[i] = (int8_t)lroundf((AR_SHIELD_R - 1) * cosf(th));
    shieldDYi[i] = (int8_t)lroundf((AR_SHIELD_R - 1) * sinf(th));
  }
}
// Nur die Ringsegmente zerstoeren, die im Explosionsradius liegen.
void destroyShieldArc(uint8_t p, int ex, int ey, int rad) {
  for (int s = 0; s < AR_SHIELD_SEG; s++) {
    float th = ((float)s + 0.5f) / AR_SHIELD_SEG * 6.2831853f;
    int sx = shieldX[p] + (int)(AR_SHIELD_R * cosf(th));
    int sy = shieldY[p] + (int)(AR_SHIELD_R * sinf(th));
    int ddx = sx - ex, ddy = sy - ey;
    if (ddx*ddx + ddy*ddy <= rad*rad) shieldMask[p] &= ~(1UL << s);
  }
}

void initMatch() {
  randomSeed(millis());
  wins1 = 0; wins2 = 0;
  hp1 = AR_MAX_HP; hp2 = AR_MAX_HP;
  angleP[1] = 45; angleP[2] = 45;
  powerP[1] = 50; powerP[2] = 50;
  activePlayer = 1;
  loadActiveSettings();
  phase = AR_PH_AIM;
  wind = random(-8, 9);
  resetTerrain();
  initAmmo();
  initShieldTable();
  clearShields();
  curWeapon = WP_BASIC; curAmmo = 0;
  prjWeapon = WP_BASIC; expWeapon = WP_BASIC;
  hostPrevA = 0; hostPrevB = 0;
  guestPrevCnt = guestFireCnt; guestPrevCycle = 0;
  guestCarved = 0; guestSettle = 0;
  expX = 0; expY = 0; expOwner = 1; expFrame = 0;
  aimReadyAt = millis() + 800;
}

void initRound() {
  hp1 = AR_MAX_HP; hp2 = AR_MAX_HP;
  activePlayer = (wins1 + wins2) % 2 == 0 ? 1 : 2;
  phase = AR_PH_AIM;
  loadActiveSettings();
  wind = random(-8, 9);
  resetTerrain();
  clearShields();
  hostPrevA = 0; hostPrevB = 0;
  guestPrevCnt = guestFireCnt; guestPrevCycle = 0;
  aimReadyAt = millis() + 800;
}

void applyDamage(int cx, int cy, int r, int dmg) {
  int d1x = cx - tank1X, d1y = cy - tank1Y;
  int d2x = cx - tank2X, d2y = cy - tank2Y;
  float d1 = sqrtf((float)(d1x*d1x + d1y*d1y));
  float d2 = sqrtf((float)(d2x*d2x + d2y*d2y));
  if (d1 < r) { int hit = (int)(dmg * (1.0f - d1 / (float)r)); if (hit > hp1) hp1 = 0; else hp1 -= hit; }
  if (d2 < r) { int hit = (int)(dmg * (1.0f - d2 / (float)r)); if (hit > hp2) hp2 = 0; else hp2 -= hit; }
}

void cycleWeapon(uint8_t p) {
  for (int i = 0; i < 4; i++) {
    selWeapon[p] = (selWeapon[p] + 1) % 4;
    if (selWeapon[p] == WP_BASIC) break;
    if (ammo[p][selWeapon[p]] > 0) break;
  }
}

void fireShot() {
  prjOwner = activePlayer;
  int srcX = (activePlayer == 1) ? tank1X : tank2X;
  int srcY = (activePlayer == 1) ? tank1Y - 6 : tank2Y - 6;
  float rad = angle * 0.0174533f;
  float dirX = cosf(rad);
  float dirY = -sinf(rad);
  if (activePlayer == 2) dirX = -dirX;
  float speed = power * 0.08f;
  prjX = srcX + dirX * 10;
  prjY = srcY + dirY * 10;
  prjVX = dirX * speed;
  prjVY = dirY * speed;
  phase = AR_PH_FLY;
  beep(220, 90);
}

void switchPlayer() {
  saveActiveSettings();
  if (hp1 == 0 || hp2 == 0) {
    if (hp1 == 0 && hp2 != 0) wins2++;
    else if (hp2 == 0 && hp1 != 0) wins1++;
    if (wins1 >= AR_WINS_NEEDED || wins2 >= AR_WINS_NEEDED) { phase = AR_PH_MATCH; return; }
    initRound();
    phase = AR_PH_ROUNDWAIT;
    roundWaitUntil = millis() + 1200;     // kleine Pause zwischen den Runden
    sendFullTerrain();
    return;
  }
  activePlayer = (activePlayer == 1) ? 2 : 1;
  phase = AR_PH_AIM;
  loadActiveSettings();
  wind = random(-8, 9);
}

// Feuern bzw. Dome aktivieren je nach gewaehlter Waffe.
void doFire() {
  uint8_t p = activePlayer;
  uint8_t w = selWeapon[p];
  if (w != WP_BASIC && ammo[p][w] == 0) { w = WP_BASIC; selWeapon[p] = WP_BASIC; }
  if (w == WP_DOME) {
    ammo[p][WP_DOME]--;
    shieldMask[p] = 0xFFFFFFFFUL;
    shieldX[p] = (p == 1) ? tank1X : tank2X;
    shieldY[p] = ((p == 1) ? tank1Y : tank2Y) - 4;
    cycleWeapon(p);            // weg von der (nun evtl. leeren) Dome
    beep(1200, 80);
    switchPlayer();            // kein Schuss -> Zug endet
    return;
  }
  if (w != WP_BASIC) { ammo[p][w]--; }
  prjWeapon = w;
  fireShot();
  if (w != WP_BASIC && ammo[p][w] == 0) cycleWeapon(p);
}

// === Drawing ===
void drawTank(int x, int y, uint16_t col) {
  canvas.fillRect(x - 6, y - 4, 12, 4, col);
  canvas.fillRect(x - 5, y - 7, 10, 3, col);
}
void drawAimingTank(int x, int y, uint16_t col, int16_t ang, bool flipX) {
  drawTank(x, y, col);
  float rad = ang * 0.0174533f;
  float dx = cosf(rad) * 14;
  float dy = -sinf(rad) * 14;
  if (flipX) dx = -dx;
  canvas.drawLine(x, y - 6, x + (int)dx, y - 6 + (int)dy, col);
  canvas.drawLine(x + 1, y - 6, x + 1 + (int)dx, y - 6 + (int)dy, col);
}
void drawTerrain() {
  uint16_t terrainCol = 0x4208;
  for (int x = 0; x < AR_W; x++) {
    int gy = 0;
    while (gy < AR_ROWS) {
      if (solid[gy][x]) {
        int start = gy;
        while (gy < AR_ROWS && solid[gy][x]) gy++;
        int y0 = start + AR_TER_TOP;
        int y1 = gy - 1 + AR_TER_TOP;
        canvas.drawFastVLine(x, y0, y1 - y0 + 1, terrainCol);
        canvas.drawPixel(x, y0, COL_TEXT);
      } else gy++;
    }
  }
}
void drawShields() {
  for (int p = 1; p <= 2; p++) {
    if (shieldMask[p] == 0) continue;
    for (int i = 0; i < AR_SHIELD_PTS; i++) {
      int seg = (i * AR_SHIELD_SEG) / AR_SHIELD_PTS;
      if (shieldMask[p] & (1UL << seg)) {
        canvas.drawPixel(shieldX[p] + shieldDXo[i], shieldY[p] + shieldDYo[i], COL_SHIELD);
        canvas.drawPixel(shieldX[p] + shieldDXi[i], shieldY[p] + shieldDYi[i], COL_SHIELD);
      }
    }
  }
}
void drawHud() {
  canvas.fillRect(0, 0, AR_W, AR_HUD_H, COL_BG);
  char buf[24];
  int hpY = 10;
  canvas.drawRect(17, hpY, 60, 12, COL_P1);
  int hpw = (hp1 * 58) / AR_MAX_HP;
  if (hpw > 0) canvas.fillRect(18, hpY + 1, hpw, 10, COL_P1);
  canvas.drawRect(AR_W - 77, hpY, 60, 12, COL_P2);
  int hpw2 = (hp2 * 58) / AR_MAX_HP;
  if (hpw2 > 0) canvas.fillRect(AR_W - 76, hpY + 1, hpw2, 10, COL_P2);
  snprintf(buf, sizeof(buf), "%u-%u", wins1, wins2);
  canvas.setFont(&FreeSans9pt7b);
  drawCenteredText(buf, 0, AR_W, 18, COL_TEXT);
  int wbY = 24;
  int wbX = AR_W / 2 - 32;
  canvas.drawRect(wbX, wbY, 64, 9, COL_FRAME);
  canvas.drawFastVLine(AR_W / 2, wbY - 2, 2, COL_DIM);
  canvas.drawFastVLine(AR_W / 2, wbY + 9, 2, COL_DIM);
  int wbar = wind * 3;
  uint16_t wc = (wind == 0) ? COL_DIM : COL_TEXT;
  if (wbar > 0) canvas.fillRect(AR_W / 2, wbY + 1, wbar, 7, wc);
  else if (wbar < 0) canvas.fillRect(AR_W / 2 + wbar, wbY + 1, -wbar, 7, wc);
  canvas.setFont(NULL);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.setCursor(AR_W / 2 - 11, 37);
  canvas.print("Wind");
}
void drawAimHud() {
  int by = AR_H - 38;
  canvas.fillRect(0, by, AR_W, 38, COL_BG);
  uint16_t myCol = (activePlayer == 1) ? COL_P1 : COL_P2;
  int barX = 15, barY = by + 2, barW = 130, barH = 18;
  canvas.drawRect(barX, barY, barW, barH, COL_FRAME);
  int filled = ((power - 10) * (barW - 2)) / 90;
  if (filled > 0) canvas.fillRect(barX + 1, barY + 1, filled, barH - 2, myCol);
  canvas.setFont(&FreeSansBold9pt7b);
  char buf[16];
  snprintf(buf, sizeof(buf), "%03u", angle);
  int16_t xx, yy; uint16_t ww, hh;
  canvas.getTextBounds(buf, 0, 0, &xx, &yy, &ww, &hh);
  int gapL = barX + barW;
  int gapR = AR_W;
  int totalW = ww + 6;
  int textX = gapL + (gapR - gapL - totalW) / 2;
  canvas.setTextColor(myCol);
  canvas.setCursor(textX, barY + 14);
  canvas.print(buf);
  canvas.drawCircle(textX + ww + 4, barY + 4, 2, myCol);
  // Waffe + Anzahl zentriert unter Winkel/Power
  canvas.setFont(&FreeSans9pt7b);
  char wbuf[28];
  if (curWeapon == WP_BASIC) snprintf(wbuf, sizeof(wbuf), "Basic Shell");
  else snprintf(wbuf, sizeof(wbuf), "%u x %s", curAmmo, wpName(curWeapon));
  drawCenteredText(wbuf, 0, AR_W, AR_H - 3, COL_TEXT);
}
void drawProjectile() {
  uint16_t c = (prjOwner == 1) ? COL_SHOT_P1 : COL_SHOT_P2;
  canvas.fillCircle((int)prjX, (int)prjY, 2, c);
}
void drawExplosion() {
  if (phase != AR_PH_EXPLODE) return;
  int maxR = wpHoleR(expWeapon);
  uint16_t col = wpColor(expWeapon, expOwner);
  int outer, inner = 0;
  if (expFrame < AR_EXP_GROW) {
    outer = (maxR * (expFrame + 1)) / AR_EXP_GROW;
  } else if (expFrame < AR_EXP_GROW + AR_EXP_PLAT) {
    outer = maxR;
  } else {
    outer = maxR;
    int s = expFrame - (AR_EXP_GROW + AR_EXP_PLAT) + 1;
    inner = (maxR * s) / AR_EXP_SHR;
  }
  if (inner >= maxR) return;
  if (outer > maxR) outer = maxR;
  canvas.fillCircle(expX, expY, outer, col);
  if (inner > 0) canvas.fillCircle(expX, expY, inner, COL_BG);
}
void drawActivePlayerArrow() {
  if (phase != AR_PH_AIM) return;
  int tx = (activePlayer == 1) ? tank1X : tank2X;
  int ty = (activePlayer == 1) ? tank1Y : tank2Y;
  uint16_t col = (activePlayer == 1) ? COL_P1 : COL_P2;
  int yTip = ty - 32;
  bool localIsActive = (myRole == ROLE_HOST) ? (activePlayer == 1) : (activePlayer == 2);
  if (localIsActive)
    canvas.fillTriangle(tx - 7, yTip, tx + 7, yTip, tx, yTip + 7, col);
  else
    canvas.drawTriangle(tx - 7, yTip, tx + 7, yTip, tx, yTip + 7, col);
}
void drawMatchEnd() {
  canvas.fillScreen(COL_BG);
  uint8_t winner = (wins1 > wins2) ? 1 : 2;
  uint16_t col = (winner == 1) ? COL_P1 : COL_P2;
  canvas.setFont(&FreeSansBold12pt7b);
  char buf[24];
  snprintf(buf, sizeof(buf), "P%u WINS", winner);
  drawCenteredText(buf, 0, AR_W, 130, col);
  canvas.setFont(&FreeSansBold9pt7b);
  snprintf(buf, sizeof(buf), "%u : %u", wins1, wins2);
  drawCenteredText(buf, 0, AR_W, 165, COL_TEXT);
  canvas.setFont(&FreeSans9pt7b);
  drawCenteredText("Press a button", 0, AR_W, 230, COL_DIM);
}
void draw() {
  if (phase == AR_PH_MATCH) { drawMatchEnd(); pushCanvas(); return; }
  canvas.fillScreen(COL_BG);
  drawTerrain();
  if (activePlayer == 1) {
    drawAimingTank(tank1X, tank1Y, COL_P1, angle, false);
    drawTank(tank2X, tank2Y, COL_P2);
  } else {
    drawTank(tank1X, tank1Y, COL_P1);
    drawAimingTank(tank2X, tank2Y, COL_P2, angle, true);
  }
  drawShields();
  if (phase == AR_PH_FLY) drawProjectile();
  if (phase == AR_PH_EXPLODE) drawExplosion();
  drawActivePlayerArrow();
  drawHud();
  drawAimHud();
  pushCanvas();
}

// === I2C ===
void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(AR_STATE);
  Link.write(phase);
  Link.write(activePlayer);
  Link.write((uint8_t)angle); Link.write((uint8_t)(angle >> 8));
  Link.write(power);
  Link.write((uint8_t)wind);
  Link.write(hp1); Link.write(hp2);
  Link.write(wins1); Link.write(wins2);
  Link.write((uint8_t)tank1X); Link.write((uint8_t)(tank1X >> 8));
  Link.write((uint8_t)tank1Y); Link.write((uint8_t)(tank1Y >> 8));
  Link.write((uint8_t)tank2X); Link.write((uint8_t)(tank2X >> 8));
  Link.write((uint8_t)tank2Y); Link.write((uint8_t)(tank2Y >> 8));
  Link.write((uint8_t)(int)prjX); Link.write((uint8_t)((int)prjX >> 8));
  Link.write((uint8_t)(int)prjY); Link.write((uint8_t)((int)prjY >> 8));
  Link.write(prjOwner);
  Link.write((uint8_t)expX); Link.write((uint8_t)(expX >> 8));
  Link.write((uint8_t)expY); Link.write((uint8_t)(expY >> 8));
  Link.write(expOwner);
  Link.write(expFrame);
  Link.write(expWeapon);
  Link.write(curWeapon);
  Link.write(curAmmo);
  Link.write((uint8_t)shieldX[1]); Link.write((uint8_t)(shieldX[1] >> 8));
  Link.write((uint8_t)shieldY[1]); Link.write((uint8_t)(shieldY[1] >> 8));
  for (int k = 0; k < 4; k++) Link.write((uint8_t)(shieldMask[1] >> (8 * k)));
  Link.write((uint8_t)shieldX[2]); Link.write((uint8_t)(shieldX[2] >> 8));
  Link.write((uint8_t)shieldY[2]); Link.write((uint8_t)(shieldY[2] >> 8));
  for (int k = 0; k < 4; k++) Link.write((uint8_t)(shieldMask[2] >> (8 * k)));
  for (int k = 0; k < 4; k++) Link.write((uint8_t)(terrainSeed >> (8 * k)));   // gemeinsamer Terrain-Seed
  Link.endTransmission();
}
// Frueher: die komplette 240-Byte-Hoehenkarte uebertragen (0xA2). Das war ein
// 246-Byte-Frame = 4 USB-Pakete und ging auf der marginalen Leitung fast nie
// vollstaendig durch. Jetzt erzeugen Host & Gast dasselbe Terrain deterministisch
// aus terrainSeed (siehe genTerrain/applyState), Krater graebt der Gast lokal.
// Die Uebertragung ist damit ueberfluessig -> No-Op (kein Grossframe mehr).
void sendFullTerrain() { }

// === Host-Logik ===
bool hostDirHeld(int gpio) { return digitalRead(gpio) == LOW; }

void hostTickAim(uint8_t guestIn, bool guestFired, bool guestCycle) {
  bool U = false, D = false, L = false, R = false, fire = false, cyc = false;
  if (activePlayer == 1) {
    U = hostDirHeld(KEY_UP); D = hostDirHeld(KEY_DOWN);
    L = hostDirHeld(KEY_LEFT); R = hostDirHeld(KEY_RIGHT);
    bool a = (digitalRead(KEY_A) == LOW);
    bool b = (digitalRead(KEY_B) == LOW);
    if (b && !hostPrevB) fire = true;
    if (a && !hostPrevA) cyc = true;
    hostPrevA = a ? 1 : 0; hostPrevB = b ? 1 : 0;
  } else {
    if (guestIn & 0x01) L = true;
    if (guestIn & 0x02) R = true;
    if (guestIn & 0x04) U = true;
    if (guestIn & 0x08) D = true;
    fire = guestFired;
    cyc = guestCycle;
  }
  if (millis() - lastAimRepeat >= 80) {
    lastAimRepeat = millis();
    int8_t aSign = (activePlayer == 1) ? +1 : -1;
    if (L) angle += 2 * aSign;
    if (R) angle -= 2 * aSign;
    if (angle > 175) angle = 175;
    if (angle <   5) angle = 5;
    if (U) { power += 2; if (power > 100) power = 100; }
    if (D) { power -= 2; if (power < 10)  power = 10; }
  }
  if (cyc) { cycleWeapon(activePlayer); beep(700, 30); }
  if (millis() < aimReadyAt) fire = false;   // kurze Pause nach Rundenstart
  if (fire) doFire();
}

void hostTickFly() {
  for (int i = 0; i < 2; i++) {
    float oldX = prjX, oldY = prjY;
    prjX += prjVX;
    prjY += prjVY;
    prjVY += 0.12f;
    prjVX += wind * 0.002f;
    int ix = (int)prjX, iy = (int)prjY;
    // Ueber dem oberen Displayrand: weiterfliegen lassen (Schwerkraft holt ihn
    // zurueck), keine Kollision, kein Zugende - der Schuss tritt wieder ins Bild.
    if (iy < 0) continue;
    if (ix < 0 || ix >= AR_W) { phase = AR_PH_AIM; switchPlayer(); return; }
    // Schild: nur wenn der Schuss von aussen durch ein INTAKTES Segment eintritt.
    // Trifft er ein bereits zerstoertes Segment (Loch), fliegt er hindurch.
    for (int p = 1; p <= 2; p++) {
      if (shieldMask[p] == 0 || p == prjOwner) continue;
      float odx = oldX - shieldX[p], ody = oldY - shieldY[p];
      float ndx = prjX - shieldX[p], ndy = prjY - shieldY[p];
      float R2 = (float)AR_SHIELD_R * AR_SHIELD_R;
      if (odx*odx + ody*ody > R2 && ndx*ndx + ndy*ndy <= R2) {
        float ang = atan2f(ndy, ndx); if (ang < 0) ang += 6.2831853f;
        int seg = (int)(ang / 6.2831853f * AR_SHIELD_SEG) % AR_SHIELD_SEG;
        if (shieldMask[p] & (1UL << seg)) {           // intakt -> Detonation am Rand
          float d = sqrtf(ndx*ndx + ndy*ndy); if (d < 1) d = 1;
          int ex = shieldX[p] + (int)(AR_SHIELD_R * ndx / d);
          int ey = shieldY[p] + (int)(AR_SHIELD_R * ndy / d);
          expX = ex; expY = ey; expOwner = prjOwner; expWeapon = prjWeapon; expFrame = 0;
          applyDamage(ex, ey, wpBlastR(prjWeapon) + 8, wpDamage(prjWeapon));
          phase = AR_PH_EXPLODE; beep(150, 200);
          return;
        }
      }
    }
    int d1x = ix - tank1X, d1y = iy - tank1Y;
    int d2x = ix - tank2X, d2y = iy - tank2Y;
    bool hitTank = (d1x*d1x + d1y*d1y < 64) || (d2x*d2x + d2y*d2y < 64);
    bool hitGround = isSolid(ix, iy);
    if (hitTank || hitGround) {
      expX = ix; expY = iy; expOwner = prjOwner; expWeapon = prjWeapon; expFrame = 0;
      applyDamage(ix, iy, wpBlastR(prjWeapon) + 8, wpDamage(prjWeapon));
      phase = AR_PH_EXPLODE;
      beep(150, 200);
      return;
    }
  }
}

void carveCrater() {
  int r = wpHoleR(expWeapon);
  for (int y = expY - r; y <= expY + r; y++) {
    if (y >= AR_BEDROCK_Y) continue;          // Grundgestein bleibt stehen
    for (int x = expX - r; x <= expX + r; x++) {
      int dx = x - expX, dy = y - expY;
      if (dx*dx + dy*dy <= r*r) setSolid(x, y, false);
    }
  }
}

bool gravityPass(bool rev) {
  (void)rev;
  bool moved = false;
  for (int gy = AR_ROWS - 2; gy >= 0; gy--) {
    int y = gy + AR_TER_TOP;
    for (int x = 0; x < AR_W; x++) {
      if (!solid[gy][x]) continue;
      if (!isSolid(x, y + 1)) {
        solid[gy][x] = 0; setSolid(x, y + 1, true); moved = true;
      }
    }
  }
  return moved;
}

void hostTickExplode() {
  if (expFrame == AR_EXP_GROW) {
    carveCrater();
    // Schild verschwindet in der Explosion wie Landschaft: alle Segmente im
    // Explosionsradius fallen weg (gilt fuer jede Explosion in Schildnaehe,
    // also auch beim zweiten/dritten Treffer).
    for (int p = 1; p <= 2; p++)
      if (shieldMask[p]) destroyShieldArc(p, expX, expY, wpHoleR(expWeapon));
    tank1Y = groundTop(tank1X);
    tank2Y = groundTop(tank2X);
  }
  expFrame++;
  if (expFrame >= AR_EXP_TOTAL) {
    phase = AR_PH_SETTLE;
    settleFrames = 0;
    expFrame = 0;
  }
}

void hostTickSettle() {
  bool moved = gravityPass(false);
  if (gravityPass(true)) moved = true;
  expFrame++;
  settleFrames++;
  tank1Y = groundTop(tank1X);
  tank2Y = groundTop(tank2X);
  if (!moved || settleFrames > 200) {
    gridToHeightmap();
    buildGrid();
    sendFullTerrain();
    switchPlayer();
  }
}

void hostTickRoundWait() {
  // Kurze Pause zwischen Runden: Eingaben ignorieren. Danach Tastenflanken auf
  // den aktuellen Zustand setzen, damit ein gehaltener Knopf nicht sofort feuert.
  if (millis() >= roundWaitUntil) {
    hostPrevA = (digitalRead(KEY_A) == LOW) ? 1 : 0;
    hostPrevB = (digitalRead(KEY_B) == LOW) ? 1 : 0;
    phase = AR_PH_AIM;
  }
}

void hostTick() {
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gIn = 0, gCnt = guestPrevCnt;
  if (Link.available() >= 1) { gIn = Link.read(); if (Link.available()) gCnt = Link.read(); }
  bool guestFired = (gCnt != guestPrevCnt);
  if (guestFired) guestPrevCnt = gCnt;
  bool guestCycle = (gIn & 0x10) && !guestPrevCycle;
  guestPrevCycle = (gIn & 0x10) != 0;

  if (phase == AR_PH_AIM) hostTickAim(gIn, guestFired, guestCycle);
  else if (phase == AR_PH_FLY) hostTickFly();
  else if (phase == AR_PH_EXPLODE) hostTickExplode();
  else if (phase == AR_PH_SETTLE) hostTickSettle();
  else if (phase == AR_PH_ROUNDWAIT) hostTickRoundWait();

  curWeapon = selWeapon[activePlayer];
  curAmmo = (curWeapon == WP_BASIC) ? 0 : ammo[activePlayer][curWeapon];

  sendState();
  if ((phase == AR_PH_AIM || phase == AR_PH_FLY || phase == AR_PH_ROUNDWAIT) && millis() > terrainResyncAt) {
    terrainResyncAt = millis() + 2000;
    sendFullTerrain();
  }
}

// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool stateSane() {
  if (wins1 > AR_WINS_NEEDED || wins2 > AR_WINS_NEEDED) return false;
  if (hp1 > AR_MAX_HP || hp2 > AR_MAX_HP) return false;
  if (activePlayer < 1 || activePlayer > 2 || phase > AR_PH_ROUNDWAIT) return false;
  if (tank1X < 0 || tank1X >= AR_W || tank2X < 0 || tank2X >= AR_W) return false;
  if (angle < -180 || angle > 360 || power > 100) return false;
  for (int x = 0; x < AR_W; x++) if (terrain[x] > AR_ROWS) return false;
  for (int p = 1; p <= 2; p++) if (selWeapon[p] > 3) return false;
  return true;
}

void hostMain() {
  if (persistUsable(GAME_ARTY) && stateSane()) {
    // Gelaende, Panzer, Leben, Siege, Munition und Schilde stehen noch im
    // Speicher. Nur die Hilfstabellen fuer die Schildringe neu berechnen (die
    // baut sonst initMatch auf) und die Zeitsteuerung neu anstossen.
    initShieldTable();
    aimReadyAt = millis() + 800;
    roundWaitUntil = 0; lastAimRepeat = 0; terrainResyncAt = 0;
    hostPrevA = 0; hostPrevB = 0; guestCarved = 0; guestSettle = 0;
    guestPrevCnt = readGuestNow().cnt;     // sonst feuert der Gast von allein
  } else {
    initMatch();
  }
  persistArm(GAME_ARTY);
  unsigned long lt = millis();
  bool pAB = false;
  draw();
  while (true) {
    Link.service();
    if (millis() - lt >= 33) { lt = millis(); hostTick(); draw(); }
    if (phase == AR_PH_MATCH) {
      bool AB = aOrB();
      bool guestRestart = false;
      Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
      uint8_t gIn = 0, gCnt = guestPrevCnt;
      if (Link.available() >= 1) { gIn = Link.read(); if (Link.available()) gCnt = Link.read(); }
      if (gCnt != guestPrevCnt) { guestPrevCnt = gCnt; guestRestart = true; }
      if ((AB && !pAB) || guestRestart) {
        sendRestart(AR_RESTART); initMatch(); draw(); beep(900, 60);
        pAB = false;
        continue;
      }
      pAB = AB;
    }
  }
}

// === Guest-Logik ===
void applyState(const uint8_t* b, uint16_t l) {
  if (l < 22) return;
  int i = 1;
  phase = b[i++];
  activePlayer = b[i++];
  angle = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  power = b[i++];
  wind = (int8_t)b[i++];
  hp1 = b[i++]; hp2 = b[i++];
  wins1 = b[i++]; wins2 = b[i++];
  tank1X = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  tank1Y = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  tank2X = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  tank2Y = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  prjX = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  prjY = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  prjOwner = b[i++];
  expX = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  expY = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  expOwner = b[i++];
  expFrame = b[i++];
  if (l < 49) return;
  expWeapon = b[i++];
  curWeapon = b[i++];
  curAmmo = b[i++];
  shieldX[1] = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  shieldY[1] = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  shieldMask[1] = (uint32_t)b[i] | ((uint32_t)b[i+1] << 8) | ((uint32_t)b[i+2] << 16) | ((uint32_t)b[i+3] << 24); i += 4;
  shieldX[2] = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  shieldY[2] = (int16_t)(b[i] | (b[i+1] << 8)); i += 2;
  shieldMask[2] = (uint32_t)b[i] | ((uint32_t)b[i+1] << 8) | ((uint32_t)b[i+2] << 16) | ((uint32_t)b[i+3] << 24); i += 4;
  // Gemeinsamer Terrain-Seed: aendert er sich (neue Runde/neues Match), das
  // Terrain lokal EXAKT wie der Host neu erzeugen. Krater danach laufen ohnehin
  // synchron (der Gast graebt sie selbst) - keine Hoehenkarte noetig.
  if (l < 53) return;
  uint32_t seed = (uint32_t)b[i] | ((uint32_t)b[i+1] << 8) | ((uint32_t)b[i+2] << 16) | ((uint32_t)b[i+3] << 24); i += 4;
  if (seed != terrainSeed) {
    terrainSeed = seed;
    resetTerrain();          // Gast: nutzt den empfangenen Seed (wuerfelt keinen neuen)
    guestCarved = 0;
  }
}
void guestMain() {
  initMatch();
  unsigned long ld = 0;
  bool pB = false;
  bool ready = false;   // erst zeichnen, wenn das gemeinsame Terrain (Seed vom Host) da ist,
                        // sonst blitzt kurz die lokale Zufallskarte auf
  canvas.fillScreen(COL_BG);
  canvas.setFont(&FreeSansBold12pt7b);
  drawCenteredText("Bereit...", 0, AR_W, AR_H / 2, COL_TEXT);
  pushCanvas();
  while (true) {
    Link.service();
    uint8_t in = 0;
    if (digitalRead(KEY_LEFT)  == LOW) in |= 0x01;
    if (digitalRead(KEY_RIGHT) == LOW) in |= 0x02;
    if (digitalRead(KEY_UP)    == LOW) in |= 0x04;
    if (digitalRead(KEY_DOWN)  == LOW) in |= 0x08;
    if (digitalRead(KEY_A)     == LOW) in |= 0x10;   // A = Waffe wechseln
    guestInputByte = in;
    bool b = (digitalRead(KEY_B) == LOW);            // B = feuern
    if (b && !pB) guestFireCnt++;
    pB = b;

    if (recvNew) {
      noInterrupts();
      uint8_t rb[256]; uint16_t rl = recvLen;
      for (uint16_t i = 0; i < rl; i++) rb[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (rl > 0) {
        if (rb[0] == AR_RESTART) { initMatch(); beep(900, 60); }
        else if (rb[0] == AR_STATE) { applyState(rb, rl); ready = true; }
      }
    }
    if (phase == AR_PH_EXPLODE) {
      if (!guestCarved && expFrame >= AR_EXP_GROW) {
        carveCrater();
        tank1Y = groundTop(tank1X);
        tank2Y = groundTop(tank2X);
        guestCarved = 1;
      }
      guestSettle = 0;
    } else if (phase == AR_PH_SETTLE) {
      while (guestSettle < expFrame) {
        gravityPass(false);
        gravityPass(true);
        guestSettle++;
      }
      tank1Y = groundTop(tank1X);
      tank2Y = groundTop(tank2X);
    } else {
      guestCarved = 0;
      guestSettle = 0;
    }
    if (ready && millis() - ld >= 33) { ld = millis(); draw(); }
  }
}
}
// ========================= SPACE INVADERS 2P =========================
// Beide Spieler sehen ihr eigenes Schiff unten, das Schild rechts daneben.
// Erreicht durch 180-Grad-Drehung der Host-Ansicht (X und Y gespiegelt).
// Schilde sind zerstoerbar (Pixelmaske). Bonus-Objekte fliegen horizontal
// in der Bildmitte, beim Abschuss faellt das Bonus-Symbol zum Schiess-Spieler.
// ========================= BOMBING BOB =========================
// Dynablaster/Bomberman-Klon fuer zwei Spieler. Host-autoritativ: der Host
// simuliert alles (fluessige Pixel-Bewegung beider Figuren, Bomben,
// Explosionen, Power-ups) und schickt jeden Tick den Renderzustand; der Gast
// sendet nur seine Eingabe und zeichnet den empfangenen Zustand -> kein Desync.
//   Steuerung: Steuerkreuz halten = laufen, A/B = Bombe legen.
//   Power-ups (unter Ziegeln): +Bombe (gleichzeitige Bomben), +Feuer (Reichweite).
namespace Bomber {
#define BB_STATE   0xC1
#define BB_RESTART 0xCE

#define BB_COLS 13
#define BB_ROWS 11
#define BB_CELL 16
#define BB_OX ((DISPLAY_WIDTH - BB_COLS * BB_CELL) / 2)   // 16
#define BB_OY 56
// Das Render-Grid kennt nur 7 Werte (0..6) -> 3 Bit pro Zelle statt 1 Byte.
// 143 Zellen = 54 Byte statt 143. Damit schrumpft das Zustandspaket von 153 auf
// 64 Byte Nutzlast (69 Byte Frame) - das war mit Abstand das groesste Frame und
// brauchte 3 USB-Pakete; jetzt sind es ~1. Weniger Angriffsflaeche fuer Aussetzer.
#define BB_GRIDB ((BB_COLS * BB_ROWS * 3 + 7) / 8)        // 54
#define BB_PKT (1 + BB_GRIDB + 3 + 3 + 3)                 // 64

#define BB_TICK     40      // ms pro Tick
#define BB_SPEED    2       // px pro Tick (fluessige Bewegung)
#define BB_HALF     6       // halbe Figurbreite (Kollision)
#define BB_FUSE     50      // ~2 s Zuendzeit
#define BB_FLAME    14      // Flammendauer in Ticks (Brenndauer je Zelle)
#define BB_FSPREAD  1       // Ticks Vorlauf pro Kachel Abstand (Ausbreitung)
#define BB_MAXBOMB  12
#define BB_CAP_BOMB 5
#define BB_CAP_RANGE 6

// Terrain
#define BC_EMPTY 0
#define BC_BRICK 1
#define BC_WALL  2
// nur im Render-Grid
#define BR_FLAME 3
#define BR_BOMB  4
#define BR_PBOMB 5          // Power-up: +Bombe
#define BR_PFIRE 6          // Power-up: +Reichweite

// Farben
#define BB_COL_WALL  0x8410
#define BB_COL_WEDGE 0xB5B6
#define BB_COL_BRICK 0xC408
#define BB_COL_BEDGE 0xE5CC
#define BB_COL_FLAME 0xFFE0
#define BB_COL_FEDGE 0xFD20
#define BB_COL_BOMB  0x2124
// A1 Bruchstein (wegbombbarer Stein, pro Feld leicht anders)
#define BS_BASE  0x8AE7
#define BS_DEEP  0x20C1
#define BS_LIGHT 0xB3EA
#define BS_SHADE 0x5183
// B3 Feuerball (Flamme)
#define FL_CORE  0xFFD9
#define FL_YEL   0xFEAC
#define FL_ORA   0xFC84
#define FL_SPRK  0xFF72
// C2 Kugel mit Zuender (Bombe)
#define BM_BLK   0x18C4
#define BM_HI    0x5B0E
#define BM_SHN   0xD71E
#define BM_FUSE  0x9388
#define BM_SPK   0xFF4F
#define BM_SPK2  0xFCA5
#define BM_RING  0x2967

uint8_t rgrid[BB_COLS][BB_ROWS];   // Render-Grid (Host baut, Gast empfaengt)

struct Player { int px, py; bool alive; uint8_t face, frame;
                uint16_t walk; int obc, obr; uint8_t maxB, range; };
// PERSIST: Diese Variablen liegen im Speicherbereich, den die Startroutine
// NICHT zuruecksetzt. Ein Watchdog-Neustart loescht den Arbeitsspeicher nicht -
// dadurch stehen Figuren, Bomben, weggebombte Bloecke und Punktestand danach
// noch exakt so da wie vorher, und es wird genau dort weitergespielt.
PERSIST Player  pl1, pl2;
PERSIST uint8_t s1, s2, status;       // 0 laeuft, 1 P1, 2 P2, 3 Remis

// --- nur Host ---
PERSIST uint8_t terr[BB_COLS][BB_ROWS];
PERSIST uint8_t fl[BB_COLS][BB_ROWS];
PERSIST uint8_t pup[BB_COLS][BB_ROWS];   // 0 keins, 1 +Bombe, 2 +Feuer
struct Bomb { bool on; int c, r; uint8_t fuse, owner, range; };
PERSIST Bomb    bombs[BB_MAXBOMB];
uint8_t guestPrevFire;
bool    hostPrevBomb = false;

// Eine Zelle brennt (sichtbar + toedlich) nur waehrend der Brennphase.
// fl > BB_FLAME  = Vorlauf: die Flammenfront ist noch unterwegs (nicht sichtbar).
// fl in 1..BB_FLAME = brennt.  fl == 0 = aus.
inline bool burning(int x, int y) { return fl[x][y] > 0 && fl[x][y] <= BB_FLAME; }

uint8_t readPad() {
  if (digitalRead(KEY_UP)    == LOW) return 0;
  if (digitalRead(KEY_RIGHT) == LOW) return 1;
  if (digitalRead(KEY_DOWN)  == LOW) return 2;
  if (digitalRead(KEY_LEFT)  == LOW) return 3;
  return 0xFF;
}
int bombIndexAt(int c, int r) {
  for (int i = 0; i < BB_MAXBOMB; i++) if (bombs[i].on && bombs[i].c == c && bombs[i].r == r) return i;
  return -1;
}
int bombCount(uint8_t o) {
  int n = 0; for (int i = 0; i < BB_MAXBOMB; i++) if (bombs[i].on && bombs[i].owner == o) n++;
  return n;
}
// solide fuer eine Figur, deren eigene (gerade gelegte) Bombe bei (sc,sr) noch durchlaesst
bool solidPx(int fx, int fy, int sc, int sr) {
  if (fx < 0 || fy < 0) return true;
  int cx = fx / BB_CELL, cy = fy / BB_CELL;
  if (cx >= BB_COLS || cy >= BB_ROWS) return true;
  uint8_t t = terr[cx][cy];
  if (t == BC_WALL || t == BC_BRICK) return true;
  int bi = bombIndexAt(cx, cy);
  if (bi >= 0 && !(cx == sc && cy == sr)) return true;
  return false;
}
bool canBeAt(int px, int py, int sc, int sr) {
  int h = BB_HALF;
  return !solidPx(px - h, py - h, sc, sr) && !solidPx(px + h, py - h, sc, sr)
      && !solidPx(px - h, py + h, sc, sr) && !solidPx(px + h, py + h, sc, sr);
}
bool boxInCell(int px, int py, int col, int row) {
  int h = BB_HALF;
  return col >= (px - h) / BB_CELL && col <= (px + h) / BB_CELL
      && row >= (py - h) / BB_CELL && row <= (py + h) / BB_CELL;
}

void movePlayer(Player& p, uint8_t dir) {
  if (dir == 0xFF) return;
  p.face = dir;
  int col = p.px / BB_CELL, row = p.py / BB_CELL;
  int ccx = col * BB_CELL + BB_CELL / 2, ccy = row * BB_CELL + BB_CELL / 2;
  bool moved = false;
  if (dir == 1 || dir == 3) {                 // horizontal: zur Korridor-Mitte (y) ziehen
    if (p.py != ccy) { int d = ccy - p.py; if (d > BB_SPEED) d = BB_SPEED; else if (d < -BB_SPEED) d = -BB_SPEED; p.py += d; moved = true; }
    int nx = p.px + (dir == 1 ? BB_SPEED : -BB_SPEED);
    if (canBeAt(nx, p.py, p.obc, p.obr)) { p.px = nx; moved = true; }
  } else {                                    // vertikal: zur Korridor-Mitte (x) ziehen
    if (p.px != ccx) { int d = ccx - p.px; if (d > BB_SPEED) d = BB_SPEED; else if (d < -BB_SPEED) d = -BB_SPEED; p.px += d; moved = true; }
    int ny = p.py + (dir == 0 ? -BB_SPEED : BB_SPEED);
    if (canBeAt(p.px, ny, p.obc, p.obr)) { p.py = ny; moved = true; }
  }
  if (moved) p.walk++;
  if (p.obc >= 0 && !boxInCell(p.px, p.py, p.obc, p.obr)) { p.obc = -1; p.obr = -1; }  // eigene Bombe verlassen
}
void tryPlace(Player& p, uint8_t owner) {
  int col = p.px / BB_CELL, row = p.py / BB_CELL;
  if (bombCount(owner) >= p.maxB) return;
  if (bombIndexAt(col, row) >= 0) return;
  if (terr[col][row] != BC_EMPTY) return;
  for (int i = 0; i < BB_MAXBOMB; i++)
    if (!bombs[i].on) { bombs[i].on = true; bombs[i].c = col; bombs[i].r = row; bombs[i].fuse = BB_FUSE; bombs[i].owner = owner; bombs[i].range = p.range; p.obc = col; p.obr = row; return; }
}
// Zellwert = Brenndauer + Vorlauf nach Abstand. Zentrum zuendet sofort,
// die Arme laufen pro Kachel BB_FSPREAD Ticks spaeter an; da alle gleich lang
// brennen, erlischt es in derselben Reihenfolge (von innen nach aussen).
void explodeAt(int bc, int br, uint8_t range) {
  fl[bc][br] = BB_FLAME;
  pup[bc][br] = 0;                                            // Power-up im Zentrum zerstoert
  int dc[4] = {0, 1, 0, -1}, dr[4] = {-1, 0, 1, 0};
  for (int d = 0; d < 4; d++)
    for (int i = 1; i <= range; i++) {
      int c = bc + dc[d] * i, r = br + dr[d] * i;
      if (c < 0 || c >= BB_COLS || r < 0 || r >= BB_ROWS) break;
      if (terr[c][r] == BC_WALL) break;
      uint8_t life = (uint8_t)(BB_FLAME + i * BB_FSPREAD);   // Vorlauf nach Abstand
      // Kiste: wird zerstoert und legt evtl. ein NEUES Power-up frei. Dieses
      // ueberlebt die aktuelle Explosion (wie im Original) - Flamme stoppt hier.
      if (terr[c][r] == BC_BRICK) { terr[c][r] = BC_EMPTY; fl[c][r] = life; if (random(100) < 16) pup[c][r] = (random(2) == 0) ? 1 : 2; break; }
      fl[c][r] = life;
      pup[c][r] = 0;                                          // freiliegendes Power-up von der Flamme zerstoert
      int bi = bombIndexAt(c, r);
      if (bi >= 0 && bombs[bi].fuse > 0) bombs[bi].fuse = 0;   // Kettenreaktion
    }
}
void collect(Player& p) {
  int col = p.px / BB_CELL, row = p.py / BB_CELL;
  uint8_t u = pup[col][row];
  if (u == 0) return;
  if      (u == 1 && p.maxB  < BB_CAP_BOMB)  { p.maxB++;  beep(1600, 60); }
  else if (u == 2 && p.range < BB_CAP_RANGE) { p.range++; beep(1900, 60); }
  else beep(1200, 40);
  pup[col][row] = 0;
}

void initRound() {
  for (int x = 0; x < BB_COLS; x++)
    for (int y = 0; y < BB_ROWS; y++) {
      if (x == 0 || y == 0 || x == BB_COLS - 1 || y == BB_ROWS - 1) terr[x][y] = BC_WALL;
      else if (x % 2 == 0 && y % 2 == 0)                            terr[x][y] = BC_WALL;
      else terr[x][y] = (random(100) < 68) ? BC_BRICK : BC_EMPTY;
      fl[x][y] = 0; pup[x][y] = 0;
    }
  int sp[3][2] = {{1, 1}, {2, 1}, {1, 2}};
  for (int i = 0; i < 3; i++) terr[sp[i][0]][sp[i][1]] = BC_EMPTY;
  int sq[3][2] = {{BB_COLS - 2, BB_ROWS - 2}, {BB_COLS - 3, BB_ROWS - 2}, {BB_COLS - 2, BB_ROWS - 3}};
  for (int i = 0; i < 3; i++) terr[sq[i][0]][sq[i][1]] = BC_EMPTY;
  for (int i = 0; i < BB_MAXBOMB; i++) bombs[i].on = false;
  pl1.px = 1 * BB_CELL + BB_CELL / 2;           pl1.py = 1 * BB_CELL + BB_CELL / 2;
  pl1.alive = true; pl1.face = 2; pl1.frame = 0; pl1.walk = 0; pl1.obc = -1; pl1.obr = -1; pl1.maxB = 1; pl1.range = 2;
  pl2.px = (BB_COLS - 2) * BB_CELL + BB_CELL / 2; pl2.py = (BB_ROWS - 2) * BB_CELL + BB_CELL / 2;
  pl2.alive = true; pl2.face = 0; pl2.frame = 0; pl2.walk = 0; pl2.obc = -1; pl2.obr = -1; pl2.maxB = 1; pl2.range = 2;
  status = 0; guestInputByte = 0xFF;
  // WICHTIG: den Feuerzaehler des Gasts FRISCH vom Link holen (die globale
  // guestFireCnt ist auf dem Host bedeutungslos). Sonst wird beim ersten Tick
  // der neuen Runde ein alter Zaehlerstand als "Bombe" fehlinterpretiert und am
  // Spawn eine Bombe gelegt -> der Spieler stirbt sofort.
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gf0 = 0;
  if (Link.available() >= 1) { Link.read(); if (Link.available()) gf0 = Link.read(); }
  guestPrevFire = gf0;
  hostPrevBomb = aOrB();   // gehaltene A/B-Taste nicht sofort als Bombe werten
}

void buildRender() {
  for (int x = 0; x < BB_COLS; x++)
    for (int y = 0; y < BB_ROWS; y++) {
      if (burning(x, y))               rgrid[x][y] = BR_FLAME;
      else if (bombIndexAt(x, y) >= 0) rgrid[x][y] = BR_BOMB;
      // Power-up erst zeigen, wenn die Flamme an dieser Zelle ganz durch ist
      // (fl==0). Sonst blitzt es waehrend des Flammen-Vorlaufs kurz auf.
      else if (fl[x][y] == 0 && pup[x][y] == 1) rgrid[x][y] = BR_PBOMB;
      else if (fl[x][y] == 0 && pup[x][y] == 2) rgrid[x][y] = BR_PFIRE;
      else                             rgrid[x][y] = terr[x][y];
    }
}
bool inField(int px, int py) {
  return px >= 0 && py >= 0 && px < BB_COLS * BB_CELL && py < BB_ROWS * BB_CELL;
}
uint8_t pflags(Player& p) { return (p.alive ? 1 : 0) | ((p.face & 3) << 1) | (((p.walk >> 2) & 1) << 3); }
// Render-Grid <-> 3-Bit-Packung (siehe BB_GRIDB)
void packGrid(uint8_t* out) {
  for (int i = 0; i < BB_GRIDB; i++) out[i] = 0;
  int bit = 0;
  for (int y = 0; y < BB_ROWS; y++)
    for (int x = 0; x < BB_COLS; x++) {
      uint8_t v = rgrid[x][y] & 7;
      for (int k = 0; k < 3; k++, bit++)
        if (v & (1 << k)) out[bit >> 3] |= (uint8_t)(1 << (bit & 7));
    }
}
void unpackGrid(const uint8_t* in) {
  int bit = 0;
  for (int y = 0; y < BB_ROWS; y++)
    for (int x = 0; x < BB_COLS; x++) {
      uint8_t v = 0;
      for (int k = 0; k < 3; k++, bit++)
        if (in[bit >> 3] & (1 << (bit & 7))) v |= (uint8_t)(1 << k);
      rgrid[x][y] = v;
    }
}
void sendState() {
  buildRender();
  uint8_t g[BB_GRIDB]; packGrid(g);
  Link.beginTransmission(I2C_ADDR);
  Link.write(BB_STATE);
  for (int i = 0; i < BB_GRIDB; i++) Link.write(g[i]);
  Link.write((uint8_t)pl1.px); Link.write((uint8_t)pl1.py); Link.write(pflags(pl1));
  Link.write((uint8_t)pl2.px); Link.write((uint8_t)pl2.py); Link.write(pflags(pl2));
  Link.write(s1); Link.write(s2); Link.write(status);
  Link.endTransmission();
}
void applyState(const uint8_t* b, uint16_t l) {
  if (l < BB_PKT) return;
  const uint8_t* p = b + 1;
  unpackGrid(p); p += BB_GRIDB;
  // Explosions-"bip" (Gast): erkennt den Start einer Explosion daran, dass
  // ganz neu Flammen im Bild auftauchen (Zaehler steigt von 0 auf >0).
  { int fc = 0;
    for (int y = 0; y < BB_ROWS; y++) for (int x = 0; x < BB_COLS; x++) if (rgrid[x][y] == BR_FLAME) fc++;
    static int prevFc = 0;
    if (prevFc == 0 && fc > 0) beep(90, 70);
    prevFc = fc; }
  pl1.px = *p++; pl1.py = *p++; { uint8_t f = *p++; pl1.alive = f & 1; pl1.face = (f >> 1) & 3; pl1.frame = (f >> 3) & 1; }
  pl2.px = *p++; pl2.py = *p++; { uint8_t f = *p++; pl2.alive = f & 1; pl2.face = (f >> 1) & 3; pl2.frame = (f >> 3) & 1; }
  uint8_t n1 = *p++, n2 = *p++, nst = *p++;
  uint8_t prev = status; s1 = n1; s2 = n2; status = nst;
  if (prev == 0 && status != 0) beep(120, 300);
}

// Figur "Rundling" (Entwurf 1): runder Koerper, helles Gesicht, grosse Augen.
// Passt in ein Tile (~12 px). Zentriert auf die Figur-Pixelposition (cx,cy).
void drawFigure(Player& p, uint16_t col) {
  if (!p.alive) return;
  int cx = BB_OX + p.px, cy = BB_OY + p.py;
  int f = p.frame ? 1 : 0;
  // Fuesse (2-Frame-Lauf)
  canvas.fillRect(cx - 4, cy + 5 + (f ? 0 : 1), 3, 2, col);
  canvas.fillRect(cx + 1, cy + 5 + (f ? 1 : 0), 3, 2, col);
  // Koerper
  canvas.fillCircle(cx, cy, 5, col);
  // helles Gesicht
  canvas.fillCircle(cx, cy - 2, 3, 0xFFFF);
  // grosse Augen (leichte Blickrichtung)
  int ex = (p.face == 1) ? 1 : (p.face == 3) ? -1 : 0;
  canvas.fillRect(cx - 2 + ex, cy - 2, 2, 1, 0x0000);
  canvas.fillRect(cx + 1 + ex, cy - 2, 2, 1, 0x0000);
}
// A1 Bruchstein: brauner Stein, koernig, dunkle Fuge, Sprengsel, kurzer Riss.
// Aussehen wird deterministisch aus der Feldposition abgeleitet -> Host & Gast
// zeichnen dasselbe, und die Felder sehen nicht alle gleich aus.
void drawBrickCell(int px, int py, int gx, int gy) {
  uint32_t s = (uint32_t)gx * 374761393u + (uint32_t)gy * 668265263u + 0x2545F491u;
  auto nx = [&]() -> uint32_t { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
  auto rf = [&]() -> float { return (nx() & 0xFFFF) / 65535.0f; };
  canvas.fillRect(px, py, BB_CELL, BB_CELL, BS_BASE);
  canvas.drawRect(px, py, BB_CELL, BB_CELL, BS_DEEP);                 // dunkle Fuge
  for (int i = 1; i < BB_CELL - 1; i++) {                            // Kanten hell/dunkel
    if (rf() < 0.5f) canvas.drawPixel(px + i, py + 1, BS_LIGHT);
    if (rf() < 0.5f) canvas.drawPixel(px + i, py + BB_CELL - 2, BS_SHADE);
  }
  for (int k = 0; k < 12; k++) {                                     // Koernung
    int ax = 1 + (int)(nx() % 14), ay = 2 + (int)(nx() % 12);
    canvas.drawPixel(px + ax, py + ay, (rf() < 0.5f) ? BS_LIGHT : BS_SHADE);
  }
  if (rf() < 0.85f) {                                                // kurzer Riss
    int cx = 3 + (int)(nx() % 9), cy = 3, len = 4 + (int)(nx() % 6);
    for (int i = 0; i < len; i++) { canvas.drawPixel(px + cx, py + cy, BS_DEEP); cy++; if (rf() < 0.4f) cx += (rf() < 0.5f) ? 1 : -1; }
  }
}
// B3 Feuerball: runder Feuerball mit umherfliegenden Funken.
void drawFlameCell(int px, int py, uint8_t anim) {
  int cx = px + BB_CELL / 2, cy = py + BB_CELL / 2;
  canvas.fillCircle(cx, cy, 4, FL_ORA);
  canvas.fillCircle(cx, cy, 3, FL_YEL);
  canvas.fillCircle(cx, cy, 1, FL_CORE);
  static const int8_t sp[8][2] = {{0,-6},{-6,0},{6,0},{0,6},{-4,-4},{4,-4},{-4,4},{4,4}};
  for (int k = 0; k < 8; k++)
    if (((anim + k) & 3) == 0) canvas.drawPixel(cx + sp[k][0], cy + sp[k][1], (k & 1) ? FL_SPRK : FL_ORA);
}
void draw() {
  canvas.fillScreen(COL_BG);
  char buf[16];
  canvas.setFont(&FreeSansBold9pt7b);
  snprintf(buf, sizeof(buf), "P1  %u", s1); canvas.setTextColor(COL_P1); canvas.setCursor(10, 32); canvas.print(buf);
  snprintf(buf, sizeof(buf), "%u  P2", s2); canvas.setTextColor(COL_P2); canvas.setCursor(DISPLAY_WIDTH - 72, 32); canvas.print(buf);
  canvas.drawRect(BB_OX - 1, BB_OY - 1, BB_COLS * BB_CELL + 2, BB_ROWS * BB_CELL + 2, COL_FRAME);
  uint8_t anim = millis() / 80;
  for (int x = 0; x < BB_COLS; x++)
    for (int y = 0; y < BB_ROWS; y++) {
      int px = BB_OX + x * BB_CELL, py = BB_OY + y * BB_CELL, cx = px + BB_CELL / 2, cy = py + BB_CELL / 2;
      switch (rgrid[x][y]) {
        case BC_WALL:
          canvas.fillRect(px, py, BB_CELL, BB_CELL, BB_COL_WALL);
          canvas.drawFastHLine(px, py, BB_CELL, BB_COL_WEDGE);
          canvas.drawFastVLine(px, py, BB_CELL, BB_COL_WEDGE); break;
        case BC_BRICK: drawBrickCell(px, py, x, y); break;
        case BR_FLAME: drawFlameCell(px, py, anim); break;
        case BR_BOMB: {
          int bx = px + 8, by = py + 9;
          uint8_t t = millis() / 90;                       // Zuender-Flackern
          canvas.fillCircle(bx, by, 5, BM_BLK);            // runde Kugel
          canvas.fillCircle(px + 5, py + 6, 1, BM_HI);     // Glanz
          canvas.drawPixel(px + 6, py + 7, BM_SHN);
          static const int8_t fseg[5][2] = {{11,5},{12,4},{13,4},{13,3},{12,2}};   // geringelte Lunte
          for (int i = 0; i < 5; i++) canvas.drawPixel(px + fseg[i][0], py + fseg[i][1], BM_FUSE);
          int tx = px + fseg[4][0], ty = py + fseg[4][1];  // Funke
          if (t & 1) { canvas.drawPixel(tx, ty - 1, BM_SPK);  canvas.drawPixel(tx + 1, ty - 1, BM_SPK2); }
          else       { canvas.drawPixel(tx, ty - 1, BM_SPK2); canvas.drawPixel(tx - 1, ty, BM_SPK); }
          if (t & 1) canvas.drawCircle(bx, by, 6, BM_RING); // dezentes Pulsieren
          break; }
        case BR_PBOMB:
          canvas.fillRoundRect(px + 2, py + 2, BB_CELL - 4, BB_CELL - 4, 3, 0x041F);
          canvas.fillCircle(cx, cy + 1, 3, 0x0000);
          canvas.drawFastVLine(cx, cy - 4, 3, BB_COL_FEDGE); break;
        case BR_PFIRE:
          canvas.fillRoundRect(px + 2, py + 2, BB_CELL - 4, BB_CELL - 4, 3, 0xF9A0);
          canvas.fillTriangle(cx - 3, cy + 3, cx + 3, cy + 3, cx, cy - 4, BB_COL_FLAME); break;
        default: break;
      }
    }
  drawFigure(pl1, COL_P1);
  drawFigure(pl2, COL_P2);
  if (status != 0) {
    canvas.setFont(&FreeSansBold12pt7b);
    const char* t = (status == 1) ? "P1 gewinnt!" : (status == 2) ? "P2 gewinnt!" : "Unentschieden";
    drawCenteredText(t, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT - 14, COL_TEXT);
  }
  pushCanvas();
}

void hostTick() {
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gDir = 0xFF, gFire = guestPrevFire;
  if (Link.available() >= 1) { gDir = Link.read(); if (Link.available()) gFire = Link.read(); }
  bool gBomb = (gFire != guestPrevFire); guestPrevFire = gFire;
  uint8_t hDir = readPad();
  bool hB = aOrB(); bool hBomb = hB && !hostPrevBomb; hostPrevBomb = hB;

  if (status == 0) {
    if (pl1.alive) movePlayer(pl1, hDir);
    if (pl2.alive) movePlayer(pl2, gDir);
    if (pl1.alive) collect(pl1);
    if (pl2.alive) collect(pl2);
    if (pl1.alive && hBomb) tryPlace(pl1, 1);
    if (pl2.alive && gBomb) tryPlace(pl2, 2);
    for (int i = 0; i < BB_MAXBOMB; i++) if (bombs[i].on && bombs[i].fuse > 0) bombs[i].fuse--;
    bool again = true, boom = false;
    while (again) {
      again = false;
      for (int i = 0; i < BB_MAXBOMB; i++)
        if (bombs[i].on && bombs[i].fuse == 0) { bombs[i].on = false; explodeAt(bombs[i].c, bombs[i].r, bombs[i].range); again = true; boom = true; }
    }
    if (boom) beep(90, 70);   // Explosions-"bip" (Host)
    for (int x = 0; x < BB_COLS; x++) for (int y = 0; y < BB_ROWS; y++) if (fl[x][y] > 0) fl[x][y]--;
    if (pl1.alive && burning(pl1.px / BB_CELL, pl1.py / BB_CELL)) pl1.alive = false;
    if (pl2.alive && burning(pl2.px / BB_CELL, pl2.py / BB_CELL)) pl2.alive = false;
    if (!pl1.alive || !pl2.alive) {
      if (!pl1.alive && !pl2.alive) status = 3;
      else if (!pl2.alive) { status = 1; s1++; }
      else                 { status = 2; s2++; }
      beep(120, 300);
    }
  }
  pl1.frame = (pl1.walk >> 2) & 1; pl2.frame = (pl2.walk >> 2) & 1;
  sendState();
}

// Prueft, ob der aus dem Speicher gerettete Zustand ueberhaupt plausibel ist.
// Ohne diese Pruefung wuerde nach einem Neustart auch voelliger Datenmuell als
// gueltiger Spielstand uebernommen - Ergebnis waere ein voellig wirres Feld
// (Flammen ueberall, Bloecke verstreut, feste Mauern an falschen Stellen).
// Faellt die Pruefung durch, wird einfach sauber neu begonnen.
bool stateSane() {
  for (int x = 0; x < BB_COLS; x++)
    for (int y = 0; y < BB_ROWS; y++) {
      uint8_t t = terr[x][y];
      if (t > BC_WALL) return false;                    // nur 0..2 zulaessig
      bool border = (x == 0 || y == 0 || x == BB_COLS - 1 || y == BB_ROWS - 1);
      bool pillar = (x % 2 == 0 && y % 2 == 0);
      if ((border || pillar) && t != BC_WALL) return false;   // feste Mauern
      if (!border && !pillar && t == BC_WALL) return false;    // dort keine Mauer
      if (fl[x][y] > BB_FLAME + BB_CAP_RANGE) return false;    // Flammenrest
      if (pup[x][y] > 2) return false;
    }
  if (!inField(pl1.px, pl1.py) || !inField(pl2.px, pl2.py)) return false;
  if (pl1.maxB == 0 || pl1.maxB > BB_CAP_BOMB) return false;
  if (pl2.maxB == 0 || pl2.maxB > BB_CAP_BOMB) return false;
  if (pl1.range == 0 || pl1.range > BB_CAP_RANGE) return false;
  if (pl2.range == 0 || pl2.range > BB_CAP_RANGE) return false;
  if (status > 3) return false;
  for (int i = 0; i < BB_MAXBOMB; i++)
    if (bombs[i].on && (bombs[i].c < 0 || bombs[i].c >= BB_COLS ||
                        bombs[i].r < 0 || bombs[i].r >= BB_ROWS ||
                        bombs[i].fuse > BB_FUSE)) return false;
  return true;
}

void hostMain() {
  randomSeed(micros());
  // Nach einem Watchdog-Neustart NICHT neu aufbauen: Spielfeld, Figuren, Bomben
  // und Punktestand liegen unveraendert im Speicher (PERSIST) - es geht exakt
  // dort weiter, wo es unterbrochen wurde.
  if (persistUsable(GAME_BOMBER) && stateSane()) {
    hostPrevBomb = aOrB();          // gehaltene Taste nicht als Bombe werten
    guestPrevFire = readGuestNow().cnt;   // echten Zaehlerstand uebernehmen,
                                          // sonst "explodiert" es ohne Zutun
  } else {
    s1 = 0; s2 = 0;                 // Punktestand: sonst Zufallswerte aus dem
    initRound();                    // nicht initialisierten Speicher
  }
  persistArm(GAME_BOMBER);
  unsigned long lt = millis(), lp = 0;
  bool pAB = false; uint8_t prevStatus = 0;
  draw();
  while (true) {
    Link.service();
    if (millis() - lt >= BB_TICK) { lt = millis(); hostTick(); draw(); }
    if (prevStatus == 0 && status != 0) { snapshotGuestRestart(); lp = millis(); }
    prevStatus = status;
    bool AB = aOrB(); bool gw = false;
    if (status != 0 && millis() - lp >= 100) { lp = millis(); gw = pollGuestRestart(); }
    if (status != 0 && ((AB && !pAB) || gw)) {
      sendRestart(BB_RESTART); initRound(); draw(); beep(900, 60); pAB = false; continue;
    }
    pAB = AB;
  }
}

void guestMain() {
  s1 = 0; s2 = 0;               // sonst Zufallswerte aus dem PERSIST-Speicher
  for (int x = 0; x < BB_COLS; x++) for (int y = 0; y < BB_ROWS; y++) rgrid[x][y] = BC_EMPTY;
  status = 0;
  pl1.alive = true; pl1.face = 2; pl1.frame = 0; pl1.px = 1 * BB_CELL + BB_CELL / 2; pl1.py = 1 * BB_CELL + BB_CELL / 2;
  pl2.alive = true; pl2.face = 0; pl2.frame = 0; pl2.px = (BB_COLS - 2) * BB_CELL + BB_CELL / 2; pl2.py = (BB_ROWS - 2) * BB_CELL + BB_CELL / 2;
  unsigned long ld = 0; uint8_t prevStatus = 0; bool pB = false;
  draw();
  while (true) {
    Link.service();
    guestInputByte = readPad();
    bool b = aOrB();
    if (status == 0 && b && !pB) guestFireCnt++;
    pB = b;
    if (prevStatus == 0 && status != 0) resetGuestRestartEdge();
    prevStatus = status;
    if (status != 0 && guestPressedRestart()) guestFireCnt++;
    if (recvNew) {
      noInterrupts();
      uint8_t bb[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) bb[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (bb[0] == BB_RESTART) { status = 0; beep(900, 60); }
        else if (bb[0] == BB_STATE) applyState(bb, l);
      }
    }
    if (millis() - ld >= 33) { ld = millis(); draw(); }
  }
}
}
// ========================= MERKEN (W'n'R) =========================
namespace WnR {
#define WR_STATE   0xF1
#define WR_RESTART 0xFE

#define WR_MAX_SEQ 100

// Phasen
#define WR_PH_INTRO   0
#define WR_PH_WATCH   1   // activePlayer schaut die Sequenz vorgespielt
#define WR_PH_REPLAY  2   // activePlayer wiederholt die Sequenz
#define WR_PH_EXTEND  3   // activePlayer haengt eine neue Richtung an
#define WR_PH_P1_WINS 4
#define WR_PH_P2_WINS 5
#define WR_PH_DRAW    6   // Maximalsequenz erreicht - beide perfekt

// Richtungs-Codes
#define WR_UP    0
#define WR_DOWN  1
#define WR_LEFT  2
#define WR_RIGHT 3

// Farben fuer die vier Felder
#define WR_COL_UP    0x07FF   // Cyan
#define WR_COL_DOWN  0xFFE0   // Gelb
#define WR_COL_LEFT  0x07E0   // Gruen
#define WR_COL_RIGHT 0xF81F   // Magenta

// Toene pro Richtung (Hz)
#define WR_TONE_UP    880
#define WR_TONE_DOWN  587
#define WR_TONE_LEFT  740
#define WR_TONE_RIGHT 1100

// Layout: vier Felder im D-Pad-Kreuz
// Layout: vier Felder im D-Pad-Kreuz, vertikal um Display-Mitte (Y=140)
// zentriert. Felder sind ca. 5% kleiner als zuvor, damit ein Quadratrahmen
// von 240x240 (zentriert auf dem 240x280-Display) genug Abstand hat.
#define WR_FW 74
#define WR_FH 66
#define WR_UP_X    83
#define WR_UP_Y    29
#define WR_DOWN_X  83
#define WR_DOWN_Y 185
#define WR_LEFT_X   5
#define WR_LEFT_Y 107
#define WR_RIGHT_X 161
#define WR_RIGHT_Y 107
// Quadratrahmen: 240x240 zentriert auf 240x280-Display
#define WR_FRAME_Y_TOP 20
#define WR_FRAME_SIZE  240

// Showing-Timing (ms)
#define WR_FLASH_ON   400
#define WR_FLASH_OFF  200
#define WR_STEP       (WR_FLASH_ON + WR_FLASH_OFF)
#define WR_PRE_DELAY  600   // kurze Pause vor Anzeige beginnt

PERSIST uint8_t  seq[WR_MAX_SEQ];        // PERSIST: ueberlebt einen Absturz
PERSIST uint8_t  seqLen;
PERSIST uint8_t  progress;
PERSIST uint8_t  phase;
PERSIST uint8_t  activePlayer;        // 1 oder 2 - wer gerade dran ist
uint8_t  watchFlash;          // Welche Richtung gerade aufblitzt waehrend WATCH (nur bei active)
uint8_t  localFlash;          // Lokales Eingabe-Feedback waehrend REPLAY/EXTEND
unsigned long localFlashUntil;
unsigned long phaseStartedAt;
unsigned long inputBlockUntil;  // Eingaben werden bis zu diesem Zeitpunkt ignoriert
uint8_t  extendBlinkCnt;      // Hilfssignal: blinkt das D-Pad zur EXTEND-Aufforderung

// Edge-Tracker
uint8_t  hostPrevDir;
uint8_t  guestPrevCnt;

uint16_t toneFor(uint8_t dir) {
  if (dir == WR_UP)    return WR_TONE_UP;
  if (dir == WR_DOWN)  return WR_TONE_DOWN;
  if (dir == WR_LEFT)  return WR_TONE_LEFT;
  if (dir == WR_RIGHT) return WR_TONE_RIGHT;
  return 0;
}
uint16_t colorFor(uint8_t dir) {
  if (dir == WR_UP)    return WR_COL_UP;
  if (dir == WR_DOWN)  return WR_COL_DOWN;
  if (dir == WR_LEFT)  return WR_COL_LEFT;
  if (dir == WR_RIGHT) return WR_COL_RIGHT;
  return COL_DIM;
}

bool iAmActive() {
  return (myRole == ROLE_HOST) ? (activePlayer == 1) : (activePlayer == 2);
}

void initMatch() {
  seqLen = 0; progress = 0;
  phase = WR_PH_INTRO;
  activePlayer = 1;
  watchFlash = 0xFF;
  localFlash = 0xFF;
  localFlashUntil = 0;
  hostPrevDir = 0xFF;
  guestPrevCnt = guestFireCnt;
  phaseStartedAt = 0;
  inputBlockUntil = 0;
  extendBlinkCnt = 0;
}

uint8_t hostDirEdge() {
  uint8_t cur = 0xFF;
  if      (digitalRead(KEY_UP)    == LOW) cur = WR_UP;
  else if (digitalRead(KEY_DOWN)  == LOW) cur = WR_DOWN;
  else if (digitalRead(KEY_LEFT)  == LOW) cur = WR_LEFT;
  else if (digitalRead(KEY_RIGHT) == LOW) cur = WR_RIGHT;
  uint8_t res = 0xFF;
  if (cur != 0xFF && hostPrevDir == 0xFF) res = cur;
  hostPrevDir = cur;
  return res;
}

// Pfeil als einfaches Dreieck
void drawArrow(int cx, int cy, uint8_t dir, uint16_t col, bool filled) {
  int x1, y1, x2, y2, x3, y3;
  if (dir == WR_UP) {
    x1 = cx;       y1 = cy - 11;
    x2 = cx - 11;  y2 = cy + 8;
    x3 = cx + 11;  y3 = cy + 8;
  } else if (dir == WR_DOWN) {
    x1 = cx;       y1 = cy + 11;
    x2 = cx - 11;  y2 = cy - 8;
    x3 = cx + 11;  y3 = cy - 8;
  } else if (dir == WR_LEFT) {
    x1 = cx - 11;  y1 = cy;
    x2 = cx + 8;   y2 = cy - 11;
    x3 = cx + 8;   y3 = cy + 11;
  } else { // RIGHT
    x1 = cx + 11;  y1 = cy;
    x2 = cx - 8;   y2 = cy - 11;
    x3 = cx - 8;   y3 = cy + 11;
  }
  if (filled) canvas.fillTriangle(x1, y1, x2, y2, x3, y3, col);
  else        canvas.drawTriangle(x1, y1, x2, y2, x3, y3, col);
}

void drawField(int x, int y, uint8_t dir, bool active) {
  uint16_t col = colorFor(dir);
  if (active) {
    for (int t = 0; t < 3; t++) {
      canvas.drawRoundRect(x + t, y + t, WR_FW - 2 * t, WR_FH - 2 * t, 8 - t, col);
    }
    canvas.drawRoundRect(x + 6, y + 6, WR_FW - 12, WR_FH - 12, 4, col);
  } else {
    canvas.drawRoundRect(x, y, WR_FW, WR_FH, 8, COL_DIM);
  }
  drawArrow(x + WR_FW / 2, y + WR_FH / 2, dir, active ? COL_TEXT : col, active);
}

// Quadratrahmen 240x240 in der Farbe MEINES Spielers - aber nur wenn ich
// gerade dran bin. Wenn der Gegner aktiv ist, kein Rahmen.
void drawPlayerFrame() {
  if (!iAmActive()) return;
  uint16_t myCol = (myRole == ROLE_HOST) ? COL_P1 : COL_P2;
  for (int t = 0; t < 4; t++) {
    canvas.drawRect(t, WR_FRAME_Y_TOP + t,
                    WR_FRAME_SIZE - 2 * t, WR_FRAME_SIZE - 2 * t, myCol);
  }
}

// Kleiner Sequenzlaengen-Anzeiger zwischen den D-Pad-Feldern
void drawCenter() {
  int cx = 120, cy = 140;
  canvas.drawCircle(cx, cy, 18, COL_FRAME);
  canvas.drawCircle(cx, cy, 19, COL_FRAME);
  canvas.setFont(&FreeSansBold12pt7b);
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", seqLen);
  drawCenteredText(buf, cx - 22, 44, cy + 7, COL_TEXT);
}

void drawScene() {
  // Sieg-/Remis-Phasen: Vollbild, Rahmen nur fuer den Gewinner
  if (phase == WR_PH_P1_WINS || phase == WR_PH_P2_WINS || phase == WR_PH_DRAW) {
    canvas.fillScreen(COL_BG);
    // Rahmen nur fuer den Sieger zeichnen - mein Geraet zeigt also nur dann
    // einen Rahmen wenn ich gewonnen habe.
    bool iWon = (phase == WR_PH_P1_WINS && myRole == ROLE_HOST)
             || (phase == WR_PH_P2_WINS && myRole == ROLE_GUEST);
    if (iWon) {
      uint16_t myCol = (myRole == ROLE_HOST) ? COL_P1 : COL_P2;
      for (int t = 0; t < 4; t++) {
        canvas.drawRect(t, WR_FRAME_Y_TOP + t,
                        WR_FRAME_SIZE - 2 * t, WR_FRAME_SIZE - 2 * t, myCol);
      }
    }
    canvas.setFont(&FreeSansBold12pt7b);
    const char* msg = (phase == WR_PH_P1_WINS) ? "P1 WINS" :
                      (phase == WR_PH_P2_WINS) ? "P2 WINS" : "DRAW";
    uint16_t col = (phase == WR_PH_P1_WINS) ? COL_P1 :
                   (phase == WR_PH_P2_WINS) ? COL_P2 : COL_TEXT;
    drawCenteredText(msg, 0, DISPLAY_WIDTH, 130, col);
    char buf[24]; snprintf(buf, sizeof(buf), "Laenge %u", seqLen);
    canvas.setFont(&FreeSansBold9pt7b);
    drawCenteredText(buf, 0, DISPLAY_WIDTH, 170, COL_TEXT);
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Press a button",
                     0, DISPLAY_WIDTH, 220, COL_DIM);
    pushCanvas();
    return;
  }

  // INTRO: D-Pad gedimmt anzeigen
  if (phase == WR_PH_INTRO) {
    canvas.fillScreen(COL_BG);
    drawPlayerFrame();
    drawField(WR_UP_X,    WR_UP_Y,    WR_UP,    false);
    drawField(WR_DOWN_X,  WR_DOWN_Y,  WR_DOWN,  false);
    drawField(WR_LEFT_X,  WR_LEFT_Y,  WR_LEFT,  false);
    drawField(WR_RIGHT_X, WR_RIGHT_Y, WR_RIGHT, false);
    pushCanvas();
    return;
  }

  // Spielfeld zeichnen. Felder sind immer sichtbar - aber nur fuer den
  // aktiven Spieler werden Flash, EXTEND-Blinken und Spielerrahmen gezeichnet.
  canvas.fillScreen(COL_BG);
  drawPlayerFrame();   // greift nur wenn iAmActive()

  uint8_t flashToDraw = 0xFF;
  bool extendInvite = false;
  if (iAmActive()) {
    if (millis() > localFlashUntil) localFlash = 0xFF;
    if      (phase == WR_PH_WATCH)  flashToDraw = watchFlash;
    else if (phase == WR_PH_REPLAY) flashToDraw = localFlash;
    else if (phase == WR_PH_EXTEND) flashToDraw = localFlash;
    extendInvite = (phase == WR_PH_EXTEND && flashToDraw == 0xFF
                    && (extendBlinkCnt & 1));
  }

  for (int d = 0; d < 4; d++) {
    int x = (d == WR_UP) ? WR_UP_X : (d == WR_DOWN) ? WR_DOWN_X
          : (d == WR_LEFT) ? WR_LEFT_X : WR_RIGHT_X;
    int y = (d == WR_UP) ? WR_UP_Y : (d == WR_DOWN) ? WR_DOWN_Y
          : (d == WR_LEFT) ? WR_LEFT_Y : WR_RIGHT_Y;
    bool active = (flashToDraw == d) || extendInvite;
    drawField(x, y, d, active);
  }
  drawCenter();
  pushCanvas();
}

void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(WR_STATE);
  Link.write(phase);
  Link.write(seqLen);
  Link.write(progress);
  Link.write(activePlayer);
  // watchFlash wird mitgeschickt, damit der Gast die Sequenz sieht wenn er
  // selbst aktiv ist. Der Host gibt watchFlash nur waehrend WATCH einen
  // Wert != 0xFF; in allen anderen Phasen ist es 0xFF.
  Link.write(watchFlash);
  Link.endTransmission();
}

void triggerLocalFlash(uint8_t dir) {
  localFlash = dir;
  localFlashUntil = millis() + 280;
  beep(toneFor(dir), 180);
}

// "Jetzt eine neue Richtung anhaengen" - akustisches und visuelles Signal
void playExtendSignal() {
  beep(1200, 80); linkDelay(100); beep(1600, 120);
  extendBlinkCnt = 0;
}

// =========== HOST-Logik ===========

// Aktuell gedrueckte Richtung am Host - liefert 0xFF wenn keine Taste haengt.
// Wird beim Phasenwechsel benutzt um eine gehaltene Taste nicht als neue
// Edge zu zaehlen.
uint8_t currentHostDir() {
  if (digitalRead(KEY_UP)    == LOW) return WR_UP;
  if (digitalRead(KEY_DOWN)  == LOW) return WR_DOWN;
  if (digitalRead(KEY_LEFT)  == LOW) return WR_LEFT;
  if (digitalRead(KEY_RIGHT) == LOW) return WR_RIGHT;
  return 0xFF;
}

void hostStartWatch() {
  phase = WR_PH_WATCH;
  watchFlash = 0xFF;
  localFlash = 0xFF; localFlashUntil = 0;
  phaseStartedAt = millis();
  sendState();
}
void hostStartReplay() {
  phase = WR_PH_REPLAY;
  progress = 0;
  watchFlash = 0xFF;
  localFlash = 0xFF; localFlashUntil = 0;
  // Gehaltene Taste uebernehmen, damit erst nach Loslassen ein Druck zaehlt
  hostPrevDir = currentHostDir();
  guestPrevCnt = guestFireCnt;
  inputBlockUntil = millis() + 500;   // Pause vorm ersten Eingabe-Slot
  sendState();
}
void hostStartExtend() {
  phase = WR_PH_EXTEND;
  watchFlash = 0xFF;
  localFlash = 0xFF; localFlashUntil = 0;
  hostPrevDir = currentHostDir();
  guestPrevCnt = guestFireCnt;
  inputBlockUntil = millis() + 600;   // laengere Pause + Signal
  // Nur der aktive Spieler hoert das Signal - er ist's, der jetzt anhaengen muss
  if (iAmActive()) playExtendSignal();
  sendState();
}
void hostNextTurn() {
  // Gegner ist als naechstes dran - der startet mit WATCH
  activePlayer = (activePlayer == 1) ? 2 : 1;
  hostStartWatch();
}

void hostHandleReplayInput(uint8_t dir) {
  if (dir == seq[progress]) {
    triggerLocalFlash(dir);
    progress++;
    inputBlockUntil = millis() + 250;   // kurze Pause zwischen Eingaben
    if (progress >= seqLen) {
      // Letzte korrekte Eingabe -> Flash explizit zeichnen, sonst kommt
      // hostStartExtend bevor drawScene den letzten Flash rendert.
      drawScene();
      linkDelay(350);
      hostStartExtend();
    }
  } else {
    beep(160, 400);
    phase = (activePlayer == 1) ? WR_PH_P2_WINS : WR_PH_P1_WINS;
    sendState();
  }
}
void hostHandleExtendInput(uint8_t dir) {
  if (seqLen >= WR_MAX_SEQ) {
    // Maximale Laenge schon erreicht - duerfte nicht passieren weil EXTEND
    // gar nicht haette starten sollen, aber defensiv: Remis
    phase = WR_PH_DRAW;
    sendState();
    return;
  }
  seq[seqLen++] = dir;
  triggerLocalFlash(dir);
  drawScene();           // letzter Pfeil muss sichtbar werden
  linkDelay(380);
  // Wenn Sequenz nun maximal ist: beide haben perfekt gespielt -> Remis
  if (seqLen >= WR_MAX_SEQ) {
    phase = WR_PH_DRAW;
    beep(1500, 200); linkDelay(220); beep(1900, 250);
    sendState();
    return;
  }
  hostNextTurn();
}

void hostTickWatch() {
  // Vor dem ersten Aufblitzen kurze Pause als Anfangssignal
  unsigned long elapsed = millis() - phaseStartedAt;
  if (elapsed < WR_PRE_DELAY) {
    if (watchFlash != 0xFF) { watchFlash = 0xFF; }
    return;
  }
  unsigned long t = elapsed - WR_PRE_DELAY;
  uint8_t step = t / WR_STEP;
  if (step >= seqLen) {
    // Sequenz fertig vorgespielt -> REPLAY beginnt
    hostStartReplay();
    return;
  }
  unsigned long phaseT = t % WR_STEP;
  uint8_t prev = watchFlash;
  if (phaseT < WR_FLASH_ON) watchFlash = seq[step];
  else                      watchFlash = 0xFF;
  if (watchFlash != prev && watchFlash != 0xFF && iAmActive()) {
    // Ton lokal beim Aufblitzen - aber nur fuer den, der zuschaut.
    // Der Gegner darf nichts hoeren.
    beep(toneFor(watchFlash), 180);
  }
}

// Wird gesetzt wenn der Gast im End-Zustand A/B drueckt (= guestFireCnt-Edge)
bool wrRestartRequested = false;

void hostTick() {
  // Gast-Eingaben holen
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gIn = 0xFF, gCnt = guestPrevCnt;
  if (Link.available() >= 1) {
    gIn = Link.read();
    if (Link.available()) gCnt = Link.read();
  }
  bool guestEdge = (gCnt != guestPrevCnt);
  uint8_t guestDir = guestEdge ? gIn : 0xFF;
  if (guestEdge) guestPrevCnt = gCnt;
  uint8_t hostDir = hostDirEdge();

  // Im Endzustand: jede Edge vom Gast (A/B) zaehlt als Restart-Wunsch
  if ((phase == WR_PH_P1_WINS || phase == WR_PH_P2_WINS || phase == WR_PH_DRAW)
      && guestEdge) {
    wrRestartRequested = true;
  }

  // Globale Eingabe-Sperre direkt nach Phasenwechsel oder Eingabe
  if (millis() < inputBlockUntil) {
    hostDir = 0xFF;
    guestDir = 0xFF;
  }

  if (phase == WR_PH_WATCH) {
    uint8_t prevWatch = watchFlash;
    hostTickWatch();
    // Bei jedem Aufblitz-Wechsel sofort senden, sonst nur alle 200 ms
    static unsigned long lastSync = 0;
    if (watchFlash != prevWatch || millis() - lastSync > 200) {
      lastSync = millis();
      sendState();
    }
    return;
  }

  if (phase == WR_PH_REPLAY) {
    if (seqLen == 0) {
      // Allererster Zug: nichts zu wiederholen, direkt zu EXTEND
      hostStartExtend();
      return;
    }
    uint8_t inDir = 0xFF;
    if (activePlayer == 1 && hostDir != 0xFF) inDir = hostDir;
    if (activePlayer == 2 && guestDir != 0xFF && guestDir < 4) inDir = guestDir;
    if (inDir != 0xFF) hostHandleReplayInput(inDir);
    return;
  }

  if (phase == WR_PH_EXTEND) {
    // EXTEND-Hinweisblinken: extendBlinkCnt rotiert in 200ms-Schritten
    extendBlinkCnt = (millis() / 200) & 0x07;
    uint8_t inDir = 0xFF;
    if (activePlayer == 1 && hostDir != 0xFF) inDir = hostDir;
    if (activePlayer == 2 && guestDir != 0xFF && guestDir < 4) inDir = guestDir;
    if (inDir != 0xFF) hostHandleExtendInput(inDir);
    return;
  }
}

// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool stateSane() {
  if (seqLen > WR_MAX_SEQ || progress > seqLen) return false;
  if (phase > WR_PH_DRAW || activePlayer < 1 || activePlayer > 2) return false;
  for (int i = 0; i < seqLen; i++) if (seq[i] > 3) return false;
  return true;
}

void hostMain() {
  if (persistUsable(GAME_WNR) && stateSane()) {
    // Sequenz, Fortschritt und wer dran ist stehen noch. Die Phase laeuft
    // zeitgesteuert - daher neu anstossen, damit es sauber weitergeht.
    phaseStartedAt = millis();
    localFlashUntil = 0;
    inputBlockUntil = millis() + 600;
    hostPrevDir = 0xFF;
    guestPrevCnt = readGuestNow().cnt;
  } else {
    initMatch();
    // Direkter Spielstart: P1 ist dran, seqLen=0, also direkt EXTEND
    activePlayer = 1;
    linkDelay(400);            // kurze Anzeige des leeren Spielfelds
    hostStartExtend();
  }
  persistArm(GAME_WNR);
  unsigned long lt = millis();
  bool pAB = false;
  drawScene();
  while (true) {
    Link.service();
    if (millis() - lt >= 33) { lt = millis(); hostTick(); drawScene(); }
    bool AB = aOrB();
    bool inEnd = (phase == WR_PH_P1_WINS || phase == WR_PH_P2_WINS || phase == WR_PH_DRAW);
    if (inEnd && ((AB && !pAB) || wrRestartRequested)) {
      wrRestartRequested = false;
      sendRestart(WR_RESTART);
      initMatch();
      activePlayer = 1;
      linkDelay(400);
      hostStartExtend();
      drawScene();
      beep(900, 60);
      pAB = false;
      continue;
    }
    pAB = AB;
  }
}

// =========== GAST-Logik ===========
//
// Gast bekommt vom Host nur: phase, seqLen, progress, activePlayer.
// Die Sequenz selbst wird NIE uebertragen - nur der Host kennt sie.
// WATCH-Animation auf Gast-Seite passiert nicht, weil der Gast in dieser
// Phase eh nur den Wartebildschirm sieht (wenn sein Gegner P1 ist) bzw.
// auf Host-Seite die Animation laeuft (wenn Gast aktiv ist - dann aber
// ist Gast = activePlayer und Host muesste die Sequenz vom Gast aus zeigen,
// was nicht moeglich ist da nur Host die Sequenz kennt).
//
// LOESUNG: Wir uebertragen waehrend WATCH zusaetzlich zum Status auch das
// momentane watchFlash, ABER nur wenn der Gast der active player ist
// (sonst wuerde ja der Gegner mitlesen koennen). In diesem Spiel kennt aber
// auch der Gast die Sequenz waehrend WATCH - das ist ja der Sinn von WATCH.
// Da ohnehin der Gast aktiv ist, sieht ihn der Host nicht.

void applyState(const uint8_t* b, uint16_t l) {
  if (l < 6) return;
  uint8_t newPhase = b[1];
  seqLen = b[2];
  progress = b[3];
  activePlayer = b[4];
  uint8_t newWatchFlash = b[5];
  uint8_t prevWatch = watchFlash;
  watchFlash = newWatchFlash;
  // Ton fuer WATCH-Animation beim Gast lokal spielen, aber nur wenn Gast
  // selbst der aktive Spieler ist
  if (phase == WR_PH_WATCH && iAmActive()
      && watchFlash != prevWatch && watchFlash != 0xFF) {
    beep(toneFor(watchFlash), 180);
  }
  uint8_t prevPhase = phase;
  phase = newPhase;
  if (prevPhase != phase) {
    if      (phase == WR_PH_P1_WINS) beep(120, 350);
    else if (phase == WR_PH_P2_WINS) beep(120, 350);
    else if (phase == WR_PH_DRAW)    beep(1500, 200);
    if (phase == WR_PH_EXTEND && iAmActive()) {
      playExtendSignal();
      inputBlockUntil = millis() + 600;
    }
    if (phase == WR_PH_REPLAY && iAmActive()) {
      inputBlockUntil = millis() + 500;
    }
  }
}

void guestMain() {
  initMatch();
  unsigned long ld = 0;
  drawScene();
  uint8_t prevDir = 0xFF;
  bool prevAB = false;
  while (true) {
    Link.service();
    // Edge-Erkennung mit Sperrzeit
    uint8_t cur = 0xFF;
    if      (digitalRead(KEY_UP)    == LOW) cur = WR_UP;
    else if (digitalRead(KEY_DOWN)  == LOW) cur = WR_DOWN;
    else if (digitalRead(KEY_LEFT)  == LOW) cur = WR_LEFT;
    else if (digitalRead(KEY_RIGHT) == LOW) cur = WR_RIGHT;
    bool blocked = (millis() < inputBlockUntil);
    if (cur != 0xFF && prevDir == 0xFF && !blocked) {
      guestInputByte = cur;
      guestFireCnt++;
      if ((phase == WR_PH_REPLAY || phase == WR_PH_EXTEND) && iAmActive()) {
        triggerLocalFlash(cur);
        inputBlockUntil = millis() + 250;
      }
    }
    prevDir = cur;

    // A/B im Endzustand: Restart-Wunsch via guestFireCnt-Edge an Host signalisieren
    bool curAB = aOrB();
    bool inEnd = (phase == WR_PH_P1_WINS || phase == WR_PH_P2_WINS || phase == WR_PH_DRAW);
    if (inEnd && curAB && !prevAB) {
      guestFireCnt++;
    }
    prevAB = curAB;

    // Auch der Gast braucht das EXTEND-Blinken wenn er aktiv ist
    if (phase == WR_PH_EXTEND && iAmActive()) {
      extendBlinkCnt = (millis() / 200) & 0x07;
    }

    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (b[0] == WR_RESTART) { initMatch(); beep(900, 60); }
        else if (b[0] == WR_STATE) applyState(b, l);
      }
    }
    if (millis() - ld >= 33) { ld = millis(); drawScene(); }
  }
}
}

// ========================= VIER GEWINNT (Connect 4) =========================
// Rundenbasiert: Host haelt das Brett, sendet bei jedem Zug den kompletten
// State zum Gast. Spieler bewegen ihren Spalten-Cursor mit LEFT/RIGHT
// und werfen mit A/B den Stein ein.
namespace C4 {
#define C4_STATE   0xC1
#define C4_RESTART 0xCF

#define C4_NX 7
#define C4_NY 6

// Layout
#define C4_OX 13
#define C4_OY 50
#define C4_CW 30          // Zellbreite
#define C4_CH 35          // Zellhoehe
#define C4_BW (C4_NX * C4_CW)   // 210
#define C4_BH (C4_NY * C4_CH)   // 210
#define C4_PR 12          // Steinradius

PERSIST uint8_t board[C4_NX][C4_NY];  // PERSIST: ueberlebt einen Absturz
PERSIST uint8_t curCol;
PERSIST uint8_t currentPlayer;          // 1 oder 2
PERSIST uint8_t status;                 // 0=laeuft, 1=P1, 2=P2, 3=remis
PERSIST uint8_t lastCol, lastRow;       // Position des letzten Steins (zum Highlight)
uint8_t hostPrevMask, guestPrevMask;

// Eingaben
#define IN_UP    0x01
#define IN_DOWN  0x02
#define IN_LEFT  0x04
#define IN_RIGHT 0x08
#define IN_AB    0x10

uint8_t readHostMask() {
  uint8_t m = 0;
  if (digitalRead(KEY_UP)    == LOW) m |= IN_UP;
  if (digitalRead(KEY_DOWN)  == LOW) m |= IN_DOWN;
  if (digitalRead(KEY_LEFT)  == LOW) m |= IN_LEFT;
  if (digitalRead(KEY_RIGHT) == LOW) m |= IN_RIGHT;
  if (aOrB())                        m |= IN_AB;
  return m;
}

void initMatch() {
  for (int c = 0; c < C4_NX; c++)
    for (int r = 0; r < C4_NY; r++) board[c][r] = 0;
  curCol = 3;
  currentPlayer = 1;
  status = 0;
  lastCol = 0xFF; lastRow = 0xFF;
  hostPrevMask = 0; guestPrevMask = 0;
}

// Sieg-Check direkt um den zuletzt platzierten Stein - schneller als
// das ganze Brett zu durchsuchen.
bool checkWinAt(int col, int row) {
  uint8_t p = board[col][row];
  if (p == 0) return false;
  const int dirs[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
  for (int d = 0; d < 4; d++) {
    int dx = dirs[d][0], dy = dirs[d][1];
    int count = 1;
    for (int k = 1; k < 4; k++) {
      int x = col + dx * k, y = row + dy * k;
      if (x < 0 || x >= C4_NX || y < 0 || y >= C4_NY) break;
      if (board[x][y] != p) break;
      count++;
    }
    for (int k = 1; k < 4; k++) {
      int x = col - dx * k, y = row - dy * k;
      if (x < 0 || x >= C4_NX || y < 0 || y >= C4_NY) break;
      if (board[x][y] != p) break;
      count++;
    }
    if (count >= 4) return true;
  }
  return false;
}

bool boardFull() {
  for (int c = 0; c < C4_NX; c++) if (board[c][0] == 0) return false;
  return true;
}

// Stein in Spalte col fallen lassen. Liefert true wenn moeglich.
bool dropPiece(int col) {
  if (col < 0 || col >= C4_NX) return false;
  if (board[col][0] != 0) return false;
  int row = C4_NY - 1;
  while (row >= 0 && board[col][row] != 0) row--;
  if (row < 0) return false;
  board[col][row] = currentPlayer;
  lastCol = col; lastRow = row;
  beep(700 + currentPlayer * 200, 60);
  if (checkWinAt(col, row)) {
    status = currentPlayer;
    beep(1500, 200); linkDelay(220); beep(1900, 250);
  } else if (boardFull()) {
    status = 3;
    beep(400, 300);
  } else {
    currentPlayer = (currentPlayer == 1) ? 2 : 1;
  }
  return true;
}

// Cursor in naechste nicht-volle Spalte verschieben
void moveCursor(int dir) {
  int c = curCol;
  for (int k = 0; k < C4_NX; k++) {
    c += dir;
    if (c < 0) c = C4_NX - 1;
    if (c >= C4_NX) c = 0;
    if (board[c][0] == 0) { curCol = c; beep(500, 15); return; }
  }
}

void drawCursor() {
  if (status != 0) return;
  int cx = C4_OX + curCol * C4_CW + C4_CW / 2;
  uint16_t col = (currentPlayer == 1) ? COL_P1 : COL_P2;
  // Stein-Vorschau oberhalb der Spalte + kleiner Pfeil
  canvas.drawCircle(cx, 32, 8, col);
  canvas.drawCircle(cx, 32, 9, col);
  canvas.fillTriangle(cx - 4, 42, cx + 4, 42, cx, 47, col);
}

void drawBoard() {
  // Rahmen
  canvas.drawRect(C4_OX - 2, C4_OY - 2, C4_BW + 4, C4_BH + 4, COL_FRAME);
  // Spalten-Trennlinien gedimmt - betont den "Schacht"-Charakter
  for (int c = 0; c <= C4_NX; c++) {
    int x = C4_OX + c * C4_CW;
    canvas.drawFastVLine(x, C4_OY, C4_BH, COL_FRAME);
  }
  // Steine
  for (int c = 0; c < C4_NX; c++) {
    for (int r = 0; r < C4_NY; r++) {
      int cx = C4_OX + c * C4_CW + C4_CW / 2;
      int cy = C4_OY + r * C4_CH + C4_CH / 2;
      uint8_t v = board[c][r];
      if (v == 0) {
        canvas.drawCircle(cx, cy, C4_PR, COL_DIM);
      } else {
        uint16_t col = (v == 1) ? COL_P1 : COL_P2;
        canvas.fillCircle(cx, cy, C4_PR, col);
        canvas.drawCircle(cx, cy, C4_PR, COL_TEXT);
        // Letzten Stein zusaetzlich markieren
        if (c == lastCol && r == lastRow && status == 0) {
          canvas.drawCircle(cx, cy, C4_PR - 4, COL_TEXT);
        }
      }
    }
  }
}

void drawHud() {
  canvas.setFont(&FreeSansBold9pt7b);
  if (status == 0) {
    const char* who = (currentPlayer == 1) ? "P1 ist dran" : "P2 ist dran";
    drawCenteredText(who, 0, DISPLAY_WIDTH, 18,
                     currentPlayer == 1 ? COL_P1 : COL_P2);
  } else {
    const char* msg = (status == 1) ? "P1 WINS" :
                      (status == 2) ? "P2 WINS" : "DRAW";
    uint16_t c = (status == 1) ? COL_P1 : (status == 2) ? COL_P2 : COL_TEXT;
    drawCenteredText(msg, 0, DISPLAY_WIDTH, 18, c);
  }
}

void drawScene() {
  canvas.fillScreen(COL_BG);
  drawHud();
  drawCursor();
  drawBoard();
  if (status != 0) {
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Press a button",
                     0, DISPLAY_WIDTH, DISPLAY_HEIGHT - 8, COL_DIM);
  }
  pushCanvas();
}

void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(C4_STATE);
  Link.write(curCol);
  Link.write(currentPlayer);
  Link.write(status);
  Link.write(lastCol);
  Link.write(lastRow);
  for (int c = 0; c < C4_NX; c++)
    for (int r = 0; r < C4_NY; r++) Link.write(board[c][r]);
  Link.endTransmission();
}
void applyState(const uint8_t* b, uint16_t l) {
  if (l < 6 + C4_NX * C4_NY) return;
  curCol = b[1];
  currentPlayer = b[2];
  uint8_t prev = status;
  status = b[3];
  lastCol = b[4]; lastRow = b[5];
  int idx = 6;
  for (int c = 0; c < C4_NX; c++)
    for (int r = 0; r < C4_NY; r++) board[c][r] = b[idx++];
  if (prev == 0 && status != 0) beep(120, 350);
}

void handleInput(uint8_t edges) {
  if (edges & IN_LEFT)  moveCursor(-1);
  if (edges & IN_RIGHT) moveCursor(+1);
  if (edges & IN_AB)    dropPiece(curCol);
}

void hostTick() {
  // Gast-Eingabe lesen
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gMask = 0;
  if (Link.available() >= 1) {
    gMask = Link.read();
    if (Link.available()) Link.read();
  }
  uint8_t hMask = readHostMask();
  uint8_t hEdges = hMask & ~hostPrevMask;
  uint8_t gEdges = gMask & ~guestPrevMask;
  hostPrevMask = hMask; guestPrevMask = gMask;

  if (status == 0) {
    uint8_t edges = (currentPlayer == 1) ? hEdges : gEdges;
    handleInput(edges);
  }
  sendState();
}

// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool stateSane() {
  if (currentPlayer < 1 || currentPlayer > 2 || status > 3) return false;
  if (curCol >= C4_NX) return false;
  for (int c = 0; c < C4_NX; c++)
    for (int r = 0; r < C4_NY; r++) if (board[c][r] > 2) return false;
  return true;
}

void hostMain() {
  if (persistUsable(GAME_C4) && stateSane()) {
    // Brett und Zugrecht stehen noch. Tastenmaske abgleichen, sonst wird eine gehaltene
    // Taste sofort als neuer Zug gewertet.
    hostPrevMask = readHostMask(); guestPrevMask = readGuestNow().dir;
  }
  else initMatch();
  persistArm(GAME_C4);
  unsigned long lt = millis();
  unsigned long lp = 0;
  bool pAB = false;
  uint8_t prevStatus = 0;
  drawScene();
  while (true) {
    Link.service();
    if (millis() - lt >= 50) { lt = millis(); hostTick(); drawScene(); }
    bool inWin = (status != 0);
    bool prevWin = (prevStatus != 0);
    if (!prevWin && inWin) { snapshotGuestRestart(); lp = millis(); }
    prevStatus = status;
    bool AB = aOrB();
    bool guestWants = false;
    if (inWin && millis() - lp >= 100) {
      lp = millis();
      guestWants = pollGuestRestart();
    }
    if (inWin && ((AB && !pAB) || guestWants)) {
      sendRestart(C4_RESTART); initMatch(); drawScene(); beep(900, 60);
      pAB = false;
      continue;
    }
    pAB = AB;
  }
}

void guestMain() {
  initMatch();
  unsigned long ld = 0;
  drawScene();
  uint8_t prevStatus = 0;
  while (true) {
    Link.service();
    guestInputByte = readHostMask();   // gleiche Bitmaske, gleicher Aufruf
    bool inWin = (status != 0);
    bool prevWin = (prevStatus != 0);
    if (!prevWin && inWin) resetGuestRestartEdge();
    prevStatus = status;
    if (inWin && guestPressedRestart()) guestFireCnt++;
    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (b[0] == C4_RESTART) { initMatch(); beep(900, 60); }
        else if (b[0] == C4_STATE) applyState(b, l);
      }
    }
    if (millis() - ld >= 33) { ld = millis(); drawScene(); }
  }
}
}
// ========================= PUNKTE & STRICHE (Dots and Boxes) =========================
// Rundenbasiert. Cursor ist eine "Linie", D-Pad navigiert zur naechsten
// Linie in der gewaehlten Richtung. A/B zeichnet die Linie. Wer eine Box
// schliesst bekommt sie zugeschrieben und ist nochmal dran.
namespace Dots {
#define DT_STATE   0xD2
#define DT_RESTART 0xDF

#define DT_NX 6
#define DT_NY 8
#define DT_NUM_H ((DT_NX-1)*DT_NY)            // 5*8  = 40
#define DT_NUM_V (DT_NX*(DT_NY-1))            // 6*7  = 42
#define DT_NUM_LINES (DT_NUM_H + DT_NUM_V)    // 82
#define DT_NUM_BOX ((DT_NX-1)*(DT_NY-1))      // 5*7  = 35

// Bereits gezogene Linien: dezenter als der Rest - duenner (1 px statt 2) und
// gedaempftes Grau statt Reinweiss. Dadurch treten Kaestchen, Punkte und der
// farbige Cursor wieder staerker hervor.
#define DT_COL_LINE 0xA514   // Grau (160,160,160)

// Layout
#define DT_OX  20
#define DT_OY  44
#define DT_CW  40
#define DT_CH  31

PERSIST bool linesH[DT_NX-1][DT_NY];  // PERSIST: ueberlebt einen Absturz
PERSIST bool linesV[DT_NX][DT_NY-1];
PERSIST uint8_t boxes[DT_NX-1][DT_NY-1];
PERSIST uint8_t score1, score2;
PERSIST int16_t cursor;
PERSIST uint8_t currentPlayer;
PERSIST uint8_t status;                 // 0=laeuft, 1=P1, 2=P2, 3=remis
uint8_t hostPrevMask, guestPrevMask;

#define DI_UP    0x01
#define DI_DOWN  0x02
#define DI_LEFT  0x04
#define DI_RIGHT 0x08
#define DI_A     0x10
#define DI_B     0x20

uint8_t readMask() {
  uint8_t m = 0;
  if (digitalRead(KEY_UP)    == LOW) m |= DI_UP;
  if (digitalRead(KEY_DOWN)  == LOW) m |= DI_DOWN;
  if (digitalRead(KEY_LEFT)  == LOW) m |= DI_LEFT;
  if (digitalRead(KEY_RIGHT) == LOW) m |= DI_RIGHT;
  if (digitalRead(KEY_A)     == LOW) m |= DI_A;
  if (digitalRead(KEY_B)     == LOW) m |= DI_B;
  return m;
}

bool isHorizontal(int idx) { return idx < DT_NUM_H; }
void getLineCoords(int idx, int& col, int& row, bool& h) {
  if (idx < DT_NUM_H) {
    h = true;
    col = idx % (DT_NX - 1);
    row = idx / (DT_NX - 1);
  } else {
    h = false;
    int v = idx - DT_NUM_H;
    col = v % DT_NX;
    row = v / DT_NX;
  }
}
void getLineCenter(int idx, float& cx, float& cy) {
  int col, row; bool h; getLineCoords(idx, col, row, h);
  if (h) { cx = DT_OX + (col + 0.5f) * DT_CW; cy = DT_OY + row * DT_CH; }
  else   { cx = DT_OX + col * DT_CW; cy = DT_OY + (row + 0.5f) * DT_CH; }
}
bool isLineDrawn(int idx) {
  int col, row; bool h; getLineCoords(idx, col, row, h);
  return h ? linesH[col][row] : linesV[col][row];
}
void setLineDrawn(int idx) {
  int col, row; bool h; getLineCoords(idx, col, row, h);
  if (h) linesH[col][row] = true;
  else   linesV[col][row] = true;
}

bool boxClosed(int col, int row) {
  return linesH[col][row] && linesH[col][row + 1]
      && linesV[col][row] && linesV[col + 1][row];
}

// Liefert Anzahl in diesem Zug geschlossener Boxen.
int doMove(int idx) {
  if (isLineDrawn(idx)) return -1;
  setLineDrawn(idx);
  int col, row; bool h; getLineCoords(idx, col, row, h);
  int closed = 0;
  if (h) {
    if (row > 0 && boxClosed(col, row - 1) && boxes[col][row - 1] == 0) {
      boxes[col][row - 1] = currentPlayer; closed++;
    }
    if (row < DT_NY - 1 && boxClosed(col, row) && boxes[col][row] == 0) {
      boxes[col][row] = currentPlayer; closed++;
    }
  } else {
    if (col > 0 && boxClosed(col - 1, row) && boxes[col - 1][row] == 0) {
      boxes[col - 1][row] = currentPlayer; closed++;
    }
    if (col < DT_NX - 1 && boxClosed(col, row) && boxes[col][row] == 0) {
      boxes[col][row] = currentPlayer; closed++;
    }
  }
  if (closed > 0) {
    if (currentPlayer == 1) score1 += closed; else score2 += closed;
    beep(1500, 80);
  } else {
    beep(700, 40);
    currentPlayer = (currentPlayer == 1) ? 2 : 1;
  }
  if (score1 + score2 >= DT_NUM_BOX) {
    if      (score1 > score2) status = 1;
    else if (score2 > score1) status = 2;
    else                       status = 3;
    beep(120, 350);
  }
  return closed;
}

// Cursor-Bewegung: finde Linie, deren Mittelpunkt in (dx,dy)-Richtung
// am dichtesten dran ist. Hauptachse minimal, Querachse stark abgestraft.
int findNextLine(int cur, int dx, int dy) {
  float cx, cy; getLineCenter(cur, cx, cy);
  int best = cur;
  float bestScore = 1e9f;
  for (int i = 0; i < DT_NUM_LINES; i++) {
    if (i == cur) continue;
    float lx, ly; getLineCenter(i, lx, ly);
    float ddx = lx - cx, ddy = ly - cy;
    if (dx > 0 && ddx <= 0) continue;
    if (dx < 0 && ddx >= 0) continue;
    if (dy > 0 && ddy <= 0) continue;
    if (dy < 0 && ddy >= 0) continue;
    float major = (dx != 0) ? fabsf(ddx) : fabsf(ddy);
    float minor = (dx != 0) ? fabsf(ddy) : fabsf(ddx);
    float score = major + minor * 3.0f;
    if (score < bestScore) { bestScore = score; best = i; }
  }
  return best;
}

void initMatch() {
  for (int c = 0; c < DT_NX - 1; c++) for (int r = 0; r < DT_NY; r++) linesH[c][r] = false;
  for (int c = 0; c < DT_NX; c++)     for (int r = 0; r < DT_NY - 1; r++) linesV[c][r] = false;
  for (int c = 0; c < DT_NX - 1; c++) for (int r = 0; r < DT_NY - 1; r++) boxes[c][r] = 0;
  cursor = 0;
  currentPlayer = 1;
  score1 = 0; score2 = 0;
  status = 0;
  hostPrevMask = 0; guestPrevMask = 0;
}

void drawHud() {
  canvas.setFont(&FreeSansBold9pt7b);
  char buf[24];
  snprintf(buf, sizeof(buf), "%u", score1);
  drawCenteredText(buf, 5, 50, 22, COL_P1);
  snprintf(buf, sizeof(buf), "%u", score2);
  drawCenteredText(buf, DISPLAY_WIDTH - 55, 50, 22, COL_P2);
  if (status == 0) {
    const char* who = (currentPlayer == 1) ? "P1" : "P2";
    drawCenteredText(who, 60, DISPLAY_WIDTH - 120, 22,
                     currentPlayer == 1 ? COL_P1 : COL_P2);
  } else {
    const char* msg = (status == 1) ? "P1 SIEGT" :
                      (status == 2) ? "P2 SIEGT" : "REMIS";
    drawCenteredText(msg, 60, DISPLAY_WIDTH - 120, 22, COL_TEXT);
  }
  canvas.drawFastHLine(0, 30, DISPLAY_WIDTH, COL_FRAME);
}

void drawBoard() {
  // Boxen (gefuellt fuer den Eigentuemer)
  for (int c = 0; c < DT_NX - 1; c++) {
    for (int r = 0; r < DT_NY - 1; r++) {
      if (boxes[c][r] == 0) continue;
      // obere Kante 1px nach unten, linke 1px nach rechts (unten/rechts bleiben)
      int x = DT_OX + c * DT_CW + 4;
      int y = DT_OY + r * DT_CH + 4;
      int w = DT_CW - 7, h = DT_CH - 7;
      uint16_t col = (boxes[c][r] == 1) ? COL_P1 : COL_P2;
      // Strichgrafik-Style: nur Konturen, kein voller Fill
      for (int t = 0; t < 2; t++) canvas.drawRect(x + t, y + t, w - 2 * t, h - 2 * t, col);
      // Initial in der Mitte
      canvas.setFont(&FreeSansBold9pt7b);
      const char* s = (boxes[c][r] == 1) ? "1" : "2";
      drawCenteredText(s, x, w, y + h / 2 + 4, col);
    }
  }
  // Gezogene Linien
  for (int r = 0; r < DT_NY; r++) {
    for (int c = 0; c < DT_NX - 1; c++) {
      if (!linesH[c][r]) continue;
      int x1 = DT_OX + c * DT_CW;
      int x2 = DT_OX + (c + 1) * DT_CW;
      int y  = DT_OY + r * DT_CH;
      canvas.drawFastHLine(x1, y, x2 - x1, DT_COL_LINE);
    }
  }
  for (int r = 0; r < DT_NY - 1; r++) {
    for (int c = 0; c < DT_NX; c++) {
      if (!linesV[c][r]) continue;
      int x = DT_OX + c * DT_CW;
      int y1 = DT_OY + r * DT_CH;
      int y2 = DT_OY + (r + 1) * DT_CH;
      canvas.drawFastVLine(x, y1, y2 - y1, DT_COL_LINE);
    }
  }
  // Punkte
  for (int r = 0; r < DT_NY; r++) {
    for (int c = 0; c < DT_NX; c++) {
      int x = DT_OX + c * DT_CW;
      int y = DT_OY + r * DT_CH;
      canvas.fillCircle(x, y, 2, COL_TEXT);
    }
  }
  // Cursor: aktuelle Linie blinkt in Spieler-Farbe - auch ueber bereits
  // gezogenen weissen Linien sichtbar, weil der farbige Cursor darueber liegt.
  if (status == 0) {
    int col, row; bool h; getLineCoords(cursor, col, row, h);
    uint16_t cc = (currentPlayer == 1) ? COL_P1 : COL_P2;
    bool blink = (millis() / 250) & 1;
    if (blink) {
      if (h) {
        int x1 = DT_OX + col * DT_CW;
        int x2 = DT_OX + (col + 1) * DT_CW;
        int y  = DT_OY + row * DT_CH;
        canvas.drawFastHLine(x1, y - 1, x2 - x1, cc);
        canvas.drawFastHLine(x1, y,     x2 - x1, cc);
        canvas.drawFastHLine(x1, y + 1, x2 - x1, cc);
      } else {
        int x  = DT_OX + col * DT_CW;
        int y1 = DT_OY + row * DT_CH;
        int y2 = DT_OY + (row + 1) * DT_CH;
        canvas.drawFastVLine(x - 1, y1, y2 - y1, cc);
        canvas.drawFastVLine(x,     y1, y2 - y1, cc);
        canvas.drawFastVLine(x + 1, y1, y2 - y1, cc);
      }
    }
  }
}

void drawScene() {
  canvas.fillScreen(COL_BG);
  drawHud();
  drawBoard();
  if (status != 0) {
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Press a button",
                     0, DISPLAY_WIDTH, DISPLAY_HEIGHT - 6, COL_DIM);
  }
  pushCanvas();
}

// State-Paket: header(1) + cursor(2) + currentPlayer(1) + score1(1) + score2(1)
//             + status(1) + lines(11 gepackt) + boxes(35) = 53 Bytes
void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(DT_STATE);
  Link.write(cursor & 0xFF);
  Link.write((cursor >> 8) & 0xFF);
  Link.write(currentPlayer);
  Link.write(score1);
  Link.write(score2);
  Link.write(status);
  // Linien als Bits packen - 82 Bits = 11 Bytes
  uint8_t bits[11]; for (int i = 0; i < 11; i++) bits[i] = 0;
  int b = 0;
  for (int r = 0; r < DT_NY; r++) for (int c = 0; c < DT_NX - 1; c++) {
    if (linesH[c][r]) bits[b >> 3] |= (1 << (b & 7));
    b++;
  }
  for (int r = 0; r < DT_NY - 1; r++) for (int c = 0; c < DT_NX; c++) {
    if (linesV[c][r]) bits[b >> 3] |= (1 << (b & 7));
    b++;
  }
  for (int i = 0; i < 11; i++) Link.write(bits[i]);
  for (int c = 0; c < DT_NX - 1; c++) for (int r = 0; r < DT_NY - 1; r++)
    Link.write(boxes[c][r]);
  Link.endTransmission();
}
void applyState(const uint8_t* buf, uint16_t l) {
  if (l < 7 + 11 + DT_NUM_BOX) return;
  cursor = (int16_t)(buf[1] | (buf[2] << 8));
  currentPlayer = buf[3];
  score1 = buf[4]; score2 = buf[5];
  uint8_t prev = status; status = buf[6];
  const uint8_t* bits = buf + 7;
  int b = 0;
  for (int r = 0; r < DT_NY; r++) for (int c = 0; c < DT_NX - 1; c++) {
    linesH[c][r] = (bits[b >> 3] >> (b & 7)) & 1; b++;
  }
  for (int r = 0; r < DT_NY - 1; r++) for (int c = 0; c < DT_NX; c++) {
    linesV[c][r] = (bits[b >> 3] >> (b & 7)) & 1; b++;
  }
  const uint8_t* bx = buf + 7 + 11;
  int idx = 0;
  for (int c = 0; c < DT_NX - 1; c++) for (int r = 0; r < DT_NY - 1; r++)
    boxes[c][r] = bx[idx++];
  if (prev == 0 && status != 0) beep(120, 350);
}

// Naechste Linie der ANDEREN Orientierung in der Naehe finden.
// Damit kann der Spieler mit A horizontal/vertikal "umschalten".
int findNearestPerpendicular(int cur) {
  int col, row; bool h; getLineCoords(cur, col, row, h);
  float cx, cy; getLineCenter(cur, cx, cy);
  int best = cur;
  float bestD2 = 1e9f;
  for (int i = 0; i < DT_NUM_LINES; i++) {
    if (i == cur) continue;
    if (isHorizontal(i) == h) continue;
    float lx, ly; getLineCenter(i, lx, ly);
    float ddx = lx - cx, ddy = ly - cy;
    float d2 = ddx * ddx + ddy * ddy;
    if (d2 < bestD2) { bestD2 = d2; best = i; }
  }
  return best;
}

void handleInput(uint8_t edges) {
  if (edges & DI_UP)    { int n = findNextLine(cursor, 0, -1); if (n != cursor) { cursor = n; beep(500, 12); } }
  if (edges & DI_DOWN)  { int n = findNextLine(cursor, 0, +1); if (n != cursor) { cursor = n; beep(500, 12); } }
  if (edges & DI_LEFT)  { int n = findNextLine(cursor, -1, 0); if (n != cursor) { cursor = n; beep(500, 12); } }
  if (edges & DI_RIGHT) { int n = findNextLine(cursor, +1, 0); if (n != cursor) { cursor = n; beep(500, 12); } }
  if (edges & DI_A)     { int n = findNearestPerpendicular(cursor); if (n != cursor) { cursor = n; beep(800, 18); } }
  if (edges & DI_B)     { doMove(cursor); }
}

void hostTick() {
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gMask = 0;
  if (Link.available() >= 1) {
    gMask = Link.read();
    if (Link.available()) Link.read();
  }
  uint8_t hMask = readMask();
  uint8_t hEdges = hMask & ~hostPrevMask;
  uint8_t gEdges = gMask & ~guestPrevMask;
  hostPrevMask = hMask; guestPrevMask = gMask;
  if (status == 0) {
    uint8_t edges = (currentPlayer == 1) ? hEdges : gEdges;
    handleInput(edges);
  }
  sendState();
}

// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool stateSane() {
  if (currentPlayer < 1 || currentPlayer > 2 || status > 3) return false;
  if (cursor < 0 || cursor >= DT_NUM_LINES) return false;
  if ((int)score1 + (int)score2 > DT_NUM_BOX) return false;
  for (int c = 0; c < DT_NX - 1; c++)
    for (int r = 0; r < DT_NY - 1; r++) if (boxes[c][r] > 2) return false;
  return true;
}

void hostMain() {
  if (persistUsable(GAME_DOTS) && stateSane()) {
    // Linien, Kaestchen und Punkte stehen noch. Tastenmaske abgleichen, sonst wird eine gehaltene
    // Taste sofort als neuer Zug gewertet.
    hostPrevMask = readMask(); guestPrevMask = readGuestNow().dir;
  }
  else initMatch();
  persistArm(GAME_DOTS);
  unsigned long lt = millis();
  unsigned long lp = 0;
  bool pAB = false;
  uint8_t prevStatus = 0;
  drawScene();
  while (true) {
    Link.service();
    if (millis() - lt >= 50) { lt = millis(); hostTick(); }
    drawScene();   // Cursor blinkt - jeden Frame zeichnen
    bool inWin = (status != 0);
    bool prevWin = (prevStatus != 0);
    if (!prevWin && inWin) { snapshotGuestRestart(); lp = millis(); }
    prevStatus = status;
    bool AB = aOrB();
    bool guestWants = false;
    if (inWin && millis() - lp >= 100) {
      lp = millis();
      guestWants = pollGuestRestart();
    }
    if (inWin && ((AB && !pAB) || guestWants)) {
      sendRestart(DT_RESTART); initMatch(); drawScene(); beep(900, 60);
      pAB = false;
      continue;
    }
    pAB = AB;
    linkDelay(15);
  }
}
void guestMain() {
  initMatch();
  unsigned long ld = 0;
  drawScene();
  uint8_t prevStatus = 0;
  while (true) {
    Link.service();
    guestInputByte = readMask();
    bool inWin = (status != 0);
    bool prevWin = (prevStatus != 0);
    if (!prevWin && inWin) resetGuestRestartEdge();
    prevStatus = status;
    if (inWin && guestPressedRestart()) guestFireCnt++;
    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (b[0] == DT_RESTART) { initMatch(); beep(900, 60); }
        else if (b[0] == DT_STATE) applyState(b, l);
      }
    }
    if (millis() - ld >= 33) { ld = millis(); drawScene(); }
  }
}
}

// ========================= SCHACH (Chess) =========================
// Rundenbasiert, Host = Weiss (P1), Gast = Schwarz (P2).
// Der Host besitzt die komplette Spiellogik und schickt den vollen
// Zustand (Brett + Metadaten) per I2C. Der Gast spiegelt nur und sendet
// seine Tasten-Bitmaske zurueck - identisch zum Aufbau von C4/Dots.
//
// Besonderheiten: Rochade, En passant, Bauernumwandlung (mit Auswahl),
// Schach-Anzeige + Verhindern illegaler Zuege, Schachmatt und Patt.
//
// Brett um 180 Grad gedreht auf dem Gast-Geraet (Schwarz hat seine
// Figuren unten). Nur das Brett wird gedreht, Texte bleiben aufrecht.
// ==== Schachfiguren aus SVG (weich, Graustufe+Alpha) ====
// je Pixel 2 Bytes: gray, alpha (zeilenweise). Alpha-Blending in drawGlyph().
namespace ChessPix {
struct Glyph { uint8_t w, h; const uint8_t* data; };
static const uint8_t W_PAWN[] = {0,0,0,0,0,0,0,0,0,2,1,0,0,96,2,210,33,243,4,215,0,90,1,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,84,61,255,207,255,240,255,208,255,51,255,0,72,2,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1,0,2,193,212,255,255,250,253,255,255,250,205,255,2,191,2,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,127,2,0,2,6,196,226,255,255,252,251,255,255,252,225,255,7,195,0,2,127,2,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,150,130,255,255,251,247,255,255,251,122,255,0,143,5,0,0,5,0,0,0,0,0,0,0,0,0,0,0,2,2,0,0,88,39,238,181,253,255,254,251,255,255,254,180,253,44,241,0,100,1,0,0,2,0,0,0,0,0,0,0,2,2,0,6,40,33,250,232,255,255,254,254,255,255,255,254,255,255,254,238,255,42,255,5,48,2,0,0,2,0,0,0,0,0,4,4,0,0,145,161,255,255,250,249,255,255,255,255,255,255,255,248,255,255,250,163,255,0,142,4,0,0,4,0,0,0,0,0,2,0,0,0,188,214,254,255,252,253,255,255,255,255,255,255,255,252,255,255,251,198,254,0,170,2,0,0,3,0,0,0,0,0,3,2,0,0,176,200,254,255,251,252,255,255,255,255,255,255,255,251,255,255,251,168,255,0,144,3,0,0,4,0,0,0,0,0,4,4,0,0,103,110,255,255,252,252,255,255,255,255,255,255,255,252,255,255,253,80,255,0,70,3,0,0,3,0,0,0,0,0,1,0,2,5,0,0,217,164,255,255,253,252,255,255,255,252,255,255,252,156,255,0,203,5,0,0,2,0,1,0,0,0,1,0,3,3,0,0,129,59,243,189,253,255,254,253,255,255,255,253,255,255,254,186,253,53,242,0,123,3,0,0,3,0,0,0,3,3,0,0,142,93,255,251,255,255,255,254,255,255,255,255,255,255,255,254,255,255,255,248,255,86,255,0,136,3,0,0,3,3,0,0,94,74,255,255,250,255,254,252,255,255,255,255,255,255,255,255,255,255,255,252,255,255,254,255,250,68,255,0,87,3,0,11,22,20,234,235,253,255,253,252,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,252,255,255,253,230,254,17,230,14,18,0,126,127,255,255,252,251,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,251,255,255,252,121,255,0,120,3,210,211,254,255,253,253,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,253,255,255,253,208,254,3,207,30,245,241,255,254,255,253,255,254,255,254,255,254,255,254,255,254,255,254,255,254,255,254,255,254,255,253,255,254,255,241,255,29,244,34,249,248,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,248,255,34,249,9,222,209,255,245,255,245,255,247,255,247,255,247,255,247,255,248,255,247,255,247,255,247,255,247,255,245,255,246,255,207,255,8,220,0,135,23,245,35,245,35,249,36,249,37,250,37,250,38,251,38,251,38,251,37,250,37,250,37,249,36,249,36,245,22,245,0,129};
static const uint8_t B_PAWN[] = {0,0,0,0,0,0,0,0,0,2,0,0,0,96,0,210,3,243,0,215,0,90,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,84,5,255,19,255,21,255,19,255,5,255,0,72,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,193,18,255,24,250,22,255,24,250,18,255,0,191,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,2,0,196,19,255,23,252,22,255,23,252,19,255,1,195,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,150,11,255,23,251,21,255,24,251,10,255,0,143,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,88,3,238,16,253,23,254,22,255,23,254,16,253,3,241,0,100,0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,40,3,250,21,255,24,254,22,255,22,255,22,255,24,254,21,255,4,255,0,48,0,0,0,2,0,0,0,0,0,4,0,0,0,145,14,255,24,250,22,255,22,255,22,255,22,255,22,255,24,250,14,255,0,142,0,0,0,4,0,0,0,0,0,2,0,0,0,188,18,254,23,252,22,255,22,255,22,255,22,255,22,255,23,251,17,254,0,170,0,0,0,3,0,0,0,0,0,3,0,0,0,176,17,254,23,251,22,255,22,255,22,255,22,255,22,255,23,251,14,255,0,144,0,0,0,4,0,0,0,0,0,4,0,0,0,103,10,255,24,252,22,255,22,255,22,255,22,255,22,255,24,253,7,255,0,70,0,0,0,3,0,0,0,0,0,1,0,2,0,0,0,217,14,255,23,253,22,255,22,255,22,255,23,252,13,255,0,203,0,0,0,2,0,1,0,0,0,1,0,3,0,0,0,129,5,243,17,253,23,254,22,255,22,255,22,255,23,254,16,253,4,242,0,123,0,0,0,3,0,0,0,3,0,0,0,142,8,255,23,255,23,255,22,255,22,255,22,255,22,255,22,255,23,255,22,255,7,255,0,136,0,0,0,3,0,0,0,94,6,255,23,250,22,254,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,254,23,250,6,255,0,87,0,0,0,22,2,234,20,253,23,253,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,23,253,20,254,1,230,0,18,0,126,11,255,24,252,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,24,252,10,255,0,120,0,210,18,254,23,253,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,23,253,18,254,0,207,2,245,21,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,21,255,2,244,3,249,21,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,21,255,3,249,1,222,18,255,22,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,22,255,18,255,1,220,0,135,2,245,3,245,3,249,3,249,3,250,3,250,3,251,3,251,3,251,3,250,3,250,3,249,3,249,3,245,2,245,0,129};
static const uint8_t W_KNIGHT[] = {0,0,0,0,0,3,0,0,0,68,0,158,0,25,0,0,63,4,0,197,0,68,0,4,0,7,0,4,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,2,0,0,90,82,255,72,228,2,103,0,151,112,255,14,171,4,0,4,0,4,0,3,0,127,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,255,2,5,90,175,252,188,255,10,255,127,255,255,253,74,252,0,115,0,108,0,93,0,42,0,0,2,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,4,2,0,0,99,32,255,89,249,206,252,255,253,253,255,215,255,146,255,166,255,144,255,81,252,2,178,0,38,2,0,0,3,0,0,0,0,0,0,0,0,0,2,2,0,0,40,35,227,188,254,255,254,255,255,252,255,255,255,255,255,255,251,255,251,255,252,255,255,199,255,41,236,0,54,2,0,0,2,0,0,0,0,0,0,127,2,63,4,16,202,218,255,255,252,251,255,253,255,255,255,255,255,253,255,250,255,249,255,251,255,251,255,255,250,228,255,31,232,10,25,255,2,0,2,0,0,0,3,4,0,0,54,122,255,181,253,101,255,255,255,251,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,250,255,255,250,191,255,1,170,2,0,0,3,0,0,0,3,255,1,4,54,120,254,183,254,195,255,255,255,253,255,255,255,254,255,255,255,217,255,194,255,255,255,253,255,255,255,251,255,255,252,85,255,0,54,4,0,0,3,0,4,4,0,0,83,121,255,255,252,255,255,255,255,255,255,255,255,251,255,253,255,158,255,106,255,255,255,251,255,255,255,252,255,255,251,204,255,1,165,2,0,0,3,255,2,21,12,24,221,233,255,255,253,252,255,255,255,254,255,252,255,251,254,255,253,83,255,180,255,255,255,252,255,255,255,255,255,253,254,255,255,56,241,0,19,3,0,3,0,0,153,172,255,255,252,252,255,255,255,253,255,255,254,255,251,255,255,128,255,88,255,255,255,253,255,255,255,255,255,255,255,251,255,255,252,136,255,0,84,4,0,0,69,85,255,255,252,251,255,255,255,253,255,255,253,249,255,177,255,74,243,0,214,189,255,255,254,252,255,255,255,255,255,255,255,252,255,255,251,203,254,0,151,1,0,7,194,222,255,255,253,250,255,252,255,255,253,205,255,50,237,0,139,0,34,1,136,196,255,255,251,252,255,255,255,255,255,255,255,254,255,255,253,246,255,29,205,0,1,72,247,188,253,147,255,255,255,255,253,215,255,19,218,0,36,5,0,4,0,25,214,242,255,255,253,254,255,255,255,255,255,255,255,255,255,253,254,255,255,71,240,0,26,57,241,114,255,177,253,173,254,245,252,52,244,0,40,4,0,255,1,0,85,115,255,255,252,251,255,255,255,255,255,255,255,255,255,255,255,251,255,255,254,111,255,0,64,0,86,51,231,67,255,110,251,133,255,0,106,4,0,127,4,13,19,28,227,236,254,255,253,254,255,255,255,255,255,255,255,255,255,255,255,251,255,255,252,150,255,0,103,1,0,0,16,0,123,72,255,10,167,1,0,51,5,4,0,0,183,187,255,255,252,252,255,255,255,255,255,255,255,255,255,255,255,255,255,252,255,255,252,182,255,0,139,0,3,2,0,3,0,0,51,0,2,0,2,3,0,0,139,141,255,255,251,251,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,253,255,255,252,207,255,0,171,0,0,0,1,0,0,2,0,85,3,3,0,0,72,87,255,255,251,250,255,254,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,253,255,255,253,226,255,14,197,0,0,0,0,0,0,0,3,127,2,85,3,21,205,234,254,255,252,253,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,253,241,255,31,218,0,0,0,0,0,0,0,2,4,0,0,32,79,253,255,255,253,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,254,255,254,255,254,255,254,255,249,255,46,240,0,0,0,0,0,0,0,2,1,0,0,26,21,236,55,248,55,247,55,248,55,248,54,247,54,247,54,246,54,246,53,246,53,246,53,245,53,245,53,244,53,245,11,235};
static const uint8_t B_KNIGHT[] = {0,0,0,0,0,3,0,0,0,68,0,158,0,25,0,0,0,4,0,197,0,68,0,4,0,7,0,4,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,90,7,255,6,228,0,103,0,151,9,255,1,171,0,0,0,0,0,0,0,0,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2,0,90,15,252,17,255,1,255,11,255,25,253,6,252,0,115,0,108,0,93,0,42,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,99,3,255,8,249,18,252,23,253,22,255,19,255,13,255,15,255,13,255,7,252,0,178,0,38,0,0,0,3,0,0,0,0,0,0,0,0,0,2,0,0,0,40,3,227,17,254,23,254,23,255,22,255,22,255,23,255,24,251,23,251,24,252,24,255,18,255,3,236,0,54,0,0,0,2,0,0,0,0,0,0,0,2,0,4,1,202,19,255,23,252,22,255,22,255,22,255,22,255,22,255,22,255,21,255,22,255,22,255,23,250,20,255,3,232,0,25,0,2,0,2,0,0,0,3,0,0,0,54,11,255,15,253,9,255,24,255,22,255,22,255,22,255,22,255,23,255,23,255,22,255,22,255,22,255,23,250,16,255,0,170,0,0,0,3,0,0,0,3,0,1,0,54,10,254,16,254,17,255,23,255,22,255,22,255,22,255,23,255,19,255,17,255,23,255,22,255,22,255,22,255,24,252,7,255,0,54,0,0,0,3,0,4,0,0,0,83,10,255,24,252,23,255,22,255,22,255,22,255,22,255,24,255,14,255,9,255,24,255,22,255,22,255,22,255,23,251,17,255,0,165,0,0,0,3,0,2,0,12,2,221,20,255,22,253,22,255,22,255,22,255,22,255,22,254,24,253,7,255,16,255,23,255,22,255,22,255,22,255,22,254,23,255,5,241,0,19,0,0,0,0,0,153,15,255,23,252,22,255,22,255,22,255,22,254,23,251,24,255,11,255,8,255,23,255,22,255,22,255,22,255,22,255,22,255,24,252,12,255,0,84,0,0,0,69,7,255,24,252,22,255,22,255,22,255,23,253,22,255,16,255,6,243,0,214,16,255,23,254,22,255,22,255,22,255,22,255,22,255,23,251,18,254,0,151,0,0,0,194,19,255,23,253,22,255,22,255,23,253,18,255,4,237,0,139,0,34,0,136,17,255,23,251,22,255,22,255,22,255,22,255,22,255,22,253,21,255,2,205,0,1,6,247,16,253,12,255,25,255,23,253,19,255,1,218,0,36,0,0,0,0,2,214,21,255,22,253,22,255,22,255,22,255,22,255,22,255,22,254,23,255,6,240,0,26,5,241,10,255,15,253,16,254,21,252,4,244,0,40,0,0,0,1,0,85,10,255,24,252,22,255,22,255,22,255,22,255,22,255,22,255,22,255,24,254,10,255,0,64,0,86,4,231,6,255,9,251,11,255,0,106,0,0,0,4,0,19,2,227,21,254,22,253,22,255,22,255,22,255,22,255,22,255,22,255,22,255,24,252,13,255,0,103,0,0,0,16,0,123,6,255,1,167,0,0,0,5,0,0,0,183,16,255,23,252,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,23,252,16,255,0,139,0,3,0,0,0,0,0,51,0,2,0,2,0,0,0,139,12,255,24,251,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,23,252,18,255,0,171,0,0,0,1,0,0,0,0,0,3,0,0,0,72,7,255,23,251,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,23,253,19,255,1,197,0,0,0,0,0,0,0,3,0,2,0,3,1,205,20,254,23,252,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,253,21,255,2,218,0,0,0,0,0,0,0,2,0,0,0,32,7,253,23,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,255,4,240,0,0,0,0,0,0,0,2,0,0,0,26,2,236,5,248,5,247,5,248,5,248,5,247,5,247,5,246,5,246,4,246,4,246,4,245,4,245,4,244,4,245,1,235};
static const uint8_t W_BISHOP[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,13,0,181,10,239,0,132,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,0,0,107,109,255,237,254,49,255,0,45,2,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,255,2,5,86,100,251,216,251,48,241,16,31,170,3,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,3,0,0,74,0,244,17,253,0,214,0,35,2,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,2,255,2,0,146,53,255,185,254,236,254,151,255,25,243,0,99,3,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,3,255,1,0,170,106,255,251,252,255,255,255,255,255,254,226,255,58,255,0,111,2,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,0,0,118,92,255,255,249,255,255,237,255,94,255,255,255,255,253,243,252,40,255,4,57,2,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,13,19,22,239,240,253,255,253,195,255,87,255,21,255,109,255,226,255,255,251,187,255,0,186,2,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,0,0,82,101,255,255,252,251,255,208,255,110,255,23,255,137,255,235,255,253,254,252,255,40,246,0,25,2,0,0,1,0,0,0,0,0,0,0,0,0,0,0,4,4,0,0,111,141,253,255,251,245,255,255,255,242,255,112,255,255,255,254,255,249,255,255,255,73,255,0,48,3,0,0,2,0,0,0,0,0,0,0,0,0,0,0,3,4,0,0,79,92,255,255,252,255,255,255,255,255,255,255,255,255,255,255,255,255,254,255,254,35,248,0,24,2,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,1,25,10,6,211,147,255,140,253,77,255,60,255,57,255,70,255,101,255,171,252,104,255,0,150,2,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,24,0,219,87,254,177,253,209,255,213,255,192,255,144,252,43,255,0,173,102,5,0,3,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,255,1,0,19,49,236,250,255,198,254,148,255,136,255,156,255,215,252,217,255,17,194,1,0,127,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,1,0,2,127,39,255,61,248,64,254,99,255,111,255,93,255,58,254,65,250,20,255,0,72,1,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,63,4,1,150,14,255,164,255,255,255,255,255,255,255,255,255,247,255,130,255,4,255,5,100,0,3,0,5,0,0,0,0,0,0,0,0,0,0,0,3,0,0,3,0,4,0,4,0,10,93,9,190,33,221,45,249,38,255,47,243,28,215,8,171,15,64,4,0,4,0,2,0,0,2,0,3,0,1,0,0,0,3,2,0,1,0,0,67,0,136,0,144,0,47,0,9,0,106,5,242,83,252,0,222,0,87,0,32,0,58,0,101,0,71,0,22,2,0,3,0,0,0,0,3,1,0,0,66,0,198,74,255,162,255,169,255,74,255,45,234,98,255,223,255,249,255,200,255,99,255,72,254,99,255,133,255,97,255,38,236,0,173,0,82,0,5,0,0,1,147,54,255,206,255,248,255,226,255,215,255,203,255,180,255,173,255,89,255,18,248,129,255,188,255,196,255,202,255,204,255,209,255,213,255,186,255,101,255,13,219,2,116,0,124,67,255,118,255,22,229,0,191,0,177,0,174,0,168,0,143,0,77,0,32,0,107,0,154,0,166,0,166,0,164,0,175,12,209,77,252,156,255,20,244,0,89,0,0,0,130,0,140,0,18,0,0,2,0,1,0,1,0,4,0,3,0,1,0,4,0,3,0,2,0,2,0,3,0,2,0,0,2,0,69,0,198,0,62,0,0};
static const uint8_t B_BISHOP[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,13,0,181,1,239,0,132,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,107,9,255,22,254,4,255,0,45,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2,0,86,9,251,20,251,4,241,0,31,0,3,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,74,0,244,1,253,0,214,0,35,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,2,0,2,0,146,5,255,16,254,21,254,13,255,2,243,0,99,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,3,0,1,0,170,9,255,22,252,23,255,22,255,23,254,20,255,5,255,0,111,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,118,8,255,23,249,24,255,20,255,8,255,24,255,23,253,21,252,4,255,0,57,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,19,2,239,21,253,23,253,17,255,7,255,2,255,9,255,20,255,24,251,16,255,0,186,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,82,9,255,24,252,22,255,19,255,9,255,2,255,12,255,20,255,22,254,22,255,3,246,0,25,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,111,12,253,24,251,21,255,23,255,21,255,10,255,24,255,22,255,22,255,23,255,6,255,0,48,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,79,8,255,24,252,24,255,23,255,23,255,24,255,23,255,24,255,24,254,22,254,3,248,0,24,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,10,0,211,13,255,12,253,7,255,5,255,5,255,6,255,9,255,15,252,9,255,0,150,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,24,0,219,7,254,16,253,18,255,19,255,17,255,13,252,4,255,0,173,0,5,0,3,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,19,4,236,22,255,17,254,13,255,12,255,14,255,19,252,19,255,1,194,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,127,3,255,5,248,6,254,9,255,10,255,8,255,5,254,5,250,2,255,0,72,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,0,4,0,150,1,255,14,255,23,255,25,255,25,255,24,255,22,255,11,255,0,255,0,100,0,3,0,5,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,93,1,190,3,221,4,249,3,255,4,243,2,215,0,171,0,64,0,0,0,0,0,0,0,2,0,3,0,1,0,0,0,3,0,0,0,0,0,67,0,136,0,144,0,47,0,9,0,106,1,242,8,252,0,222,0,87,0,32,0,58,0,101,0,71,0,22,0,0,0,0,0,0,0,3,0,0,0,66,0,198,6,255,14,255,15,255,6,255,3,234,9,255,20,255,22,255,18,255,9,255,6,254,9,255,12,255,9,255,3,236,0,173,0,82,0,5,0,0,0,147,5,255,19,255,22,255,20,255,19,255,17,255,16,255,15,255,8,255,1,248,11,255,17,255,17,255,17,255,18,255,18,255,19,255,17,255,9,255,1,219,0,116,0,124,6,255,11,255,2,229,0,191,0,177,0,174,0,168,0,143,0,77,0,32,0,107,0,154,0,166,0,166,0,164,0,175,1,209,7,252,14,255,2,244,0,89,0,0,0,130,0,140,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,69,0,198,0,62,0,0};
static const uint8_t W_ROOK[] = {0,0,0,160,33,255,43,251,17,253,32,31,204,5,13,249,43,255,42,253,43,255,13,249,204,5,31,32,17,253,43,251,33,255,0,160,0,0,1,0,0,169,192,255,252,250,68,255,0,102,0,83,44,255,248,255,245,254,248,255,44,255,0,83,0,103,68,255,252,250,192,255,0,169,1,0,1,0,0,166,207,255,255,251,170,255,109,255,109,255,156,255,255,255,255,255,255,255,156,255,109,255,109,255,170,255,255,251,207,255,0,166,1,0,1,0,0,175,171,253,206,251,210,255,220,253,220,253,215,255,202,255,202,255,202,255,215,255,220,253,220,253,210,255,206,251,171,253,0,175,1,0,0,0,1,131,2,255,64,251,71,255,66,255,67,255,67,255,70,255,70,255,70,255,67,255,67,255,66,255,70,255,64,251,2,255,1,131,0,0,0,2,255,1,1,132,77,255,235,253,249,254,249,255,250,255,250,255,250,255,250,255,250,255,249,255,249,254,235,253,77,255,1,132,255,1,0,2,0,0,0,2,1,0,0,106,19,254,55,253,53,255,54,255,54,255,54,255,54,255,54,255,53,255,55,253,19,254,0,106,1,0,0,2,0,0,0,0,0,2,255,2,20,25,56,252,239,255,238,254,240,255,240,255,240,255,240,255,240,255,238,254,239,255,55,252,21,24,255,2,0,2,0,0,0,0,0,2,3,0,0,38,64,252,255,255,255,254,255,255,255,255,255,255,255,255,255,255,255,254,255,255,63,252,0,38,3,0,0,2,0,0,0,0,0,2,3,0,0,36,62,252,254,255,252,254,254,255,254,255,254,255,254,255,254,255,252,254,254,255,61,252,0,35,3,0,0,2,0,0,0,0,0,2,3,0,0,36,62,252,255,255,253,254,255,255,255,255,255,255,255,255,255,255,253,254,255,255,61,252,0,35,3,0,0,2,0,0,0,0,0,2,3,0,0,36,62,252,255,255,253,254,255,255,255,255,255,255,255,255,255,255,253,254,255,255,61,252,0,35,3,0,0,2,0,0,0,0,0,2,3,0,0,38,62,252,254,255,252,254,254,255,254,255,254,255,254,255,254,255,252,254,254,255,61,252,0,37,3,0,0,2,0,0,0,0,0,2,255,2,8,30,64,252,255,255,255,254,255,255,255,255,255,255,255,255,255,255,255,254,255,255,63,252,8,29,255,2,0,2,0,0,0,0,0,5,1,0,0,68,8,254,60,253,59,255,60,255,60,255,60,255,60,255,60,255,59,255,60,253,8,254,0,67,1,0,0,5,0,0,0,2,1,0,14,52,35,236,201,254,225,254,224,255,226,255,226,255,226,255,226,255,226,255,224,255,225,254,201,254,35,236,14,52,1,0,0,2,0,2,10,50,0,236,137,255,202,254,198,255,200,255,200,255,200,255,200,255,200,255,200,255,200,255,198,255,203,253,137,255,0,236,10,50,0,2,1,0,3,168,69,255,91,249,77,255,79,255,79,255,79,255,79,255,79,255,79,255,79,255,79,255,79,255,77,255,91,249,69,255,3,168,1,0,0,19,0,176,181,254,235,251,231,255,234,255,234,255,234,255,234,255,234,255,234,255,234,255,234,255,234,255,231,255,235,251,181,254,0,176,0,19,8,235,48,251,56,255,58,255,58,255,58,255,58,255,58,255,58,255,58,255,58,255,58,255,58,255,58,255,58,255,58,255,56,255,48,251,8,235,39,255,243,255,247,255,245,255,246,255,246,255,246,255,246,255,246,255,246,255,246,255,246,255,246,255,246,255,246,255,245,255,247,255,243,255,39,255,6,239,34,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,36,249,34,249,6,239};
static const uint8_t B_ROOK[] = {0,0,0,160,3,255,4,251,1,253,0,31,0,5,1,249,4,255,4,253,4,255,1,249,0,5,0,32,1,253,4,251,3,255,0,160,0,0,0,0,0,169,17,255,23,250,6,255,0,102,0,83,4,255,22,255,21,254,22,255,4,255,0,83,0,103,6,255,23,250,17,255,0,169,0,0,0,0,0,166,18,255,25,251,15,255,9,255,9,255,13,255,24,255,23,255,24,255,13,255,9,255,9,255,15,255,25,251,18,255,0,166,0,0,0,0,0,175,14,253,18,251,18,255,19,253,19,253,19,255,17,255,17,255,17,255,19,255,19,253,19,253,18,255,18,251,14,253,0,175,0,0,0,0,0,131,0,255,5,251,6,255,6,255,6,255,6,255,6,255,6,255,6,255,6,255,6,255,6,255,6,255,5,251,0,255,0,131,0,0,0,2,0,1,0,132,7,255,21,253,22,254,22,255,22,255,22,255,22,255,22,255,22,255,22,255,22,254,21,253,7,255,0,132,0,1,0,2,0,0,0,2,0,0,0,106,1,254,5,253,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,253,2,254,0,106,0,0,0,2,0,0,0,0,0,2,0,2,0,25,5,252,22,255,21,254,21,255,21,255,21,255,21,255,21,255,21,254,22,255,5,252,0,24,0,2,0,2,0,0,0,0,0,2,0,0,0,38,5,252,23,255,22,254,22,255,22,255,22,255,22,255,22,255,22,254,23,255,5,252,0,38,0,0,0,2,0,0,0,0,0,2,0,0,0,36,5,252,23,255,22,254,22,255,22,255,22,255,22,255,22,255,22,254,23,255,5,252,0,35,0,0,0,2,0,0,0,0,0,2,0,0,0,36,5,252,23,255,22,254,22,255,22,255,22,255,22,255,22,255,22,254,23,255,5,252,0,35,0,0,0,2,0,0,0,0,0,2,0,0,0,36,5,252,23,255,22,254,22,255,22,255,22,255,22,255,22,255,22,254,23,255,5,252,0,35,0,0,0,2,0,0,0,0,0,2,0,0,0,38,5,252,23,255,22,254,22,255,22,255,22,255,22,255,22,255,22,254,23,255,5,252,0,37,0,0,0,2,0,0,0,0,0,2,0,2,0,30,5,252,23,255,22,254,22,255,22,255,22,255,22,255,22,255,22,254,23,255,5,252,0,29,0,2,0,2,0,0,0,0,0,5,0,0,0,68,0,254,6,253,5,255,5,255,5,255,5,255,5,255,5,255,5,255,6,253,0,254,0,67,0,0,0,5,0,0,0,2,0,0,0,52,3,236,18,254,20,254,19,255,19,255,19,255,19,255,19,255,19,255,19,255,20,254,18,254,3,236,0,52,0,0,0,2,0,2,0,50,0,236,12,255,18,254,17,255,17,255,17,255,17,255,17,255,17,255,17,255,17,255,17,255,18,253,12,255,0,236,0,50,0,2,0,0,0,168,6,255,8,249,7,255,7,255,7,255,7,255,7,255,7,255,7,255,7,255,7,255,7,255,7,255,8,249,6,255,0,168,0,0,0,19,0,176,15,254,21,251,20,255,20,255,20,255,20,255,20,255,20,255,20,255,20,255,20,255,20,255,20,255,21,251,15,254,0,176,0,19,1,235,4,251,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,5,255,4,251,1,235,3,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,21,255,3,255,0,239,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,3,249,0,239};
static const uint8_t W_QUEEN[] = {0,0,0,2,0,1,0,0,0,0,0,0,0,9,0,0,0,1,255,1,0,58,30,232,30,232,0,58,255,1,0,1,0,0,0,17,0,0,0,0,0,0,0,1,0,3,0,1,0,1,3,0,1,0,0,3,0,3,5,141,42,223,3,139,0,4,1,0,0,172,222,255,221,255,0,171,2,0,0,6,8,150,51,231,8,151,0,4,0,3,2,0,4,0,0,0,0,0,0,43,0,16,4,0,0,45,81,255,255,255,77,255,0,44,5,0,0,103,92,252,92,250,0,103,5,0,0,46,79,255,255,255,81,255,0,45,4,0,0,22,0,53,0,0,9,161,83,255,36,217,9,28,15,17,33,213,124,255,33,203,0,16,85,3,0,0,0,162,0,182,0,0,85,3,0,14,29,197,110,253,28,204,18,14,7,32,43,225,95,255,10,170,42,255,255,255,134,255,0,89,4,0,0,19,0,227,0,124,0,0,0,7,0,0,4,182,3,211,0,0,0,5,0,0,0,87,0,244,0,19,4,0,0,90,134,255,255,254,41,255,5,146,66,250,26,220,0,19,255,1,1,0,24,222,18,223,0,3,255,1,0,4,42,224,48,247,0,15,2,0,0,0,15,185,25,253,18,14,255,1,0,19,22,208,55,248,1,135,0,0,0,85,0,236,0,20,255,1,0,13,72,228,107,255,0,66,7,0,0,36,96,249,118,255,0,59,7,0,6,37,83,251,91,253,0,35,255,1,0,3,0,221,0,114,0,0,127,2,23,32,18,255,3,151,2,0,0,29,82,240,215,254,0,170,5,0,0,81,144,255,176,255,0,111,6,0,0,134,188,255,118,255,0,57,4,0,0,110,18,255,11,65,0,2,3,0,0,24,78,245,72,250,0,29,0,31,86,254,255,255,53,246,0,11,0,122,188,255,220,255,0,159,0,1,26,227,254,255,126,255,0,68,36,7,52,234,94,254,0,50,3,0,3,0,0,9,67,226,196,255,1,153,0,41,106,255,255,250,149,255,0,101,9,160,225,255,252,255,29,192,3,74,114,255,255,249,141,255,0,72,0,111,173,255,98,248,0,32,4,0,2,0,0,0,29,211,255,254,83,244,0,137,132,254,255,253,252,254,14,218,46,223,254,255,252,255,84,233,0,207,223,254,255,251,166,255,0,145,48,226,252,254,60,237,0,16,3,0,127,2,0,0,12,191,248,255,240,254,10,251,169,254,197,255,178,255,84,255,45,255,88,255,67,255,69,255,54,255,219,255,195,255,203,255,7,251,204,255,255,255,36,221,0,3,2,0,63,4,0,0,9,168,173,254,128,251,22,255,44,255,76,255,89,255,60,254,50,254,176,255,194,255,57,255,51,254,80,255,71,255,57,255,17,255,136,252,166,255,35,201,0,0,255,1,0,4,1,0,5,146,43,255,141,251,149,255,182,255,255,255,255,255,247,255,229,255,252,255,250,255,235,255,234,255,255,255,255,255,186,255,129,255,137,251,56,255,8,183,2,0,0,3,0,2,2,0,0,44,53,246,255,252,255,254,208,255,137,255,93,255,79,255,74,255,69,255,69,255,73,255,81,255,94,255,136,255,213,255,255,255,255,252,86,255,0,72,3,0,0,3,0,0,0,3,1,0,0,110,78,255,82,251,79,255,132,255,189,255,231,255,255,255,255,255,255,255,255,255,235,255,192,255,133,255,76,255,82,251,107,255,0,147,1,0,0,3,0,0,0,0,0,0,0,4,4,0,0,189,169,255,255,252,255,255,255,255,213,255,174,255,153,255,150,255,167,255,202,255,248,255,255,255,255,253,185,255,0,218,76,10,0,3,0,1,0,0,0,0,0,0,0,2,255,1,22,34,56,232,142,254,89,254,80,255,100,255,127,255,145,255,147,255,133,255,106,255,80,255,79,254,135,253,81,244,12,61,2,0,0,3,0,0,0,0,0,0,0,0,0,0,0,3,1,0,1,177,126,255,186,252,195,255,182,255,163,255,151,255,151,255,164,255,186,255,206,255,201,253,146,255,9,211,1,0,127,2,0,0,0,0,0,0,0,0,0,0,0,2,2,0,0,53,44,248,148,251,95,253,87,254,106,254,129,255,144,255,144,255,130,254,107,254,87,253,97,252,167,250,82,255,0,85,3,0,0,3,0,0,0,0,0,0,0,0,0,4,0,0,1,130,8,255,93,255,195,255,249,255,255,255,255,255,255,255,255,255,255,255,255,255,235,255,170,255,56,255,4,255,1,165,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,1,0,10,3,81,16,152,23,197,34,225,45,241,54,249,56,252,54,249,45,241,32,227,17,203,7,168,8,121,0,61,0,11,0,1,0,0,0,0,0,0};
static const uint8_t B_QUEEN[] = {0,0,0,2,0,1,0,0,0,0,0,0,0,9,0,0,0,1,0,1,0,58,2,232,2,232,0,58,0,1,0,1,0,0,0,17,0,0,0,0,0,0,0,1,0,3,0,1,0,1,0,0,0,0,0,3,0,3,0,141,3,223,0,139,0,4,0,0,0,172,19,255,19,255,0,171,0,0,0,6,0,150,4,231,0,151,0,4,0,3,0,0,0,0,0,0,0,0,0,43,0,16,0,0,0,45,7,255,26,255,7,255,0,44,0,0,0,103,8,252,8,250,0,103,0,0,0,46,7,255,26,255,7,255,0,45,0,0,0,22,0,53,0,0,0,161,7,255,3,217,0,28,0,17,2,213,11,255,2,203,0,16,0,3,0,0,0,162,0,182,0,0,0,3,0,14,2,197,10,253,2,204,0,14,0,32,3,225,8,255,1,170,4,255,25,255,12,255,0,89,0,0,0,19,0,227,0,124,0,0,0,7,0,0,0,182,0,211,0,0,0,5,0,0,0,87,0,244,0,19,0,0,0,90,11,255,25,254,4,255,0,146,6,250,2,220,0,19,0,1,0,0,2,222,1,223,0,3,0,1,0,4,3,224,4,247,0,15,0,0,0,0,1,185,2,253,0,14,0,1,0,19,2,208,5,248,0,135,0,0,0,85,0,236,0,20,0,1,0,13,6,228,9,255,0,66,1,0,0,36,8,249,10,255,0,59,1,0,0,37,7,251,8,253,0,35,0,1,0,3,0,221,0,114,0,0,0,2,0,32,1,255,0,151,0,0,0,29,7,240,18,254,0,170,0,0,0,81,12,255,15,255,0,111,1,0,0,134,16,255,10,255,0,57,0,0,0,110,2,255,0,65,0,2,0,0,0,24,6,245,6,250,0,29,0,31,7,254,24,255,4,246,0,11,0,122,16,255,19,255,0,159,0,1,2,227,23,255,11,255,0,68,0,7,4,234,8,254,0,50,0,0,0,0,0,9,5,226,17,255,0,153,0,41,9,255,25,250,13,255,0,101,0,160,19,255,22,255,2,192,0,74,10,255,25,249,12,255,0,72,0,111,15,255,8,248,0,32,0,0,0,0,0,0,2,211,22,254,7,244,0,137,11,254,25,253,22,254,1,218,4,223,22,255,22,255,7,233,0,207,19,254,25,251,14,255,0,145,4,226,23,254,5,237,0,16,0,0,0,2,0,0,1,191,21,255,21,254,1,251,15,254,17,255,16,255,7,255,4,255,7,255,6,255,6,255,5,255,19,255,17,255,17,255,1,251,18,255,24,255,3,221,0,3,0,0,0,4,0,0,0,168,15,254,11,251,2,255,4,255,7,255,8,255,5,254,4,254,16,255,17,255,5,255,4,254,7,255,6,255,5,255,2,255,12,252,14,255,2,201,0,0,0,1,0,4,0,0,0,146,4,255,12,251,13,255,16,255,25,255,25,255,21,255,20,255,22,255,22,255,20,255,20,255,24,255,25,255,16,255,11,255,13,251,5,255,1,183,0,0,0,3,0,2,0,0,0,44,4,246,24,252,24,254,18,255,12,255,8,255,7,255,6,255,6,255,6,255,6,255,7,255,8,255,12,255,19,255,24,255,26,252,7,255,0,72,0,0,0,3,0,0,0,3,0,0,0,110,7,255,7,251,7,255,12,255,17,255,20,255,22,255,23,255,23,255,23,255,21,255,17,255,12,255,7,255,7,251,9,255,0,147,0,0,0,3,0,0,0,0,0,0,0,4,0,0,0,189,15,255,26,252,25,255,22,255,18,255,15,255,13,255,13,255,14,255,17,255,21,255,25,255,26,253,16,255,0,218,0,10,0,3,0,1,0,0,0,0,0,0,0,2,0,1,0,34,4,232,13,254,8,254,7,255,9,255,11,255,13,255,13,255,12,255,9,255,7,255,7,254,12,253,7,244,0,61,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,177,11,255,17,252,17,255,16,255,14,255,13,255,13,255,14,255,16,255,18,255,18,253,13,255,1,211,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,53,4,248,13,251,8,253,8,254,9,254,11,255,13,255,13,255,11,254,9,254,8,253,8,252,15,250,7,255,0,85,0,0,0,3,0,0,0,0,0,0,0,0,0,4,0,0,0,130,1,255,8,255,17,255,22,255,24,255,24,255,24,255,24,255,24,255,23,255,21,255,15,255,5,255,0,255,0,165,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,1,0,10,0,81,1,152,2,197,3,225,4,241,5,249,5,252,5,249,4,241,3,227,1,203,0,168,0,121,0,61,0,11,0,1,0,0,0,0,0,0};
static const uint8_t W_KING[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,106,0,106,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,35,0,148,0,199,0,198,0,147,0,35,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,23,3,84,6,157,6,158,0,85,0,22,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,3,0,3,0,1,0,0,0,1,0,0,0,0,0,147,0,142,0,0,0,0,0,1,0,0,0,2,0,3,0,3,0,1,0,0,0,0,0,1,0,2,3,0,4,0,4,0,3,0,0,3,85,3,31,8,10,195,122,255,104,255,0,149,1,0,0,5,255,1,3,0,4,0,4,0,2,0,0,3,0,1,0,0,1,0,0,22,0,52,0,54,0,22,1,0,6,0,0,74,108,255,255,251,255,254,54,248,0,26,5,0,0,0,0,35,0,64,0,55,0,19,1,0,0,0,0,19,0,161,58,239,99,255,101,255,58,239,0,167,0,26,2,86,151,253,255,251,253,254,94,255,15,33,0,48,5,189,73,249,113,255,101,255,52,236,0,151,0,15,5,183,161,255,255,255,255,254,255,254,255,255,186,255,39,224,0,137,133,253,255,253,254,255,93,244,0,124,58,245,208,255,255,255,255,253,255,254,255,255,152,255,5,177,50,255,255,252,255,254,251,255,251,255,253,254,255,252,227,255,20,255,72,254,255,255,255,255,41,251,40,255,243,255,255,252,251,255,251,255,251,255,255,254,255,252,48,254,45,245,249,255,255,255,255,255,255,255,255,255,249,255,255,253,185,255,8,255,246,255,233,255,8,255,210,254,255,254,250,255,255,255,255,255,255,255,254,255,246,255,39,240,5,193,210,254,255,253,253,255,255,255,255,255,255,255,251,255,255,255,64,255,162,255,156,255,86,255,255,255,251,255,255,255,255,255,255,255,252,255,255,252,194,255,0,176,0,88,108,255,255,253,251,255,255,255,255,255,255,255,251,255,253,255,189,255,31,255,41,255,203,255,253,255,252,255,255,255,255,255,255,255,252,255,255,253,79,254,0,62,127,4,12,203,221,255,255,252,250,255,251,255,254,255,255,255,255,255,255,255,33,255,45,255,255,255,255,255,255,255,253,255,251,255,249,255,255,252,191,255,0,171,2,0,3,0,0,59,70,255,255,252,255,254,255,255,255,255,243,255,220,255,209,255,59,255,67,255,211,255,224,255,247,255,255,255,255,255,255,254,249,254,42,244,7,32,255,1,0,3,3,0,0,138,130,255,165,252,102,255,74,255,66,255,70,255,76,255,81,255,81,255,75,255,68,255,66,255,77,255,106,255,168,252,111,255,0,109,3,0,0,4,0,1,127,2,170,3,0,214,103,255,174,253,227,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,220,255,168,253,100,255,0,211,2,0,0,2,0,0,0,0,127,2,0,0,22,192,255,255,247,252,192,255,150,255,120,255,103,255,95,255,96,255,106,255,125,255,157,255,200,255,252,252,255,255,22,192,0,0,127,2,0,0,0,0,0,2,0,1,9,196,61,255,68,252,98,255,136,255,166,255,186,255,196,255,194,255,182,255,160,255,129,255,93,255,66,252,63,255,9,196,0,0,0,2,0,0,0,0,127,2,0,0,13,192,212,253,255,252,237,255,212,255,186,255,168,255,159,255,160,255,171,255,192,255,218,255,241,255,255,252,210,253,13,192,0,0,127,2,0,0,0,0,127,2,0,0,13,209,111,255,88,253,87,252,96,252,109,254,121,254,128,255,126,255,117,254,104,254,90,253,85,252,93,252,115,255,13,209,0,0,127,2,0,0,0,0,0,2,0,1,0,100,0,207,56,252,155,255,217,255,252,255,255,255,255,255,255,255,255,255,255,255,222,255,161,255,59,254,0,211,0,101,0,1,0,2,0,0,0,0,0,0,0,0,0,0,255,2,0,55,0,119,0,174,15,213,35,236,46,247,48,248,39,240,22,220,0,184,0,129,0,63,204,5,0,0,0,0,0,0,0,0};
static const uint8_t B_KING[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,106,0,106,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,35,0,148,0,199,0,198,0,147,0,35,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,23,0,84,0,157,0,158,0,85,0,22,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,3,0,3,0,1,0,0,0,1,0,0,0,0,0,147,0,142,0,0,0,0,0,1,0,0,0,2,0,3,0,3,0,1,0,0,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,3,0,3,0,8,1,195,11,255,9,255,0,149,0,0,0,5,0,1,0,0,0,0,0,0,0,0,0,3,0,1,0,0,0,0,0,22,0,52,0,54,0,22,0,0,0,0,0,74,9,255,25,251,23,254,5,248,0,26,0,0,0,0,0,35,0,64,0,55,0,19,0,0,0,0,0,19,0,161,5,239,9,255,9,255,5,239,0,167,0,26,0,86,13,253,23,251,23,254,8,255,0,33,0,48,0,189,6,249,10,255,9,255,4,236,0,151,0,15,0,183,14,255,23,255,24,254,24,254,23,255,16,255,3,224,0,137,11,253,23,253,23,255,8,244,0,124,5,245,19,255,23,255,24,253,24,254,23,255,13,255,0,177,4,255,23,252,22,254,22,255,22,255,22,254,24,252,20,255,2,255,6,254,23,255,22,255,4,251,3,255,22,255,23,252,22,255,22,255,22,255,22,254,23,252,4,254,4,245,22,255,22,255,22,255,22,255,22,255,22,255,24,253,16,255,1,255,21,255,20,255,1,255,18,254,23,254,22,255,22,255,22,255,22,255,22,255,21,255,3,240,0,193,18,254,23,253,22,255,22,255,22,255,22,255,22,255,24,255,6,255,14,255,14,255,7,255,24,255,22,255,22,255,22,255,22,255,22,255,23,252,17,255,0,176,0,88,9,255,24,253,22,255,22,255,22,255,22,255,22,255,23,255,16,255,3,255,4,255,17,255,23,255,22,255,22,255,22,255,22,255,22,255,24,253,7,254,0,62,0,4,1,203,19,255,23,252,22,255,22,255,22,255,22,255,23,255,24,255,3,255,4,255,24,255,22,255,22,255,22,255,22,255,22,255,23,252,16,255,0,171,0,0,0,0,0,59,6,255,23,252,24,254,24,255,23,255,21,255,19,255,20,255,5,255,6,255,20,255,20,255,21,255,23,255,24,255,24,254,22,254,4,244,0,32,0,1,0,3,0,0,0,138,11,255,15,252,9,255,6,255,6,255,6,255,7,255,7,255,7,255,6,255,6,255,6,255,7,255,9,255,15,252,9,255,0,109,0,0,0,4,0,1,0,2,0,3,0,214,9,255,15,253,20,255,23,255,24,255,25,255,25,255,25,255,25,255,24,255,22,255,19,255,15,253,8,255,0,211,0,0,0,2,0,0,0,0,0,2,0,0,1,192,22,255,21,252,17,255,13,255,10,255,9,255,8,255,8,255,9,255,11,255,14,255,17,255,22,252,22,255,1,192,0,0,0,2,0,0,0,0,0,2,0,1,1,196,5,255,6,252,9,255,12,255,14,255,16,255,17,255,17,255,16,255,14,255,11,255,8,255,6,252,5,255,1,196,0,0,0,2,0,0,0,0,0,2,0,0,1,192,18,253,22,252,21,255,18,255,16,255,14,255,14,255,14,255,15,255,17,255,19,255,21,255,22,252,18,253,1,192,0,0,0,2,0,0,0,0,0,2,0,0,1,209,10,255,8,253,8,252,8,252,10,254,11,254,11,255,11,255,10,254,9,254,8,253,7,252,8,252,10,255,1,209,0,0,0,2,0,0,0,0,0,2,0,1,0,100,0,207,5,252,14,255,19,255,22,255,23,255,24,255,24,255,23,255,22,255,20,255,14,255,5,254,0,211,0,101,0,1,0,2,0,0,0,0,0,0,0,0,0,0,0,2,0,55,0,119,0,174,1,213,3,236,4,247,4,248,3,240,2,220,0,184,0,129,0,63,0,5,0,0,0,0,0,0,0,0};
static const Glyph glyph[2][7] = {
  { {0,0,0}, {17,22,W_PAWN}, {22,22,W_KNIGHT}, {22,22,W_BISHOP}, {19,22,W_ROOK}, {24,22,W_QUEEN}, {22,22,W_KING} },
  { {0,0,0}, {17,22,B_PAWN}, {22,22,B_KNIGHT}, {22,22,B_BISHOP}, {19,22,B_ROOK}, {24,22,B_QUEEN}, {22,22,B_KING} },
};
}

namespace Chess {
#define CH_STATE   0xC4
#define CH_RESTART 0xC5

// --- Brett-Geometrie ---
#define CH_CELL 26
#define CH_OX   16
#define CH_OY   36
#define CH_BW   (8 * CH_CELL)   // 208
#define CH_BH   (8 * CH_CELL)   // 208

// --- Eingabe-Bitmaske (eigene Praefixe gegen Makro-Kollisionen) ---
#define CHI_UP    0x01
#define CHI_DOWN  0x02
#define CHI_LEFT  0x04
#define CHI_RIGHT 0x08
#define CHI_AB    0x10

// --- Figuren-Kodierung: Typ in Bit0..2, Schwarz = Bit3 (0x08) ---
enum { PT_NONE = 0, PT_PAWN = 1, PT_KNIGHT = 2, PT_BISHOP = 3,
       PT_ROOK = 4, PT_QUEEN = 5, PT_KING = 6 };
#define CH_BLACK 0x08

// --- Farben ---
static const uint16_t C_LIGHT   = 0xFE73;   // helles Feld (Creme, Wikipedia)
static const uint16_t C_DARK    = 0xD448;   // dunkles Feld (Orange, Wikipedia)
static const uint16_t C_WFILL   = 0xFFFF;   // weisse Figur
static const uint16_t C_WLINE   = 0x4208;   // Kontur weiss
static const uint16_t C_BFILL   = 0x18C3;   // schwarze Figur
static const uint16_t C_BLINE   = 0xC618;   // Kontur schwarz
static const uint16_t C_CURSOR  = 0xFFE0;   // Cursor (gelb)
static const uint16_t C_SEL     = 0x07E0;   // Auswahl (gruen)
static const uint16_t C_DOT     = 0x05FF;   // Zugziel (cyan)
static const uint16_t C_CHK     = 0xF800;   // Schach (rot)
static const uint16_t C_LAST    = 0xAD55;   // letzter Zug (oliv)

// --- Zustand ---
PERSIST uint8_t  board[8][8];  // PERSIST: ueberlebt einen Absturz
PERSIST uint8_t  currentPlayer;        // 1=Weiss, 2=Schwarz
PERSIST uint8_t  status;               // 0=laeuft, 1=Weiss, 2=Schwarz, 3=Patt/Remis
PERSIST uint8_t  inCheck;              // 0=keiner, sonst Farbe der ziehenden Seite
PERSIST uint8_t  phase;                // 0=normal, 1=Umwandlung
PERSIST uint8_t  promoSel;             // 0..3 -> Dame,Turm,Laeufer,Springer
PERSIST int8_t   curBx, curBy;         // Cursor
PERSIST int8_t   selBx, selBy;         // ausgewaehltes Feld (-1 = keins)
PERSIST int8_t   promoBx, promoBy;     // Umwandlungsfeld
PERSIST int8_t   epBx, epBy;           // En-passant-Zielfeld (-1 = keins)
PERSIST int8_t   lastFromBx, lastFromBy, lastToBx, lastToBy;
PERSIST bool     cWK, cWQ, cBK, cBQ;   // Rochaderechte
uint8_t  hostPrevMask, guestPrevMask;

// --- kleine Helfer ---
inline bool inside(int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8; }
inline uint8_t ptype(uint8_t p)  { return p & 7; }
inline bool isBlackP(uint8_t p)  { return (p & CH_BLACK) != 0; }
inline uint8_t colorOf(uint8_t p) { return p == 0 ? 0 : (isBlackP(p) ? 2 : 1); }
inline uint8_t mk(uint8_t type, bool black) { return type | (black ? CH_BLACK : 0); }
inline bool gFlip() { return myRole == ROLE_GUEST; }

uint8_t readMask() {
  uint8_t m = 0;
  if (digitalRead(KEY_UP)    == LOW) m |= CHI_UP;
  if (digitalRead(KEY_DOWN)  == LOW) m |= CHI_DOWN;
  if (digitalRead(KEY_LEFT)  == LOW) m |= CHI_LEFT;
  if (digitalRead(KEY_RIGHT) == LOW) m |= CHI_RIGHT;
  if (aOrB())                        m |= CHI_AB;
  return m;
}

// ---------- Schach-Logik ----------
// Wird ein Feld von einer Figur der Farbe byCol angegriffen?
bool sliderHit(uint8_t bd[8][8], int x, int y, int dx, int dy, int tx, int ty) {
  int nx = x + dx, ny = y + dy;
  while (inside(nx, ny)) {
    if (nx == tx && ny == ty) return true;
    if (bd[ny][nx] != 0) return false;
    nx += dx; ny += dy;
  }
  return false;
}
bool isAttacked(uint8_t bd[8][8], int tx, int ty, uint8_t byCol) {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint8_t p = bd[y][x];
      if (p == 0 || colorOf(p) != byCol) continue;
      uint8_t t = ptype(p);
      if (t == PT_PAWN) {
        int dir = (byCol == 1) ? -1 : 1;   // Weiss schlaegt nach oben
        if (y + dir == ty && (x - 1 == tx || x + 1 == tx)) return true;
      } else if (t == PT_KNIGHT) {
        int dxs[8] = { 1, 2, 2, 1, -1, -2, -2, -1 };
        int dys[8] = { -2, -1, 1, 2, 2, 1, -1, -2 };
        for (int k = 0; k < 8; k++)
          if (x + dxs[k] == tx && y + dys[k] == ty) return true;
      } else if (t == PT_KING) {
        if (abs(x - tx) <= 1 && abs(y - ty) <= 1 && !(x == tx && y == ty)) return true;
      } else {
        bool diag = (t == PT_BISHOP || t == PT_QUEEN);
        bool orth = (t == PT_ROOK   || t == PT_QUEEN);
        if (orth) {
          if (sliderHit(bd, x, y, 1, 0, tx, ty)) return true;
          if (sliderHit(bd, x, y, -1, 0, tx, ty)) return true;
          if (sliderHit(bd, x, y, 0, 1, tx, ty)) return true;
          if (sliderHit(bd, x, y, 0, -1, tx, ty)) return true;
        }
        if (diag) {
          if (sliderHit(bd, x, y, 1, 1, tx, ty)) return true;
          if (sliderHit(bd, x, y, 1, -1, tx, ty)) return true;
          if (sliderHit(bd, x, y, -1, 1, tx, ty)) return true;
          if (sliderHit(bd, x, y, -1, -1, tx, ty)) return true;
        }
      }
    }
  }
  return false;
}
bool findKing(uint8_t bd[8][8], uint8_t col, int& kx, int& ky) {
  uint8_t target = mk(PT_KING, col == 2);
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      if (bd[y][x] == target) { kx = x; ky = y; return true; }
  return false;
}
bool kingInCheck(uint8_t bd[8][8], uint8_t col) {
  int kx, ky;
  if (!findKing(bd, col, kx, ky)) return false;
  return isAttacked(bd, kx, ky, col == 1 ? 2 : 1);
}

// Pseudolegale Ziele (ohne Koenigssicherheit) fuer das Feld (fx,fy).
void genPseudo(int fx, int fy, bool out[8][8]) {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) out[y][x] = false;
  uint8_t p = board[fy][fx];
  if (p == 0) return;
  uint8_t col = colorOf(p), t = ptype(p), opp = (col == 1) ? 2 : 1;

  if (t == PT_PAWN) {
    int dir = (col == 1) ? -1 : 1;
    int startRank = (col == 1) ? 6 : 1;
    if (inside(fx, fy + dir) && board[fy + dir][fx] == 0) {
      out[fy + dir][fx] = true;
      if (fy == startRank && board[fy + 2 * dir][fx] == 0)
        out[fy + 2 * dir][fx] = true;
    }
    for (int s = -1; s <= 1; s += 2) {
      int nx = fx + s, ny = fy + dir;
      if (!inside(nx, ny)) continue;
      if (board[ny][nx] != 0 && colorOf(board[ny][nx]) == opp) out[ny][nx] = true;
      if (epBx == nx && epBy == ny) out[ny][nx] = true;   // En passant
    }
  } else if (t == PT_KNIGHT) {
    int dxs[8] = { 1, 2, 2, 1, -1, -2, -2, -1 };
    int dys[8] = { -2, -1, 1, 2, 2, 1, -1, -2 };
    for (int k = 0; k < 8; k++) {
      int nx = fx + dxs[k], ny = fy + dys[k];
      if (inside(nx, ny) && colorOf(board[ny][nx]) != col) out[ny][nx] = true;
    }
  } else if (t == PT_KING) {
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        int nx = fx + dx, ny = fy + dy;
        if (inside(nx, ny) && colorOf(board[ny][nx]) != col) out[ny][nx] = true;
      }
    // Rochade
    int hr = (col == 1) ? 7 : 0;
    bool right_K = (col == 1) ? cWK : cBK;
    bool right_Q = (col == 1) ? cWQ : cBQ;
    if (fx == 4 && fy == hr && !isAttacked(board, 4, hr, opp)) {
      if (right_K && board[hr][5] == 0 && board[hr][6] == 0 &&
          !isAttacked(board, 5, hr, opp) && !isAttacked(board, 6, hr, opp))
        out[hr][6] = true;
      if (right_Q && board[hr][1] == 0 && board[hr][2] == 0 && board[hr][3] == 0 &&
          !isAttacked(board, 3, hr, opp) && !isAttacked(board, 2, hr, opp))
        out[hr][2] = true;
    }
  } else {
    bool diag = (t == PT_BISHOP || t == PT_QUEEN);
    bool orth = (t == PT_ROOK   || t == PT_QUEEN);
    int dirs[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
    int from = orth ? 0 : 4;
    int to   = diag ? 8 : 4;
    for (int d = from; d < to; d++) {
      int nx = fx + dirs[d][0], ny = fy + dirs[d][1];
      while (inside(nx, ny)) {
        uint8_t q = board[ny][nx];
        if (q == 0) { out[ny][nx] = true; }
        else { if (colorOf(q) != col) out[ny][nx] = true; break; }
        nx += dirs[d][0]; ny += dirs[d][1];
      }
    }
  }
}

// Laesst der Zug (fx,fy)->(tx,ty) den eigenen Koenig im Schach? (Simulation)
bool movesIntoCheck(int fx, int fy, int tx, int ty, uint8_t col) {
  uint8_t tmp[8][8];
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) tmp[y][x] = board[y][x];
  uint8_t p = tmp[fy][fx];
  if (ptype(p) == PT_PAWN && tx != fx && tmp[ty][tx] == 0) tmp[fy][tx] = 0; // En passant
  if (ptype(p) == PT_KING && abs(tx - fx) == 2) {                          // Rochade
    if (tx > fx) { tmp[ty][tx - 1] = tmp[fy][7]; tmp[fy][7] = 0; }
    else         { tmp[ty][tx + 1] = tmp[fy][0]; tmp[fy][0] = 0; }
  }
  tmp[ty][tx] = p; tmp[fy][fx] = 0;
  return kingInCheck(tmp, col);
}

// Legale Ziele fuer (fx,fy). Fuellt mask, liefert Anzahl.
int legalFrom(int fx, int fy, bool mask[8][8]) {
  bool ps[8][8];
  genPseudo(fx, fy, ps);
  uint8_t col = colorOf(board[fy][fx]);
  int cnt = 0;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      mask[y][x] = false;
      if (ps[y][x] && !movesIntoCheck(fx, fy, x, y, col)) { mask[y][x] = true; cnt++; }
    }
  return cnt;
}

bool hasAnyLegal(uint8_t col) {
  bool m[8][8];
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      if (board[y][x] != 0 && colorOf(board[y][x]) == col)
        if (legalFrom(x, y, m) > 0) return true;
  return false;
}

void updateCastleRights(int fx, int fy, int tx, int ty) {
  if (fx == 4 && fy == 7) { cWK = cWQ = false; }
  if (fx == 4 && fy == 0) { cBK = cBQ = false; }
  if ((fx == 0 && fy == 7) || (tx == 0 && ty == 7)) cWQ = false;
  if ((fx == 7 && fy == 7) || (tx == 7 && ty == 7)) cWK = false;
  if ((fx == 0 && fy == 0) || (tx == 0 && ty == 0)) cBQ = false;
  if ((fx == 7 && fy == 0) || (tx == 7 && ty == 0)) cBK = false;
}

void finalizeTurn() {
  currentPlayer = (currentPlayer == 1) ? 2 : 1;
  bool chk = kingInCheck(board, currentPlayer);
  inCheck = chk ? currentPlayer : 0;
  bool any = hasAnyLegal(currentPlayer);
  if (!any) {
    if (chk) { status = (currentPlayer == 1) ? 2 : 1;     // matt: Gegner gewinnt
              beep(1500, 220); linkDelay(240); beep(1900, 260); }
    else     { status = 3; beep(400, 350); }              // Patt
  } else {
    status = 0;
    if (chk) { beep(1300, 120); }                          // Schach
  }
  selBx = -1;
  phase = 0;
}

void confirmPromo() {
  const uint8_t order[4] = { PT_QUEEN, PT_ROOK, PT_BISHOP, PT_KNIGHT };
  bool blk = isBlackP(board[promoBy][promoBx]);
  board[promoBy][promoBx] = mk(order[promoSel], blk);
  phase = 0;
  beep(1100, 80);
  finalizeTurn();
}

void executeMove(int fx, int fy, int tx, int ty) {
  uint8_t p = board[fy][fx];
  uint8_t col = colorOf(p);
  bool isPawn = ptype(p) == PT_PAWN;
  bool cap = board[ty][tx] != 0;

  if (isPawn && tx != fx && board[ty][tx] == 0) { board[fy][tx] = 0; cap = true; } // En passant
  if (ptype(p) == PT_KING && abs(tx - fx) == 2) {                                  // Rochade
    if (tx > fx) { board[ty][tx - 1] = board[fy][7]; board[fy][7] = 0; }
    else         { board[ty][tx + 1] = board[fy][0]; board[fy][0] = 0; }
    beep(700, 50); linkDelay(60); beep(900, 60);
  }

  int8_t newEpX = -1, newEpY = -1;
  if (isPawn && abs(ty - fy) == 2) { newEpX = tx; newEpY = (fy + ty) / 2; }

  board[ty][tx] = p; board[fy][fx] = 0;
  updateCastleRights(fx, fy, tx, ty);
  epBx = newEpX; epBy = newEpY;
  lastFromBx = fx; lastFromBy = fy; lastToBx = tx; lastToBy = ty;
  beep(cap ? 950 : 680, 60);

  bool reachesLast = isPawn && (col == 1 ? ty == 0 : ty == 7);
  if (reachesLast) {
    phase = 1; promoBx = tx; promoBy = ty; promoSel = 0; selBx = -1;
    beep(1200, 90);
  } else {
    finalizeTurn();
  }
}

void initMatch() {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) board[y][x] = 0;
  const uint8_t backRank[8] = { PT_ROOK, PT_KNIGHT, PT_BISHOP, PT_QUEEN,
                                PT_KING, PT_BISHOP, PT_KNIGHT, PT_ROOK };
  for (int x = 0; x < 8; x++) {
    board[0][x] = mk(backRank[x], true);    // Schwarz oben
    board[1][x] = mk(PT_PAWN, true);
    board[6][x] = mk(PT_PAWN, false);
    board[7][x] = mk(backRank[x], false);   // Weiss unten
  }
  currentPlayer = 1; status = 0; inCheck = 0; phase = 0; promoSel = 0;
  curBx = 4; curBy = 6; selBx = -1; selBy = -1;
  promoBx = -1; promoBy = -1; epBx = -1; epBy = -1;
  lastFromBx = lastFromBy = lastToBx = lastToBy = -1;
  cWK = cWQ = cBK = cBQ = true;
  hostPrevMask = 0; guestPrevMask = 0;
}

// ---------- Eingabe-Verarbeitung (nur Host) ----------
void handleInput(uint8_t edges, uint8_t player) {
  if (status != 0) return;

  if (phase == 1) {                          // Umwandlungs-Menue (bildschirmfest, keine Spiegelung)
    if ((edges & CHI_LEFT)  && promoSel > 0) { promoSel--; beep(500, 15); }
    if ((edges & CHI_RIGHT) && promoSel < 3) { promoSel++; beep(500, 15); }
    if (edges & CHI_AB) confirmPromo();
    return;
  }

  // Richtung je nach Spielerfarbe (Schwarz sieht das Brett gedreht)
  bool white = (player == 1);
  int dx = 0, dy = 0;
  if (edges & CHI_UP)    dy += white ? -1 : 1;
  if (edges & CHI_DOWN)  dy += white ? 1 : -1;
  if (edges & CHI_LEFT)  dx += white ? -1 : 1;
  if (edges & CHI_RIGHT) dx += white ? 1 : -1;
  if (dx || dy) {
    int nx = curBx + dx, ny = curBy + dy;
    if (nx < 0) nx = 0; if (nx > 7) nx = 7;
    if (ny < 0) ny = 0; if (ny > 7) ny = 7;
    if (nx != curBx || ny != curBy) { curBx = nx; curBy = ny; beep(500, 12); }
  }

  if (edges & CHI_AB) {
    uint8_t cp = board[curBy][curBx];
    if (selBx < 0) {
      if (cp != 0 && colorOf(cp) == player) { selBx = curBx; selBy = curBy; beep(820, 30); }
      else beep(250, 60);
    } else {
      if (curBx == selBx && curBy == selBy) { selBx = -1; beep(420, 25); }
      else {
        bool m[8][8];
        legalFrom(selBx, selBy, m);
        if (m[curBy][curBx]) executeMove(selBx, selBy, curBx, curBy);
        else if (cp != 0 && colorOf(cp) == player) { selBx = curBx; selBy = curBy; beep(820, 30); }
        else beep(250, 60);
      }
    }
  }
}

// ---------- Zeichnen ----------
void cellXY(int bx, int by, int& x, int& y) {
  int sx = gFlip() ? 7 - bx : bx;
  int sy = gFlip() ? 7 - by : by;
  x = CH_OX + sx * CH_CELL;
  y = CH_OY + sy * CH_CELL;
}
void sqBorder(int bx, int by, uint16_t c, int th) {
  int x, y; cellXY(bx, by, x, y);
  for (int i = 0; i < th; i++)
    canvas.drawRect(x + i, y + i, CH_CELL - 2 * i, CH_CELL - 2 * i, c);
}

void drawGlyph(int px, int py, uint8_t type, bool black) {
  const ChessPix::Glyph& g = ChessPix::glyph[black ? 1 : 0][type];
  const uint8_t* d = g.data;              // je Pixel: gray, alpha
  int i = 0;
  for (int yy = 0; yy < g.h; yy++)
    for (int xx = 0; xx < g.w; xx++, i += 2) {
      uint8_t gray = d[i], a = d[i + 1];
      if (a == 0) continue;
      int X = px + xx, Y = py + yy;
      if (X < 0 || X >= DISPLAY_WIDTH || Y < 0 || Y >= DISPLAY_HEIGHT) continue;
      uint16_t fg = ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
      if (a >= 248) { canvas.drawPixel(X, Y, fg); continue; }
      uint16_t bg = canvas.getPixel(X, Y);
      uint16_t fr = (fg >> 11) & 0x1F, fgg = (fg >> 5) & 0x3F, fb = fg & 0x1F;
      uint16_t br = (bg >> 11) & 0x1F, bgg = (bg >> 5) & 0x3F, bb = bg & 0x1F;
      uint16_t rr = (fr * a + br * (255 - a)) / 255;
      uint16_t rg = (fgg * a + bgg * (255 - a)) / 255;
      uint16_t rb = (fb * a + bb * (255 - a)) / 255;
      canvas.drawPixel(X, Y, (uint16_t)((rr << 11) | (rg << 5) | rb));
    }
}
// zentriert (Umwandlungsmenue)
void drawPiece(int cx, int cy, uint8_t type, bool black) {
  const ChessPix::Glyph& g = ChessPix::glyph[black ? 1 : 0][type];
  drawGlyph(cx - g.w / 2, cy - g.h / 2, type, black);
}
// auf einem Brettfeld: alle Figuren stehen auf gemeinsamer Standlinie
void drawPieceCell(int cellx, int celly, uint8_t type, bool black) {
  const ChessPix::Glyph& g = ChessPix::glyph[black ? 1 : 0][type];
  int px = cellx + (CH_CELL - g.w) / 2;
  int py = celly + (CH_CELL - 1 - 3) - (g.h - 1);
  drawGlyph(px, py, type, black);
}
void drawHud() {
  // No "to move" text - the active player is shown by the white frame.
  if (status == 0) {
    if (inCheck) {
      canvas.setFont(&FreeSansBold9pt7b);
      drawCenteredText("CHECK!", 0, DISPLAY_WIDTH, 24, C_CHK);
    }
    return;
  }
  // Game over: Titel + Ergebnis oben stapeln (Hinweis steht allein unten).
  canvas.setFont(&FreeSansBold12pt7b);
  drawCenteredText(status == 3 ? "STALEMATE" : "CHECKMATE",
                   0, DISPLAY_WIDTH, 17, COL_TEXT);
  canvas.setFont(&FreeSansBold9pt7b);
  const char* sub = (status == 3) ? "DRAW" : (status == 1) ? "WHITE WINS" : "BLACK WINS";
  uint16_t sc = (status == 3) ? COL_DIM : (status == 1) ? COL_P1 : COL_P2;
  drawCenteredText(sub, 0, DISPLAY_WIDTH, 33, sc);
}

void drawPromoPanel() {
  int pw = 204, ph = 84, px = (DISPLAY_WIDTH - pw) / 2, py = 104;
  canvas.fillRoundRect(px, py, pw, ph, 6, 0x2104);
  canvas.drawRoundRect(px, py, pw, ph, 6, COL_TEXT);
  canvas.setFont(&FreeSans9pt7b);
  drawCenteredText("PROMOTION", px, pw, py + 18, COL_TEXT);
  bool blk = isBlackP(board[promoBy][promoBx]);
  const uint8_t order[4] = { PT_QUEEN, PT_ROOK, PT_BISHOP, PT_KNIGHT };
  for (int i = 0; i < 4; i++) {
    int cx = px + 28 + i * 50;
    int cy = py + 52;
    canvas.fillRoundRect(cx - 18, cy - 18, 36, 36, 4, C_LIGHT);
    drawPiece(cx, cy, order[i], blk);
    if (i == promoSel) canvas.drawRoundRect(cx - 18, cy - 18, 36, 36, 4, C_CURSOR);
  }
}

void drawScene() {
  canvas.fillScreen(COL_BG);
  drawHud();

  // Ist der Spieler an diesem Geraet am Zug? (Host=Weiss, Gast=Schwarz)
  bool amActive = (myRole == ROLE_HOST  && currentPlayer == 1) ||
                  (myRole == ROLE_GUEST && currentPlayer == 2);

  // Felder
  for (int by = 0; by < 8; by++)
    for (int bx = 0; bx < 8; bx++) {
      int x, y; cellXY(bx, by, x, y);
      uint16_t c = ((bx + by) & 1) ? C_DARK : C_LIGHT;
      canvas.fillRect(x, y, CH_CELL, CH_CELL, c);
    }

  // Weisser Rahmen um das Brett, nur beim Spieler der am Zug ist.
  // 2 px Abstand zum Brett, 2 px Rahmenbreite.
  if (amActive && status == 0) {
    canvas.drawRect(CH_OX - 4, CH_OY - 4, CH_BW + 8, CH_BH + 8, COL_TEXT);
    canvas.drawRect(CH_OX - 3, CH_OY - 3, CH_BW + 6, CH_BH + 6, COL_TEXT);
  }

  // letzter Zug
  if (lastFromBx >= 0) {
    sqBorder(lastFromBx, lastFromBy, C_LAST, 1);
    sqBorder(lastToBx,   lastToBy,   C_LAST, 1);
  }
  // Schach: Koenig der ziehenden Seite markieren (auf beiden Geraeten)
  if (status == 0 && inCheck) {
    int kx, ky;
    if (findKing(board, currentPlayer, kx, ky)) sqBorder(kx, ky, C_CHK, 2);
  }
  // Auswahl + legale Ziele - nur fuer den aktiven Spieler sichtbar
  bool mask[8][8];
  bool showSel = (selBx >= 0) && amActive;
  if (showSel) {
    legalFrom(selBx, selBy, mask);
    sqBorder(selBx, selBy, C_SEL, 2);
  }

  // Figuren
  for (int by = 0; by < 8; by++)
    for (int bx = 0; bx < 8; bx++) {
      uint8_t p = board[by][bx];
      if (p == 0) continue;
      int x, y; cellXY(bx, by, x, y);
      drawPieceCell(x, y, ptype(p), isBlackP(p));
    }

  // Zugziel-Markierungen ueber den Figuren
  if (showSel) {
    for (int by = 0; by < 8; by++)
      for (int bx = 0; bx < 8; bx++) {
        if (!mask[by][bx]) continue;
        int x, y; cellXY(bx, by, x, y);
        int mx = x + CH_CELL / 2, my = y + CH_CELL / 2;
        if (board[by][bx] != 0 || (bx == epBx && by == epBy)) {
          canvas.drawCircle(mx, my, CH_CELL / 2 - 3, C_DOT);
          canvas.drawCircle(mx, my, CH_CELL / 2 - 4, C_DOT);
        } else {
          canvas.fillCircle(mx, my, 4, C_DOT);
        }
      }
  }

  // Cursor - nur beim aktiven Spieler
  if (amActive && status == 0 && phase == 0) sqBorder(curBx, curBy, C_CURSOR, 2);

  // Overlays / Hinweise
  if (phase == 1) {
    drawPromoPanel();
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("L/R select - A=OK", 0, DISPLAY_WIDTH, DISPLAY_HEIGHT - 8, COL_DIM);
  } else if (status != 0) {
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Press to restart", 0, DISPLAY_WIDTH, DISPLAY_HEIGHT - 8, COL_DIM);
  }

  pushCanvas();
}

// ---------- Sync ----------
void sendState() {
  Link.beginTransmission(I2C_ADDR);
  Link.write(CH_STATE);
  Link.write(currentPlayer);
  Link.write(status);
  Link.write(inCheck);
  Link.write(phase);
  Link.write(promoSel);
  Link.write((uint8_t)curBx);
  Link.write((uint8_t)curBy);
  Link.write((uint8_t)(selBx < 0 ? 255 : selBx));
  Link.write((uint8_t)(selBy < 0 ? 255 : selBy));
  Link.write((uint8_t)(promoBx < 0 ? 255 : promoBx));
  Link.write((uint8_t)(promoBy < 0 ? 255 : promoBy));
  Link.write((uint8_t)(epBx < 0 ? 255 : epBx));
  Link.write((uint8_t)(epBy < 0 ? 255 : epBy));
  uint8_t cr = (cWK ? 1 : 0) | (cWQ ? 2 : 0) | (cBK ? 4 : 0) | (cBQ ? 8 : 0);
  Link.write(cr);
  Link.write((uint8_t)(lastFromBx < 0 ? 255 : lastFromBx));
  Link.write((uint8_t)(lastFromBy < 0 ? 255 : lastFromBy));
  Link.write((uint8_t)(lastToBx < 0 ? 255 : lastToBx));
  Link.write((uint8_t)(lastToBy < 0 ? 255 : lastToBy));
  // Brett verlustfrei gepackt: eine Figur ist Typ(0..6) | CH_BLACK(8) = 4 Bit,
  // also 2 Felder pro Byte -> 32 statt 64 Byte. Damit passt das Chess-Paket
  // (51 Byte Nutzlast) in EIN USB-Paket statt in zwei.
  { uint8_t cur = 0; int n = 0;
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        uint8_t v = board[y][x] & 0x0F;
        if ((n & 1) == 0) cur = v; else Link.write((uint8_t)(cur | (v << 4)));
        n++;
      }
  }
  Link.endTransmission();
}
void applyState(const uint8_t* b, uint16_t l) {
  if (l < 19 + 32) return;
  uint8_t prevStatus = status;
  bool prevCheck = inCheck;
  currentPlayer = b[1];
  status = b[2];
  inCheck = b[3];
  phase = b[4];
  promoSel = b[5];
  curBx = (int8_t)b[6]; curBy = (int8_t)b[7];
  selBx = (b[8] == 255) ? -1 : b[8];
  selBy = (b[9] == 255) ? -1 : b[9];
  promoBx = (b[10] == 255) ? -1 : b[10];
  promoBy = (b[11] == 255) ? -1 : b[11];
  epBx = (b[12] == 255) ? -1 : b[12];
  epBy = (b[13] == 255) ? -1 : b[13];
  uint8_t cr = b[14];
  cWK = cr & 1; cWQ = cr & 2; cBK = cr & 4; cBQ = cr & 8;
  lastFromBx = (b[15] == 255) ? -1 : b[15];
  lastFromBy = (b[16] == 255) ? -1 : b[16];
  lastToBx = (b[17] == 255) ? -1 : b[17];
  lastToBy = (b[18] == 255) ? -1 : b[18];
  int idx = 19;                       // Brett: 2 Felder pro Byte (siehe sendState)
  { int n = 0; uint8_t cur = 0;
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        if ((n & 1) == 0) { cur = b[idx++]; board[y][x] = cur & 0x0F; }
        else              { board[y][x] = (cur >> 4) & 0x0F; }
        n++;
      }
  }
  // dezente Toene fuer den Gast
  if (prevStatus == 0 && status != 0) beep(status == 3 ? 400 : 120, 350);
  else if (!prevCheck && inCheck) beep(1300, 100);
}

// ---------- Host / Guest ----------
void hostTick() {
  Link.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2);
  uint8_t gMask = 0;
  if (Link.available() >= 1) {
    gMask = Link.read();
    if (Link.available()) Link.read();
  }
  uint8_t hMask = readMask();
  uint8_t hEdges = hMask & ~hostPrevMask;
  uint8_t gEdges = gMask & ~guestPrevMask;
  hostPrevMask = hMask; guestPrevMask = gMask;

  if (status == 0) {
    uint8_t edges = (currentPlayer == 1) ? hEdges : gEdges;
    handleInput(edges, currentPlayer);
  }
  sendState();
}

// Plausibilitaetspruefung des geretteten Zustands (siehe Bombing Bob).
bool stateSane() {
  if (currentPlayer < 1 || currentPlayer > 2 || status > 3 || phase > 1) return false;
  if (promoSel > 3) return false;
  if (curBx < 0 || curBx > 7 || curBy < 0 || curBy > 7) return false;
  int kings = 0;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      uint8_t v = board[y][x];
      if (v & ~0x0F) return false;
      uint8_t t = ptype(v);
      if (t > PT_KING) return false;
      if (t == PT_KING) kings++;
    }
  return kings == 2;      // genau zwei Koenige -> Stellung ist plausibel
}

void hostMain() {
  if (persistUsable(GAME_CHESS) && stateSane()) {
    // Stellung, Zugrecht und Rochaderechte stehen noch. Tastenmaske abgleichen, sonst wird eine gehaltene
    // Taste sofort als neuer Zug gewertet.
    hostPrevMask = readMask(); guestPrevMask = readGuestNow().dir;
  }
  else initMatch();
  persistArm(GAME_CHESS);
  unsigned long lt = millis(), lp = 0;
  bool pAB = false;
  uint8_t prevStatus = 0;
  drawScene();
  while (true) {
    Link.service();
    if (millis() - lt >= 50) { lt = millis(); hostTick(); drawScene(); }
    bool inWin = (status != 0);
    bool prevWin = (prevStatus != 0);
    if (!prevWin && inWin) { snapshotGuestRestart(); lp = millis(); }
    prevStatus = status;
    bool AB = aOrB();
    bool guestWants = false;
    if (inWin && millis() - lp >= 100) { lp = millis(); guestWants = pollGuestRestart(); }
    if (inWin && ((AB && !pAB) || guestWants)) {
      sendRestart(CH_RESTART); initMatch(); drawScene(); beep(900, 60);
      pAB = false;
      continue;
    }
    pAB = AB;
  }
}

void guestMain() {
  initMatch();
  unsigned long ld = 0;
  drawScene();
  uint8_t prevStatus = 0;
  while (true) {
    Link.service();
    guestInputByte = readMask();
    bool inWin = (status != 0);
    bool prevWin = (prevStatus != 0);
    if (!prevWin && inWin) resetGuestRestartEdge();
    prevStatus = status;
    if (inWin && guestPressedRestart()) guestFireCnt++;
    if (recvNew) {
      noInterrupts();
      uint8_t b[256]; uint16_t l = recvLen;
      for (uint16_t i = 0; i < l; i++) b[i] = recvBuf[i];
      recvNew = false; interrupts();
      if (l > 0) {
        if (b[0] == CH_RESTART) { initMatch(); beep(900, 60); }
        else if (b[0] == CH_STATE) applyState(b, l);
      }
    }
    if (millis() - ld >= 33) { ld = millis(); drawScene(); }
  }
}
}

// ========================= Setup & Loop =========================
void setup() {
  tft.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  // Display-Takt innerhalb der ST7789-Spezifikation (max. 62,5 MHz).
  // Hinweis: Die frueheren 125 MHz waren nie erreichbar - der SPI-Baustein
  // teilt den 150-MHz-Takt nur geradzahlig, real liefen 75 MHz (20 % ueber
  // Spezifikation). Naechste moegliche Stufe darunter ist 37,5 MHz; ein
  // Bildaufbau dauert dadurch ca. 29 statt 14 ms.
  // Zum Zuruecknehmen einfach wieder 75000000 eintragen.
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
  pinMode(SPEAKER, OUTPUT);
  tft.fillScreen(COL_BG);
  // Board beim Booten USB-UNSICHTBAR machen: erst mit der Rollenwahl meldet
  // es sich an (JOIN) bzw. wird Host. Sonst sieht ein Host-first-Geraet das
  // Boot-USB-Geraet des anderen Boards in einem Zwischenzustand und mountet
  // dessen CDC nie sauber. So enumeriert der Host in JEDER Reihenfolge ein
  // frisch angemeldetes CDC-Geraet.
  tud_disconnect();
  readCrashInfo();          // Blackbox des letzten Laufs auswerten (zeigt das Menue an)
  if (autoResume) {
    // Nach einem Watchdog-Neustart NICHT ins Menue, sondern direkt zurueck ins
    // gleiche Spiel mit gleicher Rolle. Kurz anzeigen, was passiert ist, damit
    // die Diagnose trotzdem sichtbar bleibt.
    canvas.fillScreen(COL_BG);
    canvas.drawRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COL_FRAME);
    canvas.setFont(&FreeSansBold12pt7b);
    drawCenteredText("Neustart", 0, DISPLAY_WIDTH, 110, COL_TEXT);
    canvas.setFont(&FreeSans9pt7b);
    drawCenteredText("Spiel wird fortgesetzt", 0, DISPLAY_WIDTH, 140, COL_DIM);
    canvas.setFont(NULL); canvas.setTextSize(1); canvas.setTextColor(0xFD20);
    for (int i = 0; i < 4; i++) {
      int w = (int)strlen(crashInfo[i]) * 6;
      canvas.setCursor((DISPLAY_WIDTH - w) / 2, 180 + i * 11);
      canvas.print(crashInfo[i]);
    }
    pushCanvas();
    delay(1500);
  } else {
    selectGame();
    selectRole();
  }
  // USB-Rolle zur Laufzeit festlegen (eine Firmware fuer beide Rollen):
  //   HOST -> nativer USB-Host,  JOIN -> nativer USB-Device.
  Link.startRole(myRole == ROLE_HOST);
  if (myRole == ROLE_HOST) {
    connectAsHost();
    linkDelay(400);
    Link.gameStarted();          // Link aktiv -> Reconnect-Erkennung scharf
    if (selectedGame == GAME_TRON) Tron::hostMain();
    else if (selectedGame == GAME_PONG) Pong::hostMain();
    else if (selectedGame == GAME_DUEL) Duel::hostMain();
    else if (selectedGame == GAME_ARTY) Arty::hostMain();
    else if (selectedGame == GAME_BOMBER)  Bomber::hostMain();
    else if (selectedGame == GAME_CHESS) Chess::hostMain();
    else if (selectedGame == GAME_WNR) WnR::hostMain();
    else if (selectedGame == GAME_C4) C4::hostMain();
    else Dots::hostMain();
  } else {
    connectAsGuest();
    linkDelay(400);
    Link.gameStarted();          // Link aktiv -> Reconnect-Erkennung scharf
    if (selectedGame == GAME_TRON) Tron::guestMain();
    else if (selectedGame == GAME_PONG) Pong::guestMain();
    else if (selectedGame == GAME_DUEL) Duel::guestMain();
    else if (selectedGame == GAME_ARTY) Arty::guestMain();
    else if (selectedGame == GAME_BOMBER)  Bomber::guestMain();
    else if (selectedGame == GAME_CHESS) Chess::guestMain();
    else if (selectedGame == GAME_WNR) WnR::guestMain();
    else if (selectedGame == GAME_C4) C4::guestMain();
    else Dots::guestMain();
  }
}
void loop() {}
