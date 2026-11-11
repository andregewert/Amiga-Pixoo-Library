/**
 * @file pixoo_core.c
 * @brief Portable core of pixoo.library: framebuffer drawing, Base64,
 *        JSON requests and HTTP answers (see pixoo_core.h).
 *
 * No C library calls: the library is linked without startup code, and the
 * host test uses the very same file.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#include "pixoo_core.h"

/* ------------------------------------------------------------------ */
/* strings                                                             */

void px_str_init(PxStr *s, char *buf, long cap)
{
    s->buf = buf;
    s->cap = cap;
    s->len = 0;
    s->overflow = 0;
    buf[0] = 0;
}

/**
 * @brief Append one character.
 * @param s The string.
 * @param c The character.
 */
static void str_char(PxStr *s, char c)
{
    if (s->len + 1 < s->cap) {
        s->buf[s->len++] = c;
        s->buf[s->len] = 0;
    } else
        s->overflow = 1;
}

void px_str_add(PxStr *s, const char *t)
{
    while (*t) str_char(s, *t++);
}

void px_str_num(PxStr *s, long n)
{
    char tmp[12];
    int i = 0;
    unsigned long u;

    if (n < 0) {
        str_char(s, '-');
        u = 0UL - (unsigned long)n;
    } else
        u = (unsigned long)n;
    do {
        tmp[i++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (i) str_char(s, tmp[--i]);
}

void px_str_json(PxStr *s, const char *t)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char c;

    str_char(s, '"');
    while ((c = (unsigned char)*t++)) {
        if (c == '"' || c == '\\') {
            str_char(s, '\\');
            str_char(s, (char)c);
        } else if (c == '\n') {
            px_str_add(s, "\\n");
        } else if (c < 0x20) {
            px_str_add(s, "\\u00");
            str_char(s, hex[c >> 4]);
            str_char(s, hex[c & 15]);
        } else if (c >= 0x80) {             /* ISO-8859-1 to UTF-8 */
            str_char(s, (char)(0xC0 | (c >> 6)));
            str_char(s, (char)(0x80 | (c & 0x3F)));
        } else
            str_char(s, (char)c);
    }
    str_char(s, '"');
}

long px_strlen(const char *t)
{
    const char *p = t;
    while (*p) p++;
    return (long)(p - t);
}

int px_strcpy(char *dst, const char *src, long cap)
{
    while (cap > 1 && *src) {
        *dst++ = *src++;
        cap--;
    }
    *dst = 0;
    return *src == 0;
}

/* ------------------------------------------------------------------ */
/* framebuffer                                                         */

void px_fill(unsigned char *fb, int size, unsigned long rgb)
{
    unsigned char r = (unsigned char)(rgb >> 16), g = (unsigned char)(rgb >> 8), b = (unsigned char)rgb;
    long n = (long)size * size;

    while (n--) {
        *fb++ = r;
        *fb++ = g;
        *fb++ = b;
    }
}

void px_set_pixel(unsigned char *fb, int size, long x, long y, unsigned long rgb)
{
    unsigned char *p;

    if (x < 0 || y < 0 || x >= size || y >= size) return;
    p = fb + (y * size + x) * 3;
    p[0] = (unsigned char)(rgb >> 16);
    p[1] = (unsigned char)(rgb >> 8);
    p[2] = (unsigned char)rgb;
}

unsigned long px_get_pixel(const unsigned char *fb, int size, long x, long y)
{
    const unsigned char *p;

    if (x < 0 || y < 0 || x >= size || y >= size) return 0;
    p = fb + (y * size + x) * 3;
    return ((unsigned long)p[0] << 16) | ((unsigned long)p[1] << 8) | p[2];
}

void px_line(unsigned char *fb, int size, long x0, long y0, long x1, long y1, unsigned long rgb)
{
    long dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    long dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    long err = dx + dy, e2;

    /* far away lines would take long to walk pixel by pixel */
    if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0) ||
        (x0 >= size && x1 >= size) || (y0 >= size && y1 >= size))
        return;
    if (dx > 4096 || -dy > 4096) return;

    for (;;) {
        px_set_pixel(fb, size, x0, y0, rgb);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void px_fill_rect(unsigned char *fb, int size, long x0, long y0, long x1, long y1, unsigned long rgb)
{
    long t, x, y;

    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= size) x1 = size - 1;
    if (y1 >= size) y1 = size - 1;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            px_set_pixel(fb, size, x, y, rgb);
}

void px_rect(unsigned char *fb, int size, long x0, long y0, long x1, long y1, unsigned long rgb)
{
    px_fill_rect(fb, size, x0, y0, x1, y0, rgb);
    px_fill_rect(fb, size, x0, y1, x1, y1, rgb);
    px_fill_rect(fb, size, x0, y0, x0, y1, rgb);
    px_fill_rect(fb, size, x1, y0, x1, y1, rgb);
}

/* ------------------------------------------------------------------ */
/* built-in font                                                       */

long px_text(unsigned char *fb, int size, long x, long y, const char *text, unsigned long rgb)
{
    long cx = x, widest = 0, row, col;
    const unsigned char *g;
    unsigned char c, bits;

    while ((c = (unsigned char)*text++)) {
        if (c == '\n') {
            if (cx - x - 1 > widest) widest = cx - x - 1;
            cx = x;
            y += PX_FONT_LINE;
            continue;
        }
        g = px_font_glyphs[px_font_map[c]];
        if (fb && cx < size && cx + g[0] > 0 && y < size && y + PX_FONT_ROWS > 0)
            for (row = 0; row < PX_FONT_ROWS; row++)
                for (bits = g[1 + row], col = 0; bits; bits = (unsigned char)(bits << 1), col++)
                    if (bits & 0x80) px_set_pixel(fb, size, cx + col, y + row, rgb);
        cx += g[0] + 1;
    }
    if (cx - x - 1 > widest) widest = cx - x - 1;
    return widest;
}

long px_text_width(const char *text)
{
    return px_text(0, 0, 0, 0, text, 0);
}

/* ------------------------------------------------------------------ */
/* Base64                                                              */

long px_base64(const unsigned char *src, long n, char *dst)
{
    static const char tab[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *d = dst;
    unsigned long v;

    for (; n >= 3; n -= 3, src += 3) {
        v = ((unsigned long)src[0] << 16) | ((unsigned long)src[1] << 8) | src[2];
        *d++ = tab[v >> 18];
        *d++ = tab[(v >> 12) & 63];
        *d++ = tab[(v >> 6) & 63];
        *d++ = tab[v & 63];
    }
    if (n) {
        v = (unsigned long)src[0] << 16;
        if (n == 2) v |= (unsigned long)src[1] << 8;
        *d++ = tab[v >> 18];
        *d++ = tab[(v >> 12) & 63];
        *d++ = n == 2 ? tab[(v >> 6) & 63] : '=';
        *d++ = '=';
    }
    return (long)(d - dst);
}

/* ------------------------------------------------------------------ */
/* requests                                                            */

void px_cmd_int(PxStr *s, const char *command, const char *key, long value)
{
    px_str_add(s, "{\"Command\":");
    px_str_json(s, command);
    if (key) {
        str_char(s, ',');
        px_str_json(s, key);
        str_char(s, ':');
        px_str_num(s, value);
    }
    str_char(s, '}');
}

void px_cmd_push(PxStr *s, const unsigned char *fb, int size, long picid)
{
    long n = (long)size * size * 3;

    px_str_add(s, "{\"Command\":\"Draw/SendHttpGif\",\"PicNum\":1,\"PicWidth\":");
    px_str_num(s, size);
    px_str_add(s, ",\"PicOffset\":0,\"PicID\":");
    px_str_num(s, picid);
    px_str_add(s, ",\"PicSpeed\":1000,\"PicData\":\"");
    /* the Base64 text goes straight into the buffer */
    if (!s->overflow && s->len + PX_B64LEN(n) + 3 <= s->cap) {
        s->len += px_base64(fb, n, s->buf + s->len);
        s->buf[s->len] = 0;
    } else
        s->overflow = 1;
    px_str_add(s, "\"}");
}

void px_cmd_text(PxStr *s, const PxText *t)
{
    static const char hex[] = "0123456789ABCDEF";
    char col[8];
    int i;

    col[0] = '#';
    for (i = 0; i < 6; i++) col[1 + i] = hex[(t->rgb >> (20 - 4 * i)) & 15];
    col[7] = 0;
    px_str_add(s, "{\"Command\":\"Draw/SendHttpText\",\"TextId\":");
    px_str_num(s, t->id);
    px_str_add(s, ",\"x\":");
    px_str_num(s, t->x);
    px_str_add(s, ",\"y\":");
    px_str_num(s, t->y);
    px_str_add(s, ",\"dir\":");
    px_str_num(s, t->dir);
    px_str_add(s, ",\"font\":");
    px_str_num(s, t->font);
    px_str_add(s, ",\"TextWidth\":");
    px_str_num(s, t->width);
    px_str_add(s, ",\"speed\":");
    px_str_num(s, t->speed);
    px_str_add(s, ",\"TextString\":");
    px_str_json(s, t->text);
    px_str_add(s, ",\"color\":\"");
    px_str_add(s, col);
    px_str_add(s, "\",\"align\":");
    px_str_num(s, t->align);
    str_char(s, '}');
}

void px_http_header(PxStr *s, const char *host, int port, long bodylen)
{
    px_str_add(s, "POST /post HTTP/1.1\r\nHost: ");
    px_str_add(s, host);
    if (port != 80) {
        str_char(s, ':');
        px_str_num(s, port);
    }
    px_str_add(s, "\r\nUser-Agent: pixoo.library (AmigaOS)\r\n"
                  "Content-Type: application/json\r\nContent-Length: ");
    px_str_num(s, bodylen);
    px_str_add(s, "\r\nConnection: close\r\n\r\n");
}

/* ------------------------------------------------------------------ */
/* answers                                                             */

long px_http_body(const char *buf, long len)
{
    long i;

    for (i = 0; i + 3 < len; i++)
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return i + 4;
    return -1;
}

/**
 * @brief Compare a header field name, ignoring case (ASCII).
 * @param p Start of the line.
 * @param name The name in lower case, with the colon.
 * @return Non-zero if the line starts with it.
 */
static int field_is(const char *p, const char *name)
{
    char c;

    while (*name) {
        c = *p++;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != *name++) return 0;
    }
    return 1;
}

long px_http_content_length(const char *buf, long hlen)
{
    long i, v;

    for (i = 0; i + 16 < hlen; i++) {
        if ((i == 0 || buf[i - 1] == '\n') && field_is(buf + i, "content-length:")) {
            i += 15;
            while (i < hlen && (buf[i] == ' ' || buf[i] == '\t')) i++;
            if (i >= hlen || buf[i] < '0' || buf[i] > '9') return -1;
            for (v = 0; i < hlen && buf[i] >= '0' && buf[i] <= '9'; i++)
                v = v * 10 + (buf[i] - '0');
            return v;
        }
    }
    return -1;
}

int px_http_chunked(const char *buf, long hlen)
{
    long i, j;

    for (i = 0; i + 18 < hlen; i++) {
        if ((i == 0 || buf[i - 1] == '\n') && field_is(buf + i, "transfer-encoding:")) {
            for (j = i + 18; j + 7 <= hlen && buf[j] != '\r' && buf[j] != '\n'; j++)
                if (field_is(buf + j, "chunked")) return 1;
            return 0;
        }
    }
    return 0;
}

/**
 * @brief Parse the size line of a chunk.
 * @param b The body.
 * @param len Its length.
 * @param i Offset of the size line.
 * @param[out] size Receives the chunk size.
 * @return Offset of the chunk data, -1 if the line is incomplete, -2 if
 *         it is malformed.
 */
static long chunk_line(const char *b, long len, long i, long *size)
{
    long v = 0;
    int digits = 0;
    char c;

    for (; i < len; i++, digits++) {
        c = b[i];
        if (c >= '0' && c <= '9') c -= '0';
        else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
        else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
        else break;
        if (v > 0x7FFFFL) return -2;        /* more than 8 MB: nonsense */
        v = (v << 4) | c;
    }
    if (i >= len) return -1;
    if (!digits) return -2;
    while (i < len && b[i] != '\n') i++;  /* chunk extensions, CR */
    if (i >= len) return -1;
    *size = v;
    return i + 1;
}

long px_http_chunked_end(const char *b, long len)
{
    long i = 0, j, size;

    for (;;) {
        if ((j = chunk_line(b, len, i, &size)) < 0) return j;
        if (size == 0) {
            /* optional trailer lines, then an empty line */
            for (i = j; ; ) {
                if (i + 1 >= len) return -1;
                if (b[i] == '\r' && b[i + 1] == '\n') return i + 2;
                while (i < len && b[i] != '\n') i++;
                if (i >= len) return -1;
                i++;
            }
        }
        if (j + size + 2 > len) return -1;
        if (b[j + size] != '\r' || b[j + size + 1] != '\n') return -2;
        i = j + size + 2;
    }
}

long px_http_dechunk(char *b, long len)
{
    long i = 0, j, size, out = 0;

    while ((j = chunk_line(b, len, i, &size)) >= 0 && size > 0) {
        if (size > len - j) size = len - j;
        while (size--) b[out++] = b[j++];
        i = j + 2;
        if (i > len) break;
    }
    b[out] = 0;
    return out;
}

int px_http_status(const char *buf, long len)
{
    long i = 0;
    int code = 0, digits = 0;

    if (len < 12 || buf[0] != 'H' || buf[1] != 'T' || buf[2] != 'T' || buf[3] != 'P' || buf[4] != '/')
        return -1;
    while (i < len && buf[i] != ' ') i++;
    while (i < len && buf[i] == ' ') i++;
    while (i < len && buf[i] >= '0' && buf[i] <= '9' && digits < 3) {
        code = code * 10 + (buf[i++] - '0');
        digits++;
    }
    return digits == 3 ? code : -1;
}

int px_json_int(const char *json, long len, const char *key, long *value)
{
    long klen = px_strlen(key), i, j, v;
    int neg;

    for (i = 0; i + klen + 2 < len; i++) {
        if (json[i] != '"' || json[i + klen + 1] != '"') continue;
        for (j = 0; j < klen && json[i + 1 + j] == key[j]; j++) ;
        if (j < klen) continue;
        j = i + klen + 2;
        while (j < len && (json[j] == ' ' || json[j] == '\t' || json[j] == '\r' || json[j] == '\n')) j++;
        if (j >= len || json[j] != ':') continue;
        j++;
        while (j < len && (json[j] == ' ' || json[j] == '\t' || json[j] == '\r' || json[j] == '\n')) j++;
        neg = j < len && json[j] == '-';
        if (neg) j++;
        if (j >= len || json[j] < '0' || json[j] > '9') return 0;
        for (v = 0; j < len && json[j] >= '0' && json[j] <= '9'; j++)
            v = v * 10 + (json[j] - '0');
        *value = neg ? -v : v;
        return 1;
    }
    return 0;
}
