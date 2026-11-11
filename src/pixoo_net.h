/**
 * @file pixoo_net.h
 * @brief TCP part of pixoo.library: one HTTP request per connection.
 *
 * Compiles against bsdsocket.library on the Amiga and against the POSIX
 * socket API with PIXOO_HOST (host test). All functions return PIXOOERR_*
 * codes from libraries/pixoo.h.
 *
 * Copyright (c) 2026 André Gewert <agewert@ubergeek.de>
 * Released under the MIT License, see LICENSE.
 */
#ifndef PIXOO_NET_H
#define PIXOO_NET_H

/**
 * @brief Connection settings of a device.
 *
 * On the Amiga @c socketbase belongs to the task that called pxn_init();
 * only that task may use the connection.
 */
typedef struct PxConn {
    void *socketbase;       /**< bsdsocket.library of the owning task (Amiga only) */
    unsigned long addr;     /**< IPv4 address, network byte order */
    int port;               /**< TCP port */
    long timeout;           /**< seconds to wait for connect, send and each receive */
} PxConn;

/**
 * @brief Prepare a connection: open bsdsocket.library (Amiga).
 * @param c The connection; port and timeout are set to defaults (80, 5 s).
 * @return #PIXOOERR_OK or #PIXOOERR_NOTCPIP.
 */
int pxn_init(PxConn *c);

/**
 * @brief Release what pxn_init() opened; safe after a failed pxn_init().
 * @param c The connection.
 */
void pxn_cleanup(PxConn *c);

/**
 * @brief Resolve the host name or dotted address into @c c->addr.
 * @param c The connection.
 * @param host The host name or address.
 * @return #PIXOOERR_OK or #PIXOOERR_HOST.
 */
int pxn_resolve(PxConn *c, const char *host);

/**
 * @brief Send one request and receive the whole answer.
 *
 * Connects, sends header and body, then reads until the server closes the
 * connection, Content-Length bytes of body have arrived or, for a chunked
 * body (the Pixoo's way), the last chunk has arrived. A chunked body is
 * decoded in place, so @p ans holds the header and the plain body. What
 * does not fit into @p ans is read and dropped.
 *
 * @param c The connection.
 * @param hdr The HTTP header.
 * @param hlen Its length.
 * @param body The body.
 * @param blen Its length.
 * @param[out] ans Receives the raw answer (header and body), NUL-terminated.
 * @param anscap Size of @p ans.
 * @param[out] anslen Receives the length stored in @p ans.
 * @param[out] truncated Set to non-zero if the answer did not fit.
 * @return #PIXOOERR_OK, #PIXOOERR_CONNECT, #PIXOOERR_TIMEOUT,
 *         #PIXOOERR_SEND, #PIXOOERR_RECV, #PIXOOERR_BREAK or #PIXOOERR_NOMEM.
 */
int pxn_request(PxConn *c, const char *hdr, long hlen, const char *body, long blen,
                char *ans, long anscap, long *anslen, int *truncated);

#endif /* PIXOO_NET_H */
