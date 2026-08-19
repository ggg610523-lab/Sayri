
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <dirent.h>

#include "ui.h"
#include "hamburger.h"
#include "sidebar.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_MESSAGES 32
#define MSG_MAX 512
#define INPUT_MAX 256

static UIColor lerp_color(
    UIColor a, UIColor b, float t)
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
    float thinking_timer;
} AppState;

static char *find_font(void)
{
    DIR *dir = opendir("font");
    if (!dir) return NULL;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 4 &&
            strcasecmp(name + len - 4, ".ttf") == 0) {
            char *path =
                malloc(strlen("font/") + strlen(name) + 1);
            sprintf(path, "font/%s", name);
            closedir(dir);
            return path;
        }
    }

    closedir(dir);
    return NULL;
}

static void add_message(
    AppState *state,
    const char *text,
    bool is_user)
{
    if (state->count >= MAX_MESSAGES)
        return;

    ChatMessage *m =
        &state->messages[state->count];

    strncpy(m->text, text, MSG_MAX - 1);
    m->text[MSG_MAX - 1] = '\0';
    m->is_user = is_user;
    m->alpha = 0.0f;
    m->slide_y = 16.0f;

    state->count++;
}

static void simulate_response(AppState *state)
{
    const char *responses[] = {
        "I can help you with that.",
        "Let me look into this for you.",
        "That's a great question.",
        "Here's what I found.",
        "I understand. One moment.",
        "Sure, I'll take care of that.",
    };

    int n = (int)(sizeof(responses) /
                  sizeof(responses[0]));

    int idx =
        (int)((float)n *
              fmodf(
                  (float)SDL_GetTicks() /
                  4000.0f, 1.0f));

    if (idx >= n) idx = n - 1;

    add_message(state, responses[idx], false);
}

static void draw_chat_area(
    SDL_Renderer *renderer,
    AppState *state,
    UIContext *ui,
    TTF_Font *font,
    int x, int y, int w, int h)
{
    SDL_Rect container = {x, y, w, h};

    ui_fill_rounded_rect(
        renderer, container,
        (int)roundf(18.0f * ui->scale),
        (UIColor){240, 245, 252, 175});

    SDL_RenderSetClipRect(renderer, &container);

    int pad =
        (int)roundf(14.0f * ui->scale);

    int msgH =
        (int)roundf(38.0f * ui->scale);

    int msgGap =
        (int)roundf(6.0f * ui->scale);

    int curY =
        y + h - pad +
        (int)state->scroll_offset;

    for (int i = state->count - 1; i >= 0; i--) {

        ChatMessage *m = &state->messages[i];
        if (m->alpha < 0.01f) continue;

        int maxBubbleW = w - pad * 2 -
            (int)roundf(40.0f * ui->scale);

        int bubbleW =
            m->is_user
            ? (int)(maxBubbleW * 0.60f)
            : (int)(maxBubbleW * 0.75f);

        if (bubbleW < 60) bubbleW = 60;

        int textPad =
            (int)roundf(12.0f * ui->scale);

        int bubbleX =
            m->is_user
            ? x + w - pad - bubbleW
            : x + pad;

        curY -= msgH + msgGap;
        int bubbleY =
            curY + (int)(m->slide_y * (1.0f - m->alpha));

        Uint8 bgA =
            (Uint8)(180.0f * m->alpha);

        SDL_Rect bubble = {
            bubbleX, bubbleY, bubbleW, msgH};

        if (m->is_user) {
            ui_fill_rounded_rect(
                renderer, bubble,
                (int)roundf(14.0f * ui->scale),
                (UIColor){200, 210, 240, bgA});
        } else {
            ui_fill_rounded_rect(
                renderer, bubble,
                (int)roundf(14.0f * ui->scale),
                (UIColor){230, 235, 248, bgA});
        }

        Uint8 tA =
            (Uint8)(230.0f * m->alpha);

        ui_text(
            renderer, font, m->text,
            bubbleX + textPad,
            bubbleY +
                (msgH - (int)roundf(
                    17.0f * ui->scale)) / 2,
            m->is_user
            ? (UIColor){35, 45, 80, tA}
            : (UIColor){50, 55, 85, tA});
    }

    SDL_RenderSetClipRect(renderer, NULL);
}

static void draw_input_bar(
    SDL_Renderer *renderer,
    AppState *state,
    UIContext *ui,
    TTF_Font *font,
    int x, int y, int w, int h,
    float time)
{
    SDL_Rect bar = {x, y, w, h};

    ui_fill_rounded_rect(
        renderer, bar,
        (int)roundf(14.0f * ui->scale),
        state->input_focused
        ? (UIColor){246, 250, 255, 205}
        : (UIColor){240, 245, 252, 175});

    int pad =
        (int)roundf(14.0f * ui->scale);

    if (state->input_len > 0) {
        ui_text(
            renderer, font, state->input,
            x + pad,
            y + (h - (int)roundf(
                17.0f * ui->scale)) / 2,
            (UIColor){30, 35, 60, 255});
    } else {
        ui_text(
            renderer, font,
            "Ask Sayri\u2026",
            x + pad,
            y + (h - (int)roundf(
                17.0f * ui->scale)) / 2,
            (UIColor){140, 145, 170, 180});
    }

    if (state->input_focused) {
        float blink =
            fmodf(
                (float)SDL_GetTicks() / 1000.0f,
                1.0f);

        if (blink < 0.5f) {
            int textW = 0;
            if (state->input_len > 0)
                TTF_SizeUTF8(
                    font, state->input,
                    &textW, NULL);

            int cx = x + pad + textW + 2;
            int ch = (int)roundf(16.0f * ui->scale);

            float ct = fmodf(time * 2.0f, 4.0f);
            UIColor cc;
            UIColor c1 = {110, 140, 235, 255};
            UIColor c2 = {230, 130, 195, 255};
            UIColor c3 = {170, 140, 220, 255};
            UIColor c4 = {100, 200, 185, 255};

            if (ct < 1.0f)
                cc = lerp_color(c1, c2, ct);
            else if (ct < 2.0f)
                cc = lerp_color(c2, c3, ct - 1.0f);
            else if (ct < 3.0f)
                cc = lerp_color(c3, c4, ct - 2.0f);
            else
                cc = lerp_color(c4, c1, ct - 3.0f);

            cc.a = 230;

            SDL_SetRenderDrawBlendMode(
                renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(
                renderer,
                cc.r, cc.g, cc.b, cc.a);
            SDL_RenderDrawLine(
                renderer,
                cx, y + (h - ch) / 2,
                cx, y + (h + ch) / 2);
        }
    }
}

static void draw_send_button(
    SDL_Renderer *renderer,
    AppState *state,
    TTF_Font *font,
    int x, int y, int size)
{
    bool has = state->input_len > 0;

    int r = size / 2;

    if (has) {
        ui_fill_circle(
            renderer,
            x + size / 2, y + size / 2, r,
            (UIColor){70, 130, 255, 255});
        ui_fill_circle(
            renderer,
            x + size / 2, y + size / 2,
            r + 4,
            (UIColor){70, 130, 255, 35});
    } else {
        ui_fill_circle(
            renderer,
            x + size / 2, y + size / 2, r,
            (UIColor){200, 205, 220, 150});
    }

    const char *arrow = "\u2191";
    int tw = 0, th = 0;
    TTF_SizeUTF8(font, arrow, &tw, &th);

    ui_text(
        renderer, font, arrow,
        x + (size - tw) / 2,
        y + (size - th) / 2,
        (UIColor){255, 255, 255,
                  has ? 255 : 100});
}

static void draw_background(
    SDL_Renderer *renderer,
    int width, int height, float time)
{
    for (int y = 0; y < height; ++y) {
        float t =
            (float)y / (float)(height - 1);
        Uint8 r = (Uint8)(205 + t * 25.0f);
        Uint8 g = (Uint8)(215 + t * 18.0f);
        Uint8 b = (Uint8)(242 - t * 5.0f);

        SDL_SetRenderDrawColor(
            renderer, r, g, b, 255);
        SDL_RenderDrawLine(
            renderer, 0, y, width - 1, y);
    }

    float t1 = time * 0.15f;
    float t2 = time * 0.12f + 2.0f;
    float t3 = time * 0.18f + 4.0f;

    ui_fill_radial_gradient(
        renderer,
        (int)(width * 0.55f + sinf(t1) * 60.0f),
        (int)(height * 0.25f + cosf(t1 * 0.7f) * 40.0f),
        (int)(width * 0.28f),
        (UIColor){110, 140, 235, 90},
        (UIColor){110, 140, 235, 0});

    ui_fill_radial_gradient(
        renderer,
        (int)(width * 0.72f + cosf(t2) * 50.0f),
        (int)(height * 0.70f + sinf(t2 * 0.8f) * 35.0f),
        (int)(width * 0.22f),
        (UIColor){230, 130, 195, 80},
        (UIColor){230, 130, 195, 0});

    ui_fill_radial_gradient(
        renderer,
        (int)(width * 0.38f + sinf(t3 * 0.6f) * 45.0f),
        (int)(height * 0.60f + cosf(t3) * 30.0f),
        (int)(width * 0.18f),
        (UIColor){100, 200, 185, 70},
        (UIColor){100, 200, 185, 0});

    ui_fill_radial_gradient(
        renderer,
        (int)(width * 0.85f + sinf(t1 * 0.9f) * 35.0f),
        (int)(height * 0.15f + cosf(t2 * 0.5f) * 25.0f),
        (int)(width * 0.15f),
        (UIColor){170, 140, 220, 55},
        (UIColor){170, 140, 220, 0});
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    }

    SDL_StartTextInput();

    if (TTF_Init() != 0) {
        fprintf(stderr, "%s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window =
        SDL_CreateWindow(
            "Sayri",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            520, 720,
            SDL_WINDOW_SHOWN |
            SDL_WINDOW_RESIZABLE);

    SDL_SetHint(
        SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_Renderer *renderer =
        SDL_CreateRenderer(
            window, -1,
            SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC);

    if (!window || !renderer)
        return 1;

    char *fontPath = find_font();
    if (!fontPath) {
        printf("Put a .ttf in ./font/\n");
        return 1;
    }

    TTF_Font *font =
        TTF_OpenFont(fontPath, 13);
    TTF_Font *titleFont =
        TTF_OpenFont(fontPath, 11);
    TTF_Font *smallFont =
        TTF_OpenFont(fontPath, 10);
    TTF_Font *boldFont =
        TTF_OpenFont(fontPath, 14);

    if (boldFont)
        TTF_SetFontStyle(
            boldFont, TTF_STYLE_BOLD);

    free(fontPath);

    if (!font || !titleFont || !smallFont) {
        printf("Could not load font\n");
        return 1;
    }

    AppState state;
    memset(&state, 0, sizeof(state));
    state.input_focused = true;

    add_message(&state,
        "Hi, I'm Sayri. How can I help?", false);

    UIContext ui;
    ui.dark = false;

    UIHamburger hamburger;
    hamburger_init(&hamburger);

    UISidebar sidebar;
    sidebar_init(&sidebar);

    SDL_Texture *rt = NULL;
    int rt_w = 0;
    int rt_h = 0;

    bool running = true;
    Uint64 perf_freq =
        SDL_GetPerformanceFrequency();
    Uint64 start =
        SDL_GetPerformanceCounter();
    float prev_time =
        (float)((double)start / (double)perf_freq);

    while (running) {

        int width, height;
        SDL_GetWindowSize(
            window, &width, &height);

        ui_begin(&ui, width, height);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN) {

                if (event.key.keysym.sym ==
                    SDLK_ESCAPE)
                    running = false;

                if (event.key.keysym.sym ==
                    SDLK_BACKSPACE &&
                    state.input_len > 0) {
                    state.input_len--;
                    state.input[state.input_len] = '\0';
                }

                if (event.key.keysym.sym ==
                    SDLK_RETURN &&
                    state.input_len > 0) {
                    add_message(&state,
                        state.input, true);
                    state.input[0] = '\0';
                    state.input_len = 0;
                    state.is_thinking = true;
                    state.thinking_timer = 1.0f;
                }
            }

            if (event.type == SDL_TEXTINPUT) {
                int len = strlen(state.input);
                int add = strlen(event.text.text);
                if (len + add < INPUT_MAX - 1) {
                    strcat(state.input,
                           event.text.text);
                    state.input_len =
                        strlen(state.input);
                }
            }

            if (event.type ==
                SDL_MOUSEBUTTONDOWN &&
                event.button.button ==
                SDL_BUTTON_LEFT)
            {
                int ibH =
                    (int)roundf(42.0f * ui.scale);
                int pad2 =
                    (int)roundf(14.0f * ui.scale);
                int sbW =
                    (int)roundf(
                        240.0f * ui.scale *
                        sidebar.anim);

                SDL_Rect ib = {
                    sbW + pad2,
                    height - pad2 - ibH,
                    width - sbW - pad2 * 2 -
                        (int)roundf(42.0f * ui.scale),
                    ibH};

                state.input_focused =
                    ui_point_in_rect(
                        event.button.x,
                        event.button.y, ib);
            }

            if (event.type == SDL_MOUSEWHEEL) {
                state.target_scroll +=
                    event.wheel.y * 30.0f;
                if (state.target_scroll > 0)
                    state.target_scroll = 0;
            }

            hamburger_event(&hamburger, &event);
            sidebar_event(&sidebar, &event);
        }

        if (hamburger.clicked) {
            sidebar.open = !sidebar.open;
        }

        hamburger.open = sidebar.open;

        /*
            Sidebar button actions.
        */
        for (int i = 0; i < SIDEBAR_ITEMS; i++) {
            if (sidebar.buttons[i].clicked) {
                switch (i) {
                case 0:
                    state.count = 0;
                    state.input[0] = '\0';
                    state.input_len = 0;
                    add_message(&state,
                        "Hi, I'm Sayri. "
                        "How can I help?",
                        false);
                    break;
                case 1:
                    add_message(&state,
                        "No recent conversations.",
                        false);
                    break;
                case 2:
                    add_message(&state,
                        "Search coming soon.",
                        false);
                    break;
                case 3:
                    add_message(&state,
                        "Settings coming soon.",
                        false);
                    break;
                }
            }
        }

        Uint64 now =
            SDL_GetPerformanceCounter();

        float time =
            (float)((double)(now - start) /
                    (double)perf_freq);

        float dt = time - prev_time;
        prev_time = time;
        if (dt > 0.05f) dt = 0.05f;

        state.scroll_offset +=
            (state.target_scroll -
             state.scroll_offset) * 8.0f * dt;

        for (int i = 0; i < state.count; i++) {
            ChatMessage *m = &state.messages[i];
            m->alpha += (1.0f - m->alpha) * 6.0f * dt;
            m->slide_y +=
                (0.0f - m->slide_y) * 8.0f * dt;
        }

        if (state.is_thinking) {
            state.thinking_timer -= dt;
            if (state.thinking_timer <= 0) {
                state.is_thinking = false;
                simulate_response(&state);
            }
        }

        /*
            ------------------------------------------------
            RENDER (glass at 2x, text at 1x)
            ------------------------------------------------
        */

        if (width != rt_w || height != rt_h) {

            if (rt)
                SDL_DestroyTexture(rt);

            rt_w = width;
            rt_h = height;

            rt =
                SDL_CreateTexture(
                    renderer,
                    SDL_PIXELFORMAT_ARGB8888,
                    SDL_TEXTUREACCESS_TARGET,
                    width * 2,
                    height * 2);

            SDL_SetTextureBlendMode(
                rt, SDL_BLENDMODE_BLEND);
        }

        /*
            Pass 1: background + glass at 2x.
        */

        SDL_SetRenderTarget(renderer, rt);
        SDL_RenderSetScale(renderer, 2.0f, 2.0f);

        draw_background(
            renderer, width, height, time);

        /*
            Pass 2: downscale glass.
        */

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderSetScale(
            renderer, 1.0f, 1.0f);

        SDL_Rect dst = {0, 0, width, height};
        SDL_RenderCopy(
            renderer, rt, NULL, &dst);

        /*
            Pass 3: widgets + text at 1x native.
        */

        ui_text_cache_clear();

        /*
            Layout.
        */
        int pad =
            (int)roundf(14.0f * ui.scale);
        int inputBarH =
            (int)roundf(42.0f * ui.scale);
        int sendBtnSize = inputBarH;
        float sbWF =
            ui_ease_out_cubic(sidebar.anim) *
            260.0f;
        int sbPx =
            (int)(sbWF * ui.scale);

        int chatTop = pad;
        int chatBottom =
            height - pad - inputBarH - pad;
        int chatH = chatBottom - chatTop;
        if (chatH < 80) chatH = 80;

        /*
            Sidebar.
        */
        sidebar_layout(
            &sidebar, &ui,
            0, 0, 240, height / ui.scale);

        sidebar_draw(
            &sidebar, &ui, renderer, font,
            boldFont, dt);

        /*
            Hamburger button.
        */
        hamburger_layout(
            &hamburger, &ui,
            15, 15, 40);

        hamburger_draw(
            &hamburger, &ui, renderer, dt);

        /*
            Chat area.
        */
        draw_chat_area(
            renderer, &state, &ui, font,
            sbPx + pad, chatTop,
            width - sbPx - pad * 2, chatH);

        /*
            Input bar.
        */
        draw_input_bar(
            renderer, &state, &ui, font,
            sbPx + pad,
            height - pad - inputBarH,
            width - sbPx - pad * 2 - sendBtnSize - 8,
            inputBarH, time);

        /*
            Send button.
        */
        draw_send_button(
            renderer, &state, font,
            width - pad - sendBtnSize,
            height - pad - inputBarH,
            sendBtnSize);

        SDL_RenderPresent(renderer);
    }

    if (rt)
        SDL_DestroyTexture(rt);

    TTF_CloseFont(font);
    TTF_CloseFont(titleFont);
    TTF_CloseFont(smallFont);
    if (boldFont) TTF_CloseFont(boldFont);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}
