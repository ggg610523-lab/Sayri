/*
    Sayri IPC server
    ----------------
    A tiny Unix-socket server so a second client (a
    GNOME extension, CLI, etc.) can send a message to
    the running Sayri app and receive the assistant's
    reply back over the same connection.

    Protocol (newline framed):
        client -> "message\n"
        server -> "<assistant text>" then closes

    The socket lives at $XDG_RUNTIME_DIR/sayri.sock
    (falling back to /tmp/sayri.sock).
*/

#include "ipc.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>

typedef struct {
    int fd;
    char msg[IPC_MAX_MSG];
} Peer;

static SDL_mutex *g_lock = NULL;
static SDL_cond *g_cond = NULL;
static SDL_Thread *g_thread = NULL;

static Peer g_queue[IPC_MAX_QUEUE];
static int g_head = 0;
static int g_tail = 0;
static int g_count = 0;

static bool g_running = false;
static int g_listen_fd = -1;
static char g_path[256] = "";

static void lock(void)
{
    if (g_lock)
        SDL_LockMutex(g_lock);
}

static void unlock(void)
{
    if (g_lock)
        SDL_UnlockMutex(g_lock);
}

/*
    Read one newline-terminated message from a peer.
    Returns message length, or -1 on error/EOF.
    Never blocks for more than a moment: the client
    writes its whole frame, then waits.
*/
static int read_message(int fd, char *out, size_t cap)
{
    size_t n = 0;

    for (;;) {
        char c;

        ssize_t r = recv(fd, &c, 1, 0);

        if (r == 1) {
            if (c == '\n')
                break;
            if ((c == '\r') || (c == '\n'))
                continue;
            if (n + 1 < cap)
                out[n++] = c;
        } else if (r == 0) {
            /* EOF before a newline: what we have is the frame. */
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return -1;
        }
    }

    out[n] = '\0';
    return (int)n;
}

static int accept_thread(void *data)
{
    (void)data;

    while (g_running) {
        int fd = accept(g_listen_fd, NULL, NULL);

        if (fd < 0) {
            if (!g_running)
                break;
            if (errno == EINTR)
                continue;
            SDL_Delay(50);
            continue;
        }

        Peer peer;
        memset(&peer, 0, sizeof(peer));
        peer.fd = fd;

        int len = read_message(fd, peer.msg, sizeof(peer.msg));

        if (len < 0) {
            close(fd);
            continue;
        }

        lock();
        if (g_count < IPC_MAX_QUEUE) {
            g_queue[g_tail] = peer;
            g_tail = (g_tail + 1) % IPC_MAX_QUEUE;
            g_count++;
            if (g_cond)
                SDL_CondBroadcast(g_cond);
        } else {
            /* Queue full: tell the client the app is busy. */
            const char *busy =
                "(Sayri is busy - try again in a moment.)";
            (void)!write(fd, busy, strlen(busy));
            close(fd);
        }
        unlock();
    }

    return 0;
}

int ipc_start(char *out, size_t out_size)
{
    if (g_lock)
        return -1; /* already started */

    g_lock = SDL_CreateMutex();
    g_cond = SDL_CreateCond();

    if (!g_lock || !g_cond)
        return -1;

    const char *runtime = getenv("XDG_RUNTIME_DIR");

    {
        /*
            Keep the socket path inside the 108-byte
            sun_path limit.
        */
        if (runtime && *runtime)
            snprintf(g_path, sizeof(g_path),
                     "%.92s/sayri.sock", runtime);
        else
            snprintf(g_path, sizeof(g_path),
                     "/tmp/sayri.sock");
    }

    unlink(g_path);

    g_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (g_listen_fd < 0)
        return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    {
        size_t plen = strlen(g_path);
        if (plen > sizeof(addr.sun_path) - 1)
            plen = sizeof(addr.sun_path) - 1;
        memcpy(addr.sun_path, g_path, plen);
        addr.sun_path[plen] = '\0';
    }

    if (bind(g_listen_fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    chmod(g_path, 0600);

    if (listen(g_listen_fd, 4) < 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    g_running = true;
    g_head = 0;
    g_tail = 0;
    g_count = 0;

    g_thread = SDL_CreateThread(
        accept_thread, "sayri-ipc", NULL);

    if (!g_thread) {
        g_running = false;
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    if (out && out_size)
        snprintf(out, out_size, "%s", g_path);

    return 0;
}

int ipc_recv(char *out, size_t out_size)
{
    lock();

    if (g_count == 0) {
        unlock();
        return -1;
    }

    Peer p = g_queue[g_head];
    g_head = (g_head + 1) % IPC_MAX_QUEUE;
    g_count--;

    unlock();

    if (out && out_size)
        snprintf(out, out_size, "%s", p.msg);

    return p.fd;
}

void ipc_reply(int fd, const char *text)
{
    if (fd < 0)
        return;

    if (text && *text) {
        size_t n = strlen(text);

        /* A single best-effort write is enough: the
           frame is small and the socket is local. */
        (void)!write(fd, text, n);
    }

    close(fd);
}

void ipc_stop(void)
{
    if (!g_lock)
        return;

    g_running = false;

    if (g_listen_fd >= 0) {
        shutdown(g_listen_fd, SHUT_RDWR);
        close(g_listen_fd);
        g_listen_fd = -1;
    }

    if (g_thread) {
        SDL_WaitThread(g_thread, NULL);
        g_thread = NULL;
    }

    /* Drop any unserved queue slots. */
    while (g_count > 0) {
        Peer p = g_queue[g_head];
        g_head = (g_head + 1) % IPC_MAX_QUEUE;
        g_count--;
        if (p.fd >= 0)
            close(p.fd);
    }

    if (g_path[0])
        unlink(g_path);

    if (g_cond)
        SDL_DestroyCond(g_cond);
    if (g_lock)
        SDL_DestroyMutex(g_lock);

    g_cond = NULL;
    g_lock = NULL;
}