#include "ollama.h"

#include <SDL2/SDL.h>
#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================
   Internal state
   ============================================================ */

static char g_host[256] = OLLAMA_DEFAULT_HOST;
static char g_model[128] = OLLAMA_DEFAULT_MODEL;

static SDL_mutex *g_lock = NULL;
static SDL_Thread *g_thread = NULL;
static bool g_busy = false;

/*
    Cooperative cancel: set on shutdown so
    blocking transfers abort within one
    progress tick instead of hanging quit.
*/
static SDL_atomic_t g_cancel;
static OllamaReply g_reply;

void ollama_init(
    const char *model)
{
    if (!g_lock)
        g_lock = SDL_CreateMutex();

    ollama_set_model(model);
}

void ollama_set_host(
    const char *host)
{
    snprintf(g_host, sizeof(g_host),
             "%s", host ? host : OLLAMA_DEFAULT_HOST);
}

void ollama_set_model(
    const char *model)
{
    snprintf(g_model, sizeof(g_model),
             "%s", model ? model : OLLAMA_DEFAULT_MODEL);
}

/* ============================================================
   JSON helpers
   ============================================================ */

static void json_escape(
    const char *in,
    char *out,
    size_t out_size)
{
    size_t o = 0;

    for (const unsigned char *p =
            (const unsigned char *)in;
         *p && o + 8 < out_size;
         p++) {

        switch (*p) {

        case '"':
            out[o++] = '\\';
            out[o++] = '"';
            break;

        case '\\':
            out[o++] = '\\';
            out[o++] = '\\';
            break;

        case '\n':
            out[o++] = '\\';
            out[o++] = 'n';
            break;

        case '\r':
            out[o++] = '\\';
            out[o++] = 'r';
            break;

        case '\t':
            out[o++] = '\\';
            out[o++] = 't';
            break;

        default:
            if (*p < 0x20) {
                o += (size_t)snprintf(
                    out + o, out_size - o,
                    "\\u%04x", *p);

            } else {
                out[o++] = (char)*p;
            }
            break;
        }
    }

    out[o] = '\0';
}

char *ollama_build_body(
    const char *model,
    const char **roles,
    const char **contents,
    int count)
{
    size_t cap = 256;

    for (int i = 0; i < count; i++)
        cap += strlen(contents[i]) + 32;

    char *body = malloc(cap);
    if (!body)
        return NULL;

    char esc[OLLAMA_MAX_TEXT];
    size_t len = 0;

    len += (size_t)snprintf(
        body + len, cap - len,
        "{\"model\":\"%s\",\"messages\":[",
        model ? model : "llama3.2");

    for (int i = 0; i < count; i++) {

        json_escape(contents[i],
                    esc, sizeof(esc));

        int n = snprintf(
            body + len, cap - len,
            "%s{\"role\":\"%s\",\"content\":\"%s\"}",
            i ? "," : "",
            roles[i] ? roles[i] : "user",
            esc);

        if (n < 0 || (size_t)n >= cap - len)
            break;

        len += (size_t)n;
    }

    snprintf(body + len, cap - len,
             "],\"stream\":false}");

    return body;
}

/*
    Extract a simple string field from a JSON
    document. Handles escapes including \uXXXX.

    Returns false when the key is not found.
*/
static bool json_extract_string(
    const char *json,
    const char *key,
    char *out,
    size_t out_size)
{
    char needle[64];

    snprintf(needle, sizeof(needle),
             "\"%s\"", key);

    const char *p =
        strstr(json, needle);

    if (!p)
        return false;

    p += strlen(needle);

    while (*p == ' ' ||
           *p == ':' ||
           *p == '\n' ||
           *p == '\r' ||
           *p == '\t')
        p++;

    if (*p != '"')
        return false;

    p++;

    size_t o = 0;

    while (*p && *p != '"') {

        if (o + 8 >= out_size)
            break;

        if (*p != '\\') {
            out[o++] = *p++;
            continue;
        }

        p++;

        switch (*p) {

        case '"':  out[o++] = '"';  p++; break;
        case '\\': out[o++] = '\\'; p++; break;
        case '/':  out[o++] = '/';  p++; break;
        case 'n':  out[o++] = '\n'; p++; break;
        case 'r':  out[o++] = '\r'; p++; break;
        case 't':  out[o++] = '\t'; p++; break;
        case 'b':  out[o++] = '\b'; p++; break;
        case 'f':  out[o++] = '\f'; p++; break;

        case 'u': {

            unsigned int cp = 0;

            if (sscanf(p + 1, "%4x", &cp) != 1) {
                out[o++] = '?';
                p++;
                break;
            }

            p += 5;

            /*
                Surrogate pair?
            */
            if (cp >= 0xD800 && cp <= 0xDBFF &&
                p[0] == '\\' && p[1] == 'u') {

                unsigned int lo = 0;

                if (sscanf(p + 2, "%4x", &lo) == 1 &&
                    lo >= 0xDC00 && lo <= 0xDFFF) {

                    cp = 0x10000 +
                        ((cp - 0xD800) << 10) +
                        (lo - 0xDC00);

                    p += 6;
                }
            }

            /* Encode UTF-8 */
            if (cp < 0x80) {
                out[o++] = (char)cp;

            } else if (cp < 0x800) {
                out[o++] =
                    (char)(0xC0 | (cp >> 6));
                out[o++] =
                    (char)(0x80 | (cp & 0x3F));

            } else if (cp < 0x10000) {
                out[o++] =
                    (char)(0xE0 | (cp >> 12));
                out[o++] =
                    (char)(0x80 |
                        ((cp >> 6) & 0x3F));
                out[o++] =
                    (char)(0x80 | (cp & 0x3F));

            } else {
                out[o++] =
                    (char)(0xF0 | (cp >> 18));
                out[o++] =
                    (char)(0x80 |
                        ((cp >> 12) & 0x3F));
                out[o++] =
                    (char)(0x80 |
                        ((cp >> 6) & 0x3F));
                out[o++] =
                    (char)(0x80 | (cp & 0x3F));
            }

            break;
        }

        default:
            out[o++] = *p++;
            break;
        }
    }

    /*
        Do not cut a UTF-8 sequence in half.
    */
    if (o < out_size - 1) {
        while (o > 0 &&
               ((unsigned char)out[o] & 0xC0) == 0x80)
            o--;
    }

    out[o] = '\0';

    return true;
}

/* ============================================================
   HTTP
   ============================================================ */

typedef struct {
    char *data;
    size_t size;
} HttpBuffer;

static size_t http_write_cb(
    void *contents,
    size_t size,
    size_t nmemb,
    void *userp)
{
    HttpBuffer *buf = userp;

    size_t total = size * nmemb;

    char *grown =
        realloc(buf->data, buf->size + total + 1);

    if (!grown)
        return 0;

    buf->data = grown;

    memcpy(buf->data + buf->size,
           contents, total);

    buf->size += total;
    buf->data[buf->size] = '\0';

    return total;
}

/*
    Progress callback that aborts the
    transfer once cancellation is requested.
    libcurl stops immediately when this
    returns nonzero
    (CURLE_ABORTED_BY_CALLBACK).
*/
static int cancel_xfer_cb(
    void *clientp,
    curl_off_t dltotal,
    curl_off_t dlnow,
    curl_off_t ultotal,
    curl_off_t ulnow)
{
    (void)clientp;
    (void)dltotal; (void)dlnow;
    (void)ultotal; (void)ulnow;

    return SDL_AtomicGet(&g_cancel)
        ? 1 : 0;
}

void ollama_cancel(void)
{
    SDL_AtomicSet(&g_cancel, 1);
}

bool ollama_perform_ex(
    const char *body,
    char *out,
    size_t out_size,
    bool *server_down)
{
    if (server_down)
        *server_down = false;

    CURL *curl = curl_easy_init();

    if (!curl) {
        snprintf(out, out_size,
                 "(Ollama) Could not init HTTP.");
        return false;
    }

    char url[512];
    snprintf(url, sizeof(url),
             "%s/api/chat", g_host);

    HttpBuffer buf = {0};

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers,
        "Content-Type: application/json");

    curl_easy_setopt(curl,
        CURLOPT_URL, url);

    curl_easy_setopt(curl,
        CURLOPT_POSTFIELDS, body);

    curl_easy_setopt(curl,
        CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl,
        CURLOPT_WRITEFUNCTION, http_write_cb);

    curl_easy_setopt(curl,
        CURLOPT_WRITEDATA, &buf);

    curl_easy_setopt(curl,
        CURLOPT_CONNECTTIMEOUT, 4L);

    curl_easy_setopt(curl,
        CURLOPT_TIMEOUT, 300L);

    curl_easy_setopt(curl,
        CURLOPT_LOW_SPEED_LIMIT, 1L);

    curl_easy_setopt(curl,
        CURLOPT_LOW_SPEED_TIME, 90L);

    curl_easy_setopt(curl,
        CURLOPT_XFERINFOFUNCTION,
        cancel_xfer_cb);

    curl_easy_setopt(curl,
        CURLOPT_NOPROGRESS, 0L);

    curl_easy_setopt(curl,
        CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);

    long status = 0;

    curl_easy_getinfo(curl,
        CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    bool ok = false;

    if (res == CURLE_ABORTED_BY_CALLBACK) {

        snprintf(out, out_size,
                 "(Cancelled.)");

    } else if (res != CURLE_OK) {

        if (server_down)
            *server_down = true;

        snprintf(out, out_size,
            "(Cannot reach Ollama at %s "
            "\u2014 is it running?)",
            g_host);

    } else if (status != 200) {

        char err[512] = "unknown error";

        json_extract_string(buf.data ? buf.data : "",
            "error", err, sizeof(err));

        snprintf(out, out_size,
            "(Ollama error %ld: %s)",
            status, err);

    } else {

        ok = json_extract_string(
            buf.data ? buf.data : "",
            "content", out, out_size);

        if (!ok || !*out) {

            snprintf(out, out_size,
                "(Ollama returned an empty "
                "response.)");

            ok = false;
        }
    }

    free(buf.data);

    return ok;
}

bool ollama_perform(
    const char *body,
    char *out,
    size_t out_size)
{
    return ollama_perform_ex(
        body, out, out_size, NULL);
}

/* ============================================================
   Model list
   ============================================================ */

int ollama_fetch_models(
    char out[][128],
    int max)
{
    CURL *curl = curl_easy_init();

    if (!curl)
        return -1;

    char url[512];
    snprintf(url, sizeof(url),
             "%s/api/tags", g_host);

    HttpBuffer buf = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl,
        CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl,
        CURLOPT_WRITEFUNCTION, http_write_cb);
    curl_easy_setopt(curl,
        CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl,
        CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl,
        CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl,
        CURLOPT_NOSIGNAL, 1L);

    CURLcode res =
        curl_easy_perform(curl);

    long status = 0;

    curl_easy_getinfo(curl,
        CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK || status != 200) {
        free(buf.data);
        return -1;
    }

    int count = 0;

    const char *p = buf.data;

    while (count < max &&
           (p = strstr(p, "\"name\":\""))) {

        /*
            Skip the 8-character prefix
            "name":" to reach the value.
        */
        p += 8;

        const char *e = strchr(p, '"');

        if (!e)
            break;

        size_t len = (size_t)(e - p);

        if (len > 127)
            len = 127;

        memcpy(out[count], p, len);
        out[count][len] = '\0';

        count++;
        p = e;
    }

    free(buf.data);

    return count;
}

/* ============================================================
   Model installer (streaming /api/pull)
   ============================================================ */

static OllamaPull g_pull_state;
static SDL_Thread *g_pull_thread = NULL;

/*
    Growable buffer that also remembers how many
    bytes have already been split into lines.
*/
typedef struct {
    char *data;
    size_t len;
    size_t cap;
    size_t parsed;
} LineBuffer;

static bool line_buf_append(
    LineBuffer *lb,
    const char *chunk,
    size_t n)
{
    if (lb->len + n + 1 > lb->cap) {

        size_t cap = lb->cap
            ? lb->cap
            : 4096;

        while (cap < lb->len + n + 1)
            cap *= 2;

        char *nd =
            realloc(lb->data, cap);

        if (!nd)
            return false;

        lb->data = nd;
        lb->cap = cap;
    }

    memcpy(lb->data + lb->len, chunk, n);

    lb->len += n;
    lb->data[lb->len] = '\0';

    return true;
}

/*
    Apply one NDJSON progress line to the shared
    pull state.
*/
static void pull_apply_line(
    const char *line)
{
    char status[128];
    char err[256];

    status[0] = '\0';
    err[0] = '\0';

    json_extract_string(line, "status",
                        status, sizeof(status));
    json_extract_string(line, "error",
                        err, sizeof(err));

    /*
        Compute the new state locally, then
        publish under the lock.
    */
    OllamaPull next = g_pull_state;
    next.done = false;

    if (err[0]) {

        snprintf(next.error,
                 sizeof(next.error),
                 "%s", err);
        next.ok = false;
        next.active = false;
        next.done = true;
        snprintf(next.status,
                 sizeof(next.status),
                 "Failed");

        SDL_LockMutex(g_lock);
        g_pull_state = next;
        SDL_UnlockMutex(g_lock);

        return;
    }

    if (!status[0])
        return;

    if (!strcmp(status, "success")) {

        next.fraction = 1.0f;
        next.completed = next.total;
        next.ok = true;
        next.active = false;
        next.done = true;
        snprintf(next.status,
                 sizeof(next.status),
                 "Installed");

    } else if (strstr(status,
                      "verifying")) {

        next.fraction = 1.0f;
        snprintf(next.status,
                 sizeof(next.status),
                 "Verifying\u2026");

    } else if (strstr(status,
                      "writing manifest")) {

        next.fraction = 1.0f;
        snprintf(next.status,
                 sizeof(next.status),
                 "Finalizing\u2026");

    } else if (strstr(status,
                      "pulling manifest") ||
               strstr(status,
                      "starting")) {

        snprintf(next.status,
                 sizeof(next.status),
                 "Contacting registry\u2026");

    } else {

        /*
            Downloading a blob: read the byte
            counters.
        */
        double total = 0.0;
        double completed = 0.0;

        const char *t =
            strstr(line, "\"total\":");

        const char *c =
            strstr(line, "\"completed\":");

        int have =
            t && c &&
            sscanf(t + 8, "%lf", &total) == 1 &&
            sscanf(c + 12, "%lf", &completed) == 1 &&
            total > 0.0;

        if (have) {

            next.total = total;
            next.completed = completed;
            next.fraction = (float)
                (completed / total);

            if (next.fraction > 1.0f)
                next.fraction = 1.0f;

            snprintf(next.status,
                     sizeof(next.status),
                     "Downloading");
        }
    }

    SDL_LockMutex(g_lock);
    g_pull_state = next;
    SDL_UnlockMutex(g_lock);
}

static void pull_process_lines(
    LineBuffer *lb,
    bool flush_last)
{
    char *p = lb->data + lb->parsed;

    char *nl;

    while (lb->parsed < lb->len &&
           (nl = memchr(p, '\n',
                        lb->len - lb->parsed))) {

        *nl = '\0';

        if (*p)
            pull_apply_line(p);

        lb->parsed =
            (size_t)(nl - lb->data) + 1;

        p = nl + 1;
    }

    /*
        A trailing line without newline is only
        complete once the stream ends.
    */
    if (flush_last &&
        lb->parsed < lb->len &&
        lb->data[lb->parsed]) {
        pull_apply_line(lb->data +
                        lb->parsed);
        lb->parsed = lb->len;
    }
}

static size_t pull_write_cb(
    char *ptr,
    size_t size,
    size_t nmemb,
    void *userp)
{
    LineBuffer *lb = userp;

    size_t total = size * nmemb;

    if (!line_buf_append(lb, ptr, total))
        return 0; /* abort transfer */

    pull_process_lines(lb, false);

    return total;
}

static int pull_thread(void *data)
{
    char *model = data;

    /*
        Body: {"model":"<name>","stream":true}
        Model tags are registry-safe ASCII.
    */
    char body[192];

    snprintf(body, sizeof(body),
             "{\"model\":\"%s\","
             "\"stream\":true}",
             model);

    free(model);

    CURL *curl = curl_easy_init();

    LineBuffer lb = {0};

    if (!curl) {

        SDL_LockMutex(g_lock);

        snprintf(g_pull_state.error,
                 sizeof(g_pull_state.error),
                 "Out of memory.");
        g_pull_state.done = true;
        g_pull_state.active = false;

        SDL_UnlockMutex(g_lock);

        return 1;
    }

    char url[512];
    snprintf(url, sizeof(url),
             "%s/api/pull", g_host);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl,
        CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl,
        CURLOPT_WRITEFUNCTION, pull_write_cb);
    curl_easy_setopt(curl,
        CURLOPT_WRITEDATA, &lb);
    curl_easy_setopt(curl,
        CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl,
        CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl,
        CURLOPT_LOW_SPEED_TIME, 30L);

    curl_easy_setopt(curl,
        CURLOPT_XFERINFOFUNCTION,
        cancel_xfer_cb);

    curl_easy_setopt(curl,
        CURLOPT_NOPROGRESS, 0L);

    curl_easy_setopt(curl,
        CURLOPT_NOSIGNAL, 1L);

    CURLcode res =
        curl_easy_perform(curl);

    long http_status = 0;

    curl_easy_getinfo(curl,
        CURLINFO_RESPONSE_CODE,
        &http_status);

    curl_easy_cleanup(curl);

    pull_process_lines(&lb, true);

    free(lb.data);

    /*
        Publish terminal state.
    */
    SDL_LockMutex(g_lock);

    if (res == CURLE_ABORTED_BY_CALLBACK) {

        g_pull_state.done = true;
        g_pull_state.active = false;
        g_pull_state.ok = false;

        snprintf(
            g_pull_state.error,
            sizeof(g_pull_state.error),
            "(Download cancelled.)");

    } else if (!g_pull_state.done) {

        g_pull_state.done = true;
        g_pull_state.active = false;

        if (res != CURLE_OK) {

            g_pull_state.ok = false;

            snprintf(
                g_pull_state.error,
                sizeof(g_pull_state.error),
                "(Cannot reach Ollama at %s "
                "\u2014 is it running?)",
                g_host);

        } else if (http_status != 200) {

            g_pull_state.ok = false;

            snprintf(
                g_pull_state.error,
                sizeof(g_pull_state.error),
                "(Server error %ld while "
                "pulling.)",
                http_status);
        }
    }

    SDL_UnlockMutex(g_lock);

    return 0;
}

bool ollama_pull_begin(
    const char *model)
{
    if (!g_lock)
        g_lock = SDL_CreateMutex();

    SDL_LockMutex(g_lock);

    if (g_pull_state.active ||
        !model || !*model) {
        SDL_UnlockMutex(g_lock);
        return false;
    }

    memset(&g_pull_state, 0,
           sizeof(g_pull_state));

    g_pull_state.active = true;

    SDL_AtomicSet(&g_cancel, 0);

    snprintf(g_pull_state.status,
             sizeof(g_pull_state.status),
             "Starting\u2026");

    size_t n = strlen(model) + 1;

    char *copy = malloc(n);

    if (copy)
        memcpy(copy, model, n);

    g_pull_thread = copy
        ? SDL_CreateThread(pull_thread,
                           "ollama-pull", copy)
        : NULL;

    if (!g_pull_thread) {

        g_pull_state.active = false;
        g_pull_state.done = true;
        g_pull_state.ok = false;

        snprintf(g_pull_state.error,
                 sizeof(g_pull_state.error),
                 "Could not start download.");

        SDL_UnlockMutex(g_lock);

        free(copy);

        return false;
    }

    SDL_UnlockMutex(g_lock);

    return true;
}

void ollama_poll_pull(
    OllamaPull *out)
{
    memset(out, 0, sizeof(*out));

    if (!g_lock)
        return;

    SDL_LockMutex(g_lock);

    *out = g_pull_state;

    /*
        done is delivered exactly once.
    */
    g_pull_state.done = false;

    SDL_UnlockMutex(g_lock);
}

/* ============================================================
   Full bootstrap (runtime + server + model)
   ============================================================ */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

static OllamaSetup g_setup;
static SDL_Thread *g_setup_thread = NULL;

#define OLLAMA_RELEASE_URL \
    "https://github.com/ollama/ollama/" \
    "releases/download/v0.32.15/" \
    "ollama-linux-amd64.tar.zst"

static void setup_publish(
    bool active, bool done, bool ok,
    OllamaSetupStage stage, float frac,
    const char *fmt, ...)
{
    char buf[160];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SDL_LockMutex(g_lock);

    g_setup.active = active;
    g_setup.done = done;
    g_setup.ok = ok;
    g_setup.stage = stage;
    g_setup.fraction = frac;

    snprintf(g_setup.status,
             sizeof(g_setup.status),
             "%s", buf);

    SDL_UnlockMutex(g_lock);
}

static void setup_fail(
    const char *msg)
{
    SDL_LockMutex(g_lock);

    g_setup.active = false;
    g_setup.done = true;
    g_setup.ok = false;

    snprintf(g_setup.error,
             sizeof(g_setup.error),
             "%s", msg);

    snprintf(g_setup.status,
             sizeof(g_setup.status),
             "Failed");

    SDL_UnlockMutex(g_lock);
}

/*
    Quick reachability probe.
*/
static bool probe_server(void)
{

    CURL *curl = curl_easy_init();

    if (!curl)
        return false;

    char url[512];
    snprintf(url, sizeof(url),
             "%s/api/version", g_host);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl,
        CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl,
        CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl,
        CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl,
        CURLOPT_TIMEOUT, 4L);
    curl_easy_setopt(curl,
        CURLOPT_NOSIGNAL, 1L);

    CURLcode res =
        curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl,
        CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(curl);

    return res == CURLE_OK &&
           status == 200;
}

/*
    True when the model tag is already
    installed on the server. Tolerates any
    explicit or implicit quantizer suffix
    ("llama3.2" matches "llama3.2:latest").
*/
static bool model_installed(
    const char *model)
{
    char names[16][128];

    int n = ollama_fetch_models(
        names, 16);

    if (n <= 0)
        return false;

    size_t ml = strlen(model);

    for (int i = 0; i < n; i++) {

        if (!strncmp(names[i], model, ml) &&
            (names[i][ml] == ':' ||
             names[i][ml] == '\0'))
            return true;
    }

    return false;
}

/*
    Rewrite `out` with the first executable
    `ollama` found on $PATH. Lets the app
    reuse an existing install instead of
    pulling down the pinned release again.
*/
static bool find_ollama_on_path(
    char *out,
    size_t out_size)
{
    const char *path = getenv("PATH");

    if (!path || !*path)
        return false;

    char *dirs = strdup(path);

    if (!dirs)
        return false;

    char *save = NULL;

    for (char *dir = strtok_r(dirs, ":",
                              &save);
         dir;
         dir = strtok_r(NULL, ":",
                        &save)) {

        snprintf(out, out_size,
                 "%s/ollama", dir);

        if (access(out, X_OK) == 0) {
            free(dirs);
            return true;
        }
    }

    free(dirs);

    return false;
}

typedef struct {
    OllamaSetupStage stage;
} DownloadProgressCtx;

static int download_xfer_cb(
    void *clientp,
    curl_off_t dltotal,
    curl_off_t dlnow,
    curl_off_t ultotal,
    curl_off_t ulnow)
{
    (void)ultotal; (void)ulnow;

    DownloadProgressCtx *ctx = clientp;

    if (dltotal > 0) {

        float frac =
            (float)((double)dlnow /
                    (double)dltotal);

        double mb =
            (double)dlnow /
            (1024.0 * 1024.0);

        double total_mb =
            (double)dltotal /
            (1024.0 * 1024.0);

        setup_publish(
            true, false, false,
            ctx->stage, frac,
            "Downloading Ollama  "
            "%.0f / %.0f MB",
            mb, total_mb);
    }

    return SDL_AtomicGet(&g_cancel)
        ? 1 : 0;
}

/*
    Stream a URL to dest_path with progress.
*/
static bool download_file(
    const char *url,
    const char *dest_path,
    OllamaSetupStage stage)
{
    FILE *f = fopen(dest_path, "wb");

    if (!f) {
        setup_fail("Cannot create "
                   "download file in "
                   "~/.local/opt.");
        return false;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        fclose(f);
        setup_fail("Out of memory.");
        return false;
    }

    DownloadProgressCtx ctx = { stage };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl,
        CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl,
        CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl,
        CURLOPT_XFERINFOFUNCTION,
        download_xfer_cb);
    curl_easy_setopt(curl,
        CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl,
        CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl,
        CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl,
        CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl,
        CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl,
        CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl,
        CURLOPT_NOSIGNAL, 1L);

    CURLcode res =
        curl_easy_perform(curl);

    long http_status = 0;
    curl_easy_getinfo(curl,
        CURLINFO_RESPONSE_CODE,
        &http_status);

    curl_easy_cleanup(curl);
    fclose(f);

    if (res == CURLE_ABORTED_BY_CALLBACK) {

        setup_fail(
            "(Setup cancelled.)");

        unlink(dest_path);

        return false;
    }

    if (res != CURLE_OK ||
        http_status != 200) {

        char msg[320];
        snprintf(msg, sizeof(msg),
                 "(Download failed: HTTP "
                 "%ld \u2014 check internet.)",
                 http_status);

        setup_fail(msg);

        unlink(dest_path);

        return false;
    }

    return true;
}

/*
    Start `ollama serve` detached from this
    process; returns child pid or -1.
*/
static long spawn_server(
    const char *exe_path,
    const char *models_dir)
{
    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid > 0)
        return (long)pid;

    /*
        Child: own session, quiet output.
    */
    setsid();

    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    freopen("/dev/null", "r", stdin);

    setenv("OLLAMA_MODELS",
           models_dir, 1);

    execl(exe_path, exe_path,
          "serve", (char *)NULL);

    _exit(127);
}

static int setup_thread(void *data)
{
    char *model = data;

    const char *home_env =
        getenv("HOME");

    if (!home_env || !*home_env) {
        setup_fail("$HOME is not set.");
        free(model);
        return 1;
    }

    /*
        Clamp once so every derived path has a
        bounded, compiler-verifiable length.
    */
    char home_buf[256];

    snprintf(home_buf, sizeof(home_buf),
             "%s", home_env);

    const char *home = home_buf;

    char opt_dir[320], bin_dir[320];
    char exe_path[320], models_dir[320];
    char archive[320];
    char install_dir[320];

    snprintf(opt_dir, sizeof(opt_dir),
             "%s/.local/opt/sayri", home);
    snprintf(bin_dir, sizeof(bin_dir),
             "%s/.local/bin", home);
    snprintf(exe_path, sizeof(exe_path),
             "%s/.local/opt/sayri/ollama/"
             "bin/ollama",
             home);
    snprintf(models_dir, sizeof(models_dir),
             "%s/.local/share/ollama/models",
             home);
    snprintf(archive, sizeof(archive),
             "%s/.local/opt/sayri/"
             "ollama.tar.zst",
             home);
    snprintf(install_dir,
             sizeof(install_dir),
             "%s/.local/opt/sayri/ollama",
             home);

    setup_publish(true, false, false,
                  OLLAMA_SETUP_CHECKING,
                  0.0f,
                  "Checking for Ollama\u2026");

    mkdir(opt_dir, 0755);
    mkdir(bin_dir, 0755);

    struct stat st;

    if (!probe_server()) {

        /*
            Server down: need a runtime. Reuse
            what is already on the machine
            first — managed install, then any
            `ollama` on $PATH — and only hit
            the network as a last resort.
        */
        bool managed_ok =
            stat(exe_path, &st) == 0;

        if (managed_ok) {

            /*
                An interrupted extraction leaves
                the CLI behind without its
                runners; `serve` then starts but
                every chat dies with
                "llama-server binary not found".
                Treat those as missing.
            */
            char lib_dir[512];

            snprintf(lib_dir,
                     sizeof(lib_dir),
                     "%s/lib/ollama",
                     install_dir);

            managed_ok =
                stat(lib_dir, &st) == 0;
        }

        if (!managed_ok &&
            !find_ollama_on_path(
                exe_path,
                sizeof(exe_path))) {

            /*
                A leftover archive from an
                interrupted run is still usable —
                skip the 1.4 GB re-download. If
                it turns out corrupt it is removed
                below so the next attempt fetches
                a fresh one.
            */
            bool have_archive =
                stat(archive, &st) == 0 &&
                st.st_size > 1024 * 1024;

            if (!have_archive) {

                setup_publish(
                    true, false, false,
                    OLLAMA_SETUP_DOWNLOADING,
                    0.0f,
                    "Downloading Ollama\u2026");

                if (!download_file(
                        OLLAMA_RELEASE_URL,
                        archive,
                        OLLAMA_SETUP_DOWNLOADING))
                    goto fail_keep_file;
            }

            setup_publish(
                true, false, false,
                OLLAMA_SETUP_EXTRACTING,
                0.0f,
                "Extracting\u2026");

            /*
                The release tarball has no
                wrapping directory: it contains
                bin/ollama at its top level, so
                extract straight into install_dir
                where exe_path expects it.
            */
            mkdir(install_dir, 0755);

            char cmd[760];
            snprintf(cmd, sizeof(cmd),
                     "tar --zstd -xf '%s' "
                     "-C '%s' 2>/dev/null",
                     archive, install_dir);

            int rc = system(cmd);

            if (rc != 0) {
                setup_fail(
                    "Extraction failed "
                    "(is tar/zstd "
                    "installed?)");
                unlink(archive);
                goto fail_no_file;
            }

            char lib_dir[512];

            snprintf(lib_dir,
                     sizeof(lib_dir),
                     "%s/lib/ollama",
                     install_dir);

            if (stat(exe_path, &st) != 0 ||
                stat(lib_dir, &st) != 0) {
                setup_fail(
                    "Install layout "
                    "unexpected after "
                    "extraction.");
                unlink(archive);
                goto fail_no_file;
            }

            unlink(archive);
        }

        /*
            Convenience symlink for CLI use.
            Skipped when the runtime WAS found
            through that very link, otherwise
            unlink + symlink would leave it
            dangling.
        */
        char link_path[512];
        snprintf(link_path,
                 sizeof(link_path),
                 "%s/ollama", bin_dir);

        if (strcmp(exe_path, link_path) != 0) {

            unlink(link_path);

            symlink(exe_path, link_path);
        }

        setup_publish(
            true, false, false,
            OLLAMA_SETUP_STARTING,
            0.0f,
            "Starting server\u2026");

        spawn_server(exe_path,
                     models_dir);

        bool up = false;

        for (int i = 0; i < 48 && !up;
             i++) {

            SDL_Delay(500);

            up = probe_server();
        }

        if (!up) {
            setup_fail(
                "Server did not come up.");
            goto fail_no_file;
        }
    }

    /*
        Model pull phase: reuse the pull
        machinery and mirror its progress —
        but only when the tag is actually
        missing, so re-runs (app launch,
        auto-heal) finish in a second.
    */
    if (model_installed(model)) {

        setup_publish(false, true, true,
                      OLLAMA_SETUP_DONE,
                      1.0f, "Ready.");

        free(model);

        return 0;
    }

    setup_publish(true, false, false,
                  OLLAMA_SETUP_PULLING,
                  0.0f,
                  "Preparing model\u2026");

    if (!ollama_pull_begin(model)) {

        setup_fail(
            "Could not start model "
            "download.");

        free(model);
        return 1;
    }

    free(model);
    model = NULL;

    for (;;) {

        SDL_Delay(100);

        OllamaPull snap;

        SDL_LockMutex(g_lock);
        snap = g_pull_state;
        SDL_UnlockMutex(g_lock);

        char line[160];

        if (snap.total > 0) {

            double done_mb =
                snap.completed /
                (1024.0 * 1024.0);

            double total_mb =
                snap.total /
                (1024.0 * 1024.0);

            snprintf(line, sizeof(line),
                     "Model  %.0f / %.0f MB",
                     done_mb, total_mb);

        } else {

            snprintf(line, sizeof(line),
                     "%s",
                     snap.status[0]
                     ? snap.status
                     : "Downloading "
                       "model\u2026");
        }

        setup_publish(
            true, false, false,
            OLLAMA_SETUP_PULLING,
            snap.fraction, "%s", line);

        if (!snap.active)
            break;
    }

    /*
        Final verdict of the pull.
    */
    bool pok;
    char perr[320];

    SDL_LockMutex(g_lock);

    pok = g_pull_state.ok;

    snprintf(perr, sizeof(perr),
             "%s", g_pull_state.error);

    SDL_UnlockMutex(g_lock);

    if (!pok) {

        setup_fail(
            perr[0] ? perr
                    : "Model download "
                      "failed.");

        return 1;
    }

    setup_publish(false, true, true,
                  OLLAMA_SETUP_DONE,
                  1.0f, "Ready.");

    return 0;

fail_keep_file:
fail_no_file:

    if (model)
        free(model);

    return 1;
}

bool ollama_setup_begin(
    const char *model)
{
    if (!g_lock)
        g_lock = SDL_CreateMutex();

    SDL_LockMutex(g_lock);

    if (g_setup.active ||
        !model || !*model) {
        SDL_UnlockMutex(g_lock);
        return false;
    }

    memset(&g_setup, 0,
           sizeof(g_setup));

    g_setup.active = true;
    g_setup.stage =
        OLLAMA_SETUP_CHECKING;

    SDL_AtomicSet(&g_cancel, 0);

    snprintf(g_setup.status,
             sizeof(g_setup.status),
             "Checking\u2026");

    size_t n = strlen(model) + 1;

    char *copy = malloc(n);

    if (copy)
        memcpy(copy, model, n);

    g_setup_thread = copy
        ? SDL_CreateThread(setup_thread,
                           "ollama-setup",
                           copy)
        : NULL;

    if (!g_setup_thread) {

        g_setup.active = false;
        g_setup.done = true;

        snprintf(g_setup.error,
                 sizeof(g_setup.error),
                 "Could not start setup.");

        SDL_UnlockMutex(g_lock);

        free(copy);

        return false;
    }

    SDL_UnlockMutex(g_lock);

    return true;
}

void ollama_poll_setup(
    OllamaSetup *out)
{
    memset(out, 0, sizeof(*out));

    if (!g_lock)
        return;

    SDL_LockMutex(g_lock);

    *out = g_setup;

    g_setup.done = false;

    SDL_UnlockMutex(g_lock);
}

/* ============================================================
   Async wrapper (chat)
   ============================================================ */

static int worker_thread(void *data)
{
    char *body = data;

    char text[OLLAMA_MAX_TEXT];

    bool server_down = false;

    bool ok = ollama_perform_ex(
        body, text, sizeof(text),
        &server_down);

    free(body);

    if (!g_lock)
        return 1;

    SDL_LockMutex(g_lock);

    snprintf(g_reply.text,
             sizeof(g_reply.text),
             "%s", text);

    g_reply.ok = ok;
    g_reply.server_down = server_down;
    g_reply.done = true;

    /*
        Clear busy so ollama_poll() will
        deliver the reply.
    */
    g_busy = false;

    SDL_UnlockMutex(g_lock);

    return 0;
}

bool ollama_begin(
    const char *body)
{
    if (!g_lock)
        g_lock = SDL_CreateMutex();

    SDL_LockMutex(g_lock);

    if (g_busy) {
        SDL_UnlockMutex(g_lock);
        return false;
    }

    g_busy = true;
    g_reply.done = false;
    g_reply.ok = false;
    g_reply.text[0] = '\0';

    SDL_AtomicSet(&g_cancel, 0);

    g_thread = SDL_CreateThread(
        worker_thread, "ollama",
        (void *)body);

    if (!g_thread) {
        g_busy = false;
        SDL_UnlockMutex(g_lock);
        return false;
    }

    SDL_UnlockMutex(g_lock);

    return true;
}

void ollama_poll(
    OllamaReply *reply)
{
    reply->done = false;
    reply->ok = false;
    reply->text[0] = '\0';

    if (!g_lock)
        return;

    SDL_LockMutex(g_lock);

    if (!g_busy && g_reply.done) {
        *reply = g_reply;
        g_reply.done = false;
    }

    SDL_UnlockMutex(g_lock);
}

void ollama_shutdown(void)
{
    /*
        Ask every worker to abort its
        transfer first; joining without
        this blocks quit until a multi-GB
        download or 300 s chat timeout
        finishes.
    */
    SDL_AtomicSet(&g_cancel, 1);

    if (g_thread) {
        SDL_WaitThread(g_thread, NULL);
        g_thread = NULL;
    }

    if (g_pull_thread) {
        SDL_WaitThread(g_pull_thread,
                       NULL);
        g_pull_thread = NULL;
    }

    if (g_setup_thread) {
        SDL_WaitThread(g_setup_thread,
                       NULL);
        g_setup_thread = NULL;
    }

    if (g_lock) {
        SDL_DestroyMutex(g_lock);
        g_lock = NULL;
    }

    g_busy = false;
}
