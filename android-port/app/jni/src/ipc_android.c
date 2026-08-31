/*
    Sayri IPC — Android stub
    -------------------------
    Unix-socket IPC is not used on Android.
    All functions are no-ops.
*/

#include "ipc.h"
#include <stddef.h>

int ipc_start(char *out, size_t out_size)
{
    (void)out; (void)out_size;
    return -1;
}

int ipc_recv(char *out, size_t out_size)
{
    (void)out; (void)out_size;
    return -1;
}

void ipc_reply(int fd, const char *text)
{
    (void)fd; (void)text;
}

void ipc_stop(void)
{
}
