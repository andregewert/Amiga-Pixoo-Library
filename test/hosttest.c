/**
 * @file hosttest.c
 * @brief Host test of the portable part of pixoo.library.
 *
 * Unit checks of pixoo_core.c (Base64, JSON, HTTP parsing, drawing), then
 * the device commands of pixoo_dev.c over real sockets against the mock
 * device test/mockpixoo.py. "make check" starts the mock and compares its
 * command log with test/requests.expected.
 *
 * Usage: hosttest HOST PORT [real]
 *
 * With "real", HOST is a real Pixoo and only harmless commands are sent:
 * the settings are read and the brightness is set to its current value.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libraries/pixoo.h>
#include "pixoo_private.h"

static int Failed = 0;          /**< number of failed checks */

/**
 * @brief Record and print the result of a check.
 * @param ok Non-zero if the check passed.
 * @param what Description.
 */
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) Failed++;
}

/**
 * @brief Check that a device command returned the expected code.
 * @param got The returned code.
 * @param want The expected code.
 * @param what Description.
 */
static void check_err(long got, long want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s -> %s", what, pxd_error_string(got));
    check(got == want, buf);
}

/** @brief Unit checks of pixoo_core.c without network. */
static void core_tests(void)
{
    static const struct { const char *in, *out; } b64[] = {
        { "", "" }, { "f", "Zg==" }, { "fo", "Zm8=" }, { "foo", "Zm9v" },
        { "foob", "Zm9vYg==" }, { "fooba", "Zm9vYmE=" }, { "foobar", "Zm9vYmFy" }
    };
    char out[64], buf[300];
    unsigned char fb[16 * 16 * 3];
    PxStr s;
    long v, n, i, x, y;

    for (i = 0; i < (long)(sizeof(b64) / sizeof(b64[0])); i++) {
        n = px_base64((const unsigned char *)b64[i].in, (long)strlen(b64[i].in), out);
        out[n] = 0;
        snprintf(buf, sizeof(buf), "base64 \"%s\" = \"%s\"", b64[i].in, out);
        check(strcmp(out, b64[i].out) == 0 && n == PX_B64LEN((long)strlen(b64[i].in)), buf);
    }

    px_str_init(&s, buf, sizeof(buf));
    px_cmd_int(&s, "Channel/SetBrightness", "Brightness", -5);
    check(strcmp(buf, "{\"Command\":\"Channel/SetBrightness\",\"Brightness\":-5}") == 0, "px_cmd_int");
    px_str_init(&s, buf, sizeof(buf));
    px_str_json(&s, "a\"b\\c\nd\001");
    check(strcmp(buf, "\"a\\\"b\\\\c\\nd\\u0001\"") == 0, "px_str_json escapes");
    px_str_init(&s, out, 8);
    px_str_add(&s, "0123456789");
    check(s.overflow && s.len == 7 && strcmp(out, "0123456") == 0, "PxStr overflow");

    check(px_json_int("{\"error_code\": 0, \"Brightness\":-42}", 35, "Brightness", &v) && v == -42, "px_json_int");
    check(!px_json_int("{\"Bright\":5}", 12, "Brightness", &v), "px_json_int missing key");
    check(!px_json_int("{\"Name\":\"x\"}", 12, "Name", &v), "px_json_int string value");

    strcpy(buf, "HTTP/1.1 200 OK\r\ncontent-length: 17\r\nX: y\r\n\r\n{\"error_code\":0}");
    check(px_http_status(buf, (long)strlen(buf)) == 200, "px_http_status");
    n = px_http_body(buf, (long)strlen(buf));
    check(n > 0 && buf[n] == '{', "px_http_body");
    check(px_http_content_length(buf, n) == 17, "px_http_content_length");
    check(px_http_status("<html>", 6) == -1, "px_http_status rejects non-HTTP");
    check(px_http_body("HTTP/1.1 200 OK\r\n", 17) == -1, "px_http_body incomplete");

    strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\n\r\n");
    n = (long)strlen(buf);
    check(px_http_chunked(buf, n) && px_http_content_length(buf, n) == -1, "px_http_chunked");
    check(!px_http_chunked("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n", 39), "px_http_chunked plain");
    strcpy(buf, "5\r\nHello\r\n7;ext=1\r\n, world\r\n0\r\n\r\nNEXT");
    n = (long)strlen(buf);
    check(px_http_chunked_end(buf, n) == n - 4, "px_http_chunked_end complete");
    check(px_http_chunked_end(buf, n - 6) == -1, "px_http_chunked_end incomplete");
    check(px_http_chunked_end("zz\r\n", 4) == -2, "px_http_chunked_end malformed");
    strcpy(buf, "0\r\nX-Trailer: 1\r\n\r\n");
    check(px_http_chunked_end(buf, (long)strlen(buf)) == (long)strlen(buf), "px_http_chunked_end trailer");
    strcpy(buf, "5\r\nHello\r\n7;ext=1\r\n, world\r\n0\r\n\r\n");
    n = px_http_dechunk(buf, (long)strlen(buf));
    check(n == 12 && strcmp(buf, "Hello, world") == 0, "px_http_dechunk");
    strcpy(buf, "5\r\nHello\r\nA\r\n, wo");
    n = px_http_dechunk(buf, (long)strlen(buf));
    check(n == 9 && strcmp(buf, "Hello, wo") == 0, "px_http_dechunk incomplete");

    px_fill(fb, 16, 0x000000);
    px_line(fb, 16, -10, -10, 30, 30, 0xFF0000);
    for (n = 0, y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            if (px_get_pixel(fb, 16, x, y)) n++;
    check(n == 16 && px_get_pixel(fb, 16, 15, 15) == 0xFF0000, "px_line clipped diagonal");
    px_fill(fb, 16, 0x000000);
    px_rect(fb, 16, 12, 12, 3, 3, 0x00FF00);
    for (n = 0, y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            if (px_get_pixel(fb, 16, x, y)) n++;
    check(n == 36 && px_get_pixel(fb, 16, 3, 12) == 0x00FF00 && !px_get_pixel(fb, 16, 4, 4), "px_rect outline");
    check(px_text_width(&px_builtin_font, "") == 0 && px_text_width(&px_builtin_font, "1") == 3 && px_text_width(&px_builtin_font, "12:34") == 3 * 4 + 1 + 4,
          "px_text_width digits and colon");
    check(px_text_width(&px_builtin_font, "ab\nMMM") == 17 && px_text_width(&px_builtin_font, "i") == 1, "px_text_width lines and narrow glyphs");
    check(px_text_width(&px_builtin_font, "\x80") == px_text_width(&px_builtin_font, "?"), "unknown character as '?'");
    px_fill(fb, 16, 0x000000);
    n = px_text(fb, 16, &px_builtin_font, 1, 1, "Hi", 0xFFFFFF);
    check(n == 5 && px_get_pixel(fb, 16, 1, 1) == 0xFFFFFF && px_get_pixel(fb, 16, 3, 3) == 0xFFFFFF &&
          !px_get_pixel(fb, 16, 2, 1) && px_get_pixel(fb, 16, 5, 1) && !px_get_pixel(fb, 16, 5, 2),
          "px_text draws H and i");
    px_text(fb, 16, &px_builtin_font, -3, 14, "Wg", 0xFF);    /* clipped, must not crash */
    px_str_init(&s, buf, sizeof(buf));
    px_str_json(&s, "Gr\xfc\xdf" "e");
    check(strcmp(buf, "\"Gr\xc3\xbc\xc3\x9f" "e\"") == 0, "px_str_json converts ISO-8859-1 to UTF-8");
    px_fill_rect(fb, 16, -5, 14, 100, 200, 0x0000FF);
    check(px_get_pixel(fb, 16, 0, 15) == 0xFF && px_get_pixel(fb, 16, 15, 14) == 0xFF &&
          px_get_pixel(fb, 16, 99, 99) == 0, "px_fill_rect clipped");
}

/**
 * @brief Checks of the bitmap font renderer with a small font in Amiga
 *        TextFont layout: kerning, fixed width, missing characters, planes.
 */
static void diskfont_tests(void)
{
    /* glyphs side by side: A (3 wide) "#.# ### #.#", B (2) "## #. ##",
     * C (2) "## ## ##", missing-character glyph (1) "# # #"            */
    static const unsigned char strip[] = { 0xBF, 0x00, 0xF7, 0x00, 0xBF, 0x00 };
    static const unsigned char empty[] = { 0, 0, 0, 0, 0, 0 };
    static const unsigned char loc[] = { 0, 0, 0, 3,  0, 3, 0, 2,  0, 5, 0, 2,  0, 7, 0, 1 };
    static const unsigned char space[] = { 0, 4,  0, 3,  0, 3,  0, 2 };
    static const unsigned char kern[] = { 0, 0,  0, 1,  0xFF, 0xFF,  0, 0 };
    PxFont f = { { strip, 0, 0, 0, 0, 0, 0, 0 }, 1, 2, 3, 2, 5, 'A', 'C', 1, loc, space, kern };
    unsigned char fb[16 * 16 * 3];

    check(px_text_width(&f, "AB") == 7, "font: kerning moves B right (width 7)");
    check(px_text_width(&f, "BA") == 7, "font: proportional BA (width 7)");
    check(px_text_width(&f, "C") == 1, "font: negative kerning");
    check(px_text_width(&f, "Z") == 1 && px_text_width(&f, "a") == 1, "font: missing characters use the last glyph");
    check(px_text_width(&f, "A\nABAB") == 15, "font: widest line");
    px_fill(fb, 16, 0);
    px_text(fb, 16, &f, 0, 0, "AB", 0xFFFFFF);
    check(px_get_pixel(fb, 16, 0, 0) && !px_get_pixel(fb, 16, 1, 0) && px_get_pixel(fb, 16, 2, 0) &&
          px_get_pixel(fb, 16, 1, 1) && !px_get_pixel(fb, 16, 4, 0) &&
          px_get_pixel(fb, 16, 5, 1) && !px_get_pixel(fb, 16, 6, 1) && px_get_pixel(fb, 16, 6, 2),
          "font: AB drawn at the right pixels");
    px_fill(fb, 16, 0);
    px_text(fb, 16, &f, 0, 4, "A\nB", 0xFFFFFF);
    check(px_get_pixel(fb, 16, 0, 4) && px_get_pixel(fb, 16, 1, 7 + 1) && !px_get_pixel(fb, 16, 2, 8),
          "font: newline advances by ysize");

    f.proportional = 0;
    check(px_text_width(&f, "BA") == 8, "font: fixed width uses xsize, no kerning");

    f.proportional = 1;
    f.planes[0] = empty;
    f.planes[1] = strip;
    f.nplanes = 2;
    check(px_text_width(&f, "AB") == 7, "font: two planes measure the same");
    px_fill(fb, 16, 0);
    px_text(fb, 16, &f, 0, 0, "A", 0xFF);
    check(px_get_pixel(fb, 16, 0, 0) == 0xFF && !px_get_pixel(fb, 16, 1, 0),
          "font: pixel set in the second plane is drawn");
}

/**
 * @brief Allocator for the handle (PxAllocFunc), zeroed memory.
 * @param ud Unused.
 * @param n Number of bytes.
 * @return The memory or NULL.
 */
static void *host_alloc(void *ud, long n)
{
    (void)ud;
    return calloc(1, (size_t)n);
}

/**
 * @brief Free function for the handle (PxFreeFunc).
 * @param ud Unused.
 * @param p The memory (NULL allowed).
 */
static void host_free(void *ud, void *p)
{
    (void)ud;
    free(p);
}

/**
 * @brief CRC-32 as zlib computes it.
 * @param p The data.
 * @param n Its length.
 * @return The CRC.
 */
static unsigned long crc32(const unsigned char *p, long n)
{
    unsigned long c = 0xFFFFFFFFUL;
    int k;

    while (n-- > 0) {
        c ^= *p++;
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320UL & (0UL - (c & 1)));
    }
    return c ^ 0xFFFFFFFFUL;
}

/**
 * @brief Read a file of the test directory into memory.
 * @param name The file name in test/.
 * @param[out] len Receives its length.
 * @return The data (free() it), or NULL.
 */
static unsigned char *read_file(const char *name, long *len)
{
    char path[200];
    unsigned char *d = NULL;
    FILE *f;

    snprintf(path, sizeof(path), "test/%s", name);
    if (!(f = fopen(path, "rb"))) return NULL;
    if (fseek(f, 0, SEEK_END) == 0 && (*len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 &&
        (d = malloc((size_t)*len)) && fread(d, 1, (size_t)*len, f) != (size_t)*len) {
        free(d);
        d = NULL;
    }
    fclose(f);
    return d;
}

/** @brief Unit checks of the scaler (pixoo_image.c). */
static void scaler_tests(void)
{
    static const struct { long sw, sh, bw, bh; int en; long dw, dh; } fit[] = {
        { 128, 64, 64, 64, 0, 64, 32 }, { 20, 10, 64, 64, 0, 20, 10 }, { 20, 10, 64, 64, 1, 64, 32 },
        { 1, 1000, 64, 64, 0, 1, 64 }, { 300, 200, 32, 64, 0, 32, 21 }, { 64, 64, 64, 64, 0, 64, 64 }
    };
    static const unsigned char rows[2][16] = {
        { 255, 255, 0, 0,  255, 0, 0, 255,  255, 10, 20, 30,  255, 30, 40, 50 },
        { 255, 255, 0, 0,  255, 0, 0, 255,  255, 10, 20, 30,  255, 30, 40, 50 }
    };
    static const unsigned char half[8] = { 0, 99, 99, 99,  255, 200, 100, 0 };
    static const unsigned char two[2][8] = { { 255, 1, 2, 3,  255, 4, 5, 6 }, { 255, 7, 8, 9,  255, 10, 11, 12 } };
    unsigned char fb[16 * 16 * 3];
    PxScaler s;
    long i, dw, dh;
    char buf[100];

    for (i = 0; i < (long)(sizeof(fit) / sizeof(fit[0])); i++) {
        px_fit(fit[i].sw, fit[i].sh, fit[i].bw, fit[i].bh, fit[i].en, &dw, &dh);
        snprintf(buf, sizeof(buf), "px_fit %ldx%ld in %ldx%ld = %ldx%ld", fit[i].sw, fit[i].sh,
                 fit[i].bw, fit[i].bh, dw, dh);
        check(dw == fit[i].dw && dh == fit[i].dh, buf);
    }
    check(!px_scaler_init(&s, fb, 16, 10, 10, 0, 0, 65, 10), "scaler refuses width 65");

    {
        unsigned char key[12] = { 0, 1, 2, 3,  0, 1, 2, 4,  0, 1, 2, 3 };
        px_key_row(key, 3, 0x010203L);
        check(key[0] == 0 && key[4] == 255 && key[8] == 0, "px_key_row: transparent colour");
        px_key_row(key, 3, -1);
        check(key[0] == 255 && key[4] == 255 && key[8] == 255, "px_key_row: no key, all opaque");
    }

    px_fill(fb, 16, 0);
    px_scaler_init(&s, fb, 16, 4, 2, 1, 1, 2, 1);
    px_scaler_row(&s, rows[0]);
    check(px_get_pixel(fb, 16, 1, 1) == 0, "scaler: row not finished before its last source row");
    px_scaler_row(&s, rows[1]);
    check(px_get_pixel(fb, 16, 1, 1) == 0x7F007FUL && px_get_pixel(fb, 16, 2, 1) == 0x141E28UL,
          "scaler: 4x2 shrunk to 2x1 by averaging");

    px_fill(fb, 16, 0x0000FF);
    px_scaler_init(&s, fb, 16, 2, 1, 0, 0, 1, 1);
    px_scaler_row(&s, half);
    check(px_get_pixel(fb, 16, 0, 0) == 0x643280UL, "scaler: half transparent pixel blended");

    px_fill(fb, 16, 0);
    px_scaler_init(&s, fb, 16, 2, 2, -1, 14, 4, 4);
    px_scaler_row(&s, two[0]);
    px_scaler_row(&s, two[1]);
    check(px_get_pixel(fb, 16, 0, 14) == 0x010203UL && px_get_pixel(fb, 16, 0, 15) == 0x010203UL &&
          px_get_pixel(fb, 16, 2, 15) == 0x040506UL && px_get_pixel(fb, 16, 3, 15) == 0,
          "scaler: 2x2 enlarged to 4x4, clipped");
}

/** @brief Decode the test GIFs and compare every frame with test/gifs.expected. */
static void gif_tests(void)
{
    char line[200], name[64], cur[64] = "", buf[300];
    long frame, delay, len = 0, d, frames, total, n;
    unsigned long crc;
    unsigned char *data = NULL;
    PxGif *g = calloc(1, sizeof(PxGif));
    int r = 0, ok = 1, open = 0;
    FILE *f = fopen("test/gifs.expected", "r");

    if (!f || !g) { check(0, "gif: test/gifs.expected readable"); free(g); return; }
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63s %ld %ld %lx", name, &frame, &delay, &crc) != 4) continue;
        if (strcmp(name, cur)) {
            if (open) {
                check(ok && px_gif_next(g, &d) == 0, cur);
                px_gif_close(g);
                free(data);
            }
            strcpy(cur, name);
            ok = (data = read_file(name, &len)) != NULL &&
                 px_gif_open(g, data, len, host_alloc, host_free, NULL) == PIXOOERR_OK;
            open = 1;
        }
        if (!ok) continue;
        r = px_gif_next(g, &d);
        if (r != 1 || d != delay || crc32(g->canvas, g->w * g->h * 4) != crc) {
            snprintf(buf, sizeof(buf), "gif: %s frame %ld (result %d, delay %ld)", name, frame, r, d);
            check(0, buf);
            ok = 0;
        }
    }
    if (open) {
        check(ok && px_gif_next(g, &d) == 0, cur);
        px_gif_close(g);
        free(data);
    }
    fclose(f);

    data = read_file("many.gif", &len);
    check(data && px_gif_scan(data, len, &frames, &total) == 0 && frames == 120 && total == 4800,
          "gif: scan counts 120 frames of 40 ms");
    if (data) {
        check(px_gif_scan(data, len / 2, &frames, &total) == PIXOOERR_FORMAT, "gif: scan of a truncated file fails");
        n = 0;
        if (px_gif_open(g, data, len / 2, host_alloc, host_free, NULL) == 0)
            while ((r = px_gif_next(g, &d)) == 1) n++;
        px_gif_close(g);
        check(r == -PIXOOERR_FORMAT && n > 0, "gif: decoding a truncated file fails after some frames");
        check(px_gif_scan(data + 1, len - 1, &frames, &total) == PIXOOERR_FORMAT, "gif: no signature");
    }
    free(data);
    free(g);
}

/**
 * @brief Create a handle like PixooOpenA() does.
 * @param host The host.
 * @param port The port.
 * @param size The display size.
 * @param limit The refresh limit.
 * @param[out] err Receives the error code.
 * @return The handle or NULL.
 */
static struct PixooHandle *open_handle(const char *host, int port, int size, long limit, long *err)
{
    struct PixooHandle *h = calloc(1, sizeof(*h) + PX_HANDLE_EXTRA(size));

    if (!h) { *err = PIXOOERR_NOMEM; return NULL; }
    pxd_setup(h, (unsigned char *)(h + 1), size, limit);
    h->alloc = host_alloc;
    h->free = host_free;
    px_strcpy(h->host, host, sizeof(h->host));
    if ((*err = pxn_init(&h->conn)) == 0) {
        h->conn.port = port;
        h->conn.timeout = 3;
        *err = pxn_resolve(&h->conn, h->host);
    }
    if (*err) { pxn_cleanup(&h->conn); free(h); return NULL; }
    return h;
}

/**
 * @brief Draw the test pattern of PixooCtl into the framebuffer.
 * @param h The handle.
 */
static void test_pattern(struct PixooHandle *h)
{
    int size = h->size, x, y, step = 256 / size, last = size - 1, m = size / 2;
    unsigned char *fb = h->fb;

    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++) {
            *fb++ = (unsigned char)(x * step);
            *fb++ = (unsigned char)(y * step);
            *fb++ = 0x40;
        }
    px_rect(h->fb, size, 0, 0, last, last, 0xFFFFFF);
    px_line(h->fb, size, 0, 0, last, last, 0xFF0000);
    px_line(h->fb, size, 0, last, last, 0, 0x00FF00);
    px_fill_rect(h->fb, size, m - size / 8, m - size / 8, m + size / 8 - 1, m + size / 8 - 1, 0xFFFF00);
    px_fill_rect(h->fb, size, 2, 2, 4, 4, 0x00FFFF);
}

/**
 * @brief Draw the character sheet of PixooCtl CHARSET.
 * @param h The handle.
 */
static void charset(struct PixooHandle *h)
{
    static const char *const lines[] = {
        "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "abcdefghijklm", "nopqrstuvwxyz",
        "0123456789:.,;", "!?\"#$%&'()*+-/", "<=>@[\\]^_`{|}~", "\xc4\xd6\xdc\xe4\xf6\xfc\xdf\xb0"
    };
    int i;

    px_fill(h->fb, h->size, 0);
    for (i = 0; i < 8; i++) {
        check(px_text_width(&px_builtin_font, lines[i]) <= 63, lines[i]);
        px_text(h->fb, h->size, &px_builtin_font, 1, 1 + i * (px_builtin_font.ysize + 1), lines[i], i & 1 ? 0xFFFFFF : 0xFFD040);
    }
}

/**
 * @brief Device commands against the mock.
 * @param host Host of the mock.
 * @param port Its port.
 */
static void device_tests(const char *host, int port)
{
    struct PixooHandle *h;
    char reply[600], small[10];
    long err, v;
    int i;

    h = open_handle("no.such.host.invalid", port, 64, 32, &err);
    check_err(err, PIXOOERR_HOST, "open unknown host");

    if (!(h = open_handle(host, port + 1, 64, 32, &err))) { check(0, "open closed port"); return; }
    check_err(pxd_simple(h, "Channel/SetBrightness", "Brightness", 1), PIXOOERR_CONNECT, "closed port");
    pxn_cleanup(&h->conn);
    free(h);

    if (!(h = open_handle(host, port, 64, 2, &err))) { check_err(err, 0, "open mock"); return; }
    check_err(pxd_simple(h, "Channel/SetBrightness", "Brightness", 40), 0, "SetBrightness 40");
    check_err(pxd_simple(h, "Channel/OnOffScreen", "OnOff", 0), 0, "OnOffScreen 0");
    check_err(pxd_simple(h, "Channel/OnOffScreen", "OnOff", 1), 0, "OnOffScreen 1");
    check_err(pxd_simple(h, "Channel/SetIndex", "SelectIndex", 3), 0, "SetIndex 3");
    check_err(pxd_simple(h, "Channel/SetClockSelectId", "ClockId", 182), 0, "SetClockSelectId 182");
    err = pxd_get_config(h, "Brightness", &v);
    check_err(err, 0, "GetAllConf Brightness");
    check(v == 40, "Brightness reads back as 40");
    check_err(pxd_get_config(h, "NoSuchKey", &v), PIXOOERR_NOTFOUND, "GetAllConf NoSuchKey");
    check_err(pxd_send_command(h, "{\"Command\":\"Device/SetUTC\",\"Utc\":1762891200}", reply, sizeof(reply)),
              0, "raw Device/SetUTC");
    check(strstr(reply, "\"error_code\": 0") != NULL, "raw reply has error_code 0");
    check_err(pxd_send_command(h, "{\"Command\":\"Test/Fail\"}", reply, sizeof(reply)), PIXOOERR_DEVICE,
              "error_code 1");
    check_err(pxd_send_command(h, "{\"Command\":\"Channel/GetAllConf\"}", small, sizeof(small)), PIXOOERR_TOOBIG,
              "small reply buffer");
    check(strlen(small) == sizeof(small) - 1, "small reply truncated and terminated");
    check_err(pxd_send_command(h, "{\"Command\":\"Test/Big\"}", NULL, 0), PIXOOERR_TOOBIG,
              "answer larger than the handle's buffer");
    check_err(pxd_send_command(h, "{\"Command\":\"Test/Plain\"}", reply, sizeof(reply)), 0,
              "answer with Content-Length");
    check(strcmp(reply, "{\"error_code\": 0}") == 0, "Content-Length reply is the plain body");
    check_err(pxd_send_command(h, "{\"Command\":\"Channel/GetAllConf\"}", reply, sizeof(reply)), 0,
              "chunked GetAllConf");
    check(reply[0] == '{' && reply[strlen(reply) - 1] == '}', "chunked reply decoded (no chunk sizes)");

    {
        PxText t = { "Gr\xfc\xdf" "e vom Amiga", 3, 0, 40, 1, 2, 56, 50, 2, 0x12AB34 };
        check_err(pxd_send_text(h, &t), 0, "SendHttpText");
        t.width = 8;
        check_err(pxd_send_text(h, &t), PIXOOERR_ARGS, "SendHttpText width 8");
        t.width = 64;
        t.speed = 101;
        check_err(pxd_send_text(h, &t), PIXOOERR_ARGS, "SendHttpText speed 101");
        t.speed = 50;
        t.id = 20;
        check_err(pxd_send_text(h, &t), PIXOOERR_ARGS, "SendHttpText id 20");
        check_err(pxd_simple(h, "Draw/ClearHttpText", 0, 0), 0, "ClearHttpText");
    }

    /* refresh limit 2: reset, 1, 2, reset, 1 */
    for (i = 0; i < 3; i++) {
        px_fill(h->fb, h->size, 0x102030UL * (unsigned long)(i + 1));
        check_err(pxd_push(h), 0, "push plain colour");
    }
    test_pattern(h);
    check_err(pxd_push(h), 0, "push test pattern");
    pxn_cleanup(&h->conn);
    free(h);

    if (!(h = open_handle(host, port, 16, 32, &err))) { check_err(err, 0, "open mock 16x16"); return; }
    test_pattern(h);
    check_err(pxd_push(h), 0, "push 16x16");
    pxn_cleanup(&h->conn);
    free(h);

    /* pictures and animations from GIF files */
    if (!(h = open_handle(host, port, 64, 32, &err))) { check_err(err, 0, "open mock for GIFs"); return; }
    {
        PxImageArgs a = { 0, 0, 64, 64, 1, 0, PX_MAXFRAMES, 0, 0, 0 };
        long len;
        unsigned char *data = read_file("still.gif", &len);

        if (data) {
            check_err(pxd_draw_gif(h, data, len, &a), 0, "draw still.gif enlarged");
            check(a.srcw == 20 && a.srch == 10 && a.frames == 1, "still.gif is 20x10, 1 frame");
            check(px_get_pixel(h->fb, 64, 0, 15) == 0 &&
                  px_get_pixel(h->fb, 64, 0, 16) == ((unsigned long)data[13] << 16 | data[14] << 8 | data[15]),
                  "still.gif centred at y 16 with its first colour");
            check_err(pxd_push(h), 0, "push still.gif");
            a.w = 65;
            check_err(pxd_draw_gif(h, data, len, &a), PIXOOERR_ARGS, "draw into a box of width 65");
            free(data);
        }
        test_pattern(h);
        a.w = 32; a.h = 32; a.x = 16; a.y = 16; a.enlarge = 0;
        if ((data = read_file("anim.gif", &len))) {
            check_err(pxd_send_gif(h, data, len, &a), 0, "send anim.gif (5 frames, 268 ms)");
            free(data);
        }
        a.x = a.y = 0; a.w = a.h = 64; a.enlarge = 1; a.maxframes = 10;
        if ((data = read_file("many.gif", &len))) {
            check_err(pxd_send_gif(h, data, len, &a), 0, "send many.gif thinned to 10 frames");
            check(a.frames == 120, "many.gif has 120 frames");
            a.speed = 50;
            a.maxframes = 1;
            check_err(pxd_send_gif(h, data, len, &a), 0, "send many.gif as 1 frame of 50 ms");
            a.maxframes = 60;
            check_err(pxd_send_gif(h, data, len, &a), PIXOOERR_ARGS, "60 frames refused");
            free(data);
        }
        check_err(pxd_send_gif(h, (const unsigned char *)"GIF89a", 6, &a), PIXOOERR_ARGS, "args checked first");
        a.maxframes = 5;
        check_err(pxd_send_gif(h, (const unsigned char *)"GIF89a", 6, &a), PIXOOERR_FORMAT, "broken GIF");
    }
    pxn_cleanup(&h->conn);
    free(h);

    /* last: the character sheet stays in the PPM of the mock */
    if (!(h = open_handle(host, port, 64, 32, &err))) { check_err(err, 0, "open mock again"); return; }
    test_pattern(h);
    check_err(pxd_push(h), 0, "push test pattern 64x64");
    charset(h);
    check_err(pxd_push(h), 0, "push character set");
    pxn_cleanup(&h->conn);
    free(h);
}

/**
 * @brief Harmless commands against a real device.
 * @param host Its address.
 * @param port Its port.
 */
static void real_tests(const char *host, int port)
{
    struct PixooHandle *h;
    char reply[600];
    long err, v;

    if (!(h = open_handle(host, port, 64, 32, &err))) { check_err(err, 0, "open device"); return; }
    check_err(pxd_send_command(h, "{\"Command\":\"Channel/GetAllConf\"}", reply, sizeof(reply)), 0, "GetAllConf");
    printf("     %s\n", reply);
    err = pxd_get_config(h, "Brightness", &v);
    check_err(err, 0, "read Brightness");
    if (!err) {
        printf("     Brightness = %ld\n", v);
        check_err(pxd_simple(h, "Channel/SetBrightness", "Brightness", v), 0, "SetBrightness to the same value");
    }
    pxn_cleanup(&h->conn);
    free(h);
}

/**
 * @brief Entry point.
 * @param argc Number of arguments.
 * @param argv Host and port of the mock (or of a real device, then "real").
 * @return 0 if all checks passed.
 */
int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[3], "real") == 0) {
        real_tests(argv[1], atoi(argv[2]));
        printf("%d failed\n", Failed);
        return Failed != 0;
    }
    core_tests();
    diskfont_tests();
    scaler_tests();
    gif_tests();
    if (argc == 3) device_tests(argv[1], atoi(argv[2]));
    else printf("no host/port given, device tests skipped\n");
    printf("%d failed\n", Failed);
    return Failed != 0;
}
