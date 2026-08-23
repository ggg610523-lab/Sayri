/*
    Async-path test: mimics how main.c uses
    the client — ollama_begin() once, then
    ollama_poll() every frame until done.
*/
#include "ollama.h"

#include <SDL.h>
#include <stdio.h>

int main(void)
{
    ollama_init("llama3.2");

    const char *roles[] = {"user"};
    const char *contents[] = {
        "Reply with exactly: ASYNC OK" };

    char *body = ollama_build_body(
        "llama3.2", roles, contents, 1);

    if (!ollama_begin(body)) {
        printf("FAIL begin\n");
        return 1;
    }

    /*
        Second begin must be rejected
        while busy.
    */
    if (ollama_begin(body)) {
        printf("FAIL single-flight\n");
        return 1;
    }

    OllamaReply reply;

    int frames = 0;

    for (;;) {

        ollama_poll(&reply);

        if (reply.done)
            break;

        SDL_Delay(16);
        frames++;

        if (frames > 6000) {
            printf(
                "FAIL timeout (100s)\n");
            return 1;
        }
    }

    printf("done after %d frames "
           "(~%.1fs) ok=%d text=[%s]\n",
           frames,
           frames * 0.016f,
           reply.ok,
           reply.text);

    if (!reply.ok) {
        printf("FAIL reply not ok\n");
        return 1;
    }

    ollama_shutdown();
    free(body);

    printf("PASS async\n");
    return 0;
}
