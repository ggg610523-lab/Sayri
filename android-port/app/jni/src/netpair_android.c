/*
    Sayri network-pairing client — Android
    --------------------------------------
    Background-threaded TCP client that connects to a
    desktop Sayri relay, pairs with a 4-digit code, and
    streams chat through the desktop's local Ollama.

    The wire protocol matches relay.c on the desktop:

        srv -> "HELLO <code>"
        cli -> "PAIR <code>"
        srv -> "PAIR OK" | "PAIR ERR <reason>"
        cli -> "CHAT <message>"
        srv -> "TOK <text chunk>"   (many)
        srv -> "DONE OK" | "DONE ERR <reason>"

    All framing is newline-terminated UTF-8. State is
    guarded by a mutex so the UI thread can poll safely.
*/

#include "netpair.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef struct {
    SDL_mutex *lock;
    SDL_Thread *thread;
    bool running;

    char host[NETPAIR_FIELD_LEN];
    char port[NETPAIR_FIELD_LEN];
    char code[NETPAIR_CODE_LEN + 1];

    NetpairStatus status;
    char status_text[256];

    int fd;

    char text[NETPAIR_MAX_TEXT];
    size_t text_len;
    bool new_data;

    bool done_received;
} Netpair;

static Netpair g;

static void np_lock(void)
{
    if (g.lock) SDL_LockMutex(g.lock);
}

static void np_unlock(void)
{
    if (g.lock) SDL_UnlockMutex(g.lock);
}

static void set_status(NetpairStatus st, const char *text)
{
    np_lock();
    g.status = st;
    if (text)
        snprintf(g.status_text, sizeof(g.status_text), "%s", text);
    np_unlock();
}

/* ---------------- framing ---------------- */

static int sock_send(const char *data, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = send(g.fd, data + off, n - off, 0);
        if (w <= 0) {
            if (w < 0 && errno == EINTR) continue;
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

static int send_cmd(const char *fmt, const char *arg)
{
    char buf[512];
    int n = arg
        ? snprintf(buf, sizeof(buf), fmt, arg)
        : snprintf(buf, sizeof(buf), "%s", fmt);
    if (n < 0) return -1;
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
    buf[n] = '\n';
    return sock_send(buf, (size_t)n + 1);
}

/*
    Read one newline-terminated line. Returns length,
    0 on EOF, -1 on error.
*/
static int read_line(char *out, size_t cap)
{
    size_t n = 0;
    for (;;) {
        char c;
        ssize_t r = recv(g.fd, &c, 1, 0);
        if (r == 1) {
            if (c == '\n') break;
            if (c == '\r') continue;
            if (n + 1 < cap) out[n++] = c;
        } else if (r == 0) {
            break;
        } else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -1;
        }
    }
    out[n] = '\0';
    return (int)n;
}

/* ---------------- protocol handling ---------------- */

static void append_chunk(const char *text)
{
    np_lock();
    size_t n = strlen(text);
    if (g.text_len + n + 1 < sizeof(g.text)) {
        memcpy(g.text + g.text_len, text, n);
        g.text_len += n;
        g.text[g.text_len] = '\0';
        g.new_data = true;
    }
    np_unlock();
}

static int connection_run(void)
{
    /* Resolve + connect. */
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char host[NETPAIR_FIELD_LEN], port[NETPAIR_FIELD_LEN];
    np_lock();
    snprintf(host, sizeof(host), "%s", g.host);
    snprintf(port, sizeof(port), "%s", g.port);
    np_unlock();

    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0 || !res) {
        set_status(NETPAIR_IDLE, "Cannot resolve computer address");
        return 1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        set_status(NETPAIR_IDLE, "Could not open socket");
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        close(fd);
        set_status(NETPAIR_IDLE, "Cannot reach your computer");
        return 1;
    }
    freeaddrinfo(res);

    g.fd = fd;

    char line[NETPAIR_MAX_TEXT];

    /* Expect HELLO with the current desktop code. */
    {
        int n = read_line(line, sizeof(line));
        if (n <= 0 || strncmp(line, "HELLO ", 6)) {
            close(fd);
            g.fd = -1;
            set_status(NETPAIR_IDLE, "Computer did not respond");
            return 1;
        }
    }

    /* Send PAIR with the user-entered code. */
    {
        char code[NETPAIR_CODE_LEN + 1];
        np_lock();
        snprintf(code, sizeof(code), "%s", g.code);
        np_unlock();
        send_cmd("PAIR %s", code);
    }

    /* Wait for PAIR OK / PAIR ERR. */
    for (;;) {
        int n = read_line(line, sizeof(line));
        if (n <= 0) {
            close(fd);
            g.fd = -1;
            set_status(NETPAIR_IDLE, "Connection lost while pairing");
            return 1;
        }
        if (!strncmp(line, "PAIR OK", 7)) {
            set_status(NETPAIR_PAIRED, "Paired with computer \u2014 Sayri is on");
            break;
        } else if (!strncmp(line, "PAIR ERR", 8)) {
            close(fd);
            g.fd = -1;
            set_status(NETPAIR_IDLE, line + 9);
            return 1;
        }
    }

    /* Streaming loop: read TOK / DONE frames. */
    while (g.running) {
        int n = read_line(line, sizeof(line));
        if (n <= 0) {
            close(fd);
            g.fd = -1;
            set_status(NETPAIR_IDLE,
                       "Connection to computer lost");
            return 1;
        }

        if (!strncmp(line, "TOK ", 4)) {
            append_chunk(line + 4);
        } else if (!strncmp(line, "DONE OK", 7)) {
            np_lock();
            g.done_received = true;
            np_unlock();
        } else if (!strncmp(line, "DONE ERR", 8)) {
            append_chunk("\n");
            append_chunk(line + 9);
            np_lock();
            g.done_received = true;
            np_unlock();
        } else if (!strcmp(line, "BYE")) {
            break;
        }
    }

    close(fd);
    g.fd = -1;
    return 0;
}

static int netpair_thread(void *data)
{
    (void)data;
    connection_run();
    if (!g.running) return 0;
    np_lock();
    if (g.status == NETPAIR_PAIRED || g.status == NETPAIR_STREAMING)
        set_status(NETPAIR_IDLE, "Disconnected from computer");
    np_unlock();
    return 0;
}

/* ---------------- public API ---------------- */

void netpair_set_endpoint(const char *host, const char *port)
{
    np_lock();
    snprintf(g.host, sizeof(g.host), "%s", host ? host : "");
    snprintf(g.port, sizeof(g.port), "%s",
             port && *port ? port : "5055");
    np_unlock();
}

void netpair_set_code(const char *code)
{
    np_lock();
    snprintf(g.code, sizeof(g.code), "%s", code ? code : "");
    np_unlock();
}

NetpairStatus netpair_status(void)
{
    NetpairStatus st;
    np_lock();
    st = g.status;
    np_unlock();
    return st;
}

const char *netpair_status_text(void)
{
    static char buf[256];
    np_lock();
    snprintf(buf, sizeof(buf), "%s", g.status_text);
    np_unlock();
    return buf;
}

void netpair_connect(void)
{
    if (!g.lock) g.lock = SDL_CreateMutex();
    np_lock();
    g.text_len = 0;
    g.text[0] = '\0';
    g.new_data = false;
    g.done_received = false;
    g.running = true;
    g.status = NETPAIR_CONNECTING;
    snprintf(g.status_text, sizeof(g.status_text),
             "Connecting to your computer\u2026");
    g.thread = SDL_CreateThread(netpair_thread, "sayri-netpair", NULL);
    if (!g.thread) {
        g.running = false;
        g.status = NETPAIR_IDLE;
        snprintf(g.status_text, sizeof(g.status_text),
                 "Could not start connection");
    }
    np_unlock();
}

bool netpair_send_msg(const char *msg)
{
    np_lock();
    if (g.status != NETPAIR_PAIRED || g.fd < 0) {
        np_unlock();
        return false;
    }
    g.text_len = 0;
    g.text[0] = '\0';
    g.new_data = false;
    g.done_received = false;
    g.status = NETPAIR_STREAMING;
    np_unlock();

    return send_cmd("CHAT %s", msg) == 0;
}

bool netpair_poll(char *out, size_t out_size, bool *new_data)
{
    bool finished = false;
    if (new_data) *new_data = false;
    np_lock();
    if (g.text_len > 0) {
        snprintf(out, out_size, "%s", g.text);
        if (new_data) *new_data = g.new_data;
        g.new_data = false;
    } else {
        if (out_size) out[0] = '\0';
    }
    if (g.done_received) {
        finished = true;
        g.done_received = false;
    }
    np_unlock();
    return finished;
}

void netpair_clear_text(void)
{
    np_lock();
    g.text_len = 0;
    g.text[0] = '\0';
    g.new_data = false;
    g.done_received = false;
    np_unlock();
}

void netpair_disconnect(void)
{
    np_lock();
    g.running = false;
    if (g.fd >= 0) {
        shutdown(g.fd, SHUT_RDWR);
        close(g.fd);
        g.fd = -1;
    }
    np_unlock();

    if (g.thread) {
        SDL_WaitThread(g.thread, NULL);
        g.thread = NULL;
    }

    np_lock();
    if (g.status == NETPAIR_PAIRED || g.status == NETPAIR_STREAMING)
        g.status = NETPAIR_IDLE;
    np_unlock();

    if (g.lock) {
        SDL_DestroyMutex(g.lock);
        g.lock = NULL;
    }
}
