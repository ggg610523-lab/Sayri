#ifndef SAYRI_IPC_H
#define SAYRI_IPC_H

#include <stddef.h>

#define IPC_MAX_MSG 2048
#define IPC_MAX_QUEUE 8

/*
    Start the Unix-socket server on a background
    thread. Writes the socket path into `out`.

    Returns 0 on success, -1 on failure.
*/
int ipc_start(char *out, size_t out_size);

/*
    Dequeue the next pending client message.
    Returns the remote socket fd to reply on, or -1
    when the queue is empty.
*/
int ipc_recv(char *out, size_t out_size);

/*
    Write `text` to the remote socket and close it.
    Must finally send exactly one reply per recv'd fd.
*/
void ipc_reply(int fd, const char *text);

/*
    Abort the accept loop, drop queued peers,
    unlink the socket and join the thread.
*/
void ipc_stop(void);

#endif /* SAYRI_IPC_H */