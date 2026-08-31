/*
    Sayri relay — desktop host
    --------------------------
    Always-on TCP server so a paired Sayri phone can
    stream the AI out of this desktop. The phone can't
    run Ollama itself; it connects here and this host
    forwards the chat to the local engine and streams
    the reply back in TOK chunks.

    Each accepted connection gets a background handler
    thread. Chat is handled synchronously through the
    blocking ollama_perform_ex() in that thread (it does
    NOT touch the GUI's async ollama_begin()/g_busy
    path, so remote chats never fight the local UI).
*/

#include "relay.h"

#include <SDL2/SDL.h>
#include "ollama.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define RELAY_TOK_CHUNK 128
#define RELAY_MAX_CLIENTS 8
#define RELAY_MAX_MSG 4096

typedef struct {
    int fd;
    bool paired;
    bool done;
} Client;

typedef struct {
    SDL_mutex *lock;
    Client clients[RELAY_MAX_CLIENTS];
    int listen_fd;
    bool running;
    char code[RELAY_CODE_LEN + 1];
    SDL_Thread *accept_thread;
} Relay;

static Relay g_relay;

/* ---------------- framing helpers ---------------- */

static int sock_send(int fd, const char *data, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = send(fd, data + off, n - off, MSG_NOSIGNAL);
        if (w <= 0) {
            if (w < 0 && errno == EINTR) continue;
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

static int send_line(int fd, const char *fmt, const char *arg)
{
    char buf[512];
    int n;
    if (arg)
        n = snprintf(buf, sizeof(buf), fmt, arg);
    else
        n = snprintf(buf, sizeof(buf), "%s", fmt);
    if (n < 0) return -1;
    if (n > (int)sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    /* The wire protocol is newline-framed; readers break on '\n'. */
    buf[n++] = '\n';
    return sock_send(fd, buf, (size_t)n);
}

static int send_cmd(int fd, const char *line)
{
    return send_line(fd, line, NULL);
}

/*
    Read one newline-terminated line. Returns length
    (excluding \n), 0 on clean EOF, -1 on error.
*/
static int read_line(int fd, char *out, size_t cap)
{
    size_t n = 0;
    for (;;) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 1) {
            if (c == '\n') break;
            if (c == '\r') continue;
            if (n + 1 < cap) out[n++] = c;
        } else if (r == 0) {
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            return -1;
        }
    }
    out[n] = '\0';
    return (int)n;
}

/* ---------------- client registry ---------------- */

static int client_add(int fd)
{
    int slot = -1;
    SDL_LockMutex(g_relay.lock);
    for (int i = 0; i < RELAY_MAX_CLIENTS; i++) {
        if (!g_relay.clients[i].fd) {
            g_relay.clients[i].fd = fd;
            g_relay.clients[i].paired = false;
            g_relay.clients[i].done = false;
            slot = i;
            break;
        }
    }
    SDL_UnlockMutex(g_relay.lock);
    return slot;
}

static void client_remove(int fd)
{
    SDL_LockMutex(g_relay.lock);
    for (int i = 0; i < RELAY_MAX_CLIENTS; i++) {
        if (g_relay.clients[i].fd == fd) {
            g_relay.clients[i].fd = 0;
            g_relay.clients[i].done = true;
            break;
        }
    }
    SDL_UnlockMutex(g_relay.lock);
}

static void client_mark_paired(int fd)
{
    SDL_LockMutex(g_relay.lock);
    for (int i = 0; i < RELAY_MAX_CLIENTS; i++)
        if (g_relay.clients[i].fd == fd) {
            g_relay.clients[i].paired = true;
            break;
        }
    SDL_UnlockMutex(g_relay.lock);
}

int relay_paired_count(void)
{
    int c = 0;
    if (!g_relay.lock) return 0;
    SDL_LockMutex(g_relay.lock);
    for (int i = 0; i < RELAY_MAX_CLIENTS; i++)
        if (g_relay.clients[i].fd && g_relay.clients[i].paired)
            c++;
    SDL_UnlockMutex(g_relay.lock);
    return c;
}

/* ---------------- current pairing code ---------------- */

static void code_rotate(void)
{
    SDL_LockMutex(g_relay.lock);
    unsigned int seed =
        (unsigned int)((unsigned long)SDL_GetTicks() ^
                       (unsigned long)(size_t)&g_relay);
    snprintf(g_relay.code, sizeof(g_relay.code), "%03u", seed % 1000u);
    SDL_UnlockMutex(g_relay.lock);
}

void relay_code(char *out, size_t out_size)
{
    if (!out || !out_size) return;
    SDL_LockMutex(g_relay.lock);
    snprintf(out, out_size, "%s", g_relay.code);
    SDL_UnlockMutex(g_relay.lock);
}

void relay_rotate_code(void)
{
    code_rotate();
}

/* ---------------- chat handling ---------------- */

static int handle_chat(int fd, const char *msg)
{
    const char *roles[1] = {"user"};
    const char *contents[1] = {msg};

    char *body = ollama_build_body(
        OLLAMA_DEFAULT_MODEL, roles, contents, 1);
    if (!body) {
        send_cmd(fd, "DONE ERR Out of memory");
        return -1;
    }

    char text[OLLAMA_MAX_TEXT];
    bool ok = ollama_perform_ex(body, text, sizeof(text), NULL);
    free(body);

    if (!ok) {
        send_line(fd, "DONE ERR %s", text);
        return 0;
    }

    /* Stream the reply in TOK chunks. */
    size_t len = strlen(text);
    size_t off = 0;
    while (off < len) {
        size_t n = len - off;
        if (n > RELAY_TOK_CHUNK) n = RELAY_TOK_CHUNK;

        /* Build "<chunk>\n" and split if it straddles the buffer. */
        char frame[RELAY_TOK_CHUNK + 2];
        memcpy(frame, text + off, n);
        frame[n] = '\n';
        frame[n + 1] = '\0';

        char line[RELAY_TOK_CHUNK * 2 + 32];
        int wrote = snprintf(line, sizeof(line), "TOK %s", frame);
        if (wrote < 0) break;
        if (wrote > (int)sizeof(line) - 1) wrote = (int)sizeof(line) - 1;
        if (sock_send(fd, line, (size_t)wrote) != 0)
            break;
        off += n;
    }

    send_cmd(fd, "DONE OK");
    return 0;
}

/* ---------------- per-connection thread ---------------- */

static int conn_thread(void *data)
{
    int fd = (int)(intptr_t)data;

    /* Tell the client the pairing code and wait for PAIR. */
    {
        char code[RELAY_CODE_LEN + 1];
        relay_code(code, sizeof(code));
        char hello[64];
        snprintf(hello, sizeof(hello), "HELLO %s", code);
        send_cmd(fd, hello);
    }

    bool paired = false;
    for (;;) {
        char line[RELAY_MAX_MSG];
        int n = read_line(fd, line, sizeof(line));
        if (n <= 0) break;

        if (!paired) {
            if (!strncmp(line, "PAIR ", 5)) {
                char code[RELAY_CODE_LEN + 1];
                relay_code(code, sizeof(code));
                if (!strcmp(line + 5, code)) {
                    paired = true;
                    send_cmd(fd, "PAIR OK");
                    client_mark_paired(fd);
                } else {
                    send_cmd(fd, "PAIR ERR Wrong pairing code");
                }
            }
            continue;
        }

        if (!strncmp(line, "CHAT ", 5)) {
            if (handle_chat(fd, line + 5) != 0)
                break;
        } else if (!strcmp(line, "BYE")) {
            break;
        }
    }

    close(fd);
    client_remove(fd);
    return 0;
}

/* ---------------- accept loop ---------------- */

static int accept_thread(void *data)
{
    (void)data;
    while (g_relay.running) {
        int fd = accept(g_relay.listen_fd, NULL, NULL);
        if (fd < 0) {
            if (!g_relay.running) break;
            if (errno == EINTR) continue;
            SDL_Delay(50);
            continue;
        }
        if (client_add(fd) < 0) {
            send_cmd(fd, "DONE ERR Server busy");
            close(fd);
            continue;
        }
        SDL_Thread *t = SDL_CreateThread(
            conn_thread, "sayri-relay",
            (void *)(intptr_t)fd);
        if (!t) {
            close(fd);
            client_remove(fd);
        } else {
            SDL_DetachThread(t);
        }
    }
    return 0;
}

/* ---------------- lifecycle ---------------- */

int relay_start(void)
{
    memset(&g_relay, 0, sizeof(g_relay));
    g_relay.lock = SDL_CreateMutex();
    if (!g_relay.lock) return -1;

    g_relay.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_relay.listen_fd < 0) return -1;

    int opt = 1;
    setsockopt(g_relay.listen_fd, SOL_SOCKET,
               SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)RELAY_PORT);

    if (bind(g_relay.listen_fd, (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        close(g_relay.listen_fd);
        g_relay.listen_fd = -1;
        return -1;
    }
    if (listen(g_relay.listen_fd, 8) < 0) {
        close(g_relay.listen_fd);
        g_relay.listen_fd = -1;
        return -1;
    }

    code_rotate();
    g_relay.running = true;

    g_relay.accept_thread = SDL_CreateThread(
        accept_thread, "sayri-relay-accept", NULL);
    if (!g_relay.accept_thread) {
        g_relay.running = false;
        close(g_relay.listen_fd);
        g_relay.listen_fd = -1;
        return -1;
    }

    return 0;
}

void relay_poll(void)
{
    /* Accept and chat run on background threads; the
       main loop has nothing to pump synchronously. */
}

void relay_stop(void)
{
    if (!g_relay.lock) return;

    g_relay.running = false;
    if (g_relay.listen_fd >= 0) {
        shutdown(g_relay.listen_fd, SHUT_RDWR);
        close(g_relay.listen_fd);
        g_relay.listen_fd = -1;
    }
    if (g_relay.accept_thread) {
        SDL_WaitThread(g_relay.accept_thread, NULL);
        g_relay.accept_thread = NULL;
    }

    /* Close any stray client fds. */
    SDL_LockMutex(g_relay.lock);
    for (int i = 0; i < RELAY_MAX_CLIENTS; i++)
        if (g_relay.clients[i].fd) {
            close(g_relay.clients[i].fd);
            g_relay.clients[i].fd = 0;
        }
    SDL_UnlockMutex(g_relay.lock);

    SDL_DestroyMutex(g_relay.lock);
    g_relay.lock = NULL;
}
