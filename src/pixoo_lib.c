/**
 * @file pixoo_lib.c
 * @brief Library frame of pixoo.library (RomTag, Init/Open/Close/Expunge)
 *        and the library functions.
 *
 * The functions are documented in include/clib/pixoo_protos.h. They take
 * their arguments in registers as given in sfd/pixoo_lib.sfd. Drawing and
 * request building are in pixoo_core.c, the device commands in
 * pixoo_dev.c, the sockets in pixoo_net.c.
 *
 * The library keeps no global state besides the system library bases;
 * everything else is in the handle, which belongs to one task.
 * datatypes.library is opened for each PixooLoadImageA() call, so the
 * library works without it as long as no picture is loaded.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <graphics/text.h>
#include <intuition/classes.h>
#include <intuition/gadgetclass.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>

#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/datatypes.h>

#include <libraries/pixoo.h>
#include "pixoo_private.h"

/* must be the first code in the file: running the library as a program
 * returns immediately                                                    */
__asm__(
    "   .text\n"
    "   .globl _LibStart\n"
    "_LibStart:\n"
    "   moveq   #-1,d0\n"
    "   rts\n");

struct ExecBase *SysBase = NULL;        /**< exec.library */
struct Library  *UtilityBase = NULL;    /**< utility.library (tag lists) */
struct DosLibrary *DOSBase = NULL;      /**< dos.library (picture files) */

static BPTR SegList;                    /**< our segment list, returned by LibExpunge() */

#define STR(x) #x                       /**< makes a string of @p x */
#define XSTR(x) STR(x)                  /**< makes a string of the value of macro @p x */

static const char LibName[] = LIBNAME;  /**< name of the library */
/** Version string ($VER), also the library's ID string (from offset 6). */
static const char LibId[] = "$VER: " LIBNAME " " XSTR(LIBVERSION) "." XSTR(LIBREVISION) " (" LIBDATE ")\r\n";

static struct Library *LibInit(struct Library *base __asm("d0"), BPTR seglist __asm("a0"),
                               struct ExecBase *sysbase __asm("a6"));
static struct Library *LibOpen(struct Library *base __asm("a6"));
static BPTR LibClose(struct Library *base __asm("a6"));
static BPTR LibExpunge(struct Library *base __asm("a6"));
static ULONG LibNull(void);

static struct PixooHandle *LibPixooOpenA(CONST_STRPTR host __asm("a0"), struct TagItem *tags __asm("a1"));
static void LibPixooClose(struct PixooHandle *h __asm("a0"));
static CONST_STRPTR LibPixooErrorString(LONG error __asm("d0"));
static LONG LibPixooSendCommand(struct PixooHandle *h __asm("a0"), CONST_STRPTR json __asm("a1"),
                                STRPTR reply __asm("a2"), LONG replysize __asm("d0"));
static LONG LibPixooSetBrightness(struct PixooHandle *h __asm("a0"), LONG percent __asm("d0"));
static LONG LibPixooSetScreen(struct PixooHandle *h __asm("a0"), LONG on __asm("d0"));
static LONG LibPixooSetChannel(struct PixooHandle *h __asm("a0"), LONG channel __asm("d0"));
static LONG LibPixooSetClockFace(struct PixooHandle *h __asm("a0"), LONG id __asm("d0"));
static LONG LibPixooGetConfigValue(struct PixooHandle *h __asm("a0"), CONST_STRPTR key __asm("a1"),
                                   LONG *value __asm("a2"));
static void LibPixooFill(struct PixooHandle *h __asm("a0"), ULONG rgb __asm("d0"));
static void LibPixooSetPixel(struct PixooHandle *h __asm("a0"), LONG x __asm("d0"), LONG y __asm("d1"),
                             ULONG rgb __asm("d2"));
static ULONG LibPixooGetPixel(struct PixooHandle *h __asm("a0"), LONG x __asm("d0"), LONG y __asm("d1"));
static void LibPixooDrawLine(struct PixooHandle *h __asm("a0"), LONG x0 __asm("d0"), LONG y0 __asm("d1"),
                             LONG x1 __asm("d2"), LONG y1 __asm("d3"), ULONG rgb __asm("d4"));
static void LibPixooDrawRect(struct PixooHandle *h __asm("a0"), LONG x0 __asm("d0"), LONG y0 __asm("d1"),
                             LONG x1 __asm("d2"), LONG y1 __asm("d3"), ULONG rgb __asm("d4"));
static void LibPixooFillRect(struct PixooHandle *h __asm("a0"), LONG x0 __asm("d0"), LONG y0 __asm("d1"),
                             LONG x1 __asm("d2"), LONG y1 __asm("d3"), ULONG rgb __asm("d4"));
static UBYTE *LibPixooGetBuffer(struct PixooHandle *h __asm("a0"), LONG *size __asm("a1"));
static LONG LibPixooPush(struct PixooHandle *h __asm("a0"));
static LONG LibPixooDrawText(struct PixooHandle *h __asm("a0"), LONG x __asm("d0"), LONG y __asm("d1"),
                             CONST_STRPTR text __asm("a1"), ULONG rgb __asm("d2"));
static LONG LibPixooTextWidth(struct PixooHandle *h __asm("a0"), CONST_STRPTR text __asm("a1"));
static LONG LibPixooSendTextA(struct PixooHandle *h __asm("a0"), CONST_STRPTR text __asm("a1"),
                              struct TagItem *tags __asm("a2"));
static LONG LibPixooClearText(struct PixooHandle *h __asm("a0"));
static struct TextFont *LibPixooSetFont(struct PixooHandle *h __asm("a0"), struct TextFont *font __asm("a1"));
static LONG LibPixooFontHeight(struct PixooHandle *h __asm("a0"), LONG *baseline __asm("a1"));
static LONG LibPixooLoadImageA(struct PixooHandle *h __asm("a0"), CONST_STRPTR name __asm("a1"),
                               struct TagItem *tags __asm("a2"));
static LONG LibPixooLoadAnimA(struct PixooHandle *h __asm("a0"), CONST_STRPTR name __asm("a1"),
                              struct TagItem *tags __asm("a2"));

/** Function table: the four standard vectors, then the SFD order. */
static const APTR FuncTable[] = {
    (APTR)LibOpen,
    (APTR)LibClose,
    (APTR)LibExpunge,
    (APTR)LibNull,
    (APTR)LibPixooOpenA,
    (APTR)LibPixooClose,
    (APTR)LibPixooErrorString,
    (APTR)LibPixooSendCommand,
    (APTR)LibPixooSetBrightness,
    (APTR)LibPixooSetScreen,
    (APTR)LibPixooSetChannel,
    (APTR)LibPixooSetClockFace,
    (APTR)LibPixooGetConfigValue,
    (APTR)LibPixooFill,
    (APTR)LibPixooSetPixel,
    (APTR)LibPixooGetPixel,
    (APTR)LibPixooDrawLine,
    (APTR)LibPixooDrawRect,
    (APTR)LibPixooFillRect,
    (APTR)LibPixooGetBuffer,
    (APTR)LibPixooPush,
    (APTR)LibPixooDrawText,
    (APTR)LibPixooTextWidth,
    (APTR)LibPixooSendTextA,
    (APTR)LibPixooClearText,
    (APTR)LibPixooSetFont,
    (APTR)LibPixooFontHeight,
    (APTR)LibPixooLoadImageA,
    (APTR)LibPixooLoadAnimA,
    (APTR)-1
};

/** Table for RTF_AUTOINIT: base size, functions, no data, init routine. */
static const ULONG InitTable[4] = {
    sizeof(struct Library),
    (ULONG)FuncTable,
    0,
    (ULONG)LibInit
};

/** The resident structure that makes the file a library. */
const struct Resident ROMTag = {
    RTC_MATCHWORD,
    (struct Resident *)&ROMTag,
    (APTR)(&ROMTag + 1),
    RTF_AUTOINIT,
    LIBVERSION,
    NT_LIBRARY,
    0,
    (char *)LibName,
    (char *)LibId + 6,
    (APTR)InitTable
};

/* ------------------------------------------------------------------ */
/* library frame                                                       */

/** @brief Close the system libraries opened by LibInit(). */
static void close_libs(void)
{
    if (DOSBase) CloseLibrary((struct Library *)DOSBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    DOSBase = NULL;
    UtilityBase = NULL;
}

/**
 * @brief Initialise the library: needs exec, utility and dos V39.
 * @param base The library base.
 * @param seglist Our segment list.
 * @param sysbase exec.library.
 * @return The base, or NULL on failure (the base is freed then).
 */
static struct Library *LibInit(struct Library *base __asm("d0"), BPTR seglist __asm("a0"),
                               struct ExecBase *sysbase __asm("a6"))
{
    SysBase = sysbase;
    SegList = seglist;
    base->lib_Node.ln_Type = NT_LIBRARY;
    base->lib_Node.ln_Name = (char *)LibName;
    base->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    base->lib_Version = LIBVERSION;
    base->lib_Revision = LIBREVISION;
    base->lib_IdString = (APTR)(LibId + 6);

    if (sysbase->LibNode.lib_Version >= 39 &&
        (UtilityBase = OpenLibrary((STRPTR)"utility.library", 39)) &&
        (DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 39)))
        return base;

    close_libs();
    FreeMem((UBYTE *)base - base->lib_NegSize, base->lib_NegSize + base->lib_PosSize);
    return NULL;
}

/**
 * @brief OpenLibrary() vector.
 * @param base The library base.
 * @return The base.
 */
static struct Library *LibOpen(struct Library *base __asm("a6"))
{
    base->lib_OpenCnt++;
    base->lib_Flags &= ~LIBF_DELEXP;
    return base;
}

/**
 * @brief Expunge vector: removes the library unless it is open (then the
 *        expunge is delayed).
 * @param base The library base.
 * @return The segment list, or 0 if the library stays.
 */
static BPTR LibExpunge(struct Library *base __asm("a6"))
{
    BPTR seg;

    if (base->lib_OpenCnt) {
        base->lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    seg = SegList;
    Remove(&base->lib_Node);
    close_libs();
    FreeMem((UBYTE *)base - base->lib_NegSize, base->lib_NegSize + base->lib_PosSize);
    return seg;
}

/**
 * @brief CloseLibrary() vector; expunges a delayed expunge.
 * @param base The library base.
 * @return The segment list if the library was expunged, else 0.
 */
static BPTR LibClose(struct Library *base __asm("a6"))
{
    if (--base->lib_OpenCnt == 0 && (base->lib_Flags & LIBF_DELEXP))
        return LibExpunge(base);
    return 0;
}

/** @brief Reserved vector. @return 0. */
static ULONG LibNull(void)
{
    return 0;
}

/* ------------------------------------------------------------------ */
/* library functions (documented in clib/pixoo_protos.h)               */

/**
 * @brief Allocator of the handles (PxAllocFunc): AllocVec(), cleared.
 * @param ud Unused.
 * @param n Number of bytes.
 * @return The memory or NULL.
 */
static void *mem_alloc(void *ud, long n)
{
    return AllocVec((ULONG)n, MEMF_ANY | MEMF_CLEAR);
}

/**
 * @brief Free function of the handles (PxFreeFunc).
 * @param ud Unused.
 * @param p Memory from mem_alloc(), or NULL.
 */
static void mem_free(void *ud, void *p)
{
    if (p) FreeVec(p);
}

/** @copydoc PixooOpenA() */
static struct PixooHandle *LibPixooOpenA(CONST_STRPTR host __asm("a0"), struct TagItem *tags __asm("a1"))
{
    LONG *errp = (LONG *)GetTagData(PIXOOA_ErrorPtr, 0, tags);
    LONG size = (LONG)GetTagData(PIXOOA_Size, 64, tags);
    LONG timeout = (LONG)GetTagData(PIXOOA_Timeout, 5, tags);
    LONG port = (LONG)GetTagData(PIXOOA_Port, 80, tags);
    LONG limit = (LONG)GetTagData(PIXOOA_RefreshLimit, 32, tags);
    struct PixooHandle *h = NULL;
    LONG err;

    if (!host || !*host || (size != 16 && size != 32 && size != 64) ||
        port < 1 || port > 65535 || timeout < 1 || timeout > 120 || limit < 1 || limit > 1000 ||
        px_strlen((const char *)host) >= PX_HOSTLEN) {
        err = PIXOOERR_ARGS;
        goto done;
    }
    if (!(h = AllocVec(sizeof(*h) + PX_HANDLE_EXTRA(size), MEMF_ANY | MEMF_CLEAR))) {
        err = PIXOOERR_NOMEM;
        goto done;
    }
    pxd_setup(h, (unsigned char *)(h + 1), size, limit);
    h->alloc = mem_alloc;
    h->free = mem_free;
    px_strcpy(h->host, (const char *)host, sizeof(h->host));

    if ((err = pxn_init(&h->conn))) goto done;
    h->conn.port = port;
    h->conn.timeout = timeout;
    err = pxn_resolve(&h->conn, h->host);

done:
    if (err && h) {
        pxn_cleanup(&h->conn);
        FreeVec(h);
        h = NULL;
    }
    if (errp) *errp = err;
    return h;
}

/** @copydoc PixooClose() */
static void LibPixooClose(struct PixooHandle *handle __asm("a0"))
{
    if (!handle) return;
    pxn_cleanup(&handle->conn);
    FreeVec(handle);
}

/** @copydoc PixooErrorString() */
static CONST_STRPTR LibPixooErrorString(LONG error __asm("d0"))
{
    return (CONST_STRPTR)pxd_error_string(error);
}

/** @copydoc PixooSendCommand() */
static LONG LibPixooSendCommand(struct PixooHandle *handle __asm("a0"), CONST_STRPTR json __asm("a1"),
                                STRPTR reply __asm("a2"), LONG replysize __asm("d0"))
{
    return pxd_send_command(handle, (const char *)json, (char *)reply, replysize);
}

/** @copydoc PixooSetBrightness() */
static LONG LibPixooSetBrightness(struct PixooHandle *handle __asm("a0"), LONG percent __asm("d0"))
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return pxd_simple(handle, "Channel/SetBrightness", "Brightness", percent);
}

/** @copydoc PixooSetScreen() */
static LONG LibPixooSetScreen(struct PixooHandle *handle __asm("a0"), LONG on __asm("d0"))
{
    return pxd_simple(handle, "Channel/OnOffScreen", "OnOff", on ? 1 : 0);
}

/** @copydoc PixooSetChannel() */
static LONG LibPixooSetChannel(struct PixooHandle *handle __asm("a0"), LONG channel __asm("d0"))
{
    if (channel < 0 || channel > 3) return PIXOOERR_ARGS;
    return pxd_simple(handle, "Channel/SetIndex", "SelectIndex", channel);
}

/** @copydoc PixooSetClockFace() */
static LONG LibPixooSetClockFace(struct PixooHandle *handle __asm("a0"), LONG id __asm("d0"))
{
    return pxd_simple(handle, "Channel/SetClockSelectId", "ClockId", id);
}

/** @copydoc PixooGetConfigValue() */
static LONG LibPixooGetConfigValue(struct PixooHandle *handle __asm("a0"), CONST_STRPTR key __asm("a1"),
                                   LONG *value __asm("a2"))
{
    long v = 0;
    LONG err;

    if (!value) return PIXOOERR_ARGS;
    if (!(err = pxd_get_config(handle, (const char *)key, &v))) *value = v;
    return err;
}

/** @copydoc PixooFill() */
static void LibPixooFill(struct PixooHandle *handle __asm("a0"), ULONG rgb __asm("d0"))
{
    if (handle) px_fill(handle->fb, handle->size, rgb);
}

/** @copydoc PixooSetPixel() */
static void LibPixooSetPixel(struct PixooHandle *handle __asm("a0"), LONG x __asm("d0"), LONG y __asm("d1"),
                             ULONG rgb __asm("d2"))
{
    if (handle) px_set_pixel(handle->fb, handle->size, x, y, rgb);
}

/** @copydoc PixooGetPixel() */
static ULONG LibPixooGetPixel(struct PixooHandle *handle __asm("a0"), LONG x __asm("d0"), LONG y __asm("d1"))
{
    return handle ? px_get_pixel(handle->fb, handle->size, x, y) : 0;
}

/** @copydoc PixooDrawLine() */
static void LibPixooDrawLine(struct PixooHandle *handle __asm("a0"), LONG x0 __asm("d0"), LONG y0 __asm("d1"),
                             LONG x1 __asm("d2"), LONG y1 __asm("d3"), ULONG rgb __asm("d4"))
{
    if (handle) px_line(handle->fb, handle->size, x0, y0, x1, y1, rgb);
}

/** @copydoc PixooDrawRect() */
static void LibPixooDrawRect(struct PixooHandle *handle __asm("a0"), LONG x0 __asm("d0"), LONG y0 __asm("d1"),
                             LONG x1 __asm("d2"), LONG y1 __asm("d3"), ULONG rgb __asm("d4"))
{
    if (handle) px_rect(handle->fb, handle->size, x0, y0, x1, y1, rgb);
}

/** @copydoc PixooFillRect() */
static void LibPixooFillRect(struct PixooHandle *handle __asm("a0"), LONG x0 __asm("d0"), LONG y0 __asm("d1"),
                             LONG x1 __asm("d2"), LONG y1 __asm("d3"), ULONG rgb __asm("d4"))
{
    if (handle) px_fill_rect(handle->fb, handle->size, x0, y0, x1, y1, rgb);
}

/** @copydoc PixooGetBuffer() */
static UBYTE *LibPixooGetBuffer(struct PixooHandle *handle __asm("a0"), LONG *size __asm("a1"))
{
    if (size) *size = handle ? handle->size : 0;
    return handle ? handle->fb : NULL;
}

/** @copydoc PixooPush() */
static LONG LibPixooPush(struct PixooHandle *handle __asm("a0"))
{
    return pxd_push(handle);
}

/** @copydoc PixooDrawText() */
static LONG LibPixooDrawText(struct PixooHandle *handle __asm("a0"), LONG x __asm("d0"), LONG y __asm("d1"),
                             CONST_STRPTR text __asm("a1"), ULONG rgb __asm("d2"))
{
    if (!handle || !text) return 0;
    return px_text(handle->fb, handle->size, handle->font, x, y, (const char *)text, rgb);
}

/** @copydoc PixooTextWidth() */
static LONG LibPixooTextWidth(struct PixooHandle *handle __asm("a0"), CONST_STRPTR text __asm("a1"))
{
    if (!handle || !text) return 0;
    return px_text_width(handle->font, (const char *)text);
}

/** @copydoc PixooSendTextA() */
static LONG LibPixooSendTextA(struct PixooHandle *handle __asm("a0"), CONST_STRPTR text __asm("a1"),
                              struct TagItem *tags __asm("a2"))
{
    PxText t;

    t.text = (const char *)text;
    t.id = (LONG)GetTagData(PIXOOA_TextId, 0, tags);
    t.x = (LONG)GetTagData(PIXOOA_TextX, 0, tags);
    t.y = (LONG)GetTagData(PIXOOA_TextY, 0, tags);
    t.dir = (LONG)GetTagData(PIXOOA_TextDir, PIXOODIR_LEFT, tags);
    t.font = (LONG)GetTagData(PIXOOA_TextFont, 0, tags);
    t.width = (LONG)GetTagData(PIXOOA_TextWidth, 64, tags);
    t.speed = (LONG)GetTagData(PIXOOA_TextSpeed, 100, tags);
    t.rgb = GetTagData(PIXOOA_TextColor, 0xFFFFFF, tags);
    t.align = (LONG)GetTagData(PIXOOA_TextAlign, PIXOOALIGN_LEFT, tags);
    return pxd_send_text(handle, &t);
}

/** @copydoc PixooClearText() */
static LONG LibPixooClearText(struct PixooHandle *handle __asm("a0"))
{
    return pxd_simple(handle, "Draw/ClearHttpText", NULL, 0);
}

/** @copydoc PixooSetFont() */
static struct TextFont *LibPixooSetFont(struct PixooHandle *handle __asm("a0"), struct TextFont *font __asm("a1"))
{
    struct TextFont *old;
    PxFont *f;
    int i;

    if (!handle) return NULL;
    old = (struct TextFont *)handle->textfont;
    if (!font) {
        handle->font = &px_builtin_font;
        handle->textfont = NULL;
        return old;
    }
    f = &handle->diskfont;
    for (i = 0; i < 8; i++) f->planes[i] = NULL;
    f->nplanes = 0;
    if (font->tf_Style & FSF_COLORFONT) {
        /* all planes with data; a pixel counts if it is set in any */
        struct ColorTextFont *ctf = (struct ColorTextFont *)font;
        for (i = 0; i < 8; i++)
            if (ctf->ctf_CharData[i]) f->planes[f->nplanes++] = (const unsigned char *)ctf->ctf_CharData[i];
    }
    if (!f->nplanes) {
        f->planes[0] = (const unsigned char *)font->tf_CharData;
        f->nplanes = 1;
    }
    f->modulo = font->tf_Modulo;
    f->ysize = font->tf_YSize;
    f->baseline = font->tf_Baseline;
    f->xsize = font->tf_XSize;
    f->lochar = font->tf_LoChar;
    f->hichar = font->tf_HiChar;
    f->proportional = (font->tf_Flags & FPF_PROPORTIONAL) != 0;
    f->charloc = (const unsigned char *)font->tf_CharLoc;
    f->charspace = (const unsigned char *)font->tf_CharSpace;
    f->charkern = (const unsigned char *)font->tf_CharKern;
    handle->font = f;
    handle->textfont = font;
    return old;
}

/** @copydoc PixooFontHeight() */
static LONG LibPixooFontHeight(struct PixooHandle *handle __asm("a0"), LONG *baseline __asm("a1"))
{
    const PxFont *f = handle ? handle->font : &px_builtin_font;

    if (baseline) *baseline = f->baseline;
    return f->ysize;
}


/* ------------------------------------------------------------------ */
/* pictures                                                            */

/**
 * @brief Read the tags of PixooLoadImageA() / PixooLoadAnimA().
 * @param h The handle.
 * @param tags The tag list.
 * @param[out] a Receives the parameters.
 */
static void image_args(struct PixooHandle *h, struct TagItem *tags, PxImageArgs *a)
{
    a->x = (LONG)GetTagData(PIXOOA_ImageX, 0, tags);
    a->y = (LONG)GetTagData(PIXOOA_ImageY, 0, tags);
    a->w = (LONG)GetTagData(PIXOOA_ImageWidth, h->size, tags);
    a->h = (LONG)GetTagData(PIXOOA_ImageHeight, h->size, tags);
    a->enlarge = GetTagData(PIXOOA_ImageEnlarge, FALSE, tags) != 0;
    a->speed = (LONG)GetTagData(PIXOOA_AnimSpeed, 0, tags);
    a->maxframes = (LONG)GetTagData(PIXOOA_AnimMaxFrames, PX_MAXFRAMES, tags);
    a->srcw = a->srch = a->frames = 0;
}

/**
 * @brief Store the results in the pointers of the tag list.
 * @param tags The tag list.
 * @param a The results.
 */
static void image_results(struct TagItem *tags, const PxImageArgs *a)
{
    LONG *p;

    if ((p = (LONG *)GetTagData(PIXOOA_SourceWidth, 0, tags))) *p = a->srcw;
    if ((p = (LONG *)GetTagData(PIXOOA_SourceHeight, 0, tags))) *p = a->srch;
    if ((p = (LONG *)GetTagData(PIXOOA_SourceFrames, 0, tags))) *p = a->frames;
}

/**
 * @brief An error code for a failed DOS or datatypes call (IoErr()).
 * @return #PIXOOERR_NOMEM, #PIXOOERR_FORMAT or #PIXOOERR_FILE.
 */
static LONG dos_error(void)
{
    LONG e = IoErr();

    if (e == ERROR_NO_FREE_STORE) return PIXOOERR_NOMEM;
    if (e == ERROR_OBJECT_WRONG_TYPE || e == ERROR_NOT_A_DOS_DISK || e == ERROR_SEEK_ERROR ||
        (e >= DTERROR_UNKNOWN_DATATYPE && e < DTERROR_UNKNOWN_DATATYPE + 100))
        return PIXOOERR_FORMAT;
    return PIXOOERR_FILE;
}

/**
 * @brief Read a whole file into memory, or only check for a GIF.
 * @param name The file name.
 * @param[out] data Receives the file (FreeVec() it); NULL to only check
 *                  the signature.
 * @param[out] len Receives its length.
 * @param[out] gif Set if the file starts with a GIF signature.
 * @return An error code.
 */
static LONG read_file(CONST_STRPTR name, UBYTE **data, LONG *len, int *gif)
{
    UBYTE sig[6];
    BPTR fh;
    LONG n, err = PIXOOERR_OK;

    *gif = 0;
    if (data) *data = NULL;
    if (!(fh = Open(name, MODE_OLDFILE))) return dos_error();
    n = Read(fh, sig, 6);
    *gif = n == 6 && sig[0] == 'G' && sig[1] == 'I' && sig[2] == 'F' && sig[3] == '8';
    if (data && *gif) {
        /* Seek() returns the old position: the length */
        if (Seek(fh, 0, OFFSET_END) < 0 || (n = Seek(fh, 0, OFFSET_BEGINNING)) < 0)
            err = PIXOOERR_FILE;
        else if (!(*data = AllocVec((ULONG)n, MEMF_ANY)))
            err = PIXOOERR_NOMEM;
        else if (Read(fh, *data, n) != n) {
            FreeVec(*data);
            *data = NULL;
            err = PIXOOERR_FILE;
        } else
            *len = n;
    }
    Close(fh);
    return err;
}

/**
 * @brief Invoke a method on a BOOPSI object (DoMethodA() of amiga.lib,
 *        which the library cannot link).
 * @param o The object.
 * @param msg The message.
 * @return The result of the method.
 */
static ULONG do_method(Object *o, Msg msg)
{
    return CallHookPkt(&OCLASS(o)->cl_Dispatcher, o, msg);
}

/** Working memory of load_datatype(). */
typedef struct DtWork {
    PxScaler scaler;            /**< the scaler */
    UBYTE row[1];               /**< one row of the picture, 4 bytes per pixel */
} DtWork;

/**
 * @brief Draw a picture into the framebuffer through datatypes.
 *
 * Reads the picture row by row in ARGB with PDTM_READPIXELARRAY
 * (picture.datatype V43+) and feeds the rows to the scaler. The alpha
 * byte is only used when the picture has an alpha channel
 * (mskHasAlpha or PDTA_AlphaChannel of V47.9); datatypes need not fill
 * it otherwise. A picture with one transparent colour
 * (mskHasTransparentColor, e.g. an ILBM brush) arrives as plain RGB:
 * the colour bmh_Transparent is looked up in PDTA_CRegs and matched by
 * its RGB value (px_key_row()). Other pictures are opaque; a separate
 * mask plane (mskHasMask) is not used.
 *
 * @param h The handle.
 * @param name The file name.
 * @param a Box and results.
 * @return An error code.
 */
static LONG load_datatype(struct PixooHandle *h, CONST_STRPTR name, PxImageArgs *a)
{
    struct Library *DataTypesBase;
    struct BitMapHeader *bmh = NULL;
    struct pdtBlitPixelArray bpa;
    struct gpLayout gpl;
    ULONG alpha = FALSE, ncolors = 0, *cregs = NULL;
    LONG key = -1;
    Object *o;
    DtWork *w = NULL;
    LONG err, y, width, height;

    if (!(DataTypesBase = OpenLibrary((STRPTR)"datatypes.library", 39))) return PIXOOERR_NODATATYPES;
    o = NewDTObject((APTR)name, DTA_SourceType, DTST_FILE, DTA_GroupID, GID_PICTURE,
                    PDTA_DestMode, PMODE_V43, PDTA_Remap, FALSE, TAG_DONE);
    if (!o) {
        err = dos_error();
        goto done;
    }
    gpl.MethodID = DTM_PROCLAYOUT;
    gpl.gpl_GInfo = NULL;
    gpl.gpl_Initial = 1;
    GetDTAttrs(o, PDTA_BitMapHeader, (ULONG)&bmh, TAG_DONE);
    if (!DoDTMethodA(o, NULL, NULL, (Msg)&gpl) ||
        (GetDTAttrs(o, PDTA_BitMapHeader, (ULONG)&bmh, TAG_DONE), !bmh)) {
        err = PIXOOERR_FORMAT;
        goto done;
    }
    GetDTAttrs(o, PDTA_AlphaChannel, (ULONG)&alpha, TAG_DONE);
    if (bmh->bmh_Masking == mskHasAlpha) alpha = TRUE;
    if (!alpha && bmh->bmh_Masking == mskHasTransparentColor) {
        GetDTAttrs(o, PDTA_CRegs, (ULONG)&cregs, PDTA_NumColors, (ULONG)&ncolors, TAG_DONE);
        if (cregs && bmh->bmh_Transparent < ncolors) {
            /* 32 bits per gun, left-justified */
            cregs += bmh->bmh_Transparent * 3;
            key = (LONG)((cregs[0] >> 24) << 16 | (cregs[1] >> 24) << 8 | cregs[2] >> 24);
        }
    }
    width = bmh->bmh_Width;
    height = bmh->bmh_Height;
    if (!(w = AllocVec(sizeof(DtWork) + (ULONG)width * 4, MEMF_ANY | MEMF_CLEAR))) {
        err = PIXOOERR_NOMEM;
        goto done;
    }
    if ((err = pxd_place(h, a, width, height, &w->scaler, h->fb))) goto done;
    a->frames = 1;
    for (y = 0; y < height; y++) {
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            SetSignal(0, SIGBREAKF_CTRL_C);
            err = PIXOOERR_BREAK;
            break;
        }
        bpa.MethodID = PDTM_READPIXELARRAY;
        bpa.pbpa_PixelData = w->row;
        bpa.pbpa_PixelFormat = PBPAFMT_ARGB;
        bpa.pbpa_PixelArrayMod = (ULONG)width * 4;
        bpa.pbpa_Left = 0;
        bpa.pbpa_Top = (ULONG)y;
        bpa.pbpa_Width = (ULONG)width;
        bpa.pbpa_Height = 1;
        do_method(o, (Msg)&bpa);
        if (!alpha) px_key_row(w->row, width, key);
        px_scaler_row(&w->scaler, w->row);
    }

done:
    if (w) FreeVec(w);
    if (o) DisposeDTObject(o);
    CloseLibrary(DataTypesBase);
    return err;
}

/** @copydoc PixooLoadImageA() */
static LONG LibPixooLoadImageA(struct PixooHandle *handle __asm("a0"), CONST_STRPTR name __asm("a1"),
                               struct TagItem *tags __asm("a2"))
{
    PxImageArgs a;
    UBYTE *data = NULL;
    LONG len = 0, err;
    int gif;

    if (!handle || !name) return PIXOOERR_ARGS;
    image_args(handle, tags, &a);
    if ((err = read_file(name, NULL, &len, &gif))) return err;
    if (gif) {
        if (!(err = read_file(name, &data, &len, &gif))) err = pxd_draw_gif(handle, data, len, &a);
        if (data) FreeVec(data);
    } else
        err = load_datatype(handle, name, &a);
    image_results(tags, &a);
    return err;
}

/** @copydoc PixooLoadAnimA() */
static LONG LibPixooLoadAnimA(struct PixooHandle *handle __asm("a0"), CONST_STRPTR name __asm("a1"),
                              struct TagItem *tags __asm("a2"))
{
    PxImageArgs a;
    UBYTE *data = NULL;
    LONG len = 0, err;
    int gif;

    if (!handle || !name) return PIXOOERR_ARGS;
    image_args(handle, tags, &a);
    if (!(err = read_file(name, &data, &len, &gif))) {
        if (!gif) err = PIXOOERR_FORMAT;
        else err = pxd_send_gif(handle, data, len, &a);
    }
    if (data) FreeVec(data);
    image_results(tags, &a);
    return err;
}
