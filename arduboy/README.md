# PBC Arduboy

Arduboy-Emulator für den **PicoBoy Color Plus**, nur als Teil von PBCemu
(`../pbcemu/build.sh` → `dist/pbcemu.uf2`); eine Einzel-Firmware gibt es
nicht.

Der Kern (`core/arduboy.c`) ist für den PicoBoy geschrieben
(GPL-3.0-or-later):

- der ATmega32U4 mit 16 MHz: der ganze AVR-Befehlssatz mit den Takten des
  32U4, Interrupts und SLEEP;
- die Timer 0, 1, 3 und 4 in den Betriebsarten, die Spiele benutzen, mit
  Flags, Interrupts und den Compare-Ausgängen, die den Lautsprecher treiben;
- die SPI mit dem SSD1306-OLED (128×64) dahinter;
- das EEPROM, der ADC (als Zufallsquelle), die PLL und so viel USB, dass
  der Arduino-Kern startet;
- die Ports mit den Tasten und der RGB-LED.

Das Programm wird beim Laden dekodiert und dann **in ARM-Code übersetzt**
(`core/ard_jit.c`): jeder Block (bis zum nächsten Sprung oder Sprungziel)
beim ersten Durchlauf, so landen Tabellen und Grafiken im Flash nicht im RAM.
Ein MOV wird so zu zwei ARM-Befehlen statt rund 17 Takten im Interpreter.
Genau bleibt es trotzdem: Ein Block läuft nur, wenn er ganz ins Taktbudget
bis zum nächsten Timer-Ereignis passt, sonst führt der Interpreter seine
Befehle einzeln aus; I/O geht über Hilfsfunktionen, die den Takt
abgleichen; Interrupts kommen wie gehabt zwischen den Blöcken. Die Flags
(SREG) rechnet der übersetzte Code nur aus, wo ein späterer Befehl sie liest
(Analyse über das ganze Programm). Am PC unter qemu liefern alle Testspiele
Bild, Ton und EEPROM bitgenau wie der Interpreter. Auf dem Gerät bei
200 MHz: ArduboyLife im Spiel volles Tempo bei 46 % Last (Interpreter: 78 %
Tempo), MicroCity 45 %, Catacombs (3D) 77 %. Der übersetzte Code liegt im
freien RAM (rund 200 KB; Catacombs braucht etwa 140 KB); was nicht mehr
passt, macht der Interpreter.

## Spiele

Ein Spiel ist die Intel-HEX-Datei, die die Arduino-IDE erzeugt (`.hex`).
Spiele für das **Arduboy FX** brauchen dessen externen Flash-Baustein und
laufen nicht.

Das EEPROM (1 KB, dort speichern die Spiele Highscores und Einstellungen)
liegt als `<Spiel>.sav` neben dem Spiel. Der Emulator schreibt es eine
Sekunde nach der letzten Änderung.

## Tasten

| Arduboy | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| A | A |
| B | B |

RESET führt zurück zum Starter.

## Bild

Das 128×64-Bild in zwei Größen (Einstellung im Starter):

- **Fit width**: 240×120, jedes Pixel 1,875 Panel-Pixel; wo ein Pixel
  zwischen zwei fällt, ihr Mittelwert;
- **Sharp**: 192×96, jedes Pixel 1 oder 2 Panel-Pixel, ohne Mischen.

## Stand

Am PC getestet mit den Beispielen der Arduboy2-Bibliothek (HelloWorld,
Buttons, FontDemo, ArduBreakout, BeepDemo): Bild, Tasten und Töne stimmen
(BeepDemo: 698,3 Hz statt 698,5 Hz). Dazu am PC (Interpreter und unter
qemu die Übersetzung, gleiches Ergebnis) MicroCity, ArduboyLife, Catacombs
of the Damned, Rooftop Rescue, Mystic Balloon, Minesweeper, PipeBoy,
SpaceCab und Brainboy; auf dem Gerät MicroCity, ArduboyLife und Catacombs
gemessen, Spielstände (EEPROM) werden gespeichert.
