# pixoo.library

Eine Shared Library für AmigaOS 3.x, die ein **Divoom Pixoo 64** (auch Pixoo 16/32) über das
Netzwerk ansteuert, dazu das Shell-Programm **PixooCtl**. Vorbild ist die Python-Bibliothek
[pixoo](https://pypi.org/project/pixoo/).

## Funktionen

- Display an/aus, Helligkeit, Kanal, Zifferblatt
- Geräteeinstellungen auslesen (`Channel/GetAllConf`)
- Beliebige JSON-Befehle senden und die Antwort erhalten
- 64×64-Framebuffer (16/32 ebenfalls) mit Pixeln, Linien, Rechtecken, Text und direktem
  Pufferzugriff; `PixooPush()` zeigt ihn an
- Eingebauter proportionaler Pixelfont (5 px, Umlaute, gleich breite Ziffern für Uhren)
- Beliebige Amiga-Bitmap-Fonts (`OpenDiskFont()`) für Text im Framebuffer
- Lauftext, den das Gerät selbst darstellt
- Bilder in jedem Format, das das Datatypes-System liest (IFF, PNG, JPEG …), passend
  skaliert mit festem Seitenverhältnis; Transparenz wird über den Hintergrund geblendet
- GIF-Animationen (von der Library dekodiert, bis 59 Bilder), vom Gerät abgespielt
- Läuft mit jedem TCP/IP-Stack mit bsdsocket.library (Roadshow, AmiTCP, Miami). Nur HTTP,
  daher keine SSL-Bibliothek nötig.
- Timeouts; Ctrl-C bricht Netzwerkzugriffe ab

## Voraussetzungen

- AmigaOS 3.0 oder neuer (entwickelt für 3.2), 68000 oder besser
- für Bilder außer GIF: picture.datatype V43 oder neuer (ab AmigaOS 3.5)
- ein laufender TCP/IP-Stack
- ein Pixoo 64 im selben Netz

## PixooCtl

```
PixooCtl HOST/A,PORT/K/N,TIMEOUT/K/N,SIZE/K/N,ON/S,OFF/S,BRIGHTNESS=BRI/K/N,
         CHANNEL/K/N,CLOCK/K/N,FONT/K,FILL/K,TEST/S,CHARSET/S,TEXT/K,X/K/N,Y/K/N,
         COLOR/K,TIME/S,CLEARTEXT/S,SCROLL/K,TEXTID/K/N,DEVFONT/K/N,WIDTH/K/N,
         SPEED/K/N,DIR/K,ALIGN/K,DEMO/K/N,CMD/K,GET/K,INFO/S,IMAGE/K,ANIM/K,
         ENLARGE/S,DELAY/K/N,FRAMES/K/N
```

| Option | Bedeutung |
|---|---|
| `HOST` | IP-Adresse oder Name des Pixoo |
| `ON` / `OFF` | Display an/aus |
| `BRIGHTNESS=n` | Helligkeit 0–100 |
| `CHANNEL=n` | 0 Zifferblätter, 1 Cloud, 2 Visualizer, 3 eigene Seiten |
| `CLOCK=n` | Nummer des Zifferblatts |
| `FILL=RRGGBB` | Display mit einer Farbe füllen |
| `TEST` | Testbild anzeigen |
| `FONT=name/größe` | Amiga-Font für `TEXT`, `TIME` und `CHARSET`, z. B. `FONT=SevenAlone/7` |
| `CHARSET` | alle Zeichen des aktuellen Fonts anzeigen |
| `IMAGE=datei` | Bild zeichnen, auf Displaygröße verkleinert |
| `ANIM=datei` | GIF-Animation abspielen; über dem, was `FILL`, `TEST`, `IMAGE`, `TEXT` … gezeichnet haben |
| `ENLARGE` | kleine Bilder auch auf Displaygröße vergrößern |
| `DELAY=n` | Millisekunden pro Bild von `ANIM` (Vorgabe: Mittelwert der Datei) |
| `FRAMES=n` | höchstens so viele Bilder von `ANIM` senden, 1–59 (Vorgabe 59; längere Animationen werden ausgedünnt) |
| `TEXT=text` | Text an `X`,`Y` (Vorgabe 1,1) in `COLOR` zeichnen; `*n` beginnt eine neue Zeile |
| `TIME` | aktuelle Uhrzeit (HH:MM) zentriert in `COLOR` anzeigen |
| `SCROLL=text` | Lauftext, vom Gerät gezeichnet, an `X`,`Y` (Vorgabe 0,28) in `COLOR`, über dem zuletzt gesendeten Bild; das vorher mit `FILL`, `TEST` oder `TEXT` setzen (ein neues Bild entfernt alle Lauftexte) |
| `TEXTID=n` | Nummer des Lauftexts, 0–19 (Vorgabe 0); eine andere Nummer fügt einen zweiten Text hinzu |
| `DEVFONT=n` | Font des Geräts für `SCROLL`, 0–7 (Vorgabe 0); Amiga-Fonts sind hier nicht möglich |
| `WIDTH=n` | Breite des Laufbereichs, 16–64 (Vorgabe 64) |
| `SPEED=n` | Millisekunden pro Schritt, 0–100 (Vorgabe 100; kleiner ist schneller) |
| `DIR=LEFT\|RIGHT` | Laufrichtung (Vorgabe LEFT; von der getesteten Pixoo-64-Firmware ignoriert) |
| `ALIGN=LEFT\|CENTER\|RIGHT` | Ausrichtung im Textbereich (Vorgabe LEFT) |
| `CLEARTEXT` | Lauftexte des Geräts entfernen |
| `X=n`, `Y=n`, `COLOR=RRGGBB` | Position und Farbe für `TEXT` und `SCROLL` (Vorgabe FFFFFF) |
| `DEMO=n` | springendes Quadrat für n Bilder, gibt Bilder/s aus |
| `CMD=json` | JSON-Befehl senden und Antwort ausgeben |
| `GET=key` | eine Einstellung ausgeben, z. B. `GET=Brightness` |
| `INFO` | alle Einstellungen ausgeben |
| `PORT`, `TIMEOUT`, `SIZE` | Port (80), Timeout in s (5), Displaygröße (64) |

`FILL`, `TEST`, `CHARSET`, `IMAGE`, `TEXT` und `TIME` zeichnen übereinander; das Ergebnis wird
am Ende einmal gesendet (oder dient `ANIM` als Hintergrund).

Beispiele: `PixooCtl 192.168.1.50 BRIGHTNESS=40 TEST`,
`PixooCtl 192.168.1.50 FILL=000040 TEXT="Hallo*nAmiga" COLOR=FFFF00`,
`PixooCtl 192.168.1.50 FONT=topaz/8 TIME COLOR=00FF00`,
`PixooCtl 192.168.1.50 IMAGE=Boing.iff TEXT=Boing!`,
`PixooCtl 192.168.1.50 FILL=000000 ANIM=katze.gif FRAMES=20`

## Bauen

Cross-Build mit [bebbos amiga-gcc](https://github.com/bebbo/amiga-gcc) unter `/opt/amiga`:

```sh
git submodule update --init
make            # bin/pixoo.library, bin/PixooCtl
make check      # Host-Test gegen einen Mock-Pixoo (Python 3)
make docs       # API-Referenz mit Doxygen
```

Die Schnittstelle ist in `include/libraries/pixoo.h` und `include/clib/pixoo_protos.h`
dokumentiert.

## Lizenz

MIT, siehe `LICENSE`. Die HTTP-Schnittstelle des Pixoo ist nicht offiziell dokumentiert; das
Projekt steht in keiner Verbindung zu Divoom.
