# Multiplayer über USB (zwei PicoBoys an einem USB-C-Kabel)

Der Multiplayer-Code von rp2040-doom (Lobby, Coop, Deathmatch, Tic-Sync) ist
komplett vorhanden und war auch schon in deiner Firmware — nur der Transport
lief über I²C an GP18/GP19, und das sind auf dem PicoBoy die Display-Pins.
Der Transport ist jetzt ersetzt: es läuft alles über den USB-C-Port.

## Was neu ist

| Datei | Inhalt |
|---|---|
| `doom-usb-mp.uf2` | Doom-Firmware mit USB-Mehrspieler |
| `src/pico/usblink.c/.h` | der Link selbst (D+/D- als getaktete 2-Draht-Leitung) |
| `src/pico/piconet_usb.c` | piconet-Protokoll (Lobby + Tics) über diesen Link |

WAD bleibt unverändert — die WAD-UF2 müssen nicht
neu geflasht werden. **Beide Geräte brauchen dieselbe Firmware und dasselbe
WAD**, sonst lehnt der Host mit „network game is not compatible" ab (die
Firmware vergleicht einen Hash aus Version + WHD).

## Rollenwahl

Die USB-Rolle wird beim Starten des Spiels festgelegt, nicht per Kabel oder
Jumper:

* **Options → Network Game → Host Game / Deathmatch / Deathmatch 2** →
  das Gerät ist **USB-Master** und taktet die Verbindung. Spielername wird
  automatisch **PLAYER 1**.
* **Options → Network Game → Join Game** → das Gerät ist **USB-Slave** und
  wartet darauf, dass der Master anklopft. Spielername **PLAYER 2**.

**Reihenfolge:** egal, aber am unkompliziertesten ist *erst Host, dann Join*.
Der Host stellt seine Anfrage und lässt sie bis zu 150 ms stehen, danach
versucht er es alle 20 ms erneut — es gibt also kein enges Zeitfenster, das man
treffen müsste. Der Beitretende schaut alle 500 µs nach und zusätzlich in jedem
Durchlauf der Spielschleife.

Ablauf: auf Gerät 1 Host wählen (Episode + Schwierigkeit), auf Gerät 2 Join;
Gerät 2 taucht in der Lobby von Gerät 1 auf, Gerät 1 startet das Spiel, Gerät 2
springt automatisch mit rein.

Menü-Taste am PicoBoy ist wie gehabt **A + CENTER** (ESC).

## Statusanzeige unter dem Bild

Sobald ein Netzwerkspiel aufgebaut wird (und auch während des Spiels), stehen im
schwarzen Streifen **unter** dem Doom-Bild zwei zentrierte Zeilen in doppelt
großer Schrift — sie verschwinden wieder, sobald das Netzspiel beendet ist:

```
HOST L2 RC0 H600
OK123 E4 F0/0
```

| Feld | Bedeutung |
|---|---|
| `HOST` / `JOIN` | gewählte Rolle (USB-Master bzw. -Slave) |
| `L` | Leitungspegel: Bit1 = CLK (D-), Bit0 = DATA (D+). Der Host sollte während einer Anfrage `L2` sehen (sein eigener Takt), der Client `L2`, wenn der Host anklopft. Dauerhaft `L0` auf beiden Seiten heißt: es kommt nichts über die Leitung. |
| `RC` | Rückgabe des letzten Austauschs: `≥0` = empfangene Bytes (gut), `-1` = Timeout, `-2` = CRC/Rahmenfehler, `-3` = Leitung belegt (beide Geräte fahren Host) |
| `H` | aktuelle Halbbitzeit in ns — der Host regelt sie selbst nach (400…4000) |
| `OK` | erfolgreiche Austausche (mod 1000) |
| `E` | Timeouts + CRC-Fehler (mod 1000) |
| `F` | Fehlercode/Byteposition des letzten Fehlschlags, `0/0` = letzter Austausch war sauber |

**Im laufenden Spiel** zeigt Zeile 2 stattdessen den Tic-Fortschritt:
`T123 R120 E4` — `T` = eigener Tic, `R` = Tic der Gegenstelle, `E` = Fehler.
Bleibt `R` stehen, während `T` weiterläuft, liefert das andere Gerät nichts
mehr.

Steht dort **`LINK OFF R1/0`**, hat die Spiel-Logik die Verbindung abgeräumt —
die Ziffer sagt, welche Stelle es war:

| Grund | Bedeutung |
|---|---|
| `R1` | Doom hielt die Verbindung für tot (zu lange keine Pakete) |
| `R2` | ein Tic hatte weniger als zwei Spieler markiert (Zahl dahinter = wie viele) |
| `R3` | „End Game" im Menü |
| `R4` | Programmende |

Die Fehlercodes bei `F`:

| Code | Seite | Bedeutung |
|---|---|---|
| 2 | Host | Client gab die Datenleitung nicht frei |
| 3 | Host | Client meldete sich nicht zur Antwortphase |
| 4 | Host | falsches Sync-Byte in der Antwort (Position = empfangenes Byte) |
| 5 | Host | unsinnige Längenangabe |
| 6 | Host | CRC der Antwort falsch |
| 12 | Join | Host senkte den Takt nicht |
| 13 | Join | **Taktflanke ausgeblieben** — Position = bei welchem Byte |
| 14/15/16 | Join | Sync / Länge / CRC des Host-Pakets falsch |
| 17 | Join | Taktflanke beim Senden ausgeblieben (Position = Byte) |

Abschalten mit `cmake -DPICOBOY_NET_DEBUG_SCREEN=OFF ..`.

## Spielernamen

Die Namenseingabe im Netzwerkmenü ist entfernt — ohne Tastatur war sie nicht zu
bedienen. Die Namen vergibt jetzt die Rolle: Host = `PLAYER 1`,
Beitretender = `PLAYER 2`.

## Wie der Link technisch funktioniert

Kein USB-Protokoll, keine Enumeration, kein Host-Modus, kein VBUS. Der RP2350
kann seine beiden USB-PHY-Leitungen per `USBPHY_DIRECT`-Override direkt als IO
treiben und lesen. Darauf läuft ein simpler getakteter Halbduplex-Link:

```
D- (DM) = CLK    nur der Master treibt ihn
D+ (DP) = DATA   Halbduplex, Richtung ergibt sich aus der Protokollphase
GND              über das Kabel gemeinsam
```

Ein Austausch: Master zieht CLK hoch (Attention) → Slave antwortet mit DATA
hoch (Ready) → Master taktet sein Paket raus → Richtungswechsel → Slave taktet
seine Antwort raus. **Kein Paket ist größer als 32 Byte** — die Zustellrate
fällt exponentiell mit der Länge, deshalb stehen weder Spielernamen (die ergibt
die Rolle) noch ungenutzte Spielerplätze auf der Leitung. Jedes Paket hat Sync-Byte, Länge und CRC16; ein kaputtes
Paket wird verworfen und beim nächsten Mal einfach wiederholt — das
piconet-Protokoll quittiert ohnehin jeden Tic.

Der Slave braucht dabei **keine** eigene Zeitbasis: er sampelt auf den
Taktflanken des Masters — und stellt deshalb auch nichts ein. Nur der Master hat
eine Bitzeit (Default 1500 ns pro Halbbit, gemessen fehlerfrei), einstellbar in
`usblink.c` (`USBLINK_HALF_BIT_NS`); im Betrieb regelt der Host sie zwischen
800 und 4000 ns selbst nach.

**Der Richtungswechsel ist die kritische Stelle.** Wenn der Master sein Paket
gesendet hat, gibt er die Datenleitung frei und wartet auf das erste Bit der
Antwort (immer eine 1, das oberste Bit des Startbytes). Er muss dabei zwingend
**erst abwarten, bis die Leitung unten ist**: freigeben heißt nicht sofort
niedrig, die Leitung wird nur über den Pulldown entladen. War das letzte
gesendete Bit selbst eine 1 — abhängig vom Prüfsummen-Byte also rund jedes
zweite Mal —, liest er sonst den Rest seines eigenen Signals als „Gegenstelle
bereit", taktet zu früh los und alles Weitere ist um Bits verschoben. Das war
die Ursache einer Fehlerrate von 65–80 %, unabhängig von der Taktrate.

Der gesamte flankenkritische Code liegt im RAM (`__not_in_flash_func`): im
Spiel arbeitet Core1 den XIP-Cache durch, und ein Cache-Miss mitten in der
Bitschleife verschiebt sonst eine Flanke — genau das erzeugt CRC-Fehler, die
im Testprogramm (Cache warm, nichts sonst los) nie auftreten.

Kosten im Spiel: alle 10 ms ein Austausch von ~70 Byte, dabei sind für
~0,5–1 ms die Interrupts aus. Ohne Gegenstelle klopft der Host nur alle 30 ms
an, damit die Lobby nicht zäh wird.

## Wenn keine Verbindung zustande kommt

Ehrlich gesagt: **die Elektrik ist nie auf Hardware geprüft worden.** Die
Software-Seite ist sauber gebaut, ob Kabel und Buchsen D+/D− durchreichen,
zeigt erst der Versuch.

Der häufigste Grund ist banal: **viele USB-C-Kabel sind reine Ladekabel** und
führen die Datenleitungen gar nicht. Also zuerst gegen ein bekannt gutes
Datenkabel tauschen — eines, mit dem schon einmal ein Gerät am PC erkannt
wurde.

Kommt die Lobby danach immer noch nicht zustande, ist die Taktrate der nächste
Verdächtige. Sie steht als `USBLINK_HALF_BIT_NS` in `src/pico/usblink.c`
(Grundwert 1000 ns, Untergrenze der Selbstnachregelung 800 ns). Ein höherer
Wert macht die Übertragung langsamer, aber robuster — zum Eingrenzen lohnt es,
ihn testweise deutlich anzuheben.

### Wenn im Test gar nichts durchgeht

Der Reihe nach die wahrscheinlichsten Ursachen:

1. **Kabel** — viele USB-C-Kabel sind reine Ladekabel ohne D+/D-. Ein Kabel
   nehmen, mit dem der PicoBoy sich am PC als Laufwerk meldet.
2. **Steckerorientierung** — wenn die Platine nur eines der beiden
   D+/D--Paarungen der Buchse beschaltet, hilft es, einen Stecker um 180°
   gedreht einzustecken. Alle vier Kombinationen durchprobieren.
3. **Timeout auf beiden Seiten** — beide Geräte stehen auf derselben Rolle.
   Auf einem A, auf dem anderen B.
4. Bleibt es bei 0 Paketen in allen Steckerlagen, reicht die Buchse D+/D- nicht
   zum Gegenstück durch (bzw. hängt an einem Mux) — dann geht es über diesen
   Port grundsätzlich nicht, und der Weg wäre ein anderer Draht.

## Grenzen

* **Zwei Spieler.** An einem Kabel hängen zwei Geräte; die Lobby ist auf
  Host + 1 Client ausgelegt.
* **Nicht gleichzeitig am PC.** Solange ein Netzspiel läuft, gehört der
  USB-Port dem Link. Zum Flashen erst das Spiel beenden bzw. neu starten
  (BOOTSEL ist davon unberührt, der Bootrom setzt den PHY zurück).
* **Gleiche WAD auf beiden Geräten** (Doom 1 mit Doom 1, Doom 2 mit Doom 2).
* Speichern/Laden im Netzspiel ist wie im Original gesperrt.

## Bauen

```bash
./build.sh          # baut doom_tiny_nost -> ../dist/doom-usb-mp.uf2
```

Neu konfigurieren (falls das Build-Verzeichnis fehlt):

```bash
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_PLATFORM=rp2350-arm-s \
      -DPICO_BOARD=pico2 -DPICO_FLASH_SIZE_BYTES=16777216 \
      -DPICO_SDK_PATH=$HOME/pico/pico-sdk -DPICO_EXTRAS_PATH=$HOME/pico/pico-extras \
      -Dpicotool_DIR=/usr/local/lib/cmake/picotool ..
```

Der alte I²C-Transport ist nicht gelöscht, nur abgewählt:
`cmake -DPICOBOY_NET_USB=OFF ..` baut wieder `piconet.c` (dann braucht es
SDA/SCL auf freien GPIOs — auf dem PicoBoy kollidieren die Default-Pins
GP18/GP19 mit dem Display).
