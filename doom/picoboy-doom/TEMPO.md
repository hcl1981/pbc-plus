# Die Tempo-Änderungen an der Bildausgabe

Vier Eingriffe gegenüber dem geradeaus portierten Stand, alle mit dem Ziel,
die vollen 35 Tics je Sekunde zu halten statt eine hohe Bildrate zu erkaufen.
Sie sind fester Bestandteil dieser Portierung; dieser Text hält fest, warum
sie nötig waren und was dabei verworfen wurde.

Abschaltbar sind sie einzeln, siehe die jeweiligen CMake-Schalter unten.

## Der Befund dahinter

`pd_end_frame()` läuft auf **Core0** — derselben Rechenzeit, in der auch die
Spiellogik ihre 35 Tics pro Sekunde erzeugt. Und an dessen Ende hing die
komplette Bildausgabe:

* 240 × 180 × 2 Byte = **86 400 Byte** pro Bild
* bei 62,5 MHz SPI sind das **rund 11 ms**
* `picoboy_present()` hat auf das Ende dieses Transfers **gewartet**

Bei 30 Bildern je Sekunde ist das etwa ein Drittel der Rechenzeit von Core0,
das nur mit Warten auf den SPI-Bus verbracht wurde. Im Einzelspiel reichte die
verbleibende Zeit noch für die vollen 35 Tics; kam der Multiplayer mit seinen
gut 10 % dazu, kippte es auf 24.

Core1 hilft beim Rendern (`core1_wake`, `core1_do_flats`), gibt das Bild aber
nicht aus — die Ausgabe war nie ausgelagert.

## Änderung 1: Der Bildtransfer wird nicht mehr abgewartet

`picoboy_present()` stößt den DMA an und kehrt sofort zurück. Eingeholt wird
der Transfer erst, wenn jemand wieder an den Bildpuffer oder an SPI muss —
also beim Zusammensetzen des *nächsten* Bildes. Bis dahin hat Core0
Spiellogik und Rendern erledigt, und die 11 ms sind währenddessen vergangen
statt abgewartet worden.

Zwei Bildpuffer braucht es dafür nicht: Zwischen `present()` und dem nächsten
`compose()` liegt die gesamte Spiel- und Renderphase, und die schreibt in
Dooms eigene Puffer, nicht in `fb`. Die ursprünglich angedachten 86 KB für
einen zweiten Puffer wären auch nicht da gewesen — die Doom-Zone hat insgesamt
nur rund 123 KB.

Betroffen: `src/pico/picoboy_display.c` (`picoboy_wait_dma()`, alle Stellen,
die vorher selbst auf den DMA gewartet haben).

## Änderung 2: Bilder auslassen, wenn die Tics zurückfallen

Bleibt `gametic` zwei oder mehr Tics hinter der Uhr zurück, entfällt die
Ausgabe für ein Bild. Der Renderer bekommt seinen Puffer trotzdem zurück
(`new_frame_stuff()` läuft weiter), es unterbleibt nur das Zusammensetzen und
Schieben.

Nie zwei Bilder hintereinander, damit es nicht stockt.

Dahinter steckt eine Abwägung: Doom läuft mit festen 35 Tics je Sekunde, die
Bildrate ist frei. **20 Bilder mit vollen 35 Tics spielen sich deutlich besser
als 30 Bilder mit 24** — Steuerung, Gegner und Waffen laufen dann in richtiger
Geschwindigkeit statt in Zeitlupe.

Abschaltbar: `cmake -DPICOBOY_FRAME_SKIP=OFF ..`

Betroffen: `src/pico/i_video.c` (`picoboy_should_skip_frame()`),
`src/pico/CMakeLists.txt`.

## Nicht gemacht: `I_GetTime()` reparieren

In `src/pico/i_timer.c` steht rechnerisch Falsches (Faktor 1000):

```c
return TICRATE * (uint32_t)(time_us_64() / 1000);   // = 35 * Millisekunden
```

Die Korrektur **wurde probiert und wieder zurückgenommen.** Grund: Dooms
Warteschleife in `d_loop.c` benutzt diese Uhr, um bis zu fünf Tics auf fehlende
Netzwerkdaten zu warten. Mit der korrigierten Uhr griff diese Wartezeit
erstmals wirklich — und aus jedem verspäteten Tic wurde eine Pause von rund
140 ms. Das Spiel wurde dadurch spürbar ruckeliger, und in einer ungünstigen
Konstellation blieb es stehen.

Die Spielgeschwindigkeit hängt nicht an dieser Uhr (die kommt aus
`GetAdjustedTime()` und rechnet korrekt). Wer sie später doch geradeziehen
will, muss gleichzeitig `MAX_NETGAME_STALL_TICS` anfassen.

Die Bildauslassung aus Änderung 2 hat deshalb eine **eigene Zeitmessung**: sie
vergleicht über jedes Bild, wie viele Tics hätten entstehen müssen und wie
viele wirklich entstanden sind.

## Änderung 4: Kleinere Tic-Pakete, schnellerer Takt, keine Leerrunden

* **Kein Kopfballast mehr in den Tic-Paketen.** Kennung und Prüfhash brauchte
  nur der Verbindungsaufbau; in den Tic-Paketen wurden sie 35-mal je Sekunde
  mitgeschleppt. Nutzlast je Austausch: 52 → 40 Byte.
* **Takt schneller**: Grundwert 1000 statt 1500 ns. Die Untergrenze der
  Selbstnachregelung bleibt bei 800 ns — 500 ns waren zusammen mit den anderen
  Änderungen zu viel Risiko auf einmal.
* **Leerrunden überspringen**: Hat in einem Austausch keine Seite einen Tic
  beigesteuert, wird der nächste erst nach der doppelten Wartezeit angestoßen.
  Sobald wieder etwas fließt, geht es sofort im normalen Takt weiter.

## Bauen

```bash
cd picoboy-doom/build
make -j$(nproc) doom_tiny_nost     # -> src/doom_tiny_nost.uf2
```

Neu konfigurieren:

```bash
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_PLATFORM=rp2350-arm-s \
      -DPICO_BOARD=pico2 -DPICO_FLASH_SIZE_BYTES=16777216 \
      -DPICO_SDK_PATH=$HOME/pico/pico-sdk -DPICO_EXTRAS_PATH=$HOME/pico/pico-extras \
      -Dpicotool_DIR=/usr/local/lib/cmake/picotool ..
```

## Was zu messen ist

Die `D`-Anzeige im schwarzen Streifen zeigt die Tics pro Sekunde — im
Einzelspiel wie im Netzspiel:

| Messung | vorher | Erwartung |
|---|---|---|
| Einzelspiel, aufwendige Szene | 36 | 36 (bleibt am Anschlag) |
| Netzspiel, aufwendige Szene | 24 | deutlich höher |

Wenn `D` im Netzspiel weiterhin klar unter 35 liegt, ist die nächste Stufe,
die Ausgabe ganz auf Core1 zu verlagern — dafür müsste `fb` doppelt vorliegen,
und die 86 KB müssten aus der Zone kommen.
