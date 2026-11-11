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

#define LIBNAME     "pixoo.library"     /**< library name */
#define LIBVERSION  1                   /**< library version */
#define LIBREVISION 0                   /**< library revision, raised for new features */
#define LIBDATE     "11.11.2026"        /**< date of the revision (DD.MM.YYYY) */

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
    unsigned char *fb;          /**< framebuffer, size * size * 3 bytes */
    char *body;                 /**< body buffer for Draw/SendHttpGif */
    long bodycap;               /**< its size */
    char host[PX_HOSTLEN];      /**< host name as given (Host field) */
    char hdr[PX_HDRLEN];        /**< HTTP header of the current request */
    char ans[PX_ANSLEN];        /**< answer of the last request */
};

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
 * @brief PixooErrorString().
 * @param error The error code.
 * @return A static text.
 */
const char *pxd_error_string(long error);

/** @} */

#endif /* PIXOO_PRIVATE_H */
