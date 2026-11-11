# pixoo.library

An AmigaOS 3.x shared library for controlling a **Divoom Pixoo 64** (also Pixoo 16/32) over the
network, plus the Shell tool **PixooCtl**. It was inspired by the Python library
[pixoo](https://pypi.org/project/pixoo/).

## Features

- Display on/off, brightness, channel, clock face
- Read device settings (`Channel/GetAllConf`)
- Send any JSON command and get the answer back
- 64×64 framebuffer (16/32 also supported) with pixels, lines, rectangles, text and direct
  buffer access; `PixooPush()` shows it on the display
- Built-in proportional pixel font (5 px, German umlauts, fixed-width digits for clocks)
- Any Amiga bitmap font (`OpenDiskFont()`) for text in the framebuffer
- Scrolling text rendered by the device itself
- Works with any bsdsocket.library TCP/IP stack (Roadshow, AmiTCP, Miami). Plain HTTP, so no
  SSL library is needed.
- Timeouts, and Ctrl-C interrupts network calls

## Requirements

- AmigaOS 3.0 or newer (developed for 3.2), 68000 or better
- A running TCP/IP stack
- A Pixoo 64 on the same network

## PixooCtl

```
PixooCtl HOST/A,PORT/K/N,TIMEOUT/K/N,SIZE/K/N,ON/S,OFF/S,BRIGHTNESS=BRI/K/N,
         CHANNEL/K/N,CLOCK/K/N,FONT/K,FILL/K,TEST/S,CHARSET/S,TEXT/K,X/K/N,Y/K/N,
         COLOR/K,TIME/S,CLEARTEXT/S,SCROLL/K,DEMO/K/N,CMD/K,GET/K,INFO/S
```

| Option | Meaning |
|---|---|
| `HOST` | IP address or name of the Pixoo |
| `ON` / `OFF` | switch the display on/off |
| `BRIGHTNESS=n` | brightness 0–100 |
| `CHANNEL=n` | 0 clock faces, 1 cloud, 2 visualizer, 3 custom |
| `CLOCK=n` | clock face id |
| `FILL=RRGGBB` | fill the display with one colour |
| `TEST` | show a test pattern |
| `FONT=name/size` | Amiga font for `TEXT`, `TIME` and `CHARSET`, e.g. `FONT=SevenAlone/7` |
| `CHARSET` | show all characters of the current font |
| `TEXT=text` | draw text at `X`,`Y` (default 1,1) in `COLOR` and show it; `*n` starts a new line |
| `TIME` | show the current time (HH:MM) centred in `COLOR` |
| `SCROLL=text` | scrolling text drawn by the device at `X`,`Y` (default 0,28) in `COLOR` |
| `CLEARTEXT` | remove the device's scrolling texts |
| `X=n`, `Y=n`, `COLOR=RRGGBB` | position and colour for `TEXT` and `SCROLL` (default FFFFFF) |
| `DEMO=n` | bouncing square for n frames, prints frames/s |
| `CMD=json` | send a raw JSON command and print the answer |
| `GET=key` | print one setting, e.g. `GET=Brightness` |
| `INFO` | print all settings |
| `PORT`, `TIMEOUT`, `SIZE` | port (80), timeout in s (5), display size (64) |

Examples: `PixooCtl 192.168.1.50 BRIGHTNESS=40 TEST`,
`PixooCtl 192.168.1.50 FILL=000040 TEXT="Hello*nAmiga" COLOR=FFFF00`,
`PixooCtl 192.168.1.50 FONT=topaz/8 TIME COLOR=00FF00`

## Building

Cross build with [bebbo's amiga-gcc](https://github.com/bebbo/amiga-gcc) in `/opt/amiga`:

```sh
git submodule update --init
make            # bin/pixoo.library, bin/PixooCtl
make check      # host test against a mock Pixoo (Python 3)
make docs       # API reference with Doxygen
```

The API is documented in `include/libraries/pixoo.h` and `include/clib/pixoo_protos.h`.

## License

MIT, see `LICENSE`. The Pixoo's HTTP interface is not officially documented, and this project is
not affiliated with Divoom.
