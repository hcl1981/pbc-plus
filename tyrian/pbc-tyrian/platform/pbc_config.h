/*
 * pbc_config.h -- Hardware und Bildaufteilung des PicoBoy Color Plus.
 *
 * Die Pinbelegung ist aus zwei unabhaengigen, auf dieser Hardware laufenden
 * Projekten uebernommen und stimmt dort ueberein: der Doom-Portierung
 * (pico-sdk, doom/picoboy-doom im selben Repo) und der
 * Jump-'n'-Bump-Portierung (Arduino, jumpnbump/). Wo unten eine
 * Zahl steht, ist sie also nicht geraten.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_CONFIG_H
#define PBC_CONFIG_H

/* ------------------------------------------------------------ Bildschirm */

#define PBC_TFT_W  240
#define PBC_TFT_H  280

/*
 * Der ST7789 hat 240x320 Bildspeicher, das Glas zeigt davon 240x280. Zusammen
 * mit der 180-Grad-Drehung in MADCTL (0xC8) liegt das sichtbare Fenster 20
 * Zeilen versetzt. Wert aus der laufenden Doom-Portierung.
 */
#define PBC_PANEL_Y_OFFSET 20

#define PBC_SPI_PORT  spi0
#define PBC_PIN_SCK   18
#define PBC_PIN_MOSI  19
#define PBC_PIN_CS    10   /* normaler GPIO, NICHT die SPI-CSn-Funktion */
#define PBC_PIN_DC     8
#define PBC_PIN_RST    9
#define PBC_PIN_BL    26   /* Hintergrundlicht, muss per PWM an -- sonst
                              bleibt der Schirm schwarz und man sucht den
                              Fehler stundenlang woanders */
#define PBC_SPI_HZ    62500000

/*
 * Farbreihenfolge des Panels.
 *
 * MADCTL steht auf 0xC8, und darin ist Bit 3 (0x08) gesetzt: das Panel liest
 * das 16-Bit-Wort als BGR, nicht als RGB. Blau gehoert also in die oberen
 * fuenf Bit und Rot in die unteren.
 *
 * Bestaetigt an der laufenden Doom-Portierung auf derselben Hardware: die baut
 * ihre Palette mit PICO_SCANVIDEO_PIXEL_FROM_RGB8, und dieses Makro legt
 * ebenfalls Blau nach oben (BSHIFT 11) und Rot nach unten (RSHIFT 0).
 *
 * Falls Rot und Blau auf einem anderen Geraet doch vertauscht erscheinen:
 * hier auf 0 setzen. Das ist die einzige Stelle, an der die Reihenfolge
 * entschieden wird.
 */
#ifndef PBC_PANEL_BGR
#define PBC_PANEL_BGR 1
#endif

/*
 * Eine Farbe aus 8-Bit-Anteilen in das Wort bauen, das dieses Panel erwartet.
 * Jede fest verdrahtete Farbe im Port geht hier durch -- so kippt ein
 * geaendertes PBC_PANEL_BGR alle zugleich und nicht nur die Bilddaten.
 */
#if PBC_PANEL_BGR
#define PBC_RGB(r, g, b) \
	((uint16_t)((((b) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((r) >> 3)))
#else
#define PBC_RGB(r, g, b) \
	((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#endif

/* ------------------------------------------------------------ Bedienung */

/* Alle aktiv LOW mit Pullup. */
#define PBC_PIN_JOY_CENTER  0
#define PBC_PIN_JOY_RIGHT   1
#define PBC_PIN_JOY_DOWN    2
#define PBC_PIN_JOY_LEFT    3
#define PBC_PIN_JOY_UP      4
#define PBC_PIN_BTN_A      27
#define PBC_PIN_BTN_B      28

/* ------------------------------------------------------------ Ton */

#define PBC_PIN_SPEAKER    15

/*
 * Abtastrate des Mischers: 11025 Hz -- genau die Rate, mit der Tyrians Klaenge
 * aufgezeichnet sind.
 *
 * Damit entfaellt jede Umrechnung, und, wichtiger: die Musik wird von einer
 * Software-Nachbildung des OPL-Chips erzeugt, die je AUSGABEWERT rechnet.
 * Die Rate zu halbieren halbiert deshalb unmittelbar den groessten
 * Rechenzeitposten der Tonausgabe.
 *
 * Der Ausgang ist ein Piezo ohne nennenswerten Frequenzgang; mehr Rate waere
 * Aufwand, den man nicht hoert.
 */
#define PBC_AUDIO_RATE     11025

/*
 * Zusatzverstaerkung der Klaenge vor der PWM-Ausgabe.
 *
 * Tyrians Mischer kommt aus einer Zeit, in der die Musik den Grossteil des
 * Pegels ausmachte und die Klaenge sich daneben einordnen mussten. Hier ist
 * die Musik abgeschaltet -- der Platz steht also leer. Nachgerechnet:
 *
 *     Vollausschlag eines Klangs   127 (8 Bit)
 *     x Hauptregler fxVolume 223   0,648
 *     x Kanalregler fxPlayVol 4/7  0,625
 *     ------------------------------------
 *     = 40 % des moeglichen Ausschlags
 *
 * Es liegen also rund zweieinhalb Mal Luft brach. Faktor 3 waere zu viel (er
 * kappt die lautesten Klaenge), Faktor 2 fuehrt sie genau an die Grenze:
 * doppelte Amplitude, also rund 6 dB lauter, und trotzdem ohne Verzerrung.
 *
 * Der Regler im Optionsmenue wirkt weiterhin -- wer ihn hochdreht, bekommt
 * Uebersteuerung, und das ist dann eine bewusste Entscheidung.
 */
#define PBC_FX_GAIN        2

/* ------------------------------------------------------- Bildaufteilung */

/*
 * Tyrian rechnet in 320x200 mit 8 Bit Farbtiefe. Darin steckt:
 *
 *     x   0..263, y 0..183   Spielfeld
 *     x 264..319, y 0..199   Seitenleiste (Schild, Panzerung, Waffen)
 *     x   0..263, y 184..199 unterer Streifen
 *
 * Das Panel ist 240x280 -- hochkant, also genau falsch herum fuer ein
 * 320x200-Bild, aber genau richtig fuer einen senkrecht scrollenden Shooter.
 *
 * Aufteilung (vom Nutzer so gewaehlt):
 *
 *     Zeilen   0..183   Spielfeld in Originalpixeln, 240 von 264 Spalten
 *     Zeilen 184..279   eigens gebaute Anzeigeleiste, 240x96
 *
 * Das Spielfeld wird NICHT skaliert. 264 Spalten passen nicht in 240, also
 * bleiben 24 uebrig; die Kamera verschiebt sich im Rahmen dieser 24 Spalten mit
 * dem Schiff, sodass der Rand, auf den man gerade zufliegt, immer sichtbar ist.
 * Bei ruhendem Schiff steht sie mittig (Versatz 12).
 *
 * Die runden Ecken des Panels schneiden nur die Ecken. Weil das Spielfeld oben
 * anliegt, sind die beiden oberen Ecken betroffen -- dort liegt im Spiel nichts
 * Entscheidendes (der Hintergrund scrollt hinein). Die Anzeigeleiste haelt von
 * den unteren Ecken Abstand.
 */
#define PBC_PLAY_SRC_W   264   /* Breite von Tyrians Spielfeld */
#define PBC_PLAY_SRC_H   184
#define PBC_PLAY_W       240   /* was davon auf den Schirm passt */
#define PBC_PLAY_H       184
#define PBC_PLAY_PAN_MAX (PBC_PLAY_SRC_W - PBC_PLAY_W)   /* 24 */

#define PBC_HUD_Y        PBC_PLAY_H          /* 184 */
#define PBC_HUD_W        PBC_TFT_W           /* 240 */
#define PBC_HUD_H        (PBC_TFT_H - PBC_HUD_Y)  /* 96 */

/*
 * Hoehe eines Ausgabestreifens. Das Bild wird nicht als Ganzes im RAM gehalten
 * -- 240x280x2 waeren 134 KB, die neben Tyrians drei 320x200-Puffern nicht mehr
 * hineinpassen. Stattdessen werden je acht Zeilen umgerechnet und per DMA
 * hinausgeschoben, waehrend die naechsten acht entstehen. Kostet 2 x 3840 Byte
 * statt 134 KB und ergibt nebenbei die Luecken, in denen der Multiplayer-Link
 * bedient werden kann (er vertraegt keine 17 ms Blindflug am Stueck).
 */
#define PBC_STRIP_ROWS   8

/*
 * Menueansicht: Ausschnitt in Originalgroesse statt verkleinertem Gesamtbild.
 *
 * Zuerst wurde das ganze 320x200-Bild auf 240x150 verkleinert (jede vierte
 * Spalte und Zeile weg). Das war ein Fehlgriff: Tyrians kleine Schrift ist
 * fuenf bis sieben Punkte hoch, und ein Viertel der Spalten wegzulassen macht
 * sie unlesbar -- die Schiffauswahl war damit nicht zu bedienen.
 *
 * Jetzt 1:1 und mittig: von 320 Spalten sind 240 zu sehen, je 40 fallen links
 * und rechts weg. Tyrians Menues sind um x=160 zentriert, die Beschriftungen
 * liegen also vollstaendig im Ausschnitt. Alle 200 Zeilen passen ohnehin.
 */
#define PBC_MENU_W       240
#define PBC_MENU_H       200
#define PBC_MENU_SRC_X   ((320 - PBC_MENU_W) / 2)   /* 40 */
#define PBC_MENU_Y       ((PBC_TFT_H - PBC_MENU_H) / 2)  /* 40 */

/*
 * Spielansicht: ganzes Bild verkleinert.
 *
 * Ab dem Ladenbildschirm und im Spiel selbst spannt sich der Inhalt ueber die
 * volle Breite -- links das Spielfeld, rechts die Statusspalte mit Schild,
 * Panzerung und Waffen. Ein Ausschnitt wuerde die Spalte abschneiden, also
 * wird verkleinert statt beschnitten.
 *
 * 320 -> 240 ist genau 3/4, damit 200 -> 150. Ein groesserer Massstab ginge
 * nicht: die Breite ist die Grenze. Das Bild steht senkrecht mittig.
 */
/*
 * Beim Verkleinern mitteln statt wegzulassen.
 *
 * 320 -> 240 ist 3/4, es faellt also jede vierte Spalte und Zeile weg. Bei
 * Kanten und Sprites faellt das kaum auf -- bei Tyrians Wolken schon: die
 * bestehen aus wenigen Punkten mit je einer Helligkeitsstufe Unterschied
 * (Tyrians Palette hat 16 Stufen je Farbton). Ein Viertel davon wegzulassen
 * macht aus einem weichen Schleier ein Fleckenmuster.
 *
 * Gemittelt wird ueber 2x2, gleichmaessig ueber alle vier Quellpunkte:
 * Zielpunkt k bekommt den Mittelwert aus Quelle k und k+1, waagerecht wie
 * senkrecht. Kosten: vier Palettenzugriffe und drei Mittelungen je Zielpunkt
 * statt eines Zugriffs -- rund 0,2 ms je Streifen, und die verstecken sich
 * hinter dem DMA, der fuer denselben Streifen 0,5 ms braucht.
 *
 * Auf 0 setzen, falls es doch zu langsam wird.
 */
#ifndef PBC_SCALE_AVERAGE
#define PBC_SCALE_AVERAGE 1
#endif

#define PBC_GAME_W       240
#define PBC_GAME_H       150
#define PBC_GAME_Y       ((PBC_TFT_H - PBC_GAME_H) / 2)  /* 65 */

/* Zeile fuer den Zustand des Multiplayer-Links, unter dem Bild. */
#define PBC_LINK_ROW_Y   (PBC_GAME_Y + PBC_GAME_H + 12)  /* 227 */
#define PBC_LINK_ROW_H   7

#endif /* PBC_CONFIG_H */
