/*
    Sayri Android entry point
    --------------------------
    Bridges SDLActivity's native calls to our main loop.
    On Android, main() is never called directly — SDL
    invokes SDL_main() from the Java side.
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#ifdef ANDROID
#include <jni.h>
#include <SDL2/SDL_system.h>
#endif

#include "ui.h"
#include "hamburger.h"
#include "sidebar.h"
#include "orb.h"
#include "toggle.h"
#include "popup.h"
#include "downloads.h"
#include "ollama.h"
#include "history.h"
#include "netpair.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_MESSAGES 32
#define MSG_MAX 2048
#define INPUT_MAX 256

#define OLLAMA_MODEL "llama3.2"
#define OLLAMA_HISTORY 10
#define MAX_MODELS 16

static char g_current_model[128] = OLLAMA_MODEL;
static char g_models[MAX_MODELS][128];
static int g_model_count = 0;
static int g_model_idx = 0;

static HistoryList g_history;
static char g_item_labels[HISTORY_MAX_FILES][32];

static UIPopup searchPopup;
static UISearchBar searchInput;
static char g_search_labels[POPUP_MAX_ITEMS][72];
static char g_search_files[POPUP_MAX_ITEMS][64];
static char g_last_query[SEARCHBAR_MAX];

static bool g_server_ok = true;
static bool g_retry_pending = false;
static int g_autoheal_attempts = 0;

/* Tracks whether the Android soft keyboard is currently requested. */
static bool g_kb_shown = false;

/* Touch drag state (used to scroll the chat on a phone). */
static bool g_touch_drag = false;
static float g_touch_start_y = 0.0f;
static float g_touch_last_y = 0.0f;

/* IPC is disabled on Android */
/* static int g_ipc_fd = -1; */

/* Device-pairing sheet (connect to a desktop Sayri relay). */
static bool g_pair_sheet = false;
static bool g_pair_connecting = false;
static char g_pair_host[NETPAIR_FIELD_LEN] = "";
static char g_pair_port[16] = "5055";
static char g_pair_code[NETPAIR_CODE_LEN + 1] = "";
static int g_pair_focus = 0;   /* 0 host, 1 port, 2 code */
static bool g_pair_port_touched = false;
static bool g_pair_streamed_ready = false;
static char g_pair_kbdigit = 0; /* digit inserted via SDL_KEYDOWN */
static char g_pair_reply[NETPAIR_MAX_TEXT] = "";

#ifdef ANDROID
/*
    Parse a scanned pairing URI:
        sayri://<host>:<port>?code=<code>
    Returns true and fills host/port/code on success.
*/
static bool parse_pair_uri(const char *uri, char *host, size_t host_cap,
                           char *port, size_t port_cap,
                           char *code, size_t code_cap)
{
    if (!uri || strncmp(uri, "sayri://", 8) != 0) return false;
    const char *p = uri + 8;
    const char *colon = strchr(p, ':');
    const char *q = strchr(p, '?');
    if (!colon || !q || colon > q) return false;

    size_t host_len = (size_t)(colon - p);
    if (host_len == 0 || host_len >= host_cap) return false;
    memcpy(host, p, host_len);
    host[host_len] = '\0';

    size_t port_len = (size_t)(q - colon) - 1;
    if (port_len == 0 || port_len >= port_cap) return false;
    memcpy(port, colon + 1, port_len);
    port[port_len] = '\0';

    const char *k = strstr(q + 1, "code=");
    if (!k) return false;
    const char *c = k + 5;
    size_t code_len = strlen(c);
    if (code_len == 0 || code_len >= code_cap) return false;
    memcpy(code, c, code_len);
    code[code_len] = '\0';
    return true;
}

/* Launch the Java QR scanner activity. */
static void android_launch_scanner(void)
{
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    if (!env) return;
    jobject activity = (jobject)SDL_AndroidGetActivity();
    jclass bridge = (*env)->FindClass(env,
                                     "com/sayri/assistant/SayriBridge");
    if (!bridge) {
        if (activity) (*env)->DeleteLocalRef(env, activity);
        return;
    }
    jmethodID launch = (*env)->GetStaticMethodID(env, bridge,
        "launchScanner", "(Landroid/content/Context;)V");
    if (launch && activity)
        (*env)->CallStaticVoidMethod(env, bridge, launch, activity);
    (*env)->DeleteLocalRef(env, bridge);
    if (activity) (*env)->DeleteLocalRef(env, activity);
}

/*
    Poll the bridge for a scanned URI. When one lands, fill the
    pairing sheet and auto-connect. Call every frame while the
    pairing sheet is open.
*/
static void android_apply_scanned_qr(void)
{
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    if (!env || !g_pair_sheet) return;
    jclass bridge = (*env)->FindClass(env,
                                     "com/sayri/assistant/SayriBridge");
    if (!bridge) return;
    jmethodID take = (*env)->GetStaticMethodID(env, bridge,
        "takeScannedUri", "()Ljava/lang/String;");
    if (!take) {
        (*env)->DeleteLocalRef(env, bridge);
        return;
    }
    jstring js = (jstring)(*env)->CallStaticObjectMethod(env, bridge, take);
    (*env)->DeleteLocalRef(env, bridge);
    if (!js) return;

    const char *uri = (*env)->GetStringUTFChars(env, js, NULL);
    if (uri) {
        char host[NETPAIR_FIELD_LEN];
        char port[NETPAIR_FIELD_LEN];
        char code[NETPAIR_CODE_LEN + 1];
        if (parse_pair_uri(uri, host, sizeof(host),
                           port, sizeof(port), code, sizeof(code))) {
            snprintf(g_pair_host, sizeof(g_pair_host), "%s", host);
            snprintf(g_pair_port, sizeof(g_pair_port), "%s", port);
            snprintf(g_pair_code, sizeof(g_pair_code), "%s", code);
            if (!g_pair_connecting) {
                netpair_set_endpoint(g_pair_host, g_pair_port);
                netpair_set_code(g_pair_code);
                g_pair_connecting = true;
                netpair_connect();
                g_pair_streamed_ready = true;
            }
        }
        (*env)->ReleaseStringUTFChars(env, js, uri);
    }
    (*env)->DeleteLocalRef(env, js);
}
#endif

static UIColor lerp_color(UIColor a, UIColor b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (UIColor){
        (Uint8)(a.r + (b.r - a.r) * t),
        (Uint8)(a.g + (b.g - a.g) * t),
        (Uint8)(a.b + (b.b - a.b) * t),
        (Uint8)(a.a + (b.a - a.a) * t)
    };
}

typedef struct {
    char text[MSG_MAX];
    bool is_user;
    float alpha;
    float slide_y;
} ChatMessage;

typedef struct {
    ChatMessage messages[MAX_MESSAGES];
    int count;

    char input[INPUT_MAX];
    int input_len;
    bool input_focused;

    float scroll_offset;
    float target_scroll;

    bool is_thinking;

    char cur_session[64];
} AppState;

/*
    On Android, fonts are in assets/font/ and accessed
    through SDL_RWFromFile which transparently handles
    the Android asset system.
*/
static char g_font_path[600];

typedef struct {
    TTF_Font *font;
    TTF_Font *titleFont;
    TTF_Font *smallFont;
    TTF_Font *boldFont;
    TTF_Font *menuFont;
} Fonts;

static void close_fonts(Fonts *fs)
{
    if (fs->font)        TTF_CloseFont(fs->font);
    if (fs->titleFont)   TTF_CloseFont(fs->titleFont);
    if (fs->smallFont)   TTF_CloseFont(fs->smallFont);
    if (fs->boldFont)    TTF_CloseFont(fs->boldFont);
    if (fs->menuFont)    TTF_CloseFont(fs->menuFont);
    memset(fs, 0, sizeof(*fs));
}

static bool load_fonts(Fonts *fs, float scale)
{
    close_fonts(fs);

    const int pts[5] = { 13, 15, 10, 15, 16 };
    TTF_Font **slots[5] = {
        &fs->font, &fs->titleFont, &fs->smallFont,
        &fs->boldFont, &fs->menuFont
    };

    for (int i = 0; i < 5; i++) {
        int px = (int)roundf((float)pts[i] * scale);
        if (px < 7) px = 7;

        /* On Android, use SDL_RWFromFile for assets */
        SDL_RWops *rw = SDL_RWFromFile(g_font_path, "rb");
        if (!rw) {
            close_fonts(fs);
            return false;
        }

        TTF_Font *f = TTF_OpenFontRW(rw, 1, px);
        if (!f) {
            close_fonts(fs);
            return false;
        }

        TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
        *slots[i] = f;
    }

    return true;
}

static void add_message(AppState *state, const char *text, bool is_user)
{
    if (state->count >= MAX_MESSAGES) return;
    ChatMessage *m = &state->messages[state->count];
    snprintf(m->text, sizeof(m->text), "%s", text);
    m->is_user = is_user;
    m->alpha = 0.0f;
    m->slide_y = 16.0f;
    state->count++;
}

static void persist_session(AppState *st)
{
    if (st->count == 0) return;
    if (st->count == 1 && !st->messages[0].is_user) return;

    const char *roles[MAX_MESSAGES];
    const char *texts[MAX_MESSAGES];
    for (int i = 0; i < st->count; i++) {
        roles[i] = st->messages[i].is_user ? "user" : "assistant";
        texts[i] = st->messages[i].text;
    }

    history_save(st->cur_session, st->cur_session,
                 sizeof(st->cur_session), roles, texts, st->count);
}

static bool ci_contains(const char *hay, const char *needle)
{
    if (!*needle) return true;
    size_t nlen = strlen(needle);
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < nlen && hay[i] &&
               tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == nlen) return true;
        if (!hay[i]) break;
    }
    return false;
}

static void load_session_file(AppState *st, const char *name)
{
    char (*texts)[HISTORY_TEXT_MAX] =
        malloc(sizeof(char[MAX_MESSAGES][HISTORY_TEXT_MAX]));
    if (!texts) return;

    unsigned char flags[MAX_MESSAGES];
    int n = history_load(name, texts, flags, MAX_MESSAGES);
    st->count = 0;
    st->cur_session[0] = '\0';
    for (int i = 0; i < n; i++)
        add_message(st, texts[i], flags[i] != 0);
    snprintf(st->cur_session, sizeof(st->cur_session), "%s", name);
    free(texts);
}

static void rebuild_search_results(void)
{
    g_history.count = 0;
    history_list(&g_history);
    const char *q = searchInput.text;

    char (*texts)[HISTORY_TEXT_MAX] =
        malloc(sizeof(char[MAX_MESSAGES][HISTORY_TEXT_MAX]));

    int n = 0;
    for (int k = 0; k < g_history.count && n < POPUP_MAX_ITEMS; k++) {
        bool hit;
        if (*q == '\0') hit = true;
        else if (ci_contains(g_history.names[k], q)) hit = true;
        else {
            hit = false;
            if (texts) {
                unsigned char flags[MAX_MESSAGES];
                int m = history_load(g_history.names[k], texts, flags, MAX_MESSAGES);
                for (int i = 0; i < m && !hit; i++)
                    hit = ci_contains(texts[i], q);
            }
        }
        if (!hit) continue;
        snprintf(g_search_files[n], sizeof(g_search_files[0]), "%s", g_history.names[k]);
        long t = atol(g_history.names[k]);
        struct tm *tm = localtime(&t);
        strftime(g_search_labels[n], sizeof(g_search_labels[0]), "%b %d %H:%M", tm);
        n++;
    }
    free(texts);

    if (n == 0) {
        g_search_files[0][0] = '\0';
        snprintf(g_search_labels[0], sizeof(g_search_labels[0]),
                 *q ? "(no matches)" : "(no saved chats)");
        n = 1;
    }

    const char *labels[POPUP_MAX_ITEMS];
    for (int k = 0; k < n; k++)
        labels[k] = g_search_labels[k];
    popup_set_items(&searchPopup, labels, n);
}

static bool dispatch_chat(AppState *state)
{
    const char *roles[OLLAMA_HISTORY];
    const char *contents[OLLAMA_HISTORY];
    int start = state->count - OLLAMA_HISTORY;
    if (start < 0) start = 0;
    int n = 0;
    for (int i = start; i < state->count; i++) {
        if (!state->messages[i].is_user && state->messages[i].text[0] == '(')
            continue;
        roles[n] = state->messages[i].is_user ? "user" : "assistant";
        contents[n] = state->messages[i].text;
        n++;
    }
    if (n == 0) return false;

    char *body = ollama_build_body(g_current_model, roles, contents, n);
    if (!body) {
        add_message(state, "(Out of memory.)", false);
        return false;
    }
    if (ollama_begin(body)) {
        state->is_thinking = true;
        return true;
    }
    free(body);
    add_message(state, "(Still answering the previous message\u2026)", false);
    return false;
}

static void send_user_message(AppState *state, const char *text)
{
    add_message(state, text, true);
    g_autoheal_attempts = 0;

    /*
        When paired with a desktop Sayri, stream the chat
        through the desktop's local Ollama instead of
        calling a remote/server directly.
    */
    NetpairStatus nst = netpair_status();
    if (nst == NETPAIR_PAIRED) {
        if (netpair_send_msg(text)) {
            state->is_thinking = true;
            return;
        }
    }

    dispatch_chat(state);
}

static int sidebar_item_at(UISidebar *sb, UIContext *ui, int mx, int my)
{
    if (!sb->open || sb->anim < 0.85f) return -1;
    for (int i = 0; i < SIDEBAR_ITEMS; i++) {
        SDL_Rect row = ui_rect(ui, SIDEBAR_ITEM_X,
                               SIDEBAR_ITEM_Y + i * SIDEBAR_ITEM_GAP,
                               SIDEBAR_ITEM_W, SIDEBAR_ITEM_H);
        if (ui_point_in_rect(mx, my, row)) return i;
    }
    return -1;
}

/* Rect of the sidebar "Pair devices" button (below the menu items). */
static SDL_Rect pair_row_rect_android(UIContext *ui)
{
    return ui_rect(ui, SIDEBAR_ITEM_X,
                   SIDEBAR_ITEM_Y + SIDEBAR_ITEMS * SIDEBAR_ITEM_GAP + 14.0f,
                   SIDEBAR_ITEM_W, SIDEBAR_ITEM_H + 8.0f);
}

/* Rect of the "Scan QR code instead" button inside the pairing
   modal (below the Connect button). */
static SDL_Rect pair_scan_rect_android(UIContext *ui)
{
    float designW = (float)ui->window_w / ui->scale;
    float cw = 340.0f;
    float cx = (designW - cw) / 2.0f;
    float ix = cx + 20.0f;
    float iw = cw - 40.0f;
    float connectTop = 100.0f + 60.0f + 3 * 52.0f;
    return ui_rect(ui, ix, connectTop + 46.0f + 12.0f, iw, 46.0f);
}

/* Draw the sidebar "Pair devices" row. */
static void draw_pair_row(SDL_Renderer *renderer, UIContext *ui,
                          TTF_Font *font, float sidebar_anim)
{
    if (sidebar_anim < 0.8f) return;
    float alpha = (sidebar_anim - 0.8f) / 0.2f;
    if (alpha > 1.0f) alpha = 1.0f;
    SDL_Rect row = pair_row_rect_android(ui);
    int radius = (int)roundf(SIDEBAR_ITEM_H * 0.5f * ui->scale);
    ui_glass(renderer, row, radius, g_pair_sheet, ui->dark);

    NetpairStatus st = netpair_status();
    const char *hint =
        st == NETPAIR_PAIRED ? "Paired"
        : st == NETPAIR_CONNECTING ? "Connecting"
        : st == NETPAIR_NEED_CODE ? "Enter code"
        : "Tap to pair";
    char label[96];
    snprintf(label, sizeof(label), "Pair devices \u2014 %s", hint);
    UIColor lc = ui_theme(ui->dark,
        (UIColor){25, 55, 130, 255},
        (UIColor){140, 190, 255, 255});
    lc.a = (Uint8)(lc.a * alpha);
    int fx = (int)roundf(13.0f * ui->scale);
    ui_text(renderer, font, label,
            row.x + fx,
            row.y + (row.h - TTF_FontHeight(font)) / 2, lc);
}

/* Draw the pairing bottom sheet. */
static void draw_pair_sheet(SDL_Renderer *renderer, UIContext *ui,
                            TTF_Font *font, int width, int height)
{
    if (!g_pair_sheet) return;

    /* Centered modal card (not bottom-anchored, so the
       soft keyboard can't push it off-screen). */
    float designW = (float)width / ui->scale;
    const char *labels[3] = {"Host (e.g. 192.168.1.10)", "Port", "Pairing code"};
    char *vals[3] = { g_pair_host, g_pair_port, g_pair_code };

    float cw = 340.0f;
    float ch = 360.0f;
    float cx = (designW - cw) / 2.0f;
    float cy = 100.0f;
    float ix = cx + 20.0f;
    float iw = cw - 40.0f;

    SDL_Rect card = ui_rect(ui, cx, cy, cw, ch);
    ui_fill_rounded_rect(renderer, card, (int)roundf(18.0f * ui->scale),
        ui->dark ? (UIColor){28, 28, 32, 250} : (UIColor){244, 248, 254, 250});

    /* Dim the rest of the app behind the card. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_RenderFillRect(renderer, &(SDL_Rect){0, 0, width, height});

    ui_text(renderer, font, "Connect to your computer",
            card.x + (int)roundf(20.0f * ui->scale),
            card.y + (int)roundf(20.0f * ui->scale),
            ui_theme(ui->dark, (UIColor){30, 35, 60, 255}, (UIColor){220, 228, 245, 255}));

    float fy = cy + 60.0f;
    for (int i = 0; i < 3; i++) {
        SDL_Rect field = ui_rect(ui, ix, fy, iw, 42.0f);
        ui_fill_rounded_rect(renderer, field, (int)roundf(10.0f * ui->scale),
            ui_theme(ui->dark,
                (UIColor){230, 234, 244, 255}, (UIColor){60, 60, 68, 255}));
        if (g_pair_focus == i) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 90, 120, 230, 220);
            SDL_RenderDrawRect(renderer, &field);
        }
        UIColor fc = ui_theme(ui->dark,
            (UIColor){30,35,60,255}, (UIColor){220,228,245,255});
        if (vals[i][0])
            ui_text(renderer, font, vals[i],
                    field.x + (int)roundf(12.0f * ui->scale),
                    field.y + (field.h - TTF_FontHeight(font)) / 2, fc);
        else
            ui_text(renderer, font, labels[i],
                    field.x + (int)roundf(12.0f * ui->scale),
                    field.y + (field.h - TTF_FontHeight(font)) / 2,
                    ui_theme(ui->dark,
                        (UIColor){150,150,165,200}, (UIColor){110,118,140,200}));
        fy += 42.0f + 10.0f;
    }

    NetpairStatus st2 = netpair_status();
    bool paired = (st2 == NETPAIR_PAIRED || st2 == NETPAIR_STREAMING);
    SDL_Rect btn = ui_rect(ui, ix, fy, iw, 46.0f);
    ui_fill_rounded_rect(renderer, btn, (int)roundf(12.0f * ui->scale),
        paired ? (UIColor){90,80,120,255} : (UIColor){70,130,255,255});
    ui_text_center(renderer, font, paired ? "Disconnect" : "Connect", btn,
        (UIColor){255,255,255,255});

    /* Scan QR button — launches the Java camera scanner. */
    SDL_Rect scan = pair_scan_rect_android(ui);
    ui_fill_rounded_rect(renderer, scan, (int)roundf(12.0f * ui->scale),
        (UIColor){30, 90, 210, 255});
    ui_text_center(renderer, font, "Scan QR code instead", scan,
        (UIColor){255,255,255,255});

    ui_text(renderer, font, netpair_status_text(),
            card.x + (int)roundf(20.0f * ui->scale),
            scan.y + scan.h + (int)roundf(12.0f * ui->scale),
            ui_theme(ui->dark, (UIColor){90,100,120,220}, (UIColor){150,160,180,220}));

    if (g_pair_reply[0]) {
        char tmp[160];
        snprintf(tmp, sizeof(tmp), "Sayri: %.70s", g_pair_reply);
        ui_text(renderer, font, tmp,
                card.x + (int)roundf(20.0f * ui->scale),
                card.y + card.h - (int)roundf(30.0f * ui->scale),
                ui_theme(ui->dark, (UIColor){90,100,120,220}, (UIColor){150,160,180,220}));
    }
}

/* Hit-testing for the pairing modal, matching the centered layout in
   draw_pair_sheet. Returns 0..2 for fields, 3 for the connect button,
   4 for the Scan QR button, -1 otherwise. */
static int pair_field_at(UIContext *ui, int width, int height, int mx, int my)
{
    (void)height;
    float designW = (float)width / ui->scale;
    float cw = 340.0f;
    float cx = (designW - cw) / 2.0f;
    float cy = 100.0f;
    float ix = cx + 20.0f;
    float iw = cw - 40.0f;

    for (int i = 0; i < 3; i++) {
        SDL_Rect field = ui_rect(ui, ix, cy + 60.0f + i * 52.0f, iw, 42.0f);
        if (ui_point_in_rect(mx, my, field)) return i;
    }
    SDL_Rect btn = ui_rect(ui, ix, cy + 60.0f + 3 * 52.0f, iw, 46.0f);
    if (ui_point_in_rect(mx, my, btn)) return 3;

    SDL_Rect scan = pair_scan_rect_android(ui);
    if (ui_point_in_rect(mx, my, scan)) return 4;
    return -1;
}

#define CHAT_WRAP_LINES 24
#define CHAT_LINE_CAP 512

static int wrap_lines(TTF_Font *font, const char *text, int max_w,
                      char out[][CHAT_LINE_CAP], int max_lines)
{
    int count = 0;
    out[0][0] = '\0';
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t seg_len = nl ? (size_t)(nl - p) : strlen(p);
        char seg[CHAT_LINE_CAP];
        snprintf(seg, sizeof(seg), "%.*s", (int)seg_len, p);
        char *save = NULL;
        for (char *word = strtok_r(seg, " ", &save); word;
             word = strtok_r(NULL, " ", &save)) {
            char candidate[CHAT_LINE_CAP];
            int cur_len = (int)strlen(out[count]);
            snprintf(candidate, sizeof(candidate), "%s%s%s",
                     out[count], cur_len ? " " : "", word);
            int w = 0;
            TTF_SizeUTF8(font, candidate, &w, NULL);
            if (cur_len == 0 || w <= max_w) {
                snprintf(out[count], sizeof(out[count]), "%s", candidate);
            } else {
                if (count + 1 >= max_lines) {
                    size_t len = strlen(out[count]);
                    snprintf(out[count] + len, sizeof(out[count]) - len, "\u2026");
                    return count + 1;
                }
                count++;
                snprintf(out[count], sizeof(out[count]), "%s", word);
            }
        }
        if (!nl) break;
        p = nl + 1;
        if (*p && count + 1 < max_lines) {
            count++;
            out[count][0] = '\0';
        } else if (!*p) break;
    }
    return count + 1;
}

static void draw_chat_area(SDL_Renderer *renderer, AppState *state,
                           UIContext *ui, TTF_Font *font,
                           int x, int y, int w, int h)
{
    SDL_Rect container = {x, y, w, h};
    ui_fill_rounded_rect(renderer, container,
        (int)roundf(18.0f * ui->scale),
        ui->dark ? (UIColor){26, 26, 29, 190}
                 : (UIColor){240, 245, 252, 175});
    SDL_RenderSetClipRect(renderer, &container);

    int pad = (int)roundf(14.0f * ui->scale);
    int msgH = (int)roundf(38.0f * ui->scale);
    int msgGap = (int)roundf(6.0f * ui->scale);
    int curY = y + h - pad + (int)state->scroll_offset;

    for (int i = state->count - 1; i >= 0; i--) {
        ChatMessage *m = &state->messages[i];
        if (m->alpha < 0.01f) continue;

        int maxBubbleW = w - pad * 2 - (int)roundf(40.0f * ui->scale);
        int bubbleW = m->is_user ? (int)(maxBubbleW * 0.60f) : (int)(maxBubbleW * 0.75f);
        if (bubbleW < 60) bubbleW = 60;
        int textPad = (int)roundf(12.0f * ui->scale);
        int bubbleX = m->is_user ? x + w - pad - bubbleW : x + pad;

        char lines[CHAT_WRAP_LINES][CHAT_LINE_CAP];
        int lineCount = wrap_lines(font, m->text, bubbleW - textPad * 2,
                                   lines, CHAT_WRAP_LINES);
        int fontH = TTF_FontHeight(font);
        int lineH = fontH + (int)roundf(3.0f * ui->scale);
        int padV = (int)roundf(9.0f * ui->scale);
        int bubbleH = lineCount * lineH + padV * 2;
        if (bubbleH < msgH) bubbleH = msgH;

        curY -= bubbleH + msgGap;
        int bubbleY = curY + (int)(m->slide_y * (1.0f - m->alpha));
        Uint8 bgA = (Uint8)(180.0f * m->alpha);

        SDL_Rect bubble = {bubbleX, bubbleY, bubbleW, bubbleH};
        if (m->is_user) {
            ui_fill_rounded_rect(renderer, bubble, (int)roundf(14.0f * ui->scale),
                ui->dark ? (UIColor){62, 62, 70, bgA}
                         : (UIColor){200, 210, 240, bgA});
        } else {
            ui_fill_rounded_rect(renderer, bubble, (int)roundf(14.0f * ui->scale),
                ui->dark ? (UIColor){44, 44, 49, bgA}
                         : (UIColor){230, 235, 248, bgA});
        }

        Uint8 tA = (Uint8)(230.0f * m->alpha);
        int textY = bubbleY + (bubbleH - lineCount * lineH) / 2;
        UIColor textColor = m->is_user
            ? ui_theme(ui->dark, (UIColor){35, 45, 80, tA}, (UIColor){225, 232, 250, tA})
            : ui_theme(ui->dark, (UIColor){50, 55, 85, tA}, (UIColor){208, 216, 236, tA});

        for (int k = 0; k < lineCount; k++)
            ui_text(renderer, font, lines[k], bubbleX + textPad,
                    textY + k * lineH, textColor);
    }
    SDL_RenderSetClipRect(renderer, NULL);
}

static void draw_input_bar(SDL_Renderer *renderer, AppState *state,
                           UIContext *ui, TTF_Font *font,
                           int x, int y, int w, int h, float time)
{
    SDL_Rect bar = {x, y, w, h};
    ui_fill_rounded_rect(renderer, bar, (int)roundf(14.0f * ui->scale),
        state->input_focused
        ? ui_theme(ui->dark, (UIColor){246, 250, 255, 205}, (UIColor){40, 40, 45, 215})
        : ui_theme(ui->dark, (UIColor){240, 245, 252, 175}, (UIColor){24, 24, 27, 185}));

    int pad = (int)roundf(14.0f * ui->scale);
    if (state->input_len > 0) {
        ui_text(renderer, font, state->input, x + pad,
                y + (h - (int)roundf(17.0f * ui->scale)) / 2,
                ui_theme(ui->dark, (UIColor){30, 35, 60, 255},
                                   (UIColor){220, 228, 245, 255}));
    } else {
        ui_text(renderer, font, "Ask Sayri\u2026", x + pad,
                y + (h - (int)roundf(17.0f * ui->scale)) / 2,
                ui_theme(ui->dark, (UIColor){140, 145, 170, 180},
                                   (UIColor){125, 133, 158, 180}));
    }

    if (state->input_focused) {
        float blink = fmodf((float)SDL_GetTicks() / 1000.0f, 1.0f);
        if (blink < 0.5f) {
            int textW = 0;
            if (state->input_len > 0)
                TTF_SizeUTF8(font, state->input, &textW, NULL);
            int cx = x + pad + textW + 2;
            int ch = (int)roundf(16.0f * ui->scale);
            float ct = fmodf(time * 2.0f, 4.0f);
            UIColor cc;
            UIColor c1 = {110, 140, 235, 255};
            UIColor c2 = {230, 130, 195, 255};
            UIColor c3 = {170, 140, 220, 255};
            UIColor c4 = {100, 200, 185, 255};
            if (ct < 1.0f) cc = lerp_color(c1, c2, ct);
            else if (ct < 2.0f) cc = lerp_color(c2, c3, ct - 1.0f);
            else if (ct < 3.0f) cc = lerp_color(c3, c4, ct - 2.0f);
            else cc = lerp_color(c4, c1, ct - 3.0f);
            cc.a = 230;
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, cc.a);
            SDL_RenderDrawLine(renderer, cx, y + (h - ch) / 2,
                                    cx, y + (h + ch) / 2);
        }
    }
}

static void draw_send_button(SDL_Renderer *renderer, AppState *state,
                             UIContext *ui, TTF_Font *font,
                             int x, int y, int size)
{
    bool has = state->input_len > 0;
    int r = size / 2;
    if (has) {
        ui_fill_circle(renderer, x + size / 2, y + size / 2, r,
                       (UIColor){70, 130, 255, 255});
        ui_fill_circle(renderer, x + size / 2, y + size / 2, r + 4,
                       (UIColor){70, 130, 255, 35});
    } else {
        ui_fill_circle(renderer, x + size / 2, y + size / 2, r,
            ui_theme(ui->dark, (UIColor){200, 205, 220, 150},
                               (UIColor){70, 78, 98, 170}));
    }
    const char *arrow = "\u2191";
    int tw = 0, th = 0;
    TTF_SizeUTF8(font, arrow, &tw, &th);
    ui_text(renderer, font, arrow, x + (size - tw) / 2,
            y + (size - th) / 2,
            (UIColor){255, 255, 255, has ? 255 : 100});
}

static void draw_background(SDL_Renderer *renderer, UIContext *ui,
                            int width, int height, float time)
{
    bool dark = ui->dark;
    for (int y = 0; y < height; ++y) {
        float t = (float)y / (float)(height - 1);
        Uint8 r = dark ? (Uint8)(16 + t * 10.0f) : (Uint8)(205 + t * 25.0f);
        Uint8 g = dark ? (Uint8)(18 + t * 10.0f) : (Uint8)(215 + t * 18.0f);
        Uint8 b = dark ? (Uint8)(28 + t * 12.0f) : (Uint8)(242 - t * 5.0f);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, width - 1, y);
    }

    float orbA = dark ? 0.45f : 1.0f;
    float t1 = time * 0.15f;
    float t2 = time * 0.12f + 2.0f;
    float t3 = time * 0.18f + 4.0f;

    ui_fill_radial_gradient(renderer,
        (int)(width * 0.55f + sinf(t1) * 60.0f),
        (int)(height * 0.25f + cosf(t1 * 0.7f) * 40.0f),
        (int)(width * 0.28f),
        (UIColor){110, 140, 235, (Uint8)(90 * orbA)},
        (UIColor){110, 140, 235, 0});

    ui_fill_radial_gradient(renderer,
        (int)(width * 0.72f + cosf(t2) * 50.0f),
        (int)(height * 0.70f + sinf(t2 * 0.8f) * 35.0f),
        (int)(width * 0.22f),
        (UIColor){230, 130, 195, (Uint8)(80 * orbA)},
        (UIColor){230, 130, 195, 0});

    ui_fill_radial_gradient(renderer,
        (int)(width * 0.38f + sinf(t3 * 0.6f) * 45.0f),
        (int)(height * 0.60f + cosf(t3) * 30.0f),
        (int)(width * 0.18f),
        (UIColor){100, 200, 185, (Uint8)(70 * orbA)},
        (UIColor){100, 200, 185, 0});

    ui_fill_radial_gradient(renderer,
        (int)(width * 0.85f + sinf(t1 * 0.9f) * 35.0f),
        (int)(height * 0.15f + cosf(t2 * 0.5f) * 25.0f),
        (int)(width * 0.15f),
        (UIColor){170, 140, 220, (Uint8)(55 * orbA)},
        (UIColor){170, 140, 220, 0});
}

/* ============================================================
   SDL_main — Android entry point
   ============================================================ */

#ifdef ANDROID
int SDL_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    }

#ifdef ANDROID
    /*
        We translate finger events into mouse events
        ourselves in the main loop, so tell SDL not to
        also synthesize them (otherwise every tap fires
        twice).
    */
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif

    SDL_StartTextInput();

    if (TTF_Init() != 0) {
        fprintf(stderr, "%s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Sayri",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        520, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!window || !renderer)
        return 1;

    /* Font path: on Android assets use "font/default.ttf" */
    snprintf(g_font_path, sizeof(g_font_path), "font/default.ttf");

    /* Check via RWops that the font is accessible */
    {
        SDL_RWops *rw = SDL_RWFromFile(g_font_path, "rb");
        if (!rw) {
            SDL_Log("Cannot find font at %s", g_font_path);
            /* Try alternate path */
            snprintf(g_font_path, sizeof(g_font_path), "default.ttf");
            rw = SDL_RWFromFile(g_font_path, "rb");
            if (!rw) {
                SDL_Log("Font not found in assets");
                return 1;
            }
            SDL_RWclose(rw);
        } else {
            SDL_RWclose(rw);
        }
    }

    int win_w0, win_h0;
    SDL_GetWindowSize(window, &win_w0, &win_h0);
    UIContext ui0;
    ui0.dark = false;
    ui_begin(&ui0, win_w0, win_h0);
    float loaded_scale = ui0.scale;

    Fonts fonts;
    if (!load_fonts(&fonts, loaded_scale)) {
        printf("Could not load font\n");
        return 1;
    }

    TTF_Font *font = fonts.font;
    TTF_Font *titleFont = fonts.titleFont;
    TTF_Font *smallFont = fonts.smallFont;
    TTF_Font *boldFont = fonts.boldFont;
    TTF_Font *menuFont = fonts.menuFont;
    (void)titleFont; (void)smallFont;

    AppState state;
    memset(&state, 0, sizeof(state));
    state.input_focused = true;
    add_message(&state, "Hi, I'm Sayri. How can I help?", false);

    UIContext ui;
    ui.dark = false;

    UIHamburger hamburger;
    hamburger_init(&hamburger);

    UISidebar sidebar;
    sidebar_init(&sidebar);
    sidebar.items[0] = "New Chat";
    sidebar.items[1] = "Recents";
    sidebar.items[2] = "Search";
    sidebar.items[3] = "Downloads";
    sidebar.items[4] = "Settings";

    Orb orb;
    orb_init(&orb, renderer);

    ollama_init(OLLAMA_MODEL);

    UIToggle darkToggle;
    toggle_init(&darkToggle, false);

    UIPopup settings;
    popup_init(&settings, "Settings");
    popup_set_row_label(&settings, "Dark Mode");
    popup_link_toggle(&settings, &darkToggle);

    UIDropDown modelDropdown;
    dropdown_init(&modelDropdown);
    popup_link_dropdown(&settings, &modelDropdown);

    UIPopup recentsPopup;
    popup_init(&recentsPopup, "Recents");

    popup_init(&searchPopup, "Search");
    searchbar_init(&searchInput);
    popup_link_search(&searchPopup, &searchInput);

    UIDownloads dlPanel;
    downloads_init(&dlPanel, "llama3.2");

    ollama_init(g_current_model);

    /* On Android, skip auto-setup — only support remote Ollama server */
    g_server_ok = true;

    SDL_Texture *rt = NULL;
    int rt_w = 0, rt_h = 0;

    bool running = true;
    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 start = SDL_GetPerformanceCounter();
    float prev_time = (float)((double)start / (double)perf_freq);

    float orb_grow = 0.0f;
    float orb_dt = 0.0f;
    int frame_count = 0;

    while (running) {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        ui_begin(&ui, width, height);

        if (fabsf(ui.scale - loaded_scale) > 0.01f) {
            if (!load_fonts(&fonts, ui.scale)) break;
            loaded_scale = ui.scale;
            font = fonts.font;
            titleFont = fonts.titleFont;
            smallFont = fonts.smallFont;
            boldFont = fonts.boldFont;
            menuFont = fonts.menuFont;
            ui_text_cache_clear();
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;

#ifdef ANDROID
            /*
                Touch -> mouse bridge.

                Android hands us raw finger events. We convert each
                into the equivalent mouse event (in window pixel
                coordinates) and feed it through the exact same code
                paths the desktop build uses, so every button and
                widget reacts to taps. SDL's own synthesis is disabled
                (see init) so nothing fires twice.
            */
            if (event.type == SDL_FINGERDOWN ||
                event.type == SDL_FINGERMOTION ||
                event.type == SDL_FINGERUP) {

                int mx = (int)(event.tfinger.x * (float)width);
                int my = (int)(event.tfinger.y * (float)height);

                /*
                    Vertical drag on the chat area scrolls it (there is
                    no mouse wheel on a phone). Track the delta between
                    successive moves and feed it into the same scroll
                    target the wheel uses.
                */
                if (event.type == SDL_FINGERDOWN) {
                    g_touch_drag = false;
                    g_touch_start_y = (float)my;
                    g_touch_last_y = (float)my;
                } else if (event.type == SDL_FINGERMOTION) {
                    float dy = (float)my - g_touch_last_y;
                    g_touch_last_y = (float)my;
                    if (fabsf((float)my - g_touch_start_y) > 12.0f)
                        g_touch_drag = true;
                    if (g_touch_drag) {
                        state.target_scroll -= dy * 1.0f;
                        if (state.target_scroll > 0)
                            state.target_scroll = 0;
                    }
                } else {
                    g_touch_drag = false;
                }

                SDL_Event m;
                SDL_zero(m);
                m.motion.windowID = SDL_GetWindowID(window);
                m.motion.which = 0;
                m.motion.x = mx;
                m.motion.y = my;

                if (event.type == SDL_FINGERDOWN) {
                    m.motion.type = SDL_MOUSEMOTION;
                    SDL_PushEvent(&m);
                    m.type = SDL_MOUSEBUTTONDOWN;
                    m.button.button = SDL_BUTTON_LEFT;
                    m.button.state = SDL_PRESSED;
                    m.button.x = mx;
                    m.button.y = my;
                    SDL_PushEvent(&m);
                } else if (event.type == SDL_FINGERMOTION) {
                    m.motion.type = SDL_MOUSEMOTION;
                    SDL_PushEvent(&m);
                } else {
                    m.type = SDL_MOUSEBUTTONUP;
                    m.button.button = SDL_BUTTON_LEFT;
                    m.button.state = SDL_RELEASED;
                    m.button.x = mx;
                    m.button.y = my;
                    SDL_PushEvent(&m);
                }
                continue;
            }
#endif

            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = false;

                /* On Android, handle back button */
                if (event.key.keysym.sym == SDLK_AC_BACK) {
                    if (g_pair_sheet) {
                        g_pair_sheet = false;
                    } else if (sidebar.open) {
                        sidebar.open = false;
                    } else if (settings.open) {
                        settings.open = false;
                    } else if (recentsPopup.open) {
                        recentsPopup.open = false;
                    } else if (searchPopup.open) {
                        searchPopup.open = false;
                    } else if (dlPanel.open) {
                        dlPanel.open = false;
                    } else {
                        running = false;
                    }
                    continue;
                }

                bool sb_focus = searchPopup.open && searchInput.focused;
                bool sf_focus = g_pair_sheet && g_pair_focus >= 0 && g_pair_focus <= 2;
                char *sf_val = sf_focus
                    ? (g_pair_focus == 0 ? g_pair_host
                       : g_pair_focus == 1 ? g_pair_port : g_pair_code)
                    : NULL;
                size_t sf_cap = sf_focus
                    ? (g_pair_focus == 0 ? sizeof(g_pair_host)
                       : g_pair_focus == 1 ? sizeof(g_pair_port)
                       : sizeof(g_pair_code))
                    : 0;

                if (event.key.keysym.sym == SDLK_BACKSPACE &&
                    !sb_focus && !sf_focus && state.input_len > 0) {
                    state.input_len--;
                    state.input[state.input_len] = '\0';
                }

                if (sf_focus &&
                    event.key.keysym.sym == SDLK_BACKSPACE &&
                    strlen(sf_val) > 0) {
                    size_t l = strlen(sf_val);
                    while (l > 0 &&
                           ((unsigned char)sf_val[l-1] & 0xC0) == 0x80)
                        l--;
                    if (l > 0) l--;
                    sf_val[l] = '\0';
                }

                /* Android's on-screen numeric keyboard often delivers
                   digits as SDL_KEYDOWN rather than SDL_TEXTINPUT.
                   Insert digit keys ourselves so the pairing code (and
                   port) always accept input beyond a couple of chars.
                   The matching SDL_TEXTINPUT for the same digit is
                   skipped via g_pair_kbdigit to avoid double entry. */
                if (sf_focus &&
                    event.key.keysym.sym >= SDLK_0 &&
                    event.key.keysym.sym <= SDLK_9) {
                    char d[2] = {
                        (char)('0' + (event.key.keysym.sym - SDLK_0)), 0 };
                    size_t len = strlen(sf_val);
                    if (len + 1 < sf_cap) {
                        sf_val[len] = d[0];
                        sf_val[len + 1] = '\0';
                        g_pair_kbdigit = d[0];
                    }
                }

                if (sf_focus &&
                    event.key.keysym.sym == SDLK_RETURN) {
                    /* Tapping Enter in the sheet connects. */
                    if (!g_pair_connecting) {
                        NetpairStatus st = netpair_status();
                        if (st == NETPAIR_PAIRED || st == NETPAIR_STREAMING) {
                            netpair_disconnect();
                        } else {
                            netpair_set_endpoint(g_pair_host, g_pair_port);
                            netpair_set_code(g_pair_code);
                            g_pair_connecting = true;
                            netpair_connect();
                            g_pair_streamed_ready = true;
                        }
                    }
                }

                if (sb_focus)
                    searchbar_event(&searchInput, &event);

                if (event.key.keysym.sym == SDLK_RETURN &&
                    !sb_focus && !sf_focus &&
                    state.input_len > 0 && !state.is_thinking) {
                    char msg[INPUT_MAX];
                    snprintf(msg, sizeof(msg), "%s", state.input);
                    state.input[0] = '\0';
                    state.input_len = 0;
                    send_user_message(&state, msg);
                }
            }

            if (event.type == SDL_TEXTINPUT) {
                if (searchPopup.open && searchInput.focused) {
                    searchbar_event(&searchInput, &event);
                } else if (g_pair_sheet && g_pair_focus >= 0 && g_pair_focus <= 2) {
                    char *f = g_pair_focus == 0 ? g_pair_host
                             : g_pair_focus == 1 ? g_pair_port : g_pair_code;
                    size_t cap = g_pair_focus == 0 ? sizeof(g_pair_host)
                               : g_pair_focus == 1 ? sizeof(g_pair_port)
                               : sizeof(g_pair_code);
                    size_t len = strlen(f);
                    const char *t = event.text.text;
                    char buffered[INPUT_MAX];
                    if (g_pair_focus == 1) {
                        /* Port is numeric: drop anything that isn't a
                           digit before inserting. */
                        size_t di = 0;
                        for (size_t si = 0; t[si] && si < sizeof(buffered) - 1; si++) {
                            if (t[si] >= '0' && t[si] <= '9')
                                buffered[di++] = t[si];
                        }
                        buffered[di] = '\0';
                        t = buffered;
                    }
                    size_t add = strlen(t);
                    if (g_pair_kbdigit &&
                        add == 1 && t[0] == g_pair_kbdigit) {
                        /* This digit was already inserted via
                           SDL_KEYDOWN; ignore the duplicate. */
                        g_pair_kbdigit = 0;
                    } else if (len + add < cap) {
                        memcpy(f + len, t, add);
                        f[len + add] = '\0';
                    }
                } else {
                    int len = strlen(state.input);
                    int add = strlen(event.text.text);
                    if (len + add < INPUT_MAX - 1) {
                        strcat(state.input, event.text.text);
                        state.input_len = strlen(state.input);
                    }
                }
            }

            popup_event(&settings, &ui, &event);
            popup_event(&recentsPopup, &ui, &event);
            popup_event(&searchPopup, &ui, &event);
            downloads_event(&dlPanel, &ui, &event);

            if (event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.button == SDL_BUTTON_LEFT) {

                /* Pairing sheet consumes taps while it is open. */
                if (g_pair_sheet) {
                    int f = pair_field_at(&ui, width, height,
                                          event.button.x, event.button.y);
                    if (f >= 0 && f <= 2) {
                        g_pair_focus = f;
                        /* Clear the prefilled default port on first focus
                           so the user can type a fresh value. */
                        if (f == 1 && !g_pair_port_touched) {
                            g_pair_port_touched = true;
                            g_pair_port[0] = '\0';
                        }
                    } else if (f == 3) {
                        if (!g_pair_connecting) {
                            NetpairStatus st = netpair_status();
                            if (st == NETPAIR_PAIRED ||
                                st == NETPAIR_STREAMING) {
                                netpair_disconnect();
                            } else {
                                netpair_set_endpoint(g_pair_host, g_pair_port);
                                netpair_set_code(g_pair_code);
                                g_pair_connecting = true;
                                netpair_connect();
                                g_pair_streamed_ready = true;
                            }
                        }
                    } else if (f == 4) {
#ifdef ANDROID
                        android_launch_scanner();
#endif
                        g_pair_focus = -1;
                    } else {
                        g_pair_focus = -1;
                    }
                    state.input_focused = false;
                    continue;
                }

                /* Sidebar pairing row toggles the sheet. */
                if (sidebar.anim >= 0.85f) {
                    SDL_Rect prow = pair_row_rect_android(&ui);
                    if (ui_point_in_rect(event.button.x,
                                         event.button.y, prow)) {
                        g_pair_sheet = !g_pair_sheet;
                        if (g_pair_sheet) {
                            g_pair_focus = 0;
                            g_pair_port_touched = false;
                            g_pair_port[0] = '\0';
                            snprintf(g_pair_port, sizeof(g_pair_port), "5055");
                        }
                        sidebar.open = false;
                        continue;
                    }
                }

                int item = sidebar_item_at(&sidebar, &ui,
                                           event.button.x, event.button.y);
                if (item >= 0) {
                    switch (item) {
                    case 0:
                        persist_session(&state);
                        state.count = 0;
                        state.cur_session[0] = '\0';
                        state.input[0] = '\0';
                        state.input_len = 0;
                        add_message(&state, "Hi, I'm Sayri. How can I help?", false);
                        break;
                    case 1: {
                        settings.open = false;
                        searchPopup.open = false;
                        g_history.count = 0;
                        history_list(&g_history);
                        if (g_history.count > POPUP_MAX_ITEMS)
                            g_history.count = POPUP_MAX_ITEMS;
                        if (g_history.count == 0)
                            snprintf(g_item_labels[0], sizeof(g_item_labels[0]),
                                     "(no saved chats)");
                        for (int k = 0; k < g_history.count; k++) {
                            long t = atol(g_history.names[k]);
                            struct tm *tm = localtime(&t);
                            strftime(g_item_labels[k], sizeof(g_item_labels[0]),
                                     "%b %d %H:%M", tm);
                        }
                        const char *labels[POPUP_MAX_ITEMS];
                        int label_count = g_history.count;
                        if (label_count == 0) label_count = 1;
                        for (int k = 0; k < label_count; k++)
                            labels[k] = g_item_labels[k];
                        popup_set_items(&recentsPopup, labels, label_count);
                        recentsPopup.open = true;
                        break;
                    }
                    case 2: {
                        if (searchPopup.clicked_outside) break;
                        settings.open = false;
                        recentsPopup.open = false;
                        popup_toggle(&searchPopup);
                        if (searchPopup.open) {
                            searchInput.text[0] = '\0';
                            searchInput.len = 0;
                            searchInput.focused = true;
                            g_last_query[0] = '\0';
                            rebuild_search_results();
                            snprintf(g_last_query, sizeof(g_last_query), "%s",
                                     searchInput.text);
                        }
                        sidebar.open = false;
                        break;
                    }
                    case 3: {
                        if (settings.clicked_outside ||
                            recentsPopup.clicked_outside ||
                            searchPopup.clicked_outside) break;
                        settings.open = false;
                        recentsPopup.open = false;
                        searchPopup.open = false;
                        bool was_open = dlPanel.open;
                        dlPanel.open = !was_open;
                        if (dlPanel.open && !dlPanel.pulling) {
                            char models_check[MAX_MODELS][128];
                            int n = ollama_fetch_models(models_check, MAX_MODELS);
                            g_server_ok = (n >= 0);
                            bool found = false;
                            size_t ml = strlen(dlPanel.model);
                            for (int k = 0; k < n; k++) {
                                if (!strncmp(models_check[k], dlPanel.model, ml) &&
                                    (models_check[k][ml] == ':' || models_check[k][ml] == '\0')) {
                                    found = true;
                                    break;
                                }
                            }
                            if (g_server_ok) downloads_set_installed(&dlPanel, found);
                        }
                        sidebar.open = false;
                        break;
                    }
                    case 4:
                        if (!settings.clicked_outside) {
                            popup_toggle(&settings);
                            if (settings.open) {
                                recentsPopup.open = false;
                                searchPopup.open = false;
                                g_model_count = ollama_fetch_models(g_models, MAX_MODELS);
                                int n = g_model_count > 0 ? g_model_count : 0;
                                if (n > DROPDOWN_MAX_ITEMS) n = DROPDOWN_MAX_ITEMS;
                                modelDropdown.itemCount = 0;
                                for (int k = 0; k < n; k++)
                                    dropdown_add_item(&modelDropdown, g_models[k]);
                                modelDropdown.selected = n > 0 ? g_model_idx : -1;
                            }
                        }
                        sidebar.open = false;
                        break;
                    }
                }

                int ibH = (int)roundf(42.0f * ui.scale);
                int pad2 = (int)roundf(14.0f * ui.scale);
                int sbW = (int)roundf(240.0f * ui.scale * sidebar.anim);
                int sendSize = ibH;
                SDL_Rect sendBtn = {width - pad2 - sendSize,
                                    height - pad2 - ibH,
                                    sendSize, sendSize};

                if (ui_point_in_rect(event.button.x, event.button.y, sendBtn)) {
                    if (state.input_len > 0 && !state.is_thinking) {
                        char msg[INPUT_MAX];
                        snprintf(msg, sizeof(msg), "%s", state.input);
                        state.input[0] = '\0';
                        state.input_len = 0;
                        state.input_focused = true;
                        send_user_message(&state, msg);
                    }
                }

                SDL_Rect ib = {sbW + pad2,
                               height - pad2 - ibH,
                               width - sbW - pad2 * 2 - (int)roundf(42.0f * ui.scale),
                               ibH};
                state.input_focused = ui_point_in_rect(event.button.x,
                                                       event.button.y, ib);
            }

            if (event.type == SDL_MOUSEWHEEL) {
                state.target_scroll += event.wheel.y * 30.0f;
                if (state.target_scroll > 0)
                    state.target_scroll = 0;
            }

            hamburger_event(&hamburger, &event);
            sidebar_event(&sidebar, &event);
        }

#ifdef ANDROID
        /*
            Keep the soft keyboard in sync with what the user is
            editing: the main prompt or the search box.
        */
        bool want_kb = state.input_focused ||
                       (searchPopup.open && searchInput.focused) ||
                       (g_pair_sheet && g_pair_focus >= 0 && g_pair_focus <= 2);
        if (want_kb && !g_kb_shown) {
            SDL_StartTextInput();
            g_kb_shown = true;
        } else if (!want_kb && g_kb_shown) {
            SDL_StopTextInput();
            g_kb_shown = false;
        }
#endif

        ui.dark = darkToggle.on;

        int picked = popup_consume_item_click(&recentsPopup);
        if (picked >= 0 && picked < g_history.count) {
            load_session_file(&state, g_history.names[picked]);
            recentsPopup.open = false;
        }

        if (searchPopup.open) {
            if (strcmp(g_last_query, searchInput.text) != 0) {
                snprintf(g_last_query, sizeof(g_last_query), "%s", searchInput.text);
                rebuild_search_results();
            }
            int spicked = popup_consume_item_click(&searchPopup);
            if (spicked >= 0 && spicked < POPUP_MAX_ITEMS &&
                g_search_files[spicked][0]) {
                load_session_file(&state, g_search_files[spicked]);
                searchPopup.open = false;
            }
        }

        if (settings.open && modelDropdown.selected >= 0 &&
            g_model_count > 0 && modelDropdown.selected < g_model_count &&
            modelDropdown.selected != g_model_idx) {
            g_model_idx = modelDropdown.selected;
            snprintf(g_current_model, sizeof(g_current_model),
                     "%s", g_models[g_model_idx]);
            ollama_set_model(g_current_model);
        }

        OllamaPull pull;
        ollama_poll_pull(&pull);
        downloads_set_pull(&dlPanel, &pull);

        OllamaSetup setup;
        ollama_poll_setup(&setup);
        downloads_set_setup(&dlPanel, &setup, !g_server_ok);

        if (setup.done && setup.ok) {
            g_model_count = ollama_fetch_models(g_models, MAX_MODELS);
            g_server_ok = (g_model_count >= 0);
            g_model_idx = 0;
            for (int k = 0; k < g_model_count; k++) {
                if (!strcmp(g_models[k], g_current_model)) {
                    g_model_idx = k;
                    break;
                }
            }
            modelDropdown.selected = g_model_count > 0 ? g_model_idx : -1;

            if (g_retry_pending) {
                g_retry_pending = false;
                g_autoheal_attempts = 0;
                if (!dispatch_chat(&state))
                    add_message(&state, "(Ready \u2014 tap send to retry.)", false);
            }
        }

        if (downloads_consume_install_click(&dlPanel)) {
            bool busy = dlPanel.pulling || dlPanel.setting_up;
            if (!busy) {
                if (!g_server_ok) {
                    g_server_ok = true;
                    dlPanel.failed = false;
                    dlPanel.note[0] = '\0';
                    ollama_setup_begin(dlPanel.model);
                } else {
                    ollama_pull_begin(dlPanel.model);
                }
            }
        }

        if (hamburger.clicked) {
            hamburger.clicked = false;
            sidebar.open = !sidebar.open;
        }
        hamburger.open = sidebar.open;

        Uint64 now = SDL_GetPerformanceCounter();
        float time = (float)((double)(now - start) / (double)perf_freq);
        float dt = time - prev_time;
        prev_time = time;
        if (dt > 0.05f) dt = 0.05f;

        state.scroll_offset += (state.target_scroll - state.scroll_offset) * 8.0f * dt;

        for (int i = 0; i < state.count; i++) {
            ChatMessage *m = &state.messages[i];
            m->alpha += (1.0f - m->alpha) * 6.0f * dt;
            m->slide_y += (0.0f - m->slide_y) * 8.0f * dt;
        }

        /* IPC disabled on Android */

        /* Stream assistant replies arriving from a paired computer. */
        {
            char buf[NETPAIR_MAX_TEXT];
            bool nd = false;
            bool finished = netpair_poll(buf, sizeof(buf), &nd);
            if (finished) {
                state.is_thinking = false;
                g_pair_connecting = false;
                if (buf[0]) {
                    add_message(&state, buf, false);
                    g_pair_reply[0] = '\0';
                }
            }
            if (nd) {
                snprintf(g_pair_reply, sizeof(g_pair_reply), "%s", buf);
            }
            if (netpair_status() == NETPAIR_IDLE)
                g_pair_connecting = false;
        }

#ifdef ANDROID
        /* If the user scanned a QR, auto-fill the pairing sheet. */
        android_apply_scanned_qr();
#endif

        OllamaReply reply;
        ollama_poll(&reply);
        if (reply.done) {
            state.is_thinking = false;
            if (!reply.ok && reply.server_down &&
                !g_retry_pending && g_autoheal_attempts < 2) {
                g_autoheal_attempts++;
                g_retry_pending = true;
                if (!dlPanel.setting_up)
                    ollama_setup_begin(g_current_model);
                add_message(&state, "(Starting the local AI service\u2026)", false);
            } else {
                if (reply.ok) g_autoheal_attempts = 0;
                add_message(&state, reply.text, false);
            }
        }

        /* Render */
        if (width != rt_w || height != rt_h) {
            if (rt) SDL_DestroyTexture(rt);
            rt_w = width;
            rt_h = height;
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
            rt = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_TARGET,
                                   width * 2, height * 2);
            SDL_SetTextureBlendMode(rt, SDL_BLENDMODE_BLEND);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        }

        SDL_SetRenderTarget(renderer, rt);
        SDL_RenderSetScale(renderer, 2.0f, 2.0f);
        draw_background(renderer, &ui, width, height, time);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_Rect dst = {0, 0, width, height};
        SDL_RenderCopy(renderer, rt, NULL, &dst);

        ui_text_cache_clear();

        int pad = (int)roundf(14.0f * ui.scale);
        int inputBarH = (int)roundf(42.0f * ui.scale);
        int sendBtnSize = inputBarH;
        float sbWF = ui_ease_out_cubic(sidebar.anim) * 300.0f;
        int sbPx = (int)(sbWF * ui.scale);

        int chatTop = pad;
        int chatBottom = height - pad - inputBarH - pad;
        int chatH = chatBottom - chatTop;
        if (chatH < 80) chatH = 80;

        sidebar_layout(&sidebar, &ui, 0, 0, 300, height / ui.scale);
        sidebar_draw(&sidebar, &ui, renderer, boldFont, menuFont, dt);

        /* Pair devices button in the sidebar (with the panel). */
        draw_pair_row(renderer, &ui, smallFont, sidebar.anim);

        hamburger_layout(&hamburger, &ui, 15, 15, 44);
        hamburger_draw(&hamburger, &ui, renderer);

        draw_chat_area(renderer, &state, &ui, font,
                       sbPx + pad, chatTop,
                       width - sbPx - pad * 2, chatH);

        orb_grow += ((state.is_thinking ? 1.0f : 0.0f) - orb_grow) * 5.0f * dt;
        int chatW = width - sbPx - pad * 2;
        /* On a portrait phone the orb was 24% of the chat width and capped
           at 240px, which read as a small dot. Base it on both dimensions
           and let it grow much larger. */
        float orbBase = fminf((float)chatW, (float)chatH);
        float orbSizeF = orbBase * 0.42f * (1.0f + 0.10f * orb_grow);
        if (orbSizeF > height * 0.46f) orbSizeF = height * 0.46f;
        if (orbSizeF > 360.0f) orbSizeF = 360.0f;

        SDL_Rect orbRect = {
            sbPx + pad + (chatW - (int)orbSizeF) / 2,
            chatTop + pad,
            (int)orbSizeF, (int)orbSizeF
        };

        orb_dt += dt;
        frame_count++;
        if ((frame_count & 1) == 0) {
            orb_update(&orb, orb_dt);
            orb_dt = 0.0f;
        }
        orb_draw(&orb, renderer, orbRect);

        draw_input_bar(renderer, &state, &ui, font,
                       sbPx + pad, height - pad - inputBarH,
                       width - sbPx - pad * 2 - sendBtnSize - 8,
                       inputBarH, time);

        draw_send_button(renderer, &state, &ui, font,
                         width - pad - sendBtnSize,
                         height - pad - inputBarH,
                         sendBtnSize);

        float sheetMargin = 10.0f;
        float sheetW = (float)width / ui.scale - sheetMargin * 2.0f;

        popup_layout(&settings, &ui,
                     sheetMargin, 64.0f, sheetW, POPUP_H_DROPDOWN);
        popup_draw(&settings, &ui, renderer, font, boldFont, dt);

        popup_layout(&recentsPopup, &ui, sheetMargin, 64.0f,
                     sheetW,
                     recentsPopup.item_count
                         ? POPUP_HEIGHT_FOR(recentsPopup.item_count)
                         : POPUP_DEFAULT_H);
        popup_draw(&recentsPopup, &ui, renderer, font, boldFont, dt);

        popup_layout(&searchPopup, &ui, sheetMargin, 64.0f,
                     sheetW,
                     POPUP_HEIGHT_FOR_SEARCH(
                         searchPopup.item_count ? searchPopup.item_count : 1));
        popup_draw(&searchPopup, &ui, renderer, font, boldFont, dt);

        downloads_layout(&dlPanel, &ui,
                         sheetMargin, 64.0f, sheetW, DL_DEFAULT_H);
        downloads_draw(&dlPanel, &ui, renderer, font, boldFont, dt);

        /* Pairing sheet is topmost, drawn last. */
        draw_pair_sheet(renderer, &ui, smallFont, width, height);

        SDL_RenderPresent(renderer);
    }

    persist_session(&state);
    if (rt) SDL_DestroyTexture(rt);
    orb_free(&orb);
    netpair_disconnect();
    ollama_shutdown();
    close_fonts(&fonts);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
