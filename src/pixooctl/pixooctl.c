/**
 * @file pixooctl.c
 * @brief PixooCtl: Shell tool for pixoo.library.
 *
 * Usage (ReadArgs template in #TEMPLATE):
 *
 * @code
 * PixooCtl 192.168.1.50 BRIGHTNESS=40
 * PixooCtl 192.168.1.50 OFF
 * PixooCtl 192.168.1.50 FILL=FF8000
 * PixooCtl 192.168.1.50 TEST
 * PixooCtl 192.168.1.50 FILL=000040 TEXT="Hallo*nAmiga" X=2 Y=2 COLOR=FFFF00
 * PixooCtl 192.168.1.50 FILL=000020 SCROLL="Lauftext vom Pixoo selbst" Y=10
 * PixooCtl 192.168.1.50 SCROLL="Zweiter Text" Y=40 DEVFONT=2 SPEED=30 TEXTID=1
 * PixooCtl 192.168.1.50 FONT=SevenAlone/7 TEXT="Mit Diskfont"
 * PixooCtl 192.168.1.50 FONT=topaz/8 FILL=000000 TIME COLOR=00FF00
 * PixooCtl 192.168.1.50 IMAGE=Work:Bilder/Boing.iff
 * PixooCtl 192.168.1.50 IMAGE=Icon.png ENLARGE TEXT="Hallo" COLOR=FFFFFF
 * PixooCtl 192.168.1.50 FILL=000000 ANIM=Work:Bilder/Katze.gif FRAMES=20
 * PixooCtl 192.168.1.50 SHOW=Work:Bilder PATTERN=#?.(iff|png) SECONDS=20 REPEAT
 * PixooCtl 192.168.1.50 SHOW=Work:Bilder RANDOM REPEAT
 * PixooCtl 192.168.1.50 DEMO=100
 * PixooCtl 192.168.1.50 GET=Brightness
 * PixooCtl 192.168.1.50 CMD={"Command":"Channel/GetAllConf"}
 * @endcode
 *
 * Several options may be given at once; they run in the order of the
 * template. The drawing options FILL, TEST, CHARSET, IMAGE, TEXT and TIME
 * draw on top of each other into the framebuffer, which is sent once
 * after them, or serves as the background of ANIM and SHOW. Opens pixoo.library
 * from LIBS: or PROGDIR:.
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
                 "TIME/S,CLEARTEXT/S,SCROLL/K,TEXTID/K/N,DEVFONT/K/N,WIDTH/K/N,SPEED/K/N,DIR/K,ALIGN/K," \
                 "DEMO/K/N,CMD/K,GET/K,INFO/S,IMAGE/K,ANIM/K,ENLARGE/S,DELAY/K/N,FRAMES/K/N," \
                 "SHOW/K,PATTERN/K,SECONDS/K/N,REPEAT/S,RANDOM/S"

/** Positions of the arguments in the ReadArgs array. */
enum {
    ARG_HOST, ARG_PORT, ARG_TIMEOUT, ARG_SIZE, ARG_ON, ARG_OFF, ARG_BRIGHTNESS,
    ARG_CHANNEL, ARG_CLOCK, ARG_FONT, ARG_FILL, ARG_TEST, ARG_CHARSET, ARG_TEXT, ARG_X, ARG_Y, ARG_COLOR,
    ARG_TIME, ARG_CLEARTEXT, ARG_SCROLL, ARG_TEXTID, ARG_DEVFONT, ARG_WIDTH, ARG_SPEED, ARG_DIR,
    ARG_ALIGN, ARG_DEMO, ARG_CMD, ARG_GET, ARG_INFO, ARG_IMAGE, ARG_ANIM, ARG_ENLARGE, ARG_DELAY,
    ARG_FRAMES, ARG_SHOW, ARG_PATTERN, ARG_SECONDS, ARG_REPEAT, ARG_RANDOM,
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
 * @brief Draw a test pattern.
 *
 * Colour gradient written straight into the framebuffer, a white frame,
 * red and green diagonals and a yellow square in the middle; the corners
 * show whether the picture is rotated or mirrored.
 *
 * @param px The handle.
 */
static void test_pattern(struct PixooHandle *px)
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
}

/**
 * @brief Draw all characters of the built-in font.
 * @param px The handle.
 */
static void charset(struct PixooHandle *px)
{
    static const char *const lines[] = {
        "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "abcdefghijklm", "nopqrstuvwxyz",
        "0123456789:.,;", "!?\"#$%&'()*+-/", "<=>@[\\]^_`{|}~", "\xc4\xd6\xdc\xe4\xf6\xfc\xdf\xb0"
    };
    LONG i, line = PixooFontHeight(px, NULL) + 1;

    PixooFill(px, 0x000000);
    for (i = 0; i < 8; i++)
        PixooDrawText(px, 1, 1 + i * line, (CONST_STRPTR)lines[i], i & 1 ? 0xFFFFFF : 0xFFD040);
}

/**
 * @brief Draw the current time (HH:MM) centred in the current font.
 * @param px The handle.
 * @param rgb The colour.
 */
static void show_time(struct PixooHandle *px, ULONG rgb)
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
 * @brief Parse a keyword or its number (DIR, ALIGN).
 * @param arg The argument.
 * @param names Keywords, NULL-terminated; values[i] belongs to names[i].
 * @param values Their values.
 * @param lo Smallest number accepted instead of a keyword.
 * @param hi Largest number accepted.
 * @param[out] value Receives the value.
 * @return Non-zero if the argument is valid.
 */
static int parse_choice(const char *arg, const char *const *names, const LONG *values, LONG lo, LONG hi,
                        LONG *value)
{
    LONG i;

    for (i = 0; names[i]; i++)
        if (same_name(arg, names[i])) {
            *value = values[i];
            return 1;
        }
    if (StrToLong((STRPTR)arg, value) > 0 && *value >= lo && *value <= hi) return 1;
    return 0;
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
 * @brief Ticks (1/50 s) since a time stamp.
 * @param t0 The time stamp.
 * @return The ticks.
 */
static LONG ticks_since(const struct DateStamp *t0)
{
    struct DateStamp t1;

    DateStamp(&t1);
    return (t1.ds_Days - t0->ds_Days) * 24 * 60 * 60 * TICKS_PER_SECOND +
           (t1.ds_Minute - t0->ds_Minute) * 60 * TICKS_PER_SECOND + (t1.ds_Tick - t0->ds_Tick);
}

/**
 * @brief Wait until a number of ticks has passed since a time stamp.
 * @param t0 The time stamp.
 * @param ticks The ticks.
 * @return #PIXOOERR_OK, or #PIXOOERR_BREAK on Ctrl-C.
 */
static LONG wait_until(const struct DateStamp *t0, LONG ticks)
{
    LONG left;

    while ((left = ticks - ticks_since(t0)) > 0) {
        if (CheckSignal(SIGBREAKF_CTRL_C)) return PIXOOERR_BREAK;
        Delay(left < 10 ? left : 10);       /* look at Ctrl-C 5 times a second */
    }
    return PIXOOERR_OK;
}

/** A file of the slideshow; the list is sorted by name. */
struct Slide {
    struct Slide *next;         /**< next file */
    char name[1];               /**< file name (allocated longer) */
};

/**
 * @brief Compare two names ignoring the case of ASCII letters.
 * @param a One name.
 * @param b The other.
 * @return Less than, equal to or greater than 0, like strcmp().
 */
static int compare_names(const char *a, const char *b)
{
    int ca, cb;

    do {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
    } while (ca && ca == cb);
    return ca - cb;
}

/**
 * @brief Free the list of a slideshow.
 * @param s The first file, or NULL.
 */
static void free_slides(struct Slide *s)
{
    struct Slide *next;

    for (; s; s = next) {
        next = s->next;
        FreeVec(s);
    }
}

/**
 * @brief Collect the files of a drawer that match a pattern, sorted by name.
 *
 * Only files, no drawers; icons (".info") are always left out.
 *
 * @param dir The drawer.
 * @param pattern AmigaDOS pattern, compared without case (e.g. "#?.iff").
 * @param[out] count Receives the number of files.
 * @return The list (free_slides()), or NULL if no file matches or on an
 *         error (reported).
 */
static struct Slide *collect_slides(const char *dir, const char *pattern, LONG *count)
{
    struct Slide *list = NULL, *s, **pp;
    struct FileInfoBlock *fib = NULL;
    char *pat = NULL;
    BPTR lock = 0;
    LONG plen, n;

    *count = 0;
    for (plen = 0; pattern[plen]; plen++) ;
    plen = plen * 2 + 2;
    if (!(pat = AllocVec(plen, MEMF_ANY)) || !(fib = AllocDosObject(DOS_FIB, NULL))) {
        PrintFault(ERROR_NO_FREE_STORE, (STRPTR)"PixooCtl");
        goto done;
    }
    if (ParsePatternNoCase((STRPTR)pattern, (STRPTR)pat, plen) < 0) {
        Printf("PixooCtl: invalid PATTERN %s\n", (LONG)pattern);
        goto done;
    }
    if (!(lock = Lock((STRPTR)dir, ACCESS_READ)) || !Examine(lock, fib)) {
        PrintFault(IoErr(), (STRPTR)dir);
        goto done;
    }
    if (fib->fib_DirEntryType <= 0) {
        Printf("PixooCtl: %s is not a drawer\n", (LONG)dir);
        goto done;
    }
    while (ExNext(lock, fib)) {
        if (fib->fib_DirEntryType >= 0) continue;
        for (n = 0; fib->fib_FileName[n]; n++) ;
        if (n >= 5 && compare_names(fib->fib_FileName + n - 5, ".info") == 0) continue;
        if (!MatchPatternNoCase((STRPTR)pat, (STRPTR)fib->fib_FileName)) continue;
        if (!(s = AllocVec(sizeof(*s) + n, MEMF_ANY))) {
            PrintFault(ERROR_NO_FREE_STORE, (STRPTR)"PixooCtl");
            free_slides(list);
            list = NULL;
            *count = 0;
            goto done;
        }
        CopyMem(fib->fib_FileName, s->name, n + 1);
        for (pp = &list; *pp && compare_names((*pp)->name, s->name) <= 0; pp = &(*pp)->next) ;
        s->next = *pp;
        *pp = s;
        ++*count;
    }
    if (!list) Printf("PixooCtl: no files matching %s in %s\n", (LONG)pattern, (LONG)dir);

done:
    if (lock) UnLock(lock);
    if (fib) FreeDosObject(DOS_FIB, fib);
    if (pat) FreeVec(pat);
    return list;
}

/**
 * @brief Next pseudo-random number (xorshift32).
 * @param[in,out] state The generator state, never 0.
 * @return The number.
 */
static ULONG next_random(ULONG *state)
{
    ULONG x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

/**
 * @brief Put a slideshow list into random order (Fisher-Yates).
 *
 * If @p avoid would come first, it is swapped with the second file, so
 * that a repeated show does not show the same picture twice in a row.
 *
 * @param[in,out] list The list; receives the new first file.
 * @param count Number of files.
 * @param[in,out] state Random generator state.
 * @param avoid The file shown last, or NULL.
 */
static void shuffle_slides(struct Slide **list, LONG count, ULONG *state, const struct Slide *avoid)
{
    struct Slide **v, *s, *t;
    LONG i, j;

    if (count < 2 || !(v = AllocVec(count * sizeof(*v), MEMF_ANY))) return;
    for (i = 0, s = *list; i < count && s; i++, s = s->next) v[i] = s;
    count = i;
    for (i = count - 1; i > 0; i--) {
        j = (LONG)(next_random(state) % (ULONG)(i + 1));
        t = v[i];
        v[i] = v[j];
        v[j] = t;
    }
    if (v[0] == avoid) {
        t = v[0];
        v[0] = v[1];
        v[1] = t;
    }
    for (i = 0; i < count - 1; i++) v[i]->next = v[i + 1];
    v[count - 1]->next = NULL;
    *list = v[0];
    FreeVec(v);
}

/** Settings of a slideshow. */
struct ShowArgs {
    const char *dir;            /**< the drawer */
    LONG ticks;                 /**< display time per picture in 1/50 s */
    int repeat;                 /**< start again after the last picture */
    int enlarge;                /**< scale small pictures up */
    LONG delay;                 /**< milliseconds per frame of animations, 0 = from the file */
    LONG frames;                /**< most frames of animations, 0 = default */
    int random;                 /**< random order, shuffled again for every round */
    LONG count;                 /**< number of files */
};

/**
 * @brief Show the files of a list one after the other.
 *
 * The next picture is loaded and scaled into the framebuffer right after
 * the current one was sent, while it is on display, and sent when the
 * display time is over; so loading costs no time unless it takes longer
 * than the display time. GIF animations are sent when their turn comes
 * (decoding and sending take a moment) and are shown for the display
 * time after that. Every picture is drawn over a copy of the framebuffer
 * at the start. Files that cannot be loaded are reported and skipped.
 * In random order, the list is shuffled before each round.
 *
 * @param px The handle.
 * @param[in,out] list The files; reordered in random order.
 * @param a The settings.
 * @return An error code (#PIXOOERR_BREAK on Ctrl-C).
 */
static LONG slideshow(struct PixooHandle *px, struct Slide **list, const struct ShowArgs *a)
{
    struct DateStamp t0;
    struct Slide *s, *last = NULL;
    ULONG seed;
    char path[512];
    UBYTE *fb, *bg;
    LONG size, fbsize, err = PIXOOERR_OK, w, h, frames, shown = 0, first = 1, i;

    fb = PixooGetBuffer(px, &size);
    fbsize = size * size * 3;
    if (!(bg = AllocVec(fbsize, MEMF_ANY))) return PIXOOERR_NOMEM;
    CopyMem(fb, bg, fbsize);
    if (a->random) {
        DateStamp(&t0);
        seed = (ULONG)t0.ds_Days * 1440 * 3000 + (ULONG)t0.ds_Minute * 3000 + (ULONG)t0.ds_Tick;
        seed ^= (ULONG)FindTask(NULL);
        if (!seed) seed = 1;
        shuffle_slides(list, a->count, &seed, NULL);
    }
    s = *list;
    for (;;) {
        /* prepare the next picture while the current one is shown */
        for (i = 0; a->dir[i] && i < (LONG)sizeof(path) - 1; i++) path[i] = a->dir[i];
        path[i] = 0;
        AddPart((STRPTR)path, (STRPTR)s->name, sizeof(path));
        CopyMem(bg, fb, fbsize);
        w = h = frames = 0;
        err = PixooLoadImage(px, (CONST_STRPTR)path, PIXOOA_ImageEnlarge, a->enlarge,
                             PIXOOA_SourceWidth, (ULONG)&w, PIXOOA_SourceHeight, (ULONG)&h,
                             PIXOOA_SourceFrames, (ULONG)&frames, TAG_DONE);
        if (err == PIXOOERR_BREAK) break;
        if (err) {
            Printf("%s: %s, skipped\n", (LONG)s->name, (LONG)PixooErrorString(err));
            err = PIXOOERR_OK;
        } else {
            if (!first && (err = wait_until(&t0, a->ticks))) break;
            if (frames > 1) {
                CopyMem(bg, fb, fbsize);
                err = PixooLoadAnim(px, (CONST_STRPTR)path, PIXOOA_ImageEnlarge, a->enlarge,
                                    a->delay ? PIXOOA_AnimSpeed : TAG_IGNORE, a->delay,
                                    a->frames ? PIXOOA_AnimMaxFrames : TAG_IGNORE, a->frames,
                                    TAG_DONE);
                DateStamp(&t0);             /* it plays once all frames are there */
            } else {
                DateStamp(&t0);
                err = PixooPush(px);
            }
            if (err) {
                Printf("%s: %s\n", (LONG)s->name, (LONG)PixooErrorString(err));
                break;
            }
            if (frames > 1) Printf("%s (%ld x %ld, %ld frames)\n", (LONG)s->name, w, h, frames);
            else Printf("%s (%ld x %ld)\n", (LONG)s->name, w, h);
            first = 0;
            shown++;
            last = s;
        }
        if (!(s = s->next)) {
            if (!a->repeat) break;
            if (!shown) {
                Printf("PixooCtl: no picture could be shown\n");
                err = PIXOOERR_FORMAT;
                break;
            }
            if (a->random) shuffle_slides(list, a->count, &seed, last);
            s = *list;
            shown = 0;
        }
    }
    FreeVec(bg);
    return err;
}

/**
 * @brief Bouncing square for a number of frames; prints the frame rate.
 * @param px The handle.
 * @param frames Number of frames.
 * @return An error code.
 */
static LONG demo(struct PixooHandle *px, LONG frames)
{
    struct DateStamp t0;
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
    ticks = ticks_since(&t0);
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
    LONG err, rc = RETURN_OK, v, x, y, sw = 0, sh = 0, frames = 0;
    int drawn = 0;
    ULONG rgb, color = 0xFFFFFF;
    LONG dir = PIXOODIR_LEFT, align = PIXOOALIGN_LEFT;
    static const char *const dirnames[] = { "LEFT", "RIGHT", NULL };
    static const LONG dirvalues[] = { PIXOODIR_LEFT, PIXOODIR_RIGHT };
    static const char *const alignnames[] = { "LEFT", "CENTER", "CENTRE", "RIGHT", NULL };
    static const LONG alignvalues[] = { PIXOOALIGN_LEFT, PIXOOALIGN_CENTER, PIXOOALIGN_CENTER, PIXOOALIGN_RIGHT };
    struct TextFont *font = NULL;
    struct Slide *slides = NULL;

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

    if (args[ARG_DIR] && !parse_choice((const char *)args[ARG_DIR], dirnames, dirvalues, 0, 1, &dir)) {
        Printf("PixooCtl: DIR must be LEFT or RIGHT\n");
        rc = RETURN_FAIL;
        goto done;
    }
    if (args[ARG_ALIGN] && !parse_choice((const char *)args[ARG_ALIGN], alignnames, alignvalues, 1, 3, &align)) {
        Printf("PixooCtl: ALIGN must be LEFT, CENTER or RIGHT\n");
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
        drawn = 1;
    }
    if (args[ARG_TEST]) {
        test_pattern(px);
        drawn = 1;
    }
    if (args[ARG_CHARSET]) {
        charset(px);
        drawn = 1;
    }
    if (args[ARG_IMAGE]) {
        err = PixooLoadImage(px, (CONST_STRPTR)args[ARG_IMAGE],
                             PIXOOA_ImageEnlarge, args[ARG_ENLARGE] ? TRUE : FALSE,
                             PIXOOA_SourceWidth, (ULONG)&sw, PIXOOA_SourceHeight, (ULONG)&sh,
                             TAG_DONE);
        if (!err) Printf("Image: %ld x %ld pixels\n", sw, sh);
        STEP("Image", err);
        drawn = 1;
    }
    if (args[ARG_TEXT]) {
        unescape(TextBuf, (const char *)args[ARG_TEXT], sizeof(TextBuf));
        PixooDrawText(px, x >= 0 ? x : 1, y >= 0 ? y : 1, (CONST_STRPTR)TextBuf, color);
        drawn = 1;
    }
    if (args[ARG_TIME]) {
        show_time(px, color);
        drawn = 1;
    }
    /* ANIM and SHOW send their own pictures, over the framebuffer as background */
    if (drawn && !args[ARG_ANIM] && !args[ARG_SHOW]) STEP("Picture", PixooPush(px));
    if (args[ARG_ANIM]) {
        err = PixooLoadAnim(px, (CONST_STRPTR)args[ARG_ANIM],
                            PIXOOA_ImageEnlarge, args[ARG_ENLARGE] ? TRUE : FALSE,
                            args[ARG_DELAY] ? PIXOOA_AnimSpeed : TAG_IGNORE,
                            args[ARG_DELAY] ? *(LONG *)args[ARG_DELAY] : 0,
                            args[ARG_FRAMES] ? PIXOOA_AnimMaxFrames : TAG_IGNORE,
                            args[ARG_FRAMES] ? *(LONG *)args[ARG_FRAMES] : 0,
                            PIXOOA_SourceWidth, (ULONG)&sw, PIXOOA_SourceHeight, (ULONG)&sh,
                            PIXOOA_SourceFrames, (ULONG)&frames,
                            TAG_DONE);
        if (frames) Printf("Animation: %ld x %ld pixels, %ld frames\n", sw, sh, frames);
        STEP("Animation", err);
    }
    if (args[ARG_SHOW]) {
        struct ShowArgs sa;
        LONG n;

        sa.dir = (const char *)args[ARG_SHOW];
        sa.ticks = (args[ARG_SECONDS] ? *(LONG *)args[ARG_SECONDS] : 10) * TICKS_PER_SECOND;
        sa.repeat = args[ARG_REPEAT] != 0;
        sa.enlarge = args[ARG_ENLARGE] != 0;
        sa.delay = args[ARG_DELAY] ? *(LONG *)args[ARG_DELAY] : 0;
        sa.frames = args[ARG_FRAMES] ? *(LONG *)args[ARG_FRAMES] : 0;
        sa.random = args[ARG_RANDOM] != 0;
        if (sa.ticks < TICKS_PER_SECOND) {
            Printf("PixooCtl: SECONDS must be at least 1\n");
            rc = RETURN_FAIL;
            goto done;
        }
        if (!(slides = collect_slides(sa.dir, args[ARG_PATTERN] ? (const char *)args[ARG_PATTERN] : "#?", &n))) {
            rc = RETURN_WARN;
            goto done;
        }
        Printf("Slideshow: %ld files\n", n);
        sa.count = n;
        STEP("Slideshow", slideshow(px, &slides, &sa));
    }
    if (args[ARG_CLEARTEXT]) STEP("Clear text", PixooClearText(px));
    if (args[ARG_SCROLL]) {
        /* No picture is pushed here: every Draw/SendHttpGif removes all
         * texts of the device, so a second SCROLL call (another TEXTID)
         * would wipe the first. The text needs a picture sent before
         * (FILL, TEST, TEXT ... in this or an earlier call).          */
        STEP("Scroll text", PixooSendText(px, (CONST_STRPTR)args[ARG_SCROLL],
                                          PIXOOA_TextX, x >= 0 ? x : 0,
                                          PIXOOA_TextY, y >= 0 ? y : 28,
                                          PIXOOA_TextColor, color,
                                          PIXOOA_TextDir, dir,
                                          PIXOOA_TextAlign, align,
                                          args[ARG_TEXTID] ? PIXOOA_TextId : TAG_IGNORE,
                                          args[ARG_TEXTID] ? *(LONG *)args[ARG_TEXTID] : 0,
                                          args[ARG_DEVFONT] ? PIXOOA_TextFont : TAG_IGNORE,
                                          args[ARG_DEVFONT] ? *(LONG *)args[ARG_DEVFONT] : 0,
                                          args[ARG_WIDTH] ? PIXOOA_TextWidth : TAG_IGNORE,
                                          args[ARG_WIDTH] ? *(LONG *)args[ARG_WIDTH] : 0,
                                          args[ARG_SPEED] ? PIXOOA_TextSpeed : TAG_IGNORE,
                                          args[ARG_SPEED] ? *(LONG *)args[ARG_SPEED] : 0,
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
    free_slides(slides);
    PixooClose(px);
    if (font) CloseFont(font);          /* after PixooClose(): no longer used */
    if (DiskfontBase) CloseLibrary(DiskfontBase);
    CloseLibrary(PixooBase);
    CloseLibrary((struct Library *)GfxBase);
    FreeArgs(rda);
    return rc;
}
