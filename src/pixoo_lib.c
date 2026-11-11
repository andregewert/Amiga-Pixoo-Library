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

#include <proto/exec.h>
#include <proto/utility.h>

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

/**
 * @brief Initialise the library: needs exec V39 and utility.library.
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
        (UtilityBase = OpenLibrary((STRPTR)"utility.library", 39)))
        return base;

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
    if (UtilityBase) CloseLibrary(UtilityBase);
    UtilityBase = NULL;
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
    return px_text(handle->fb, handle->size, x, y, (const char *)text, rgb);
}

/** @copydoc PixooTextWidth() */
static LONG LibPixooTextWidth(struct PixooHandle *handle __asm("a0"), CONST_STRPTR text __asm("a1"))
{
    return text ? px_text_width((const char *)text) : 0;
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
