/**
 * @file clib/pixoo_protos.h
 * @brief Prototypes and reference of the pixoo.library functions.
 *
 * Written by hand (not by sfdc) because it is the documentation of the
 * functions; proto/pixoo.h, inline/pixoo.h and fd/pixoo_lib.fd are made
 * from sfd/pixoo_lib.sfd with "make headers". See libraries/pixoo.h for
 * an overview, the tags and the error codes.
 *
 * <b>Framebuffer:</b> the drawing functions only change the framebuffer
 * in memory, they never talk to the device. Coordinates outside the
 * display are clipped. Colours are 0xRRGGBB. Call PixooPush() to show
 * the framebuffer.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#ifndef CLIB_PIXOO_PROTOS_H
#define CLIB_PIXOO_PROTOS_H

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/pixoo.h>

/** @name Connection */
/** @{ */

/**
 * @brief Create a handle for a Pixoo device.
 *
 * Opens bsdsocket.library for the calling task, resolves the host name
 * and allocates the framebuffer (cleared to black). Does not contact the
 * device yet, so a wrong address shows up with the first command.
 *
 * @param host IP address ("192.168.1.50") or host name of the device; copied.
 * @param tags #PIXOOA_Port, #PIXOOA_Timeout, #PIXOOA_Size,
 *             #PIXOOA_RefreshLimit, #PIXOOA_ErrorPtr; may be NULL.
 * @return The handle, or NULL (reason in #PIXOOA_ErrorPtr). Free it with
 *         PixooClose() from the same task.
 */
struct PixooHandle *PixooOpenA(CONST_STRPTR host, struct TagItem *tags);

/**
 * @brief Varargs form of PixooOpenA().
 * @param host IP address or host name of the device.
 * @param tag1 First tag, the list ends with TAG_DONE.
 * @return The handle, or NULL.
 */
struct PixooHandle *PixooOpen(CONST_STRPTR host, Tag tag1, ...);

/**
 * @brief Free a handle: closes its bsdsocket.library and frees the
 *        framebuffer.
 * @param handle The handle; NULL is allowed.
 */
void PixooClose(struct PixooHandle *handle);

/**
 * @brief A short English text for an error code.
 * @param error A PIXOOERR_* code.
 * @return A static, read-only string (also for unknown codes).
 */
CONST_STRPTR PixooErrorString(LONG error);

/** @} */

/** @name Commands */
/** @{ */

/**
 * @brief Send any command to the device and return its answer.
 *
 * For commands the library has no function for, e.g.
 * <tt>{"Command":"Device/SetUTC","Utc":1762891200}</tt>. The answer's
 * "error_code" is checked like for every other command.
 *
 * @param handle The handle.
 * @param json The complete JSON object of the command.
 * @param[out] reply Receives the JSON answer, NUL-terminated (may be NULL).
 * @param replysize Size of @p reply in bytes, including the NUL.
 * @return #PIXOOERR_OK, #PIXOOERR_TOOBIG if the answer was truncated, or
 *         another error code.
 */
LONG PixooSendCommand(struct PixooHandle *handle, CONST_STRPTR json, STRPTR reply, LONG replysize);

/**
 * @brief Set the display brightness (Channel/SetBrightness).
 * @param handle The handle.
 * @param percent 0 to 100; clipped.
 * @return An error code.
 */
LONG PixooSetBrightness(struct PixooHandle *handle, LONG percent);

/**
 * @brief Switch the display on or off (Channel/OnOffScreen).
 * @param handle The handle.
 * @param on Non-zero to switch it on.
 * @return An error code.
 */
LONG PixooSetScreen(struct PixooHandle *handle, LONG on);

/**
 * @brief Select a channel (Channel/SetIndex).
 * @param handle The handle.
 * @param channel #PIXOOCH_FACES, #PIXOOCH_CLOUD, #PIXOOCH_VISUALIZER or
 *                #PIXOOCH_CUSTOM.
 * @return An error code.
 */
LONG PixooSetChannel(struct PixooHandle *handle, LONG channel);

/**
 * @brief Select a clock face (Channel/SetClockSelectId).
 *
 * The ids are those of Divoom's clock gallery, e.g. 182 for the default
 * face of the Pixoo 64.
 *
 * @param handle The handle.
 * @param id Id of the clock face.
 * @return An error code.
 */
LONG PixooSetClockFace(struct PixooHandle *handle, LONG id);

/**
 * @brief Read a numeric setting of the device (Channel/GetAllConf).
 *
 * A Pixoo 64 answers with "Brightness", "LightSwitch" (screen on/off),
 * "CurClockId", "RotationFlag", "MirrorFlag", "Time24Flag",
 * "TemperatureMode", "GyrateAngle", "ClockTime", "GalleryTime",
 * "SingleGalleyTime", "PowerOnChannelId" and "GalleryShowTimeFlag"; other
 * firmware versions may differ. The current channel is not included.
 *
 * @param handle The handle.
 * @param key Name of the JSON field (case sensitive).
 * @param[out] value Receives its value.
 * @return #PIXOOERR_OK, #PIXOOERR_NOTFOUND if the answer has no numeric
 *         field of that name, or another error code.
 */
LONG PixooGetConfigValue(struct PixooHandle *handle, CONST_STRPTR key, LONG *value);

/** @} */

/** @name Framebuffer */
/** @{ */

/**
 * @brief Fill the whole framebuffer with one colour.
 * @param handle The handle.
 * @param rgb The colour, 0xRRGGBB.
 */
void PixooFill(struct PixooHandle *handle, ULONG rgb);

/**
 * @brief Set one pixel.
 * @param handle The handle.
 * @param x Column, 0 is left.
 * @param y Row, 0 is top.
 * @param rgb The colour, 0xRRGGBB.
 */
void PixooSetPixel(struct PixooHandle *handle, LONG x, LONG y, ULONG rgb);

/**
 * @brief Read one pixel.
 * @param handle The handle.
 * @param x Column.
 * @param y Row.
 * @return Its colour, 0xRRGGBB; 0 outside the display.
 */
ULONG PixooGetPixel(struct PixooHandle *handle, LONG x, LONG y);

/**
 * @brief Draw a line (Bresenham), both ends included.
 * @param handle The handle.
 * @param x0 Start column.
 * @param y0 Start row.
 * @param x1 End column.
 * @param y1 End row.
 * @param rgb The colour, 0xRRGGBB.
 */
void PixooDrawLine(struct PixooHandle *handle, LONG x0, LONG y0, LONG x1, LONG y1, ULONG rgb);

/**
 * @brief Draw the outline of a rectangle; the corners may be given in
 *        any order and are included.
 * @param handle The handle.
 * @param x0 Column of one corner.
 * @param y0 Row of one corner.
 * @param x1 Column of the opposite corner.
 * @param y1 Row of the opposite corner.
 * @param rgb The colour, 0xRRGGBB.
 */
void PixooDrawRect(struct PixooHandle *handle, LONG x0, LONG y0, LONG x1, LONG y1, ULONG rgb);

/**
 * @brief Fill a rectangle; the corners may be given in any order and are
 *        included.
 * @param handle The handle.
 * @param x0 Column of one corner.
 * @param y0 Row of one corner.
 * @param x1 Column of the opposite corner.
 * @param y1 Row of the opposite corner.
 * @param rgb The colour, 0xRRGGBB.
 */
void PixooFillRect(struct PixooHandle *handle, LONG x0, LONG y0, LONG x1, LONG y1, ULONG rgb);

/**
 * @brief Direct access to the framebuffer.
 *
 * The buffer holds size x size pixels, row by row from the top left,
 * three bytes (R, G, B) per pixel. It may be changed freely; it belongs
 * to the handle and is freed by PixooClose().
 *
 * @param handle The handle.
 * @param[out] size Receives the width/height in pixels (may be NULL).
 * @return The framebuffer.
 */
UBYTE *PixooGetBuffer(struct PixooHandle *handle, LONG *size);

/**
 * @brief Show the framebuffer on the display (Draw/SendHttpGif).
 *
 * Sends the framebuffer as a single-frame picture, Base64-encoded (about
 * 16 KB for 64x64). Resets the device's picture counter before the first
 * picture and every #PIXOOA_RefreshLimit pictures (one more request).
 *
 * @param handle The handle.
 * @return An error code.
 */
LONG PixooPush(struct PixooHandle *handle);

/** @} */

#endif /* CLIB_PIXOO_PROTOS_H */
