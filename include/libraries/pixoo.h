/**
 * @file libraries/pixoo.h
 * @brief pixoo.library: controls a Divoom Pixoo 64 (or Pixoo 16/32) over
 *        its HTTP interface from AmigaOS 3.x.
 *
 * $VER: pixoo.h 1.0 (11.11.2026)
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
#define PIXOOERR_MAX       13   /**< highest error code */
/** @} */

/** @name Channels for PixooSetChannel() */
/** @{ */
#define PIXOOCH_FACES      0    /**< clock faces (see PixooSetClockFace()) */
#define PIXOOCH_CLOUD      1    /**< cloud channel (gallery) */
#define PIXOOCH_VISUALIZER 2    /**< audio visualizer */
#define PIXOOCH_CUSTOM     3    /**< custom pages */
/** @} */

#ifndef PIXOO_HOST

/** First tag of the library (for PixooOpenA()). */
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

#endif /* PIXOO_HOST */

#endif /* LIBRARIES_PIXOO_H */
