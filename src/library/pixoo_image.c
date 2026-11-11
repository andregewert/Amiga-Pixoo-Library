/**
 * @file pixoo_image.c
 * @brief Portable image code of pixoo.library: row-by-row scaler and GIF
 *        decoder (see pixoo_image.h).
 *
 * GIF: the decoder follows the GIF89a specification. The LZW decoder
 * writes pixels straight onto the canvas (no index buffer); interlaced
 * images are put into their rows pass by pass. Errors in the data end
 * the frame early instead of failing, like browsers do, as long as the
 * block structure is intact.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#include <libraries/pixoo.h>
#include "pixoo_image.h"

/* ------------------------------------------------------------------ */
/* scaling                                                             */

/** 16 x 16 bit product: one mulu.w on the 68000 instead of a call of __mulsi3. */
#define MUL16(a, b) ((unsigned long)(unsigned short)(a) * (unsigned short)(b))

void px_fit(long sw, long sh, long bw, long bh, int enlarge, long *dw, long *dh)
{
    long w = sw, h = sh;

    if (sw < 1 || sh < 1 || bw < 1 || bh < 1) {
        *dw = *dh = 1;
        return;
    }
    if (sw > bw || sh > bh || enlarge) {
        /* the smaller of the two factors bw/sw and bh/sh */
        if (bw * sh <= bh * sw) {
            w = bw;
            h = (sh * bw + sw / 2) / sw;
        } else {
            h = bh;
            w = (sw * bh + sh / 2) / sh;
        }
    }
    *dw = w < 1 ? 1 : w;
    *dh = h < 1 ? 1 : h;
}

int px_scaler_init(PxScaler *s, unsigned char *fb, int size, long sw, long sh,
                   long dx, long dy, long dw, long dh)
{
    long c, x0, x1, j;

    if (sw < 1 || sh < 1 || sw > PX_MAXSOURCE || sh > PX_MAXSOURCE || dw < 1 || dh < 1 ||
        dw > PX_MAXSIZE || dh > PX_MAXSIZE)
        return 0;
    /* the sums of a display pixel must not overflow: at most 66051
     * source pixels of 255 * 255 each */
    if ((sw / dw + 1) * (sh / dh + 1) > 66051L) return 0;
    s->fb = fb;
    s->size = size;
    s->sw = sw;
    s->sh = sh;
    s->dx = dx;
    s->dy = dy;
    s->dw = dw;
    s->dh = dh;
    s->row = 0;
    s->drow = 0;
    for (c = 0; c < dw; c++) {
        x0 = c * sw / dw;
        x1 = (c + 1) * sw / dw;
        if (x1 <= x0) x1 = x0 + 1;
        s->xs0[c] = x0;
        s->xs1[c] = x1;
        for (j = 0; j < 4; j++) s->sum[c][j] = 0;
    }
    return 1;
}

/**
 * @brief Blend a finished display row into the framebuffer and clear the
 *        sums.
 * @param s The scaler.
 * @param n Number of source rows the row was collected from.
 */
static void emit_row(PxScaler *s, long n)
{
    long c, y = s->dy + s->drow, x, cnt;
    unsigned long a, r, g, b, inv, *sm;
    unsigned char *p;

    for (c = 0; c < s->dw; c++) {
        sm = s->sum[c];
        x = s->dx + c;
        if (sm[3] && y >= 0 && y < s->size && x >= 0 && x < s->size) {
            cnt = (s->xs1[c] - s->xs0[c]) * n;
            r = sm[0] / sm[3];
            g = sm[1] / sm[3];
            b = sm[2] / sm[3];
            a = sm[3] / (unsigned long)cnt;
            p = s->fb + (y * s->size + x) * 3;
            if (a >= 255) {
                p[0] = (unsigned char)r;
                p[1] = (unsigned char)g;
                p[2] = (unsigned char)b;
            } else {
                /* x * a / 255 without a division per pixel */
                inv = 255 - a;
                r = r * a + p[0] * inv + 128; p[0] = (unsigned char)((r + (r >> 8)) >> 8);
                g = g * a + p[1] * inv + 128; p[1] = (unsigned char)((g + (g >> 8)) >> 8);
                b = b * a + p[2] * inv + 128; p[2] = (unsigned char)((b + (b >> 8)) >> 8);
            }
        }
        sm[0] = sm[1] = sm[2] = sm[3] = 0;
    }
}

void px_scaler_row(PxScaler *s, const unsigned char *argb)
{
    long y = s->row++, y0, y1, c, x;
    const unsigned char *p;
    unsigned long a, *sm;

    while (s->drow < s->dh) {
        y0 = s->drow * s->sh / s->dh;
        y1 = (s->drow + 1) * s->sh / s->dh;
        if (y1 <= y0) y1 = y0 + 1;
        if (y < y0) break;                  /* row not needed */
        if (y < y1)
            for (c = 0; c < s->dw; c++) {
                sm = s->sum[c];
                for (x = s->xs0[c], p = argb + x * 4; x < s->xs1[c]; x++, p += 4) {
                    a = p[0];
                    if (!a) continue;
                    sm[0] += MUL16(a, p[1]);
                    sm[1] += MUL16(a, p[2]);
                    sm[2] += MUL16(a, p[3]);
                    sm[3] += a;
                }
            }
        if (y < y1 - 1) break;              /* more rows belong to this one */
        emit_row(s, y1 - y0);
        s->drow++;                          /* enlarging: the same row again */
    }
}

void px_key_row(unsigned char *argb, long width, long key)
{
    unsigned char r = (unsigned char)(key >> 16), g = (unsigned char)(key >> 8), b = (unsigned char)key;

    for (; width > 0; width--, argb += 4)
        argb[0] = key >= 0 && argb[1] == r && argb[2] == g && argb[3] == b ? 0 : 255;
}

/* ------------------------------------------------------------------ */
/* GIF                                                                 */

/** Delay in milliseconds from a GCE value in 1/100 s (browser rule). */
#define GIF_DELAY(cs) ((cs) < 2 ? 100L : (long)(cs) * 10)

/**
 * @brief Skip a chain of data sub-blocks.
 * @param d The file.
 * @param len Its length.
 * @param pos Offset of the first sub-block's length byte.
 * @return Offset after the terminating empty block, or -1 if the file ends.
 */
static long skip_blocks(const unsigned char *d, long len, long pos)
{
    while (pos < len) {
        if (!d[pos]) return pos + 1;
        pos += d[pos] + 1;
    }
    return -1;
}

/**
 * @brief Check the header and logical screen descriptor.
 * @param d The file.
 * @param len Its length.
 * @return Offset after the global colour table, or -1.
 */
static long gif_header(const unsigned char *d, long len)
{
    long pos = 13;

    if (len < 13 || d[0] != 'G' || d[1] != 'I' || d[2] != 'F' || d[3] != '8' ||
        (d[4] != '7' && d[4] != '9') || d[5] != 'a')
        return -1;
    if (d[10] & 0x80) pos += 3L << ((d[10] & 7) + 1);
    return pos <= len ? pos : -1;
}

int px_gif_scan(const unsigned char *d, long len, long *frames, long *delay)
{
    long pos = gif_header(d, len), n = 0, sum = 0;
    int cs = 0;

    *frames = 0;
    *delay = 0;
    if (pos < 0) return PIXOOERR_FORMAT;
    while (pos < len) {
        switch (d[pos]) {
        case 0x21:                          /* extension */
            if (pos + 2 >= len) return PIXOOERR_FORMAT;
            if (d[pos + 1] == 0xF9 && d[pos + 2] >= 4 && pos + 6 < len)
                cs = d[pos + 4] | (d[pos + 5] << 8);
            if ((pos = skip_blocks(d, len, pos + 2)) < 0) return PIXOOERR_FORMAT;
            break;
        case 0x2C:                          /* image */
            if (pos + 10 > len) return PIXOOERR_FORMAT;
            {
                int flags = d[pos + 9];
                pos += 10;
                if (flags & 0x80) pos += 3L << ((flags & 7) + 1);
            }
            if (pos + 1 > len || (pos = skip_blocks(d, len, pos + 1)) < 0) return PIXOOERR_FORMAT;
            n++;
            sum += GIF_DELAY(cs);
            cs = 0;
            break;
        case 0x3B:                          /* trailer */
            pos = len;
            break;
        default:
            if (n) pos = len;               /* garbage after the last frame: ignore */
            else return PIXOOERR_FORMAT;
        }
    }
    if (!n) return PIXOOERR_FORMAT;
    *frames = n;
    *delay = sum;
    return PIXOOERR_OK;
}

int px_gif_open(PxGif *g, const unsigned char *d, long len, PxAllocFunc alloc, PxFreeFunc free, void *ud)
{
    long pos = gif_header(d, len), i;

    g->d = d;
    g->len = len;
    g->canvas = 0;
    g->prev = 0;
    g->alloc = alloc;
    g->free = free;
    g->ud = ud;
    g->delay = 0;
    g->transp = -1;
    g->disposal = 0;
    g->pdisp = 0;
    g->px = g->py = g->pw = g->ph = 0;
    if (pos < 0) return PIXOOERR_FORMAT;
    g->w = d[6] | (d[7] << 8);
    g->h = d[8] | (d[9] << 8);
    if (g->w < 1 || g->h < 1 || g->w > PX_MAXSOURCE || g->h > PX_MAXSOURCE) return PIXOOERR_FORMAT;
    g->gctsize = 0;
    if (d[10] & 0x80) {
        g->gctsize = 2 << (d[10] & 7);
        for (i = 0; i < g->gctsize * 3; i++) g->gct[i] = d[13 + i];
    }
    g->pos = pos;
    /* canvas starts transparent: the allocator returns zeroed memory */
    if (!(g->canvas = alloc(ud, g->w * g->h * 4))) return PIXOOERR_NOMEM;
    return PIXOOERR_OK;
}

void px_gif_close(PxGif *g)
{
    if (g->canvas) g->free(g->ud, g->canvas);
    if (g->prev) g->free(g->ud, g->prev);
    g->canvas = g->prev = 0;
}

/**
 * @brief Clear a rectangle of the canvas to transparent (disposal 2).
 * @param g The decoder.
 */
static void clear_prev_rect(PxGif *g)
{
    long x, y;
    unsigned char *p;

    for (y = g->py; y < g->py + g->ph && y < g->h; y++)
        for (x = g->px, p = g->canvas + (y * g->w + x) * 4; x < g->px + g->pw && x < g->w; x++, p += 4)
            p[0] = p[1] = p[2] = p[3] = 0;
}

/** Writes decoded colour indices onto the canvas. */
typedef struct PixelSink {
    PxGif *g;                   /**< the decoder */
    const unsigned char *ct;    /**< colour table in use */
    int ctsize;                 /**< its number of entries */
    long fx;                    /**< frame rectangle: left */
    long fy;                    /**< top */
    long fw;                    /**< width */
    long fh;                    /**< height */
    long x;                     /**< next column within the frame */
    long y;                     /**< row within the frame */
    int interlaced;             /**< interlaced image */
    int pass;                   /**< interlace pass, 0-3 */
} PixelSink;

/**
 * @brief Put one colour index onto the canvas and advance.
 * @param s The sink.
 * @param idx The colour index.
 */
static void sink_pixel(PixelSink *s, int idx)
{
    static const unsigned char start[4] = { 0, 4, 2, 1 }, step[4] = { 8, 8, 4, 2 };
    PxGif *g = s->g;
    long cx, cy;
    unsigned char *p;

    if (s->y >= s->fh) return;              /* surplus data */
    cx = s->fx + s->x;
    cy = s->fy + s->y;
    if (idx != g->transp && idx < s->ctsize && cx < g->w && cy < g->h) {
        p = g->canvas + (cy * g->w + cx) * 4;
        p[0] = 255;
        p[1] = s->ct[idx * 3];
        p[2] = s->ct[idx * 3 + 1];
        p[3] = s->ct[idx * 3 + 2];
    }
    if (++s->x < s->fw) return;
    s->x = 0;
    if (!s->interlaced) {
        s->y++;
        return;
    }
    s->y += step[s->pass];
    while (s->y >= s->fh && s->pass < 3) {
        s->pass++;
        s->y = start[s->pass];
    }
}

/**
 * @brief LZW-decode the image data of a frame onto the canvas.
 * @param g The decoder; g->pos is at the minimum code size byte.
 * @param s The pixel sink.
 * @return Non-zero if the block structure was intact (g->pos after it).
 */
static int lzw_decode(PxGif *g, PixelSink *s)
{
    const unsigned char *d = g->d;
    long pos = g->pos, blockend, end;
    int minsize, clear, eoi, next, codesize, prev = -1, first = 0, code, in, sp;
    unsigned long bits = 0;
    int nbits = 0, done = 0;

    if (pos >= g->len) return 0;
    minsize = d[pos++];
    if (minsize < 2 || minsize > 11) minsize = 8;   /* broken: decode garbage, keep structure */
    clear = 1 << minsize;
    eoi = clear + 1;
    next = clear + 2;
    codesize = minsize + 1;
    for (code = 0; code < clear; code++) {
        g->prefix[code] = 0;
        g->suffix[code] = (unsigned char)code;
    }
    blockend = pos;                         /* at a length byte */

    for (;;) {
        /* fill the bit buffer from the sub-blocks */
        while (nbits < codesize && !done) {
            if (pos >= blockend) {
                if (pos >= g->len) return 0;
                if (!d[pos]) {              /* end of the data */
                    g->pos = pos + 1;
                    return 1;
                }
                blockend = pos + 1 + d[pos];
                pos++;
                if (blockend > g->len) return 0;
                continue;
            }
            bits |= (unsigned long)d[pos++] << nbits;
            nbits += 8;
        }
        if (done || nbits < codesize) break;
        code = (int)(bits & ((1UL << codesize) - 1));
        bits >>= codesize;
        nbits -= codesize;

        if (code == clear) {
            codesize = minsize + 1;
            next = clear + 2;
            prev = -1;
            continue;
        }
        if (code == eoi) {
            done = 1;
            break;
        }
        if (prev < 0) {
            if (code >= clear) { done = 1; break; }     /* broken */
            sink_pixel(s, code);
            prev = first = code;
            continue;
        }
        in = code;
        sp = 0;
        if (code >= next) {
            if (code > next) { done = 1; break; }       /* broken */
            g->stack[sp++] = (unsigned char)first;
            code = prev;
        }
        while (code >= clear && sp < 4096) {
            g->stack[sp++] = g->suffix[code];
            code = g->prefix[code];
        }
        first = code;
        g->stack[sp++] = (unsigned char)first;
        while (sp) sink_pixel(s, g->stack[--sp]);
        if (next < 4096) {
            g->prefix[next] = (unsigned short)prev;
            g->suffix[next] = (unsigned char)first;
            next++;
            if (next == (1 << codesize) && codesize < 12) codesize++;
        }
        prev = in;
    }
    /* skip what is left of the data */
    if (pos < blockend) pos = blockend;
    end = skip_blocks(d, g->len, pos);
    if (end < 0) return 0;
    g->pos = end;
    return 1;
}

int px_gif_next(PxGif *g, long *delay)
{
    const unsigned char *d = g->d;
    PixelSink s;
    int flags, i, cs = 0;
    long n;

    while (g->pos < g->len) {
        switch (d[g->pos]) {
        case 0x21:
            if (g->pos + 2 >= g->len) return -PIXOOERR_FORMAT;
            if (d[g->pos + 1] == 0xF9 && d[g->pos + 2] >= 4 && g->pos + 6 < g->len) {
                flags = d[g->pos + 3];
                g->disposal = (flags >> 2) & 7;
                g->transp = (flags & 1) ? d[g->pos + 6] : -1;
                g->delay = d[g->pos + 4] | (d[g->pos + 5] << 8);
            }
            if ((g->pos = skip_blocks(d, g->len, g->pos + 2)) < 0) return -PIXOOERR_FORMAT;
            break;

        case 0x2C:
            if (g->pos + 10 > g->len) return -PIXOOERR_FORMAT;
            /* dispose of the frame shown before */
            if (g->pdisp == 2) clear_prev_rect(g);
            else if (g->pdisp == 3 && g->prev) {
                for (n = 0; n < g->w * g->h * 4; n++) g->canvas[n] = g->prev[n];
            }
            s.g = g;
            s.fx = d[g->pos + 1] | (d[g->pos + 2] << 8);
            s.fy = d[g->pos + 3] | (d[g->pos + 4] << 8);
            s.fw = d[g->pos + 5] | (d[g->pos + 6] << 8);
            s.fh = d[g->pos + 7] | (d[g->pos + 8] << 8);
            flags = d[g->pos + 9];
            g->pos += 10;
            s.ct = g->gct;
            s.ctsize = g->gctsize;
            if (flags & 0x80) {
                s.ctsize = 2 << (flags & 7);
                if (g->pos + s.ctsize * 3 > g->len) return -PIXOOERR_FORMAT;
                for (i = 0; i < s.ctsize * 3; i++) g->lct[i] = d[g->pos + i];
                g->pos += s.ctsize * 3;
                s.ct = g->lct;
            }
            s.interlaced = (flags & 0x40) != 0;
            s.x = s.y = 0;
            s.pass = 0;
            /* "restore to previous": keep the canvas as it is now */
            if (g->disposal == 3) {
                if (!g->prev && !(g->prev = g->alloc(g->ud, g->w * g->h * 4))) return -PIXOOERR_NOMEM;
                for (n = 0; n < g->w * g->h * 4; n++) g->prev[n] = g->canvas[n];
            }
            if (s.fw > 0 && s.fh > 0) {
                if (!lzw_decode(g, &s)) return -PIXOOERR_FORMAT;
            } else if (g->pos >= g->len || (g->pos = skip_blocks(d, g->len, g->pos + 1)) < 0)
                return -PIXOOERR_FORMAT;
            g->pdisp = g->disposal;
            g->px = s.fx;
            g->py = s.fy;
            g->pw = s.fw;
            g->ph = s.fh;
            cs = g->delay;
            g->delay = 0;
            g->transp = -1;
            g->disposal = 0;
            *delay = GIF_DELAY(cs);
            return 1;

        case 0x3B:
            return 0;

        default:
            return 0;                       /* garbage after the frames: end */
        }
    }
    return 0;
}
