# pixoo.library

Eine Shared Library für AmigaOS 3.x, die ein **Divoom Pixoo 64** (auch Pixoo 16/32) über das
Netzwerk ansteuert, dazu das Shell-Programm **PixooCtl**. Vorbild ist die Python-Bibliothek
[pixoo](https://pypi.org/project/pixoo/).

## Funktionen

- Display an/aus, Helligkeit, Kanal, Zifferblatt
- Geräteeinstellungen auslesen (`Channel/GetAllConf`)
- Beliebige JSON-Befehle senden und die Antwort erhalten
- 64×64-Framebuffer (16/32 ebenfalls) mit Pixeln, Linien, Rechtecken und direktem Pufferzugriff;
  `PixooPush()` zeigt ihn an
- Läuft mit jedem TCP/IP-Stack mit bsdsocket.library (Roadshow, AmiTCP, Miami). Nur HTTP,
  daher keine SSL-Bibliothek nötig.
- Timeouts; Ctrl-C bricht Netzwerkzugriffe ab

## Voraussetzungen

- AmigaOS 3.0 oder neuer (entwickelt für 3.2), 68000 oder besser
- ein laufender TCP/IP-Stack
- ein Pixoo 64 im selben Netz

## PixooCtl

```
PixooCtl HOST/A,PORT/K/N,TIMEOUT/K/N,SIZE/K/N,ON/S,OFF/S,BRIGHTNESS=BRI/K/N,
         CHANNEL/K/N,CLOCK/K/N,FILL/K,TEST/S,DEMO/K/N,CMD/K,GET/K,INFO/S
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
| `DEMO=n` | springendes Quadrat für n Bilder, gibt Bilder/s aus |
| `CMD=json` | JSON-Befehl senden und Antwort ausgeben |
| `GET=key` | eine Einstellung ausgeben, z. B. `GET=Brightness` |
| `INFO` | alle Einstellungen ausgeben |
| `PORT`, `TIMEOUT`, `SIZE` | Port (80), Timeout in s (5), Displaygröße (64) |

Beispiel: `PixooCtl 192.168.1.50 BRIGHTNESS=40 TEST`

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
