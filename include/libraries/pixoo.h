/**
 * @file libraries/pixoo.h
 * @brief pixoo.library: controls a Divoom Pixoo 64 (or Pixoo 16/32) over
 *        its HTTP interface from AmigaOS 3.x.
 *
 * $VER: pixoo.h 1.0 (13.11.2026)
 *
 * The Pixoo has a small, officially undocumented HTTP interface: every
 * command is a JSON object POSTed to http://DEVICE/post, every answer a
 * JSON object with an "error_code" field. The library builds these
 * requests, sends them through bsdsocket.library (any TCP/IP stack:
 * Roadshow, AmiTCP, Miami) and keeps a framebuffer of 64x64 RGB pixels to
 * draw into; PixooPush() sends the framebuffer to the display.
 *
 * Plain HTTP only: the device does not speak HTTPS, so no SSL library is
 * needed.
 *
 * @code
 * #include <proto/exec.h>
 * #include <proto/pixoo.h>
 *
 * struct Library *PixooBase;
 *
 * if ((PixooBase = OpenLibrary(PIXOONAME, PIXOOVERSION))) {
 *     LONG err;
 *     struct PixooHandle *px = PixooOpen("192.168.1.50", PIXOOA_ErrorPtr, &err, TAG_DONE);
 *     if (px) {
 *         PixooSetBrightness(px, 50);
 *         PixooFill(px, 0x000040);
 *         PixooDrawLine(px, 0, 0, 63, 63, 0xFFFF00);
 *         err = PixooPush(px);
 *         PixooClose(px);
 *     }
 *     CloseLibrary(PixooBase);
 * }
 * @endcode
 *
 * <b>Context:</b> a handle opens its own bsdsocket.library, and socket
 * library bases belong to the task that opened them. A handle must
 * therefore only be used by the task (process) that called PixooOpenA().
 * Several tasks may use the library at the same time with their own
 * handles. Network calls wait; they return #PIXOOERR_BREAK when the task
 * receives Ctrl-C.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#ifndef LIBRARIES_PIXOO_H
#define LIBRARIES_PIXOO_H

#ifndef PIXOO_HOST
#include <exec/types.h>
#include <utility/tagitem.h>
#endif

#define PIXOONAME    "pixoo.library"   /**< name for OpenLibrary() */
#define PIXOOVERSION 1                 /**< version of this interface */

/**
 * @brief A connection to one Pixoo device, with its framebuffer.
 *
 * Private: created by PixooOpenA(), freed by PixooClose(), only used
 * through the library functions.
 */
struct PixooHandle;

/** @name Error codes
 *  Returned by the functions that talk to the device; PixooErrorString()
 *  gives a short English text for each. */
/** @{ */
#define PIXOOERR_OK        0    /**< success */
#define PIXOOERR_NOMEM     1    /**< out of memory */
#define PIXOOERR_NOTCPIP   2    /**< bsdsocket.library could not be opened (no TCP/IP stack running) */
#define PIXOOERR_HOST      3    /**< host name unknown */
#define PIXOOERR_CONNECT   4    /**< connection refused or failed */
#define PIXOOERR_TIMEOUT   5    /**< device did not answer in time */
#define PIXOOERR_SEND      6    /**< sending the request failed */
#define PIXOOERR_RECV      7    /**< receiving the answer failed or connection closed early */
#define PIXOOERR_HTTP      8    /**< answer is not "HTTP 200 OK" or not HTTP at all */
#define PIXOOERR_DEVICE    9    /**< device answered with a non-zero "error_code" */
#define PIXOOERR_ARGS      10   /**< invalid argument (NULL handle, buffer too small, ...) */
#define PIXOOERR_BREAK     11   /**< interrupted by Ctrl-C */
#define PIXOOERR_NOTFOUND  12   /**< key not found in the device's answer */
#define PIXOOERR_TOOBIG    13   /**< answer did not fit into the buffer (truncated) */
#define PIXOOERR_FILE      14   /**< file not found or not readable */
#define PIXOOERR_FORMAT    15   /**< not a picture, unknown format or broken file */
#define PIXOOERR_NODATATYPES 16 /**< datatypes.library V39 or its picture class not available */
#define PIXOOERR_MAX       16   /**< highest error code */
/** @} */

/** @name Channels for PixooSetChannel() */
/** @{ */
#define PIXOOCH_FACES      0    /**< clock faces (see PixooSetClockFace()) */
#define PIXOOCH_CLOUD      1    /**< cloud channel (gallery) */
#define PIXOOCH_VISUALIZER 2    /**< audio visualizer */
#define PIXOOCH_CUSTOM     3    /**< custom pages */
/** @} */

/** @name Text alignment for #PIXOOA_TextAlign */
/** @{ */
#define PIXOOALIGN_LEFT    1    /**< left aligned */
#define PIXOOALIGN_CENTER  2    /**< centred */
#define PIXOOALIGN_RIGHT   3    /**< right aligned */
/** @} */

/** @name Scroll directions for #PIXOOA_TextDir */
/** @{ */
#define PIXOODIR_LEFT      0    /**< text scrolls to the left */
#define PIXOODIR_RIGHT     1    /**< text scrolls to the right */
/** @} */

/** Line height of the built-in font in pixels (5 above the baseline, 1 descender, 1 gap);
 *  for the current font see PixooFontHeight(). */
#define PIXOO_FONTLINE     7

#ifndef PIXOO_HOST

/** First tag of the library. */
#define PIXOOA_Dummy       (TAG_USER + 0x50580000)

/** @name Tags for PixooOpenA() */
/** @{ */

/**
 * @brief TCP port of the device's HTTP server.
 *
 * Type: LONG. Default: 80.
 */
#define PIXOOA_Port        (PIXOOA_Dummy + 1)

/**
 * @brief Seconds to wait for the connection and for each answer.
 *
 * Type: LONG. Default: 5. Valid: 1 to 120.
 */
#define PIXOOA_Timeout     (PIXOOA_Dummy + 2)

/**
 * @brief Width and height of the display in pixels.
 *
 * Sets the size of the framebuffer and of the pictures PixooPush() sends.
 *
 * Type: LONG. Default: 64. Valid: 16, 32, 64.
 */
#define PIXOOA_Size        (PIXOOA_Dummy + 3)

/**
 * @brief Where PixooOpenA() stores an error code.
 *
 * Receives #PIXOOERR_OK on success, otherwise the reason why no handle
 * was returned.
 *
 * Type: LONG *. Default: NULL.
 */
#define PIXOOA_ErrorPtr    (PIXOOA_Dummy + 4)

/**
 * @brief Number of pictures after which PixooPush() resets the device's
 *        picture counter.
 *
 * The Pixoo numbers the pictures it receives (PicID). Firmware versions
 * are known to hang after a few hundred pictures unless the counter is
 * reset now and then (Draw/ResetHttpGifId), so PixooPush() does this
 * before the first picture and then every this many pictures.
 *
 * Type: LONG. Default: 32. Valid: 1 to 1000.
 */
#define PIXOOA_RefreshLimit (PIXOOA_Dummy + 5)

/** @} */

/** @name Tags for PixooSendTextA()
 *  The parameters of the device's Draw/SendHttpText command. Ranges are
 *  those of Divoom's description or, where noted, observed on a Pixoo 64;
 *  values outside give #PIXOOERR_ARGS. */
/** @{ */

/**
 * @brief Text slot; sending text with the same id replaces it.
 *
 * Type: LONG. Default: 0. Valid: 0 to 19.
 */
#define PIXOOA_TextId      (PIXOOA_Dummy + 10)

/**
 * @brief Left edge of the text area.
 *
 * Type: LONG. Default: 0. Valid: 0 to 63.
 */
#define PIXOOA_TextX       (PIXOOA_Dummy + 11)

/**
 * @brief Top edge of the text area.
 *
 * Type: LONG. Default: 0. Valid: 0 to 63.
 */
#define PIXOOA_TextY       (PIXOOA_Dummy + 12)

/**
 * @brief Scroll direction, #PIXOODIR_LEFT or #PIXOODIR_RIGHT.
 *
 * Sent as documented by Divoom ("dir": 0 left, 1 right), but the firmware
 * of a Pixoo 64 tested (November 2026) ignored it and always scrolled to
 * the left.
 *
 * Type: LONG. Default: #PIXOODIR_LEFT.
 */
#define PIXOOA_TextDir     (PIXOOA_Dummy + 13)

/**
 * @brief Font of the device (not a font of the library).
 *
 * One of the device's animation fonts; the text is 16 pixels high.
 * Amiga fonts cannot be used for device text, see PixooSetFont() for
 * text in the framebuffer instead.
 *
 * Type: LONG. Default: 0. Valid: 0 to 7.
 */
#define PIXOOA_TextFont    (PIXOOA_Dummy + 14)

/**
 * @brief Width of the text area in pixels; longer text scrolls.
 *
 * Type: LONG. Default: 64. Valid: 16 to 64.
 */
#define PIXOOA_TextWidth   (PIXOOA_Dummy + 15)

/**
 * @brief Scroll speed: milliseconds per step (smaller is faster).
 *
 * Divoom documents no limit, but a Pixoo 64 shows no text at all for
 * values above 100 although it answers error_code 0, so larger values are
 * refused.
 *
 * Type: LONG. Default: 100. Valid: 0 to 100.
 */
#define PIXOOA_TextSpeed   (PIXOOA_Dummy + 16)

/**
 * @brief Colour of the text, 0xRRGGBB.
 *
 * Type: ULONG. Default: 0xFFFFFF.
 */
#define PIXOOA_TextColor   (PIXOOA_Dummy + 17)

/**
 * @brief Alignment, #PIXOOALIGN_LEFT, #PIXOOALIGN_CENTER or
 *        #PIXOOALIGN_RIGHT.
 *
 * Only has an effect on text narrower than #PIXOOA_TextWidth; longer
 * text scrolls.
 *
 * Type: LONG. Default: #PIXOOALIGN_LEFT.
 */
#define PIXOOA_TextAlign   (PIXOOA_Dummy + 18)

/** @} */

/** @name Tags for PixooLoadImageA() and PixooLoadAnimA()
 *  The picture is fitted into a box, keeping its aspect ratio, and centred
 *  in it. The box may reach beyond the display; the picture is clipped. */
/** @{ */

/**
 * @brief Left edge of the box.
 *
 * Type: LONG. Default: 0.
 */
#define PIXOOA_ImageX      (PIXOOA_Dummy + 20)

/**
 * @brief Top edge of the box.
 *
 * Type: LONG. Default: 0.
 */
#define PIXOOA_ImageY      (PIXOOA_Dummy + 21)

/**
 * @brief Width of the box.
 *
 * Type: LONG. Default: the display size. Valid: 1 to 64.
 */
#define PIXOOA_ImageWidth  (PIXOOA_Dummy + 22)

/**
 * @brief Height of the box.
 *
 * Type: LONG. Default: the display size. Valid: 1 to 64.
 */
#define PIXOOA_ImageHeight (PIXOOA_Dummy + 23)

/**
 * @brief Also scale pictures smaller than the box up to its size.
 *
 * Larger pictures are always shrunk to fit; smaller ones keep their size
 * unless this is set. Enlarging repeats pixels (no smoothing).
 *
 * Type: BOOL. Default: FALSE.
 */
#define PIXOOA_ImageEnlarge (PIXOOA_Dummy + 24)

/**
 * @brief Milliseconds per frame of an animation (PixooLoadAnimA() only).
 *
 * The device plays all frames at one speed. 0 uses the average delay of
 * the file (of the frames sent, see #PIXOOA_AnimMaxFrames), so the
 * animation keeps its total duration.
 *
 * Type: LONG. Default: 0. Valid: 0 to 999999.
 */
#define PIXOOA_AnimSpeed   (PIXOOA_Dummy + 25)

/**
 * @brief Most frames to send (PixooLoadAnimA() only).
 *
 * The device accepts at most 59 frames per animation. A file with more
 * frames is thinned out evenly: every n-th frame is sent. Fewer frames
 * also mean less to send: each frame is a request of about 16 KB.
 *
 * Type: LONG. Default: 59. Valid: 1 to 59.
 */
#define PIXOOA_AnimMaxFrames (PIXOOA_Dummy + 26)

/**
 * @brief Receives the width of the picture in the file.
 *
 * Type: LONG *. Default: NULL.
 */
#define PIXOOA_SourceWidth (PIXOOA_Dummy + 27)

/**
 * @brief Receives the height of the picture in the file.
 *
 * Type: LONG *. Default: NULL.
 */
#define PIXOOA_SourceHeight (PIXOOA_Dummy + 28)

/**
 * @brief Receives the number of frames in the file (1 for pictures that
 *        are not GIFs).
 *
 * Type: LONG *. Default: NULL.
 */
#define PIXOOA_SourceFrames (PIXOOA_Dummy + 29)

/** @} */

#endif /* PIXOO_HOST */

#endif /* LIBRARIES_PIXOO_H */
