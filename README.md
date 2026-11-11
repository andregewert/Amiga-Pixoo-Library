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
- Pictures in any format the datatypes system reads (IFF, PNG, JPEG, …), scaled to fit
  with their aspect ratio kept; transparency is blended over the background
- GIF animations (decoded by the library, up to 59 frames) played by the device
- PixooCtl: slideshow of all pictures in a drawer
- Works with any bsdsocket.library TCP/IP stack (Roadshow, AmiTCP, Miami). Plain HTTP, so no
  SSL library is needed.
- Timeouts, and Ctrl-C interrupts network calls

## Requirements

- AmigaOS 3.0 or newer (developed for 3.2), 68000 or better
- For pictures other than GIF: picture.datatype V43 or newer (AmigaOS 3.5 and later)
- A running TCP/IP stack
- A Pixoo 64 on the same network

## PixooCtl

```
PixooCtl HOST/A,PORT/K/N,TIMEOUT/K/N,SIZE/K/N,ON/S,OFF/S,BRIGHTNESS=BRI/K/N,
         CHANNEL/K/N,CLOCK/K/N,FONT/K,FILL/K,TEST/S,CHARSET/S,TEXT/K,X/K/N,Y/K/N,
         COLOR/K,TIME/S,CLEARTEXT/S,SCROLL/K,TEXTID/K/N,DEVFONT/K/N,WIDTH/K/N,
         SPEED/K/N,DIR/K,ALIGN/K,DEMO/K/N,CMD/K,GET/K,INFO/S,IMAGE/K,ANIM/K,
         ENLARGE/S,DELAY/K/N,FRAMES/K/N,SHOW/K,PATTERN/K,SECONDS/K/N,REPEAT/S,
         RANDOM/S
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
| `IMAGE=file` | draw a picture, shrunk to fit the display |
| `ANIM=file` | play a GIF animation; drawn over what `FILL`, `TEST`, `IMAGE`, `TEXT` … drew |
| `ENLARGE` | also scale small pictures up to the display size |
| `DELAY=n` | milliseconds per frame of `ANIM` (default: average of the file) |
| `FRAMES=n` | most frames of `ANIM` to send, 1–59 (default 59; longer animations are thinned out) |
| `SHOW=drawer` | slideshow of the pictures in a drawer, sorted by name; Ctrl-C stops it |
| `PATTERN=pattern` | files for `SHOW`, e.g. `#?.(iff\|png)` (default all; icons are skipped) |
| `SECONDS=n` | display time per picture of `SHOW` (default 10) |
| `REPEAT` | start `SHOW` again after the last picture |
| `RANDOM` | show the pictures of `SHOW` in random order (new order for every round) |
| `TEXT=text` | draw text at `X`,`Y` (default 1,1) in `COLOR`; `*n` starts a new line |
| `TIME` | show the current time (HH:MM) centred in `COLOR` |
| `SCROLL=text` | scrolling text drawn by the device at `X`,`Y` (default 0,28) in `COLOR`, over the picture shown last; set one first with `FILL`, `TEST` or `TEXT` (a new picture removes all scrolling texts) |
| `TEXTID=n` | slot of the scrolling text, 0–19 (default 0); another id adds a second text |
| `DEVFONT=n` | font of the device for `SCROLL`, 0–7 (default 0); Amiga fonts cannot be used here |
| `WIDTH=n` | width of the scrolling area, 16–64 (default 64) |
| `SPEED=n` | milliseconds per scroll step, 0–100 (default 100; smaller is faster) |
| `DIR=LEFT\|RIGHT` | scroll direction (default LEFT; ignored by the Pixoo 64 firmware tested) |
| `ALIGN=LEFT\|CENTER\|RIGHT` | alignment within the text area (default LEFT) |
| `CLEARTEXT` | remove the device's scrolling texts |
| `X=n`, `Y=n`, `COLOR=RRGGBB` | position and colour for `TEXT` and `SCROLL` (default FFFFFF) |
| `DEMO=n` | bouncing square for n frames, prints frames/s |
| `CMD=json` | send a raw JSON command and print the answer |
| `GET=key` | print one setting, e.g. `GET=Brightness` |
| `INFO` | print all settings |
| `PORT`, `TIMEOUT`, `SIZE` | port (80), timeout in s (5), display size (64) |

`FILL`, `TEST`, `CHARSET`, `IMAGE`, `TEXT` and `TIME` draw on top of each other; the result is
sent once at the end (or used as the background of `ANIM` and `SHOW`).

Examples: `PixooCtl 192.168.1.50 BRIGHTNESS=40 TEST`,
`PixooCtl 192.168.1.50 FILL=000040 TEXT="Hello*nAmiga" COLOR=FFFF00`,
`PixooCtl 192.168.1.50 FONT=topaz/8 TIME COLOR=00FF00`,
`PixooCtl 192.168.1.50 IMAGE=Boing.iff TEXT=Boing!`,
`PixooCtl 192.168.1.50 FILL=000000 ANIM=cat.gif FRAMES=20`,
`PixooCtl 192.168.1.50 SHOW=Work:Pictures PATTERN=#?.iff SECONDS=20 REPEAT`

## Demo

The drawer `demo` holds a Shell script that shows the main features of PixooCtl one after
the other, with a few pictures for it (a bouncing ball, copper bars, a floppy disk, drawn for
this project). Start it in its drawer with the address of your Pixoo:

```
CD demo
Execute PixooDemo 192.168.1.50
```

## Building

Cross build with [bebbo's amiga-gcc](https://github.com/bebbo/amiga-gcc) in `/opt/amiga`:

```sh
git submodule update --init
make            # bin/pixoo.library, bin/PixooCtl, bin/demo
make check      # host test against a mock Pixoo (Python 3)
make docs       # API reference with Doxygen
```

The API is documented in `include/libraries/pixoo.h` and `include/clib/pixoo_protos.h`.

## License

MIT, see `LICENSE`. The Pixoo's HTTP interface is not officially documented, and this project is
not affiliated with Divoom.
