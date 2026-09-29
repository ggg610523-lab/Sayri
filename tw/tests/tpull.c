/*
    Model installer test.

    1. Pull a small real model (all-minilm,
       ~46 MB) and watch progress until done.
    2. Pull a nonexistent model (error path).
    3. Pull with no server reachable.
*/
#include "ollama.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static int wait_pull(
    const char *label,
    OllamaPull *p)
{
    int frames = 0;

    for (;;) {

        ollama_poll_pull(p);

        if (p->done)
            break;

        SDL_Delay(50);

        if (++frames > 1200) {
            printf("FAIL %s timeout\n",
                   label);
            return 0;
        }
    }

    printf("%s done=%d ok=%d "
           "frac=%.2f status=[%s]\n",
           label, p->done, p->ok,
           p->fraction, p->status);

    if (p->error[0])
        printf("  error: %s\n", p->error);

    return 1;
}

int main(void)
{
    ollama_init("llama3.2");

    /*
        1. Real small pull.
    */
    if (!ollama_pull_begin("all-minilm")) {
        printf("FAIL begin\n");
        return 1;
    }

    /*
        Single-flight: second begin must be
        refused while active.
    */
    if (ollama_pull_begin("all-minilm")) {
        printf("FAIL single-flight\n");
        return 1;
    }

    OllamaPull p;

    if (!wait_pull("SMALL", &p))
        return 1;

    if (!p.ok || !strstr(p.status,
                         "Installed")) {

        if (strstr(p.error,
                   "Cannot reach")) {
            printf(
                "SKIP: no Ollama server "
                "on :11434\n");
            ollama_shutdown();
            return 0;
        }

        printf("FAIL small pull\n");
        return 1;
    }

    /*
        2. Nonexistent model.
    */
    ollama_pull_begin(
        "definitely-not-a-real-model-xyz");

    if (!wait_pull("BADMODEL", &p))
        return 1;

    if (p.ok || !p.error[0]) {
        printf("FAIL bad-model path\n");
        return 1;
    }

    /*
        3. Offline.
    */
    ollama_set_host("http://127.0.0.1:11599");

    ollama_pull_begin("all-minilm");

    if (!wait_pull("OFFLINE", &p))
        return 1;

    if (p.ok || !strstr(p.error,
                        "Cannot reach")) {
        printf("FAIL offline path\n");
        return 1;
    }

    ollama_shutdown();

    printf("PASS pull\n");
    return 0;
}
