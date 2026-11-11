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
 * PixooCtl 192.168.1.50 FILL=000040 TEXT="Hallo*nAmiga" X=2 Y=2 COLOR=FFFF00
 * PixooCtl 192.168.1.50 SCROLL="Lauftext vom Pixoo selbst" Y=28
 * PixooCtl 192.168.1.50 FONT=SevenAlone/7 TEXT="Mit Diskfont"
 * PixooCtl 192.168.1.50 FONT=topaz/8 FILL=000000 TIME COLOR=00FF00
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
#include <dos/datetime.h>
#include <exec/memory.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <diskfont/diskfont.h>
#include <proto/diskfont.h>
#include <proto/pixoo.h>

/** Version string for the Version command. */
static const char VersTag[] = "$VER: PixooCtl 1.0 (11.11.2026)";

/** Stack size, set by the libnix startup code (linked with ___stkinit). */
unsigned long __stack = 16384;

struct Library *PixooBase = NULL;       /**< pixoo.library */
struct GfxBase *GfxBase = NULL;         /**< graphics.library (CloseFont) */
struct Library *DiskfontBase = NULL;    /**< diskfont.library, only for FONT */

/** ReadArgs template. */
#define TEMPLATE "HOST/A,PORT/K/N,TIMEOUT/K/N,SIZE/K/N,ON/S,OFF/S,BRIGHTNESS=BRI/K/N," \
                 "CHANNEL/K/N,CLOCK/K/N,FONT/K,FILL/K,TEST/S,CHARSET/S,TEXT/K,X/K/N,Y/K/N,COLOR/K," \
                 "TIME/S,CLEARTEXT/S,SCROLL/K,DEMO/K/N,CMD/K,GET/K,INFO/S"

/** Positions of the arguments in the ReadArgs array. */
enum {
    ARG_HOST, ARG_PORT, ARG_TIMEOUT, ARG_SIZE, ARG_ON, ARG_OFF, ARG_BRIGHTNESS,
    ARG_CHANNEL, ARG_CLOCK, ARG_FONT, ARG_FILL, ARG_TEST, ARG_CHARSET, ARG_TEXT, ARG_X, ARG_Y, ARG_COLOR,
    ARG_TIME, ARG_CLEARTEXT, ARG_SCROLL, ARG_DEMO, ARG_CMD, ARG_GET, ARG_INFO,
    ARG_COUNT       /**< number of arguments */
};

/** Answer buffer for CMD and INFO. */
static char Reply[2048];

/** TEXT argument with "*n" turned into newlines. */
static char TextBuf[512];

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
 * @brief Show all characters of the built-in font and push them.
 * @param px The handle.
 * @return An error code.
 */
static LONG charset(struct PixooHandle *px)
{
    static const char *const lines[] = {
        "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "abcdefghijklm", "nopqrstuvwxyz",
        "0123456789:.,;", "!?\"#$%&'()*+-/", "<=>@[\\]^_`{|}~", "\xc4\xd6\xdc\xe4\xf6\xfc\xdf\xb0"
    };
    LONG i, line = PixooFontHeight(px, NULL) + 1;

    PixooFill(px, 0x000000);
    for (i = 0; i < 8; i++)
        PixooDrawText(px, 1, 1 + i * line, (CONST_STRPTR)lines[i], i & 1 ? 0xFFFFFF : 0xFFD040);
    return PixooPush(px);
}

/**
 * @brief Draw the current time (HH:MM) centred in the current font and
 *        push it.
 * @param px The handle.
 * @param rgb The colour.
 * @return An error code.
 */
static LONG show_time(struct PixooHandle *px, ULONG rgb)
{
    struct DateTime dt;
    char time[LEN_DATSTRING];
    LONG size, w, h;

    DateStamp(&dt.dat_Stamp);
    dt.dat_Format = FORMAT_DOS;
    dt.dat_Flags = 0;
    dt.dat_StrDay = NULL;
    dt.dat_StrDate = NULL;
    dt.dat_StrTime = (STRPTR)time;
    DateToStr(&dt);
    time[5] = 0;                        /* "HH:MM:SS" -> "HH:MM" */
    PixooGetBuffer(px, &size);
    w = PixooTextWidth(px, (CONST_STRPTR)time);
    h = PixooFontHeight(px, NULL);
    PixooDrawText(px, (size - w) / 2, (size - h) / 2, (CONST_STRPTR)time, rgb);
    return PixooPush(px);
}

/**
 * @brief Compare two names ignoring the case of ASCII letters.
 * @param a One name.
 * @param b The other.
 * @return Non-zero if they are equal.
 */
static int same_name(const char *a, const char *b)
{
    char ca, cb;

    do {
        ca = *a++;
        cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
    } while (ca && ca == cb);
    return ca == cb;
}

/**
 * @brief Replace a font name by its exact spelling from AvailFonts().
 *
 * OpenDiskFont() compares names case-sensitively with the fonts in memory
 * (topaz.font from ROM, fonts already loaded), while the file system
 * ignores case. So "Topaz.font" would not be found and "Times.font"
 * would be loaded once more although times.font is in memory.
 *
 * @param[in,out] name The name with ".font"; changed if a font in memory
 *                     or in FONTS: has the same name in another case.
 * @param cap Size of @p name.
 */
static void canonical_font_name(char *name, LONG cap)
{
    struct AvailFontsHeader *afh;
    struct AvailFonts *af;
    ULONG size = 4096;
    LONG more, i;

    for (;;) {
        if (!(afh = AllocVec(size, MEMF_ANY))) return;
        if ((more = AvailFonts(afh, (LONG)size, AFF_MEMORY | AFF_DISK)) <= 0) break;
        FreeVec(afh);
        size += more;
    }
    af = (struct AvailFonts *)(afh + 1);
    for (i = 0; i < afh->afh_NumEntries; i++, af++) {
        const char *n = (const char *)af->af_Attr.ta_Name;
        if (n && same_name(n, name)) {
            LONG k;
            for (k = 0; n[k] && k < cap - 1; k++) name[k] = n[k];
            name[k] = 0;
            break;
        }
    }
    FreeVec(afh);
}

/**
 * @brief Open a font given as "name/size" ("topaz/8", "SevenAlone.font/7");
 *        the case of the name does not matter.
 * @param spec The argument.
 * @return The font, or NULL (message printed).
 */
static struct TextFont *open_font(const char *spec)
{
    static char name[108];
    struct TextAttr ta;
    struct TextFont *tf;
    LONG n = 0, slash = -1, ysize = 8;

    while (spec[n] && n < (LONG)sizeof(name) - 6) {
        if (spec[n] == '/') slash = n;
        name[n] = spec[n];
        n++;
    }
    name[n] = 0;
    if (slash >= 0) {
        if (StrToLong((STRPTR)name + slash + 1, &ysize) < 0 || ysize < 1) {
            Printf("PixooCtl: FONT needs name/size, e.g. FONT=SevenAlone/7\n");
            return NULL;
        }
        name[n = slash] = 0;
    }
    if (n < 5 || name[n - 5] != '.' || (name[n - 4] | 0x20) != 'f' || (name[n - 3] | 0x20) != 'o' ||
        (name[n - 2] | 0x20) != 'n' || (name[n - 1] | 0x20) != 't') {
        name[n++] = '.'; name[n++] = 'f'; name[n++] = 'o'; name[n++] = 'n'; name[n++] = 't';
        name[n] = 0;
    }
    if (!DiskfontBase && !(DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 36))) {
        Printf("PixooCtl: cannot open diskfont.library\n");
        return NULL;
    }
    canonical_font_name(name, sizeof(name));
    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = (UWORD)ysize;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    if (!(tf = OpenDiskFont(&ta))) {
        Printf("PixooCtl: cannot open font %s %ld\n", (LONG)name, ysize);
        return NULL;
    }
    Printf("Font: %s %ld (%s, line height %ld)\n", (LONG)name, (LONG)tf->tf_YSize,
           (LONG)(tf->tf_Flags & FPF_PROPORTIONAL ? "proportional" : "fixed width"), (LONG)tf->tf_YSize);
    return tf;
}

/**
 * @brief Copy a ReadArgs string and turn "*n" into a newline (the Shell
 *        has no other way to pass one).
 * @param[out] dst The destination.
 * @param src The argument.
 * @param cap Size of @p dst.
 */
static void unescape(char *dst, const char *src, LONG cap)
{
    while (*src && cap > 1) {
        if (src[0] == '*' && (src[1] == 'n' || src[1] == 'N')) {
            *dst++ = '\n';
            src += 2;
        } else
            *dst++ = *src++;
        cap--;
    }
    *dst = 0;
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
    LONG err, rc = RETURN_OK, v, x, y;
    ULONG rgb, color = 0xFFFFFF;
    BOOL pushed = FALSE;
    struct TextFont *font = NULL;

    (void)VersTag;
    if (!(rda = ReadArgs((STRPTR)TEMPLATE, args, NULL))) {
        PrintFault(IoErr(), (STRPTR)"PixooCtl");
        return RETURN_FAIL;
    }
    if (!(GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36))) {
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    if (!(PixooBase = OpenLibrary((STRPTR)PIXOONAME, PIXOOVERSION)) &&
        !(PixooBase = OpenLibrary((STRPTR)"PROGDIR:" PIXOONAME, PIXOOVERSION))) {
        Printf("PixooCtl: cannot open %s V%ld\n", (LONG)PIXOONAME, (LONG)PIXOOVERSION);
        CloseLibrary((struct Library *)GfxBase);
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
        CloseLibrary((struct Library *)GfxBase);
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    x = args[ARG_X] ? *(LONG *)args[ARG_X] : -1;
    y = args[ARG_Y] ? *(LONG *)args[ARG_Y] : -1;
    if (args[ARG_COLOR] && !parse_rgb((const char *)args[ARG_COLOR], &color)) {
        Printf("PixooCtl: COLOR needs a colour as RRGGBB, e.g. COLOR=FFFF00\n");
        rc = RETURN_FAIL;
        goto done;
    }

    if (args[ARG_FONT]) {
        if (!(font = open_font((const char *)args[ARG_FONT]))) {
            rc = RETURN_FAIL;
            goto done;
        }
        PixooSetFont(px, font);
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
        pushed = TRUE;
    }
    if (args[ARG_TEST]) { STEP("Test pattern", test_pattern(px)); pushed = TRUE; }
    if (args[ARG_CHARSET]) { STEP("Character set", charset(px)); pushed = TRUE; }
    if (args[ARG_TEXT]) {
        unescape(TextBuf, (const char *)args[ARG_TEXT], sizeof(TextBuf));
        PixooDrawText(px, x >= 0 ? x : 1, y >= 0 ? y : 1, (CONST_STRPTR)TextBuf, color);
        STEP("Text", PixooPush(px));
        pushed = TRUE;
    }
    if (args[ARG_TIME]) { STEP("Time", show_time(px, color)); pushed = TRUE; }
    if (args[ARG_CLEARTEXT]) STEP("Clear text", PixooClearText(px));
    if (args[ARG_SCROLL]) {
        /* the device shows its text only over a picture sent with PixooPush() */
        if (!pushed) STEP("Background", PixooPush(px));
        STEP("Scroll text", PixooSendText(px, (CONST_STRPTR)args[ARG_SCROLL],
                                          PIXOOA_TextX, x >= 0 ? x : 0,
                                          PIXOOA_TextY, y >= 0 ? y : 28,
                                          PIXOOA_TextColor, color,
                                          TAG_DONE));
    }
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
    if (font) CloseFont(font);          /* after PixooClose(): no longer used */
    if (DiskfontBase) CloseLibrary(DiskfontBase);
    CloseLibrary(PixooBase);
    CloseLibrary((struct Library *)GfxBase);
    FreeArgs(rda);
    return rc;
}
