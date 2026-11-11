# pixoo.library – Hinweise für Claude

Shared Library für AmigaOS 3.2, die ein Divoom Pixoo 64 (auch 16/32) über seine inoffizielle
HTTP-Schnittstelle ansteuert (`POST http://<gerät>/post`, JSON). Vorbild ist die Python-Bibliothek
`pixoo` (PyPI). Cross-kompiliert mit bebbos amiga-gcc; Netzwerk über bsdsocket.library, kein SSL.

Gemeinsame Regeln aller Amiga-Projekte (Zeichensatz, Bauen, Testen, Versionen, Aminet-Paket,
Icons) kommen aus dem Submodule `amiga-devkit`:

@amiga-devkit/docs/Amiga-Regeln.md

Regeln für Dokumentationskommentare (Doxygen, `make docs`):

@amiga-devkit/docs/Dokumentation.md

## Aufbau

| Datei | Inhalt |
|---|---|
| `src/pixoo_lib.c` | Library-Rahmen (RomTag, Init/Open/Close/Expunge), Funktionstabelle, dünne Register-Wrapper |
| `src/pixoo_dev.c` | portabel: Requests auf einem Handle, Antwortprüfung (`error_code`), Push mit PicID-Reset |
| `src/pixoo_core.c` | portabel: Framebuffer-Zeichnen, Base64, JSON-Bodies, HTTP-Header und -Antworten |
| `src/pixoo_net.c` | TCP: bsdsocket.library (Amiga) bzw. POSIX (`PIXOO_HOST`), nicht blockierend mit `WaitSelect()` |
| `src/pixoo_private.h` | Version (`LIBVERSION`, `LIBREVISION`, `LIBDATE`), `struct PixooHandle`, `pxd_*` |
| `include/libraries/pixoo.h` | öffentlich: Überblick, Tags, Fehlercodes, Kanäle |
| `include/clib/pixoo_protos.h` | **von Hand** geschrieben, Referenz aller Funktionen (Doxygen) |
| `sfd/pixoo_lib.sfd` | Schnittstelle; `include/proto`, `inline`, `fd` mit `make headers` (sfdc) erzeugt |
| `demo/pixooctl.c` | PixooCtl, Shell-Testprogramm (ReadArgs, keine stdio) |
| `test/hosttest.c`, `test/mockpixoo.py` | Host-Test gegen einen Mock-Pixoo; `test/requests.expected` ist das erwartete Befehlsprotokoll |

## Regeln

- Neue Library-Funktion: in `sfd/pixoo_lib.sfd` **hinten** anfügen (LVO-Reihenfolge nie
  ändern), in derselben Reihenfolge in `FuncTable` (`src/pixoo_lib.c`), dokumentiert in
  `include/clib/pixoo_protos.h`, dann `make headers`.
- Logik gehört nach `pixoo_core.c` / `pixoo_dev.c` (portabel, ohne C-Bibliothek: die Library
  linkt ohne Startup-Code), damit `make check` sie auf dem Host testet. `pixoo_lib.c` enthält
  nur Wrapper mit Registerparametern.
- Ein Handle gehört der Task, die es geöffnet hat (bsdsocket.library ist pro Task).
- Der echte Pixoo 64 antwortet mit `Transfer-Encoding: chunked` (ohne Content-Length) und
  schließt die Verbindung trotz `Connection: close` nicht: Das Ende der Antwort ist nur am
  letzten, leeren Chunk erkennbar (`pixoo_net.c`). Der Mock verhält sich genauso.
- Befehle und Parameter der Geräte-API sind inoffiziell; Quelle ist die Python-Bibliothek
  `pixoo` bzw. Divooms Teil-Doku. Unbestätigte Annahmen im Code kennzeichnen.

## Bauen und Testen

- `make`: `bin/pixoo.library` und `bin/PixooCtl` (enthält `charcheck`).
- `make check`: Host-Test (ASan/UBSan) gegen `test/mockpixoo.py` auf Port 18080; vergleicht
  das Befehlsprotokoll des Mocks mit `test/requests.expected` (`make check-update` erneuert es,
  Diff prüfen). Das zuletzt gesendete Bild liegt danach in `build/test/frame.ppm`.
- `./test/hosttest <ip> 80 real`: harmlose Befehle gegen ein echtes Gerät (Einstellungen lesen,
  Helligkeit auf den aktuellen Wert setzen).
- `make docs` / `make docs-lib`: Doxygen, schlägt bei jeder Warnung fehl.
- Auf dem Amiga testet der Benutzer mit dem echten Gerät. Zum Probieren im Emulator ohne Gerät:
  `python3 test/mockpixoo.py --bind 0.0.0.0 --port 8080` und
  `PixooCtl <host-ip> PORT=8080 TEST`.
