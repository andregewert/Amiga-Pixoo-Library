/**
 * @file pixoo_private.h
 * @brief Version and handle structure of pixoo.library.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#ifndef PIXOO_PRIVATE_H
#define PIXOO_PRIVATE_H

#include "pixoo_core.h"
#include "pixoo_net.h"
#include "pixoo_image.h"

#define LIBNAME     "pixoo.library"     /**< library name */
#define LIBVERSION  1                   /**< library version */
#define LIBREVISION 0                   /**< library revision, raised for new features */
#define LIBDATE     "13.11.2026"        /**< date of the revision (DD.MM.YYYY) */

#define PX_HOSTLEN  128                 /**< room for the host name */
#define PX_HDRLEN   256                 /**< room for an HTTP request header */
#define PX_ANSLEN   2048                /**< room for an answer (header and body) */
#define PX_CMDLEN   160                 /**< room for the body of a simple command */

/**
 * @brief A connection to one device (struct PixooHandle of the public
 *        header).
 *
 * Allocated in one piece by PixooOpenA(): the structure, then the
 * framebuffer, then the body buffer for PixooPush().
 */
struct PixooHandle {
    PxConn conn;                /**< socket library, address, port, timeout */
    int size;                   /**< width and height of the display */
    long refreshlimit;          /**< pictures between counter resets */
    long picid;                 /**< PicID of the last picture; 0: reset before the next */
    const PxFont *font;         /**< font of PixooDrawText(): &px_builtin_font or &diskfont */
    PxFont diskfont;            /**< the TextFont given to PixooSetFont(), as PxFont */
    void *textfont;             /**< that struct TextFont (owned by the caller), or NULL */
    unsigned char *fb;          /**< framebuffer, size * size * 3 bytes */
    char *body;                 /**< body buffer for Draw/SendHttpGif */
    long bodycap;               /**< its size */
    PxAllocFunc alloc;          /**< allocator for pictures (zeroed memory) */
    PxFreeFunc free;            /**< and its free function */
    void *ud;                   /**< user data for both */
    char host[PX_HOSTLEN];      /**< host name as given (Host field) */
    char hdr[PX_HDRLEN];        /**< HTTP header of the current request */
    char ans[PX_ANSLEN];        /**< answer of the last request */
};

/**
 * @brief Parameters and results of loading a picture or an animation.
 *
 * The picture is fitted into the box (see px_fit()) and centred in it;
 * the box may reach beyond the display, the picture is clipped then.
 */
typedef struct PxImageArgs {
    long x;                     /**< box: left edge on the display */
    long y;                     /**< box: top edge */
    long w;                     /**< box: width, 1 to 64 */
    long h;                     /**< box: height, 1 to 64 */
    int enlarge;                /**< scale small pictures up to the box */
    long speed;                 /**< animation: milliseconds per frame, 0 = from the file */
    long maxframes;             /**< animation: most frames to send, 1 to 59 */
    long srcw;                  /**< out: width of the picture file */
    long srch;                  /**< out: height of the picture file */
    long frames;                /**< out: frames in the file */
} PxImageArgs;

/** Most frames of an animation the device accepts (PicNum below 60). */
#define PX_MAXFRAMES 59

/** Size of the memory block after the handle: framebuffer and push body. */
#define PX_HANDLE_EXTRA(size) ((long)(size) * (size) * 3 + PX_PUSH_BODYSIZE(size))

/** @name Device commands (pixoo_dev.c)
 *  Portable; all return PIXOOERR_* codes. */
/** @{ */

/**
 * @brief Point a cleared handle at its framebuffer and body buffer.
 * @param h The handle (zeroed, connection not yet set up).
 * @param mem PX_HANDLE_EXTRA(size) bytes, zeroed (black framebuffer).
 * @param size Width and height of the display.
 * @param refreshlimit Pictures between counter resets.
 */
void pxd_setup(struct PixooHandle *h, unsigned char *mem, int size, long refreshlimit);

/**
 * @brief Send a request body and check the answer.
 *
 * On success (and on #PIXOOERR_DEVICE) @p json points to the JSON body
 * of the answer in the handle's answer buffer.
 *
 * @param h The handle.
 * @param body The JSON body.
 * @param blen Its length.
 * @param[out] json Receives the start of the answer's body (may be NULL).
 * @param[out] jlen Receives its length (may be NULL).
 * @param[out] truncated Set if the answer did not fit (may be NULL).
 * @return An error code.
 */
long pxd_request(struct PixooHandle *h, const char *body, long blen,
                 const char **json, long *jlen, int *truncated);

/**
 * @brief Send a command with at most one integer parameter.
 * @param h The handle.
 * @param command The command name.
 * @param key Name of the parameter, or NULL.
 * @param value Its value.
 * @return An error code.
 */
long pxd_simple(struct PixooHandle *h, const char *command, const char *key, long value);

/**
 * @brief PixooSendCommand().
 * @param h The handle.
 * @param json The command.
 * @param[out] reply Receives the answer (may be NULL).
 * @param replysize Size of @p reply.
 * @return An error code.
 */
long pxd_send_command(struct PixooHandle *h, const char *json, char *reply, long replysize);

/**
 * @brief PixooGetConfigValue().
 * @param h The handle.
 * @param key The field name.
 * @param[out] value Receives the value.
 * @return An error code.
 */
long pxd_get_config(struct PixooHandle *h, const char *key, long *value);

/**
 * @brief PixooPush().
 * @param h The handle.
 * @return An error code.
 */
long pxd_push(struct PixooHandle *h);

/**
 * @brief PixooSendTextA(): checks the parameters and sends the text.
 *
 * Builds the request in the handle's body buffer (no large stack use).
 *
 * @param h The handle.
 * @param t The parameters.
 * @return An error code.
 */
long pxd_send_text(struct PixooHandle *h, const PxText *t);

/**
 * @brief Prepare a scaler that puts a picture into the box of @p a.
 * @param h The handle.
 * @param a The box; srcw and srch are set to the picture size.
 * @param sw Width of the picture.
 * @param sh Height of the picture.
 * @param s The scaler to prepare.
 * @param fb The picture to draw into (size * size RGB).
 * @return #PIXOOERR_OK, or #PIXOOERR_ARGS for an invalid box.
 */
long pxd_place(struct PixooHandle *h, PxImageArgs *a, long sw, long sh, PxScaler *s, unsigned char *fb);

/**
 * @brief Draw the first frame of a GIF file into the framebuffer.
 *
 * Transparent pixels leave the framebuffer as it is. Nothing is sent.
 *
 * @param h The handle (its allocator is used).
 * @param data The file in memory.
 * @param len Its length.
 * @param a Box and results (frames is set, too).
 * @return An error code.
 */
long pxd_draw_gif(struct PixooHandle *h, const unsigned char *data, long len, PxImageArgs *a);

/**
 * @brief Send a GIF file as an animation.
 *
 * Every frame is composed over a copy of the framebuffer, which itself
 * stays unchanged, and sent with one PicID. Files with more than
 * a->maxframes frames are thinned out evenly (every n-th frame); the
 * device knows only one speed for all frames, by default the average
 * delay of the file, so the animation keeps its total duration.
 *
 * @param h The handle.
 * @param data The file in memory.
 * @param len Its length.
 * @param a Box, speed, frame limit and results.
 * @return An error code.
 */
long pxd_send_gif(struct PixooHandle *h, const unsigned char *data, long len, PxImageArgs *a);

/**
 * @brief PixooErrorString().
 * @param error The error code.
 * @return A static text.
 */
const char *pxd_error_string(long error);

/** @} */

#endif /* PIXOO_PRIVATE_H */
