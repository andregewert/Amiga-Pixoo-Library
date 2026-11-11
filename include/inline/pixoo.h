/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef _INLINE_PIXOO_H
#define _INLINE_PIXOO_H

#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
#ifdef __HAVE_IPTR_ATTR__
typedef APTR _sfdc_vararg __attribute__((iptr));
#else
typedef ULONG _sfdc_vararg;
#endif /* __HAVE_IPTR_ATTR__ */
#endif /* _SFDC_VARARG_DEFINED */

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef PIXOO_BASE_NAME
#define PIXOO_BASE_NAME PixooBase
#endif /* !PIXOO_BASE_NAME */

#define PixooOpenA(___host, ___tags) \
      LP2(0x1e, struct PixooHandle *, PixooOpenA , CONST_STRPTR, ___host, a0, struct TagItem *, ___tags, a1,\
      , PIXOO_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define PixooOpen(___host, ___tag1, ...) \
    ({_sfdc_vararg _tags[] = { ___tag1, __VA_ARGS__ }; PixooOpenA((___host), (struct TagItem *) _tags); })
#endif /* !NO_INLINE_STDARG */

#define PixooClose(___handle) \
      LP1NR(0x24, PixooClose , struct PixooHandle *, ___handle, a0,\
      , PIXOO_BASE_NAME)

#define PixooErrorString(___error) \
      LP1(0x2a, CONST_STRPTR, PixooErrorString , LONG, ___error, d0,\
      , PIXOO_BASE_NAME)

#define PixooSendCommand(___handle, ___json, ___reply, ___replysize) \
      LP4(0x30, LONG, PixooSendCommand , struct PixooHandle *, ___handle, a0, CONST_STRPTR, ___json, a1, STRPTR, ___reply, a2, LONG, ___replysize, d0,\
      , PIXOO_BASE_NAME)

#define PixooSetBrightness(___handle, ___percent) \
      LP2(0x36, LONG, PixooSetBrightness , struct PixooHandle *, ___handle, a0, LONG, ___percent, d0,\
      , PIXOO_BASE_NAME)

#define PixooSetScreen(___handle, ___on) \
      LP2(0x3c, LONG, PixooSetScreen , struct PixooHandle *, ___handle, a0, LONG, ___on, d0,\
      , PIXOO_BASE_NAME)

#define PixooSetChannel(___handle, ___channel) \
      LP2(0x42, LONG, PixooSetChannel , struct PixooHandle *, ___handle, a0, LONG, ___channel, d0,\
      , PIXOO_BASE_NAME)

#define PixooSetClockFace(___handle, ___id) \
      LP2(0x48, LONG, PixooSetClockFace , struct PixooHandle *, ___handle, a0, LONG, ___id, d0,\
      , PIXOO_BASE_NAME)

#define PixooGetConfigValue(___handle, ___key, ___value) \
      LP3(0x4e, LONG, PixooGetConfigValue , struct PixooHandle *, ___handle, a0, CONST_STRPTR, ___key, a1, LONG *, ___value, a2,\
      , PIXOO_BASE_NAME)

#define PixooFill(___handle, ___rgb) \
      LP2NR(0x54, PixooFill , struct PixooHandle *, ___handle, a0, ULONG, ___rgb, d0,\
      , PIXOO_BASE_NAME)

#define PixooSetPixel(___handle, ___x, ___y, ___rgb) \
      LP4NR(0x5a, PixooSetPixel , struct PixooHandle *, ___handle, a0, LONG, ___x, d0, LONG, ___y, d1, ULONG, ___rgb, d2,\
      , PIXOO_BASE_NAME)

#define PixooGetPixel(___handle, ___x, ___y) \
      LP3(0x60, ULONG, PixooGetPixel , struct PixooHandle *, ___handle, a0, LONG, ___x, d0, LONG, ___y, d1,\
      , PIXOO_BASE_NAME)

#define PixooDrawLine(___handle, ___x0, ___y0, ___x1, ___y1, ___rgb) \
      LP6NR(0x66, PixooDrawLine , struct PixooHandle *, ___handle, a0, LONG, ___x0, d0, LONG, ___y0, d1, LONG, ___x1, d2, LONG, ___y1, d3, ULONG, ___rgb, d4,\
      , PIXOO_BASE_NAME)

#define PixooDrawRect(___handle, ___x0, ___y0, ___x1, ___y1, ___rgb) \
      LP6NR(0x6c, PixooDrawRect , struct PixooHandle *, ___handle, a0, LONG, ___x0, d0, LONG, ___y0, d1, LONG, ___x1, d2, LONG, ___y1, d3, ULONG, ___rgb, d4,\
      , PIXOO_BASE_NAME)

#define PixooFillRect(___handle, ___x0, ___y0, ___x1, ___y1, ___rgb) \
      LP6NR(0x72, PixooFillRect , struct PixooHandle *, ___handle, a0, LONG, ___x0, d0, LONG, ___y0, d1, LONG, ___x1, d2, LONG, ___y1, d3, ULONG, ___rgb, d4,\
      , PIXOO_BASE_NAME)

#define PixooGetBuffer(___handle, ___size) \
      LP2(0x78, UBYTE *, PixooGetBuffer , struct PixooHandle *, ___handle, a0, LONG *, ___size, a1,\
      , PIXOO_BASE_NAME)

#define PixooPush(___handle) \
      LP1(0x7e, LONG, PixooPush , struct PixooHandle *, ___handle, a0,\
      , PIXOO_BASE_NAME)

#endif /* !_INLINE_PIXOO_H */
