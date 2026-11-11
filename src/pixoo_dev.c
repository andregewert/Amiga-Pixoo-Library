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

long pxd_push(struct PixooHandle *h)
{
    PxStr s;
    long err;

    if (!h) return PIXOOERR_ARGS;
    if (h->picid == 0 || h->picid >= h->refreshlimit) {
        if ((err = pxd_simple(h, "Draw/ResetHttpGifId", 0, 0))) return err;
        h->picid = 0;
    }
    px_str_init(&s, h->body, h->bodycap);
    px_cmd_push(&s, h->fb, h->size, h->picid + 1);
    if (s.overflow) return PIXOOERR_NOMEM;
    err = pxd_request(h, h->body, s.len, 0, 0, 0);
    /* after a failure the device's counter is unknown: reset next time */
    h->picid = err ? 0 : h->picid + 1;
    return err;
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
        "Answer too long, truncated"
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
}
