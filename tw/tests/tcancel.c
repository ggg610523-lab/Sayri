/*
    tcancel: verify ollama_shutdown aborts
    an in-flight chat transfer instead of
    blocking until its 300 s timeout.

    Usage: nc -l -p 9999 &  then run this.
*/
#include <SDL2/SDL.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>

#include "ollama.h"

/*
    Child process: accept one connection on
    :9999 and then stay silent forever, so
    the worker's transfer stalls mid-flight.
*/
static void spawn_stall_server(void)
{
    pid_t pid = fork();

    if (pid != 0)
        return;

    int fd = socket(AF_INET,
                    SOCK_STREAM, 0);

    struct sockaddr_in a = {0};

    a.sin_family = AF_INET;
    a.sin_port = htons(9999);
    a.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

    int yes = 1;

    setsockopt(fd, SOL_SOCKET,
               SO_REUSEADDR,
               &yes, sizeof(yes));

    if (bind(fd, (struct sockaddr *)
                  &a, sizeof(a)) == 0 &&
        listen(fd, 1) == 0) {

        int c = accept(fd, NULL, NULL);

        sleep(30);

        close(c);
    }

    _exit(0);
}

static double now_s(void)
{
    struct timespec ts;

    clock_gettime(
        CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec +
           (double)ts.tv_nsec /
               1e9;
}

int main(void)
{
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL: %s\n",
                SDL_GetError());
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    spawn_stall_server();

    ollama_init("fake-model");
    ollama_set_host("http://127.0.0.1:9999");

    const char *msg =
        "{\"model\":\"fake-model\","
        "\"messages\":[{\"role\":\"user\","
        "\"content\":\"hi\"}]}";

    char *body = malloc(strlen(msg) + 1);

    strcpy(body, msg);

    if (!ollama_begin(body)) {
        fprintf(stderr,
                "ollama_begin failed\n");
        return 1;
    }

    printf("chat in flight; "
           "shutting down...\n");

    SDL_Delay(400);

    OllamaReply mid;

    ollama_poll(&mid);

    printf("mid-flight done=%d "
           "ok=%d text='%.40s'\n",
           (int)mid.done,
           (int)mid.ok,
           mid.text);

    double t0 = now_s();

    ollama_shutdown();

    printf("shutdown took %.2f s\n",
           now_s() - t0);

    OllamaReply fin;

    ollama_poll(&fin);

    printf("final text='%s'\n",
           fin.text);

    SDL_Quit();

    return 0;
}
