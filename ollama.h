#ifndef OLLAMA_H
#define OLLAMA_H

#include <stdbool.h>
#include <stddef.h>

#define OLLAMA_MAX_TEXT 2048
#define OLLAMA_DEFAULT_HOST "http://127.0.0.1:11434"
#define OLLAMA_DEFAULT_MODEL "llama3.2"

typedef struct {
    /*
        Valid only when done == true.
    */
    char text[OLLAMA_MAX_TEXT];

    bool done;

    /*
        False when the request failed; text holds
        a human-readable reason.
    */
    bool ok;
} OllamaReply;

/*
    Progress snapshot for a model pull.
*/
typedef struct {
    bool active;    /* pull running      */
    bool done;      /* finished this frame */
    bool ok;

    float fraction; /* 0..1 current blob */

    /*
        Byte counts of the current download.
    */
    double completed;
    double total;

    char status[128];
    char error[320];
} OllamaPull;

void ollama_init(
    const char *model
);

void ollama_set_host(
    const char *host
);

void ollama_set_model(
    const char *model
);

/*
    Build the JSON body for /api/chat.

    roles[]   : "user" / "assistant" / "system"
    contents[]: UTF-8 message text

    Returns malloc'd string, NULL on error.
    Free with free().
*/
char *ollama_build_body(
    const char *model,
    const char **roles,
    const char **contents,
    int count
);

/*
    Fetch installed models from /api/tags.

    Returns model count, or -1 when the server
    cannot be reached.
*/
int ollama_fetch_models(
    char out[][128],
    int max
);

/*
    Blocking request. Writes assistant text (or
    an error description) into out.

    body must be a JSON document as produced by
    ollama_build_body().
*/
bool ollama_perform(
    const char *body,
    char *out,
    size_t out_size
);

/*
    Async API.

    ollama_begin() takes ownership of body (freed
    internally). Returns false if a request is
    already in flight.

    Poll ollama_poll() every frame until
    reply->done is true.
*/
bool ollama_begin(
    const char *body
);

void ollama_poll(
    OllamaReply *reply
);

/*
    Model installer.

    Starts pulling a model from the registry
    (POST /api/pull, streamed). Returns false if
    a pull is already running or the thread
    could not start.

    Poll ollama_poll_pull() every frame; the
    OllamaPull struct tracks progress while
    active is true and done is true exactly once
    at the end.
*/
bool ollama_pull_begin(
    const char *model
);

void ollama_poll_pull(
    OllamaPull *out
);

/*
    Full bootstrap.

    One worker thread walks every step needed
    to get a working local model:

      1. probe the server
      2. download the Ollama runtime if the
         server is missing (pinned release,
         ~1.4 GB, streamed to ~/.local/opt)
      3. extract it (needs system tar + zstd)
      4. start `ollama serve` detached
      5. pull the model

    Poll ollama_poll_setup() every frame.
*/
typedef enum {
    OLLAMA_SETUP_IDLE = 0,
    OLLAMA_SETUP_CHECKING,
    OLLAMA_SETUP_DOWNLOADING,
    OLLAMA_SETUP_EXTRACTING,
    OLLAMA_SETUP_STARTING,
    OLLAMA_SETUP_PULLING,
    OLLAMA_SETUP_DONE
} OllamaSetupStage;

typedef struct {
    bool active;
    bool done;   /* delivered exactly once */
    bool ok;

    OllamaSetupStage stage;

    float fraction; /* 0..1 current phase */

    char status[160];
    char error[320];
} OllamaSetup;

bool ollama_setup_begin(
    const char *model
);

void ollama_poll_setup(
    OllamaSetup *out
);

/*
    Wait for any in-flight request and clean up.
*/
void ollama_shutdown(void);

#endif /* OLLAMA_H */
