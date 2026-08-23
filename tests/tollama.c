/*
    Blocking client test against a real
    server (default :11434) plus offline
    error paths.

    Usage: tollama [prompt]
*/
#include "ollama.h"

#include <stdio.h>
#include <stdlib.h>

#define MODELS_TEST_MAX 16

int main(int argc, char **argv)
{
    const char *prompt =
        argc > 1
        ? argv[1]
        : "Reply with exactly: OK";

    ollama_init("llama3.2");

    const char *roles[] = {"user"};
    const char *contents[] = { prompt };

    char *body = ollama_build_body(
        "llama3.2", roles, contents, 1);

    char out[OLLAMA_MAX_TEXT];

    /* 1. Real server */
    bool ok = ollama_perform(
        body, out, sizeof(out));

    printf("REAL ok=%d -> %s\n",
           ok, out);

    /* 2. Model list */
    char models[MODELS_TEST_MAX][128];

    int n = ollama_fetch_models(
        models, MODELS_TEST_MAX);

    printf("MODELS n=%d:", n);

    for (int i = 0; i < n; i++)
        printf(" [%s]", models[i]);

    printf("\n");

    /* 3. Offline path */
    ollama_set_host(
        "http://127.0.0.1:11599");

    ok = ollama_perform(
        body, out, sizeof(out));

    printf("OFFLINE ok=%d -> %s\n",
           ok, out);

    n = ollama_fetch_models(
        models, MODELS_TEST_MAX);

    printf("OFFLINE models n=%d\n", n);

    free(body);
    return 0;
}
