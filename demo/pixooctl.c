/**
 * @file pixooctl.c
 * @brief PixooCtl: Shell test program for pixoo.library.
 *
 * Usage (ReadArgs template in #TEMPLATE):
 *
 * @code
 * PixooCtl 192.168.1.50 BRIGHTNESS=40
 * PixooCtl 192.168.1.50 OFF
 * PixooCtl 192.168.1.50 FILL=FF8000
 * PixooCtl 192.168.1.50 TEST
 * PixooCtl 192.168.1.50 DEMO=100
 * PixooCtl 192.168.1.50 GET=Brightness
 * PixooCtl 192.168.1.50 CMD={"Command":"Channel/GetAllConf"}
 * @endcode
 *
 * Several options may be given at once; they run in the order of the
 * template. Opens pixoo.library from LIBS: or PROGDIR:.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/pixoo.h>

/** Version string for the Version command. */
static const char VersTag[] = "$VER: PixooCtl 1.0 (11.11.2026)";

/** Stack size, set by the libnix startup code (linked with ___stkinit). */
unsigned long __stack = 16384;

struct Library *PixooBase = NULL;       /**< pixoo.library */

/** ReadArgs template. */
#define TEMPLATE "HOST/A,PORT/K/N,TIMEOUT/K/N,SIZE/K/N,ON/S,OFF/S,BRIGHTNESS=BRI/K/N," \
                 "CHANNEL/K/N,CLOCK/K/N,FILL/K,TEST/S,DEMO/K/N,CMD/K,GET/K,INFO/S"

/** Positions of the arguments in the ReadArgs array. */
enum {
    ARG_HOST, ARG_PORT, ARG_TIMEOUT, ARG_SIZE, ARG_ON, ARG_OFF, ARG_BRIGHTNESS,
    ARG_CHANNEL, ARG_CLOCK, ARG_FILL, ARG_TEST, ARG_DEMO, ARG_CMD, ARG_GET, ARG_INFO,
    ARG_COUNT       /**< number of arguments */
};

/** Answer buffer for CMD and INFO. */
static char Reply[2048];

/**
 * @brief Print the outcome of a step.
 * @param what Name of the step.
 * @param err Its error code.
 * @return Non-zero if it failed.
 */
static int report(const char *what, LONG err)
{
    if (err == PIXOOERR_OK) {
        Printf("%s: ok\n", (LONG)what);
        return 0;
    }
    Printf("%s: %s (%ld)\n", (LONG)what, (LONG)PixooErrorString(err), err);
    return err != PIXOOERR_TOOBIG;
}

/**
 * @brief Parse a colour as hex RRGGBB, with optional "#", "$" or "0x".
 * @param s The text.
 * @param[out] rgb Receives the colour.
 * @return Non-zero if the text is a valid colour.
 */
static int parse_rgb(const char *s, ULONG *rgb)
{
    ULONG v = 0;
    int n = 0;
    char c;

    if (*s == '#' || *s == '$') s++;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while ((c = *s++)) {
        if (c >= '0' && c <= '9') c -= '0';
        else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
        else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
        else return 0;
        v = (v << 4) | (ULONG)c;
        if (++n > 6) return 0;
    }
    *rgb = v;
    return n == 6;
}

/**
 * @brief Draw a test pattern and push it.
 *
 * Colour gradient written straight into the framebuffer, a white frame,
 * red and green diagonals and a yellow square in the middle; the corners
 * show whether the picture is rotated or mirrored.
 *
 * @param px The handle.
 * @return An error code.
 */
static LONG test_pattern(struct PixooHandle *px)
{
    LONG size, x, y, step;
    UBYTE *fb = PixooGetBuffer(px, &size);
    LONG last = size - 1, m = size / 2;

    step = 256 / size;
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++) {
            *fb++ = (UBYTE)(x * step);
            *fb++ = (UBYTE)(y * step);
            *fb++ = 0x40;
        }
    PixooDrawRect(px, 0, 0, last, last, 0xFFFFFF);
    PixooDrawLine(px, 0, 0, last, last, 0xFF0000);
    PixooDrawLine(px, 0, last, last, 0, 0x00FF00);
    PixooFillRect(px, m - size / 8, m - size / 8, m + size / 8 - 1, m + size / 8 - 1, 0xFFFF00);
    PixooFillRect(px, 2, 2, 4, 4, 0x00FFFF);    /* top left marker */
    return PixooPush(px);
}

/**
 * @brief Bouncing square for a number of frames; prints the frame rate.
 * @param px The handle.
 * @param frames Number of frames.
 * @return An error code.
 */
static LONG demo(struct PixooHandle *px, LONG frames)
{
    struct DateStamp t0, t1;
    LONG size, x = 3, y = 11, dx = 1, dy = 1, n, ticks, err = PIXOOERR_OK, s = 8;
    static const ULONG colours[] = { 0xFF4040, 0x40FF40, 0x4040FF, 0xFFFF40, 0xFF40FF, 0x40FFFF };

    PixooGetBuffer(px, &size);
    if (s > size / 4) s = size / 4;
    DateStamp(&t0);
    for (n = 0; n < frames; n++) {
        if (CheckSignal(SIGBREAKF_CTRL_C)) {
            err = PIXOOERR_BREAK;
            break;
        }
        PixooFill(px, 0x000000);
        PixooDrawRect(px, 0, 0, size - 1, size - 1, 0x202020);
        PixooFillRect(px, x, y, x + s - 1, y + s - 1, colours[(n / 16) % 6]);
        if ((err = PixooPush(px))) break;
        x += dx;
        y += dy;
        if (x <= 1 || x + s >= size - 1) dx = -dx;
        if (y <= 1 || y + s >= size - 1) dy = -dy;
    }
    DateStamp(&t1);
    ticks = (t1.ds_Days - t0.ds_Days) * 24 * 60 * 60 * TICKS_PER_SECOND +
            (t1.ds_Minute - t0.ds_Minute) * 60 * TICKS_PER_SECOND + (t1.ds_Tick - t0.ds_Tick);
    if (n > 0 && ticks > 0)
        Printf("%ld frames in %ld.%02ld s = %ld.%ld frames/s\n", n, ticks / TICKS_PER_SECOND,
               (ticks % TICKS_PER_SECOND) * 2, n * TICKS_PER_SECOND / ticks,
               (n * TICKS_PER_SECOND * 10 / ticks) % 10);
    return err;
}

/** Run a step, report it and stop at the first failure (in main()). */
#define STEP(name, call) do { if (report(name, call)) { rc = RETURN_WARN; goto done; } } while (0)

/**
 * @brief Entry point.
 * @return RETURN_OK, RETURN_WARN (a command failed) or RETURN_FAIL.
 */
int main(void)
{
    LONG args[ARG_COUNT] = { 0 };
    struct RDArgs *rda;
    struct PixooHandle *px;
    LONG err, rc = RETURN_OK, v;
    ULONG rgb;

    (void)VersTag;
    if (!(rda = ReadArgs((STRPTR)TEMPLATE, args, NULL))) {
        PrintFault(IoErr(), (STRPTR)"PixooCtl");
        return RETURN_FAIL;
    }
    if (!(PixooBase = OpenLibrary((STRPTR)PIXOONAME, PIXOOVERSION)) &&
        !(PixooBase = OpenLibrary((STRPTR)"PROGDIR:" PIXOONAME, PIXOOVERSION))) {
        Printf("PixooCtl: cannot open %s V%ld\n", (LONG)PIXOONAME, (LONG)PIXOOVERSION);
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    px = PixooOpen((CONST_STRPTR)args[ARG_HOST],
                   args[ARG_PORT] ? PIXOOA_Port : TAG_IGNORE, args[ARG_PORT] ? *(LONG *)args[ARG_PORT] : 0,
                   args[ARG_TIMEOUT] ? PIXOOA_Timeout : TAG_IGNORE, args[ARG_TIMEOUT] ? *(LONG *)args[ARG_TIMEOUT] : 0,
                   args[ARG_SIZE] ? PIXOOA_Size : TAG_IGNORE, args[ARG_SIZE] ? *(LONG *)args[ARG_SIZE] : 0,
                   PIXOOA_ErrorPtr, (ULONG)&err,
                   TAG_DONE);
    if (!px) {
        Printf("PixooCtl: %s (%ld)\n", (LONG)PixooErrorString(err), err);
        CloseLibrary(PixooBase);
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    if (args[ARG_ON]) STEP("Screen on", PixooSetScreen(px, TRUE));
    if (args[ARG_OFF]) STEP("Screen off", PixooSetScreen(px, FALSE));
    if (args[ARG_BRIGHTNESS]) STEP("Brightness", PixooSetBrightness(px, *(LONG *)args[ARG_BRIGHTNESS]));
    if (args[ARG_CHANNEL]) STEP("Channel", PixooSetChannel(px, *(LONG *)args[ARG_CHANNEL]));
    if (args[ARG_CLOCK]) STEP("Clock face", PixooSetClockFace(px, *(LONG *)args[ARG_CLOCK]));
    if (args[ARG_FILL]) {
        if (!parse_rgb((const char *)args[ARG_FILL], &rgb)) {
            Printf("PixooCtl: FILL needs a colour as RRGGBB, e.g. FILL=FF8000\n");
            rc = RETURN_FAIL;
            goto done;
        }
        PixooFill(px, rgb);
        STEP("Fill", PixooPush(px));
    }
    if (args[ARG_TEST]) STEP("Test pattern", test_pattern(px));
    if (args[ARG_DEMO]) STEP("Demo", demo(px, *(LONG *)args[ARG_DEMO]));
    if (args[ARG_CMD]) {
        err = PixooSendCommand(px, (CONST_STRPTR)args[ARG_CMD], (STRPTR)Reply, sizeof(Reply));
        if (Reply[0]) Printf("%s\n", (LONG)Reply);
        STEP("Command", err);
    }
    if (args[ARG_GET]) {
        err = PixooGetConfigValue(px, (CONST_STRPTR)args[ARG_GET], &v);
        if (!err) Printf("%s = %ld\n", args[ARG_GET], v);
        else STEP("Get", err);
    }
    if (args[ARG_INFO]) {
        err = PixooSendCommand(px, (CONST_STRPTR)"{\"Command\":\"Channel/GetAllConf\"}", (STRPTR)Reply, sizeof(Reply));
        if (Reply[0]) Printf("%s\n", (LONG)Reply);
        STEP("Info", err);
    }

done:
    PixooClose(px);
    CloseLibrary(PixooBase);
    FreeArgs(rda);
    return rc;
}
