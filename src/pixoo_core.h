/**
 * @file pixoo_core.h
 * @brief Portable core of pixoo.library: framebuffer drawing, Base64,
 *        JSON requests and HTTP answers.
 *
 * Plain ANSI C without Amiga or C library functions, so the same code runs
 * in the library and in the host test (test/hosttest.c).
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#ifndef PIXOO_CORE_H
#define PIXOO_CORE_H

#define PX_MAXSIZE 64           /**< largest display (Pixoo 64) */

/** Base64 length of @p n bytes. */
#define PX_B64LEN(n) ((((n) + 2) / 3) * 4)

/** Room for everything but the picture data in a Draw/SendHttpGif body. */
#define PX_PUSH_EXTRA 160

/** Size of a Draw/SendHttpGif body for a display of @p size pixels. */
#define PX_PUSH_BODYSIZE(size) (PX_B64LEN((long)(size) * (size) * 3) + PX_PUSH_EXTRA)

/**
 * @brief A string being built in a fixed buffer.
 *
 * Appending never writes past the buffer; once something did not fit,
 * @c overflow is set and the string stays truncated (but terminated).
 */
typedef struct PxStr {
    char *buf;      /**< the buffer */
    long len;       /**< current length, without the NUL */
    long cap;       /**< size of the buffer, including the NUL */
    int overflow;   /**< set when something did not fit */
} PxStr;

/**
 * @brief Start a string in a buffer.
 * @param s The string.
 * @param buf The buffer.
 * @param cap Its size (at least 1).
 */
void px_str_init(PxStr *s, char *buf, long cap);

/**
 * @brief Append text.
 * @param s The string.
 * @param t NUL-terminated text.
 */
void px_str_add(PxStr *s, const char *t);

/**
 * @brief Append a decimal number.
 * @param s The string.
 * @param n The number.
 */
void px_str_num(PxStr *s, long n);

/**
 * @brief Append text as a JSON string literal, with quotes and escapes.
 *
 * Bytes from 0x80 are copied unchanged (the device expects UTF-8; the
 * caller converts if needed).
 *
 * @param s The string.
 * @param t NUL-terminated text.
 */
void px_str_json(PxStr *s, const char *t);

/**
 * @brief Length of a NUL-terminated string.
 * @param t The string.
 * @return Its length.
 */
long px_strlen(const char *t);

/**
 * @brief Copy a string with truncation.
 * @param[out] dst The destination, always terminated.
 * @param src The source.
 * @param cap Size of @p dst (at least 1).
 * @return Non-zero if the whole string fit.
 */
int px_strcpy(char *dst, const char *src, long cap);

/** @name Framebuffer
 *  The framebuffer holds size x size pixels as R, G, B bytes, row by row.
 *  All functions clip to the display. */
/** @{ */

/**
 * @brief Fill the whole framebuffer.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param rgb The colour, 0xRRGGBB.
 */
void px_fill(unsigned char *fb, int size, unsigned long rgb);

/**
 * @brief Set one pixel.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param x Column.
 * @param y Row.
 * @param rgb The colour, 0xRRGGBB.
 */
void px_set_pixel(unsigned char *fb, int size, long x, long y, unsigned long rgb);

/**
 * @brief Read one pixel.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param x Column.
 * @param y Row.
 * @return The colour, 0xRRGGBB, or 0 outside.
 */
unsigned long px_get_pixel(const unsigned char *fb, int size, long x, long y);

/**
 * @brief Draw a line, both ends included.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param x0 Start column.
 * @param y0 Start row.
 * @param x1 End column.
 * @param y1 End row.
 * @param rgb The colour, 0xRRGGBB.
 */
void px_line(unsigned char *fb, int size, long x0, long y0, long x1, long y1, unsigned long rgb);

/**
 * @brief Fill a rectangle, corners in any order and included.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param x0 Column of one corner.
 * @param y0 Row of one corner.
 * @param x1 Column of the opposite corner.
 * @param y1 Row of the opposite corner.
 * @param rgb The colour, 0xRRGGBB.
 */
void px_fill_rect(unsigned char *fb, int size, long x0, long y0, long x1, long y1, unsigned long rgb);

/**
 * @brief Draw the outline of a rectangle, corners in any order and included.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param x0 Column of one corner.
 * @param y0 Row of one corner.
 * @param x1 Column of the opposite corner.
 * @param y1 Row of the opposite corner.
 * @param rgb The colour, 0xRRGGBB.
 */
void px_rect(unsigned char *fb, int size, long x0, long y0, long x1, long y1, unsigned long rgb);

/** @} */

/**
 * @brief Base64-encode bytes (standard alphabet, with padding).
 * @param src The bytes.
 * @param n Their number.
 * @param[out] dst Receives PX_B64LEN(n) characters, not terminated.
 * @return The number of characters written.
 */
long px_base64(const unsigned char *src, long n, char *dst);

/** @name Requests (JSON bodies and HTTP header) */
/** @{ */

/**
 * @brief Body of a command with one integer parameter, e.g.
 *        <tt>{"Command":"Channel/SetBrightness","Brightness":50}</tt>.
 * @param s Receives the body.
 * @param command The command name.
 * @param key Name of the parameter, or NULL for a command without one.
 * @param value Its value.
 */
void px_cmd_int(PxStr *s, const char *command, const char *key, long value);

/**
 * @brief Body of Draw/SendHttpGif with one picture of the framebuffer.
 * @param s Receives the body; needs PX_PUSH_BODYSIZE(size) bytes.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param picid Number of the picture (counter of the device).
 */
void px_cmd_push(PxStr *s, const unsigned char *fb, int size, long picid);

/**
 * @brief HTTP/1.1 header of a POST to /post.
 * @param s Receives the header, ending with the empty line.
 * @param host Host name or address (for the Host field).
 * @param port The port (added to the Host field unless 80).
 * @param bodylen Length of the body that follows.
 */
void px_http_header(PxStr *s, const char *host, int port, long bodylen);

/** @} */

/** @name Answers */
/** @{ */

/**
 * @brief Find the end of the HTTP header in received data.
 * @param buf The data so far.
 * @param len Its length.
 * @return Offset of the body (after the empty line), or -1 if the header
 *         is not complete yet.
 */
long px_http_body(const char *buf, long len);

/**
 * @brief Read the Content-Length field of a complete HTTP header.
 * @param buf The header.
 * @param hlen Its length (offset of the body).
 * @return The length, or -1 if the field is missing.
 */
long px_http_content_length(const char *buf, long hlen);

/**
 * @brief Does a complete HTTP header announce "Transfer-Encoding: chunked"?
 *
 * The Pixoo answers this way (without Content-Length) and keeps the
 * connection open even when asked to close it, so the end of the answer
 * is only known from the last, empty chunk.
 *
 * @param buf The header.
 * @param hlen Its length (offset of the body).
 * @return Non-zero if the body is chunked.
 */
int px_http_chunked(const char *buf, long hlen);

/**
 * @brief Find the end of a chunked body.
 * @param b The body as received so far.
 * @param len Its length.
 * @return Length of the complete chunked body including the last chunk
 *         and trailer, -1 if it is not complete yet, -2 if it is malformed.
 */
long px_http_chunked_end(const char *b, long len);

/**
 * @brief Decode a chunked body in place.
 *
 * Stops at the last chunk, at malformed data or at the end of the data;
 * an incomplete chunk at the end is kept as far as it was received.
 *
 * @param[in,out] b The body; receives the plain data, NUL-terminated
 *                  (needs no extra room: the result is shorter).
 * @param len Its length.
 * @return Length of the plain data.
 */
long px_http_dechunk(char *b, long len);

/**
 * @brief Check the status line of an HTTP answer.
 * @param buf The answer.
 * @param len Its length.
 * @return The status code (200 ...), or -1 if it is not HTTP.
 */
int px_http_status(const char *buf, long len);

/**
 * @brief Find a numeric field in a JSON object.
 *
 * A simple search for <tt>"key"</tt> followed by a colon and an integer,
 * which is enough for the flat answers of the Pixoo. A key that also
 * occurs as a string value could be found by mistake; the device's
 * answers have no such values.
 *
 * @param json The JSON text.
 * @param len Its length.
 * @param key The field name.
 * @param[out] value Receives the value.
 * @return Non-zero if the field was found with a numeric value.
 */
int px_json_int(const char *json, long len, const char *key, long *value);

/** @} */

#endif /* PIXOO_CORE_H */
