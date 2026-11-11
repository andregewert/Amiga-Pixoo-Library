/**
 * @file pixoo_dev.c
 * @brief Device commands of pixoo.library on a handle: request/answer
 *        checking, simple commands, configuration values and PixooPush().
 *
 * Portable like pixoo_core.c; pixoo_lib.c wraps these functions into the
 * library vectors, test/hosttest.c calls them directly.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#include <libraries/pixoo.h>
#include "pixoo_private.h"

long pxd_request(struct PixooHandle *h, const char *body, long blen,
                 const char **json, long *jlen, int *truncated)
{
    PxStr s;
    long anslen, off, code;
    int err, trunc;

    if (json) *json = h->ans;
    if (jlen) *jlen = 0;
    if (truncated) *truncated = 0;

    px_str_init(&s, h->hdr, sizeof(h->hdr));
    px_http_header(&s, h->host, h->conn.port, blen);
    if (s.overflow) return PIXOOERR_ARGS;

    err = pxn_request(&h->conn, h->hdr, s.len, body, blen, h->ans, sizeof(h->ans), &anslen, &trunc);
    if (err) return err;
    if (px_http_status(h->ans, anslen) != 200) return PIXOOERR_HTTP;
    if ((off = px_http_body(h->ans, anslen)) < 0) return trunc ? PIXOOERR_TOOBIG : PIXOOERR_HTTP;

    if (json) *json = h->ans + off;
    if (jlen) *jlen = anslen - off;
    if (truncated) *truncated = trunc;
    if (px_json_int(h->ans + off, anslen - off, "error_code", &code) && code != 0)
        return PIXOOERR_DEVICE;
    return PIXOOERR_OK;
}

long pxd_simple(struct PixooHandle *h, const char *command, const char *key, long value)
{
    char buf[PX_CMDLEN];
    PxStr s;

    if (!h) return PIXOOERR_ARGS;
    px_str_init(&s, buf, sizeof(buf));
    px_cmd_int(&s, command, key, value);
    if (s.overflow) return PIXOOERR_ARGS;
    return pxd_request(h, buf, s.len, 0, 0, 0);
}

long pxd_send_command(struct PixooHandle *h, const char *json, char *reply, long replysize)
{
    const char *ans;
    long alen, i;
    int trunc;
    long err;

    if (!h || !json) return PIXOOERR_ARGS;
    if (reply && replysize > 0) reply[0] = 0;
    err = pxd_request(h, json, px_strlen(json), &ans, &alen, &trunc);
    if (reply && replysize > 0 && (err == PIXOOERR_OK || err == PIXOOERR_DEVICE)) {
        for (i = 0; i < alen && i < replysize - 1; i++) reply[i] = ans[i];
        reply[i] = 0;
        if (!err && (trunc || i < alen)) err = PIXOOERR_TOOBIG;
    } else if (!err && trunc)
        err = PIXOOERR_TOOBIG;
    return err;
}

long pxd_get_config(struct PixooHandle *h, const char *key, long *value)
{
    char buf[PX_CMDLEN];
    const char *ans;
    long alen, v, err;
    PxStr s;

    if (!h || !key || !value) return PIXOOERR_ARGS;
    px_str_init(&s, buf, sizeof(buf));
    px_cmd_int(&s, "Channel/GetAllConf", 0, 0);
    if ((err = pxd_request(h, buf, s.len, &ans, &alen, 0))) return err;
    if (!px_json_int(ans, alen, key, &v)) return PIXOOERR_NOTFOUND;
    *value = v;
    return PIXOOERR_OK;
}

/**
 * @brief Reset the device's picture counter if it is due.
 *
 * Before the first picture, after an error and every refreshlimit
 * pictures; h->picid + 1 is the number of the next picture then.
 *
 * @param h The handle.
 * @return An error code.
 */
static long reset_picid(struct PixooHandle *h)
{
    long err;

    if (h->picid == 0 || h->picid >= h->refreshlimit) {
        if ((err = pxd_simple(h, "Draw/ResetHttpGifId", 0, 0))) return err;
        h->picid = 0;
    }
    return PIXOOERR_OK;
}

/**
 * @brief Send one picture or one frame of an animation.
 * @param h The handle.
 * @param fb The picture.
 * @param picid Its number.
 * @param picnum Frames of the animation.
 * @param offset Index of this frame.
 * @param speed Milliseconds per frame.
 * @return An error code.
 */
static long send_frame(struct PixooHandle *h, const unsigned char *fb, long picid, long picnum,
                       long offset, long speed)
{
    PxStr s;

    px_str_init(&s, h->body, h->bodycap);
    px_cmd_push(&s, fb, h->size, picid, picnum, offset, speed);
    if (s.overflow) return PIXOOERR_NOMEM;
    return pxd_request(h, h->body, s.len, 0, 0, 0);
}

long pxd_push(struct PixooHandle *h)
{
    long err;

    if (!h) return PIXOOERR_ARGS;
    if ((err = reset_picid(h))) return err;
    err = send_frame(h, h->fb, h->picid + 1, 1, 0, 1000);
    /* after a failure the device's counter is unknown: reset next time */
    h->picid = err ? 0 : h->picid + 1;
    return err;
}

long pxd_place(struct PixooHandle *h, PxImageArgs *a, long sw, long sh, PxScaler *s, unsigned char *fb)
{
    long dw, dh;

    a->srcw = sw;
    a->srch = sh;
    if (a->w < 1 || a->w > PX_MAXSIZE || a->h < 1 || a->h > PX_MAXSIZE) return PIXOOERR_ARGS;
    px_fit(sw, sh, a->w, a->h, a->enlarge, &dw, &dh);
    if (!px_scaler_init(s, fb, h->size, sw, sh, a->x + (a->w - dw) / 2, a->y + (a->h - dh) / 2, dw, dh))
        return PIXOOERR_FORMAT;
    return PIXOOERR_OK;
}

/** Working memory of pxd_draw_gif() and pxd_send_gif(), allocated in one piece. */
typedef struct GifWork {
    PxGif gif;                  /**< the decoder */
    PxScaler scaler;            /**< the scaler */
    unsigned char frame[1];     /**< animation: size * size * 3 bytes for the frame sent */
} GifWork;

/**
 * @brief Scale the GIF canvas into a picture.
 * @param w The working memory (scaler prepared by pxd_place(), restarted here).
 * @param fb The picture.
 * @param size Its width and height.
 */
static void scale_canvas(GifWork *w, unsigned char *fb, int size)
{
    PxScaler *s = &w->scaler;
    long y;

    px_scaler_init(s, fb, size, s->sw, s->sh, s->dx, s->dy, s->dw, s->dh);
    for (y = 0; y < w->gif.h; y++) px_scaler_row(s, w->gif.canvas + y * w->gif.w * 4);
}

long pxd_draw_gif(struct PixooHandle *h, const unsigned char *data, long len, PxImageArgs *a)
{
    GifWork *w;
    long delay, err;
    int r;

    if (!h || !data) return PIXOOERR_ARGS;
    if ((err = px_gif_scan(data, len, &a->frames, &delay))) return err;
    if (!(w = h->alloc(h->ud, sizeof(GifWork)))) return PIXOOERR_NOMEM;
    if (!(err = px_gif_open(&w->gif, data, len, h->alloc, h->free, h->ud)) &&
        !(err = pxd_place(h, a, w->gif.w, w->gif.h, &w->scaler, h->fb))) {
        if ((r = px_gif_next(&w->gif, &delay)) == 1) scale_canvas(w, h->fb, h->size);
        else err = r < 0 ? -r : PIXOOERR_FORMAT;
    }
    px_gif_close(&w->gif);
    h->free(h->ud, w);
    return err;
}

long pxd_send_gif(struct PixooHandle *h, const unsigned char *data, long len, PxImageArgs *a)
{
    long fbsize, total, step, count, speed, delay, i, k = 0, id, err, n;
    GifWork *w;
    int r = 0;

    if (!h || !data || a->maxframes < 1 || a->maxframes > PX_MAXFRAMES ||
        a->speed < 0 || a->speed > 999999)
        return PIXOOERR_ARGS;
    if ((err = px_gif_scan(data, len, &a->frames, &total))) return err;
    step = (a->frames + a->maxframes - 1) / a->maxframes;
    count = (a->frames + step - 1) / step;
    speed = a->speed ? a->speed : total / count;
    if (speed < 1) speed = 1;
    if (speed > 999999) speed = 999999;

    fbsize = (long)h->size * h->size * 3;
    if (!(w = h->alloc(h->ud, sizeof(GifWork) + fbsize))) return PIXOOERR_NOMEM;
    if (!(err = px_gif_open(&w->gif, data, len, h->alloc, h->free, h->ud)) &&
        !(err = pxd_place(h, a, w->gif.w, w->gif.h, &w->scaler, w->frame)) &&
        !(err = reset_picid(h))) {
        id = h->picid + 1;
        for (i = 0; !err && (r = px_gif_next(&w->gif, &delay)) == 1; i++) {
            if (i % step) continue;
            for (n = 0; n < fbsize; n++) w->frame[n] = h->fb[n];
            scale_canvas(w, w->frame, h->size);
            err = send_frame(h, w->frame, id, count, k++, speed);
        }
        if (!err && r < 0) err = -r;
        if (!err && k != count) err = PIXOOERR_FORMAT;    /* scan and decoder disagree */
        h->picid = err ? 0 : id;
    }
    px_gif_close(&w->gif);
    h->free(h->ud, w);
    return err;
}

long pxd_send_text(struct PixooHandle *h, const PxText *t)
{
    const unsigned char *p;
    long n = 0;
    PxStr s;

    if (!h || !t->text) return PIXOOERR_ARGS;
    for (p = (const unsigned char *)t->text; *p; p++) n += *p >= 0x80 ? 2 : 1;
    if (n > 511 || t->id < 0 || t->id > 19 || t->x < 0 || t->x > 63 || t->y < 0 || t->y > 63 ||
        (t->dir != 0 && t->dir != 1) || t->font < 0 || t->font > 7 ||
        t->width < 16 || t->width > 64 || t->speed < 0 || t->speed > 100 ||
        t->align < 1 || t->align > 3)
        return PIXOOERR_ARGS;
    px_str_init(&s, h->body, h->bodycap);
    px_cmd_text(&s, t);
    if (s.overflow) return PIXOOERR_ARGS;
    return pxd_request(h, h->body, s.len, 0, 0, 0);
}

const char *pxd_error_string(long error)
{
    static const char *const msg[PIXOOERR_MAX + 1] = {
        "No error",
        "Out of memory",
        "bsdsocket.library not available (TCP/IP stack not running)",
        "Unknown host",
        "Connection failed",
        "Device did not answer in time",
        "Sending failed",
        "Receiving failed",
        "Invalid HTTP answer",
        "Device reported an error",
        "Invalid argument",
        "Interrupted by Ctrl-C",
        "Value not found in the answer",
        "Answer too long, truncated",
        "File not found or not readable",
        "Unknown picture format or broken file",
        "datatypes.library not available"
    };
    if (error < 0 || error > PIXOOERR_MAX) return "Unknown error";
    return msg[error];
}

void pxd_setup(struct PixooHandle *h, unsigned char *mem, int size, long refreshlimit)
{
    long fbsize = (long)size * size * 3;

    h->fb = mem;
    h->body = (char *)mem + fbsize;
    h->bodycap = PX_PUSH_BODYSIZE(size);
    h->size = size;
    h->refreshlimit = refreshlimit;
    h->picid = 0;
    h->font = &px_builtin_font;
    h->textfont = 0;
}
