/**
 * @file pixoo_image.h
 * @brief Portable image code of pixoo.library: scaling ARGB rows into the
 *        framebuffer and decoding GIF files (also animated).
 *
 * Plain ANSI C without C library calls, like pixoo_core.c; memory comes
 * from allocator functions the caller passes in.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#ifndef PIXOO_IMAGE_H
#define PIXOO_IMAGE_H

#include "pixoo_core.h"

/** Largest width and height of a source picture (keeps sizes and sums in 32 bits). */
#define PX_MAXSOURCE 16384L

/** Allocates @p n bytes (zeroed), or returns NULL. */
typedef void *(*PxAllocFunc)(void *ud, long n);
/** Frees memory from a PxAllocFunc (NULL allowed). */
typedef void (*PxFreeFunc)(void *ud, void *p);

/** @name Scaling */
/** @{ */

/**
 * @brief Size of an image fitted into a box, keeping the aspect ratio.
 *
 * Images larger than the box are shrunk to fit; smaller ones keep their
 * size unless @p enlarge is set. The result is at least 1x1.
 *
 * @param sw Width of the image.
 * @param sh Height of the image.
 * @param bw Width of the box.
 * @param bh Height of the box.
 * @param enlarge Non-zero to scale small images up to the box as well.
 * @param[out] dw Receives the width on the display.
 * @param[out] dh Receives the height on the display.
 */
void px_fit(long sw, long sh, long bw, long bh, int enlarge, long *dw, long *dh);

/**
 * @brief State of the row-by-row scaler.
 *
 * The source is fed one row at a time (px_scaler_row()), so a large
 * picture never has to be in memory as a whole. Shrinking averages all
 * source pixels that fall on a display pixel (area averaging), weighted
 * by their alpha; enlarging repeats pixels. Each finished display pixel
 * is blended over the framebuffer with its average alpha.
 */
typedef struct PxScaler {
    unsigned char *fb;                  /**< the framebuffer */
    int size;                           /**< its width and height */
    long sw;                            /**< width of the source */
    long sh;                            /**< height of the source */
    long dx;                            /**< left edge on the display (may be outside) */
    long dy;                            /**< top edge on the display */
    long dw;                            /**< width on the display, at most PX_MAXSIZE */
    long dh;                            /**< height on the display */
    long row;                           /**< number of the next source row */
    long drow;                          /**< display row being collected */
    long xs0[PX_MAXSIZE];               /**< first source column of each display column */
    long xs1[PX_MAXSIZE];               /**< column after the last one */
    unsigned long sum[PX_MAXSIZE][4];   /**< sums of a*r, a*g, a*b and a per display column */
} PxScaler;

/**
 * @brief Prepare scaling a source of sw x sh pixels to dw x dh pixels at
 *        dx, dy of the framebuffer.
 * @param s The scaler.
 * @param fb The framebuffer.
 * @param size Its width and height.
 * @param sw Width of the source.
 * @param sh Height of the source.
 * @param dx Left edge on the display.
 * @param dy Top edge on the display.
 * @param dw Width on the display (1 to PX_MAXSIZE).
 * @param dh Height on the display (1 to PX_MAXSIZE).
 * @return Non-zero if the sizes are valid: source up to #PX_MAXSOURCE,
 *         and at most 66051 source pixels per display pixel (e.g. 16384
 *         pixels shrunk to 64, but not to 32).
 */
int px_scaler_init(PxScaler *s, unsigned char *fb, int size, long sw, long sh,
                   long dx, long dy, long dw, long dh);

/**
 * @brief Feed the next source row.
 * @param s The scaler.
 * @param argb sw pixels of 4 bytes: alpha (255 opaque), red, green, blue.
 */
void px_scaler_row(PxScaler *s, const unsigned char *argb);

/**
 * @brief Set the alpha bytes of a row that has no alpha channel of its own.
 *
 * Pixels of the transparent colour become transparent, all others
 * opaque. Used for pictures with one transparent colour, which datatypes
 * deliver as plain RGB: the colour is matched by its RGB value, so
 * another palette entry with the very same RGB value turns transparent,
 * too.
 *
 * @param argb The row, 4 bytes per pixel (alpha, red, green, blue).
 * @param width Number of pixels.
 * @param key The transparent colour 0xRRGGBB, or -1 for none (all opaque).
 */
void px_key_row(unsigned char *argb, long width, long key);

/** @} */

/** @name GIF decoding */
/** @{ */

/**
 * @brief State of the GIF decoder.
 *
 * Decodes from a GIF file in memory and composes the frames on a canvas
 * of the logical screen size like a web browser: transparency, the
 * disposal methods "keep", "restore to background" (transparent) and
 * "restore to previous", interlaced images, local and global colour
 * tables. Allocate it with the caller's allocator: it holds the LZW
 * tables (about 17 KB).
 */
typedef struct PxGif {
    const unsigned char *d;             /**< the file */
    long len;                           /**< its length */
    long pos;                           /**< read position */
    long w;                             /**< width of the logical screen */
    long h;                             /**< height of the logical screen */
    unsigned char *canvas;              /**< w * h pixels ARGB, alpha 0 = transparent */
    unsigned char *prev;                /**< copy for disposal 3, allocated when needed */
    int gctsize;                        /**< entries of the global colour table, 0 = none */
    int delay;                          /**< delay of the next frame (1/100 s) from its GCE */
    int transp;                         /**< transparent index of the next frame, -1 = none */
    int disposal;                       /**< disposal method of the next frame */
    int pdisp;                          /**< disposal method of the frame shown */
    long px;                            /**< rectangle of the frame shown: left */
    long py;                            /**< top */
    long pw;                            /**< width */
    long ph;                            /**< height */
    PxAllocFunc alloc;                  /**< allocator */
    PxFreeFunc free;                    /**< and its free function */
    void *ud;                           /**< user data for both */
    unsigned char gct[768];             /**< global colour table */
    unsigned char lct[768];             /**< local colour table of the current frame */
    unsigned short prefix[4096];        /**< LZW: prefix code of each code */
    unsigned char suffix[4096];         /**< LZW: last byte of each code */
    unsigned char stack[4097];          /**< LZW: output of one code, reversed */
} PxGif;

/**
 * @brief Count the frames of a GIF file and add up their delays, without
 *        decoding the pictures.
 * @param d The file.
 * @param len Its length.
 * @param[out] frames Receives the number of frames.
 * @param[out] delay Receives the sum of the delays in milliseconds;
 *                   delays below 20 ms count as 100 ms, as in browsers.
 * @return #PIXOOERR_OK, or #PIXOOERR_FORMAT if it is not a valid GIF.
 */
int px_gif_scan(const unsigned char *d, long len, long *frames, long *delay);

/**
 * @brief Start decoding a GIF file.
 * @param g The decoder (contents undefined).
 * @param d The file; must stay valid until px_gif_close().
 * @param len Its length.
 * @param alloc Allocator for the canvas.
 * @param free Its free function.
 * @param ud User data for both.
 * @return #PIXOOERR_OK, #PIXOOERR_FORMAT or #PIXOOERR_NOMEM; call
 *         px_gif_close() in any case.
 */
int px_gif_open(PxGif *g, const unsigned char *d, long len, PxAllocFunc alloc, PxFreeFunc free, void *ud);

/**
 * @brief Decode the next frame onto the canvas.
 * @param g The decoder.
 * @param[out] delay Receives the frame's delay in milliseconds (see px_gif_scan()).
 * @return 1 if a frame is on the canvas, 0 at the end of the file, or
 *         -#PIXOOERR_FORMAT / -#PIXOOERR_NOMEM.
 */
int px_gif_next(PxGif *g, long *delay);

/**
 * @brief Free the canvas (not the decoder itself).
 * @param g The decoder.
 */
void px_gif_close(PxGif *g);

/** @} */

#endif /* PIXOO_IMAGE_H */
