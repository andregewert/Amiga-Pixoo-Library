/**
 * @file pixoo_net.c
 * @brief TCP part of pixoo.library (see pixoo_net.h).
 *
 * The socket is non-blocking from the start; every connect, send and
 * receive waits with WaitSelect() (select() on the host) for at most the
 * timeout. This works with every bsdsocket.library (AmiTCP has no
 * SO_RCVTIMEO) and bounds the wait for a device that is switched off.
 *
 * Ctrl-C: bsdsocket.library interrupts WaitSelect() when the task gets a
 * signal of its break mask (Ctrl-C by default); that becomes
 * PIXOOERR_BREAK. The Ctrl-C signal is left set, so the caller's own
 * check (e.g. CheckSignal()) still sees it.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#include <libraries/pixoo.h>
#include "pixoo_core.h"
#include "pixoo_net.h"

#ifdef PIXOO_HOST

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#define SOCKBASE(c)        /**< nothing to fetch on the host */
#define CLOSESOCK(fd)      close(fd)              /**< close a socket */
#define SOCKERRNO()        errno                  /**< error of the last call */
#ifdef MSG_NOSIGNAL
#define SENDFLAGS          MSG_NOSIGNAL           /**< no SIGPIPE on a closed connection */
#else
#define SENDFLAGS          0                      /**< flags for send() */
#endif
#define E_INTR             EINTR                  /**< interrupted */
#define E_INPROGRESS       EINPROGRESS            /**< non-blocking connect running */
#define E_WOULDBLOCK       EWOULDBLOCK            /**< would block */
#define E_AGAIN            EAGAIN                 /**< would block (other name) */
#define E_TIMEDOUT         ETIMEDOUT              /**< timed out */

#else /* Amiga */

/* the socket functions come from proto/bsdsocket.h (inline calls with the
 * task's SocketBase), not from the link library prototypes */
#define __NO_NET_API      /**< no link library prototypes in sys/socket.h */
#include <exec/types.h>
#include <dos/dos.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>

/** Makes the task's bsdsocket.library visible to the inline macros. */
#define SOCKBASE(c)        struct Library *SocketBase = (struct Library *)(c)->socketbase;
#define CLOSESOCK(fd)      CloseSocket(fd)        /**< close a socket */
#define SOCKERRNO()        Errno()                /**< error of the last call */
#define SENDFLAGS          0                      /**< flags for send() */
/* BSD error numbers as returned by Errno() */
#define E_INTR             4                      /**< interrupted (break signal) */
#define E_INPROGRESS       36                     /**< non-blocking connect running */
#define E_WOULDBLOCK       35                     /**< would block */
#define E_AGAIN            35                     /**< would block (same in BSD) */
#define E_TIMEDOUT         60                     /**< timed out */

#endif

int pxn_init(PxConn *c)
{
    c->socketbase = 0;
    c->addr = 0;
    c->port = 80;
    c->timeout = 5;
#ifndef PIXOO_HOST
    if (!(c->socketbase = OpenLibrary((STRPTR)"bsdsocket.library", 4)))
        return PIXOOERR_NOTCPIP;
#endif
    return PIXOOERR_OK;
}

void pxn_cleanup(PxConn *c)
{
#ifndef PIXOO_HOST
    if (c->socketbase) CloseLibrary((struct Library *)c->socketbase);
#endif
    c->socketbase = 0;
}

int pxn_resolve(PxConn *c, const char *host)
{
    SOCKBASE(c)
    struct hostent *he;
    unsigned long a;

    a = (unsigned long)inet_addr((char *)host);
    if (a != 0xFFFFFFFFUL) {
        c->addr = a;
        return PIXOOERR_OK;
    }
    he = gethostbyname((char *)host);
    if (!he || he->h_addrtype != AF_INET || he->h_length != 4 || !he->h_addr_list[0])
        return PIXOOERR_HOST;
    {
        const unsigned char *p = (const unsigned char *)he->h_addr_list[0];
        unsigned char *q = (unsigned char *)&c->addr;
        q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; q[3] = p[3];
    }
    return PIXOOERR_OK;
}

/**
 * @brief Wait until a socket is readable or writable.
 * @param c The connection (timeout, socket library).
 * @param fd The socket.
 * @param forwrite Non-zero to wait for writability, else for readability.
 * @return #PIXOOERR_OK when ready, #PIXOOERR_TIMEOUT, #PIXOOERR_BREAK or
 *         @p failcode.
 * @param failcode Error code to return if waiting itself fails.
 */
static int wait_sock(PxConn *c, long fd, int forwrite, int failcode)
{
    SOCKBASE(c)
    struct timeval tv;
    long r;
#ifdef PIXOO_HOST
    fd_set set;

    FD_ZERO(&set);
    FD_SET((int)fd, &set);
    tv.tv_sec = c->timeout;
    tv.tv_usec = 0;
    do {
        r = select((int)fd + 1, forwrite ? 0 : &set, forwrite ? &set : 0, 0, &tv);
    } while (r < 0 && errno == EINTR);
#else
    /* fd_set of bsdsocket.library: 32 bit words, bit fd%32 of word fd/32 */
    ULONG set[8];
    int i;

    if (fd >= 256) return failcode;
    for (i = 0; i < 8; i++) set[i] = 0;
    set[fd >> 5] = 1UL << (fd & 31);
    tv.tv_secs = c->timeout;
    tv.tv_micro = 0;
    r = WaitSelect(fd + 1, forwrite ? NULL : set, forwrite ? set : NULL, NULL, &tv, NULL);
    if (r < 0 && (SOCKERRNO() == E_INTR || (SetSignal(0, 0) & SIGBREAKF_CTRL_C)))
        return PIXOOERR_BREAK;
#endif
    if (r > 0) return PIXOOERR_OK;
    if (r == 0) return PIXOOERR_TIMEOUT;
    return failcode;
}

/**
 * @brief Connect a new non-blocking socket to the device.
 * @param c The connection.
 * @param[out] fdp Receives the socket.
 * @return An error code; on error no socket is left open.
 */
static int open_conn(PxConn *c, long *fdp)
{
    SOCKBASE(c)
    struct sockaddr_in sa;
    long fd, e;
    int err;
    unsigned char *p;
    unsigned int i;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return PIXOOERR_CONNECT;
#ifdef PIXOO_HOST
    fcntl((int)fd, F_SETFL, fcntl((int)fd, F_GETFL) | O_NONBLOCK);
#else
    {
        LONG one = 1;
        IoctlSocket(fd, FIONBIO, (char *)&one);
    }
#endif
    for (p = (unsigned char *)&sa, i = 0; i < sizeof(sa); i++) p[i] = 0;
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)c->port);
    sa.sin_addr.s_addr = c->addr;

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        e = SOCKERRNO();
        if (e != E_INPROGRESS && e != E_WOULDBLOCK) {
            CLOSESOCK(fd);
            return e == E_INTR ? PIXOOERR_BREAK : PIXOOERR_CONNECT;
        }
        if ((err = wait_sock(c, fd, 1, PIXOOERR_CONNECT))) {
            CLOSESOCK(fd);
            return err;
        }
        {
#ifdef PIXOO_HOST
            int soerr = 0;
            socklen_t len = sizeof(soerr);
#else
            LONG soerr = 0;
            socklen_t len = sizeof(soerr);
#endif
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0 || soerr) {
                CLOSESOCK(fd);
                return soerr == E_TIMEDOUT ? PIXOOERR_TIMEOUT : PIXOOERR_CONNECT;
            }
        }
    }
    *fdp = fd;
    return PIXOOERR_OK;
}

/**
 * @brief Send a whole buffer on a non-blocking socket.
 * @param c The connection.
 * @param fd The socket.
 * @param buf The data.
 * @param len Its length.
 * @return An error code.
 */
static int send_all(PxConn *c, long fd, const char *buf, long len)
{
    SOCKBASE(c)
    long n, e;
    int err;

    while (len > 0) {
        n = send(fd, (void *)buf, len, SENDFLAGS);
        if (n > 0) {
            buf += n;
            len -= n;
            continue;
        }
        e = SOCKERRNO();
        if (n < 0 && (e == E_WOULDBLOCK || e == E_AGAIN)) {
            if ((err = wait_sock(c, fd, 1, PIXOOERR_SEND))) return err;
            continue;
        }
        return n < 0 && e == E_INTR ? PIXOOERR_BREAK : PIXOOERR_SEND;
    }
    return PIXOOERR_OK;
}

int pxn_request(PxConn *c, const char *hdr, long hlen, const char *body, long blen,
                char *ans, long anscap, long *anslen, int *truncated)
{
    SOCKBASE(c)
    char drop[256], tail[7] = { 0 };
    long fd, n, e, i, len = 0, total = 0, bodyoff = -1, clen = -1;
    int err, chunked = 0, done = 0;

    *anslen = 0;
    *truncated = 0;
    ans[0] = 0;
    if ((err = open_conn(c, &fd))) return err;
    if ((err = send_all(c, fd, hdr, hlen)) || (err = send_all(c, fd, body, blen))) {
        CLOSESOCK(fd);
        return err;
    }

    for (;;) {
        char *dst = len < anscap - 1 ? ans + len : drop;
        long room = len < anscap - 1 ? anscap - 1 - len : (long)sizeof(drop);

        n = recv(fd, dst, room, 0);
        if (n > 0) {
            if (dst == drop) *truncated = 1;
            else {
                len += n;
                ans[len] = 0;
            }
            total += n;
            /* the last 7 bytes, to see the end of a truncated chunked answer */
            for (i = 0; i < n; i++) {
                tail[0] = tail[1]; tail[1] = tail[2]; tail[2] = tail[3]; tail[3] = tail[4];
                tail[4] = tail[5]; tail[5] = tail[6]; tail[6] = dst[i];
            }
            if (bodyoff < 0 && (bodyoff = px_http_body(ans, len)) >= 0) {
                clen = px_http_content_length(ans, bodyoff);
                chunked = clen < 0 && px_http_chunked(ans, bodyoff);
            }
            if (bodyoff < 0) continue;
            if (clen >= 0) {
                if (total >= bodyoff + clen) break;
            } else if (chunked) {
                /* the Pixoo does not close the connection: stop at the last chunk */
                if (!*truncated) {
                    long end = px_http_chunked_end(ans + bodyoff, len - bodyoff);
                    if (end == -2) { err = PIXOOERR_HTTP; break; }
                    if (end >= 0) { done = 1; break; }
                } else if (tail[0] == '\r' && tail[1] == '\n' && tail[2] == '0' && tail[3] == '\r' &&
                           tail[4] == '\n' && tail[5] == '\r' && tail[6] == '\n') {
                    done = 1;
                    break;
                }
            }
            continue;
        }
        if (n == 0) break;      /* server closed the connection */
        e = SOCKERRNO();
        if (e == E_WOULDBLOCK || e == E_AGAIN) {
            if ((err = wait_sock(c, fd, 0, PIXOOERR_RECV))) break;
            continue;
        }
        err = e == E_INTR ? PIXOOERR_BREAK : PIXOOERR_RECV;
        break;
    }
    CLOSESOCK(fd);
    if (chunked) {
        len = bodyoff + px_http_dechunk(ans + bodyoff, len - bodyoff);
        if (!err && !done) err = PIXOOERR_RECV;     /* closed before the last chunk */
    }
    *anslen = len;
    if (!err && total == 0) err = PIXOOERR_RECV;
    if (!err && bodyoff >= 0 && clen >= 0 && total < bodyoff + clen) err = PIXOOERR_RECV;
    return err;
}
