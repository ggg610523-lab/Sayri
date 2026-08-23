#ifndef DOWNLOADS_H
#define DOWNLOADS_H

#include "ui.h"
#include "button.h"
#include "progressBar.h"
#include "ollama.h"

/*
    Default panel size (design pixels).
*/
#define DL_DEFAULT_W 300.0f
#define DL_DEFAULT_H 172.0f

typedef struct {
    /*
        Pixel-space rect (filled by layout).
    */
    SDL_Rect rect;

    /*
        Design-space geometry.
    */
    float x;
    float y;
    float w;
    float h;

    bool open;

    /*
        True for a single frame after a click
        outside closed the panel.
    */
    bool clicked_outside;

    /*
        Model offered by this panel.
    */
    char model[128];

    UIButton install_btn;
    UIProgressBar bar;

    /*
        True when the server reports the model
        as already installed.
    */
    bool installed;

    /*
        True when /api is unreachable — the
        button then offers full bootstrap.
    */
    bool server_missing;

    /*
        Latest pull/setup snapshots (set via
        downloads_set_pull each frame).
    */
    bool pulling;
    bool setting_up;
    bool failed;
    char note[336];
    float fraction;

    float anim;

    int mouse_x;
    int mouse_y;
} UIDownloads;

void downloads_init(
    UIDownloads *dl,
    const char *model
);

void downloads_layout(
    UIDownloads *dl,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h
);

/*
    Outside clicks dismiss the panel; clicks on
    the install button are forwarded to it.
*/
void downloads_event(
    UIDownloads *dl,
    UIContext *ui,
    SDL_Event *event
);

/*
    Returns true exactly once per click and
    clears the button flag.
*/
bool downloads_consume_install_click(
    UIDownloads *dl
);

/*
    Feed the latest ollama_poll_pull()
    snapshot.
*/
void downloads_set_pull(
    UIDownloads *dl,
    const OllamaPull *pull
);

/*
    Feed the latest ollama_poll_setup()
    snapshot and current server state.
*/
void downloads_set_setup(
    UIDownloads *dl,
    const OllamaSetup *setup,
    bool server_missing
);

void downloads_set_installed(
    UIDownloads *dl,
    bool installed
);

void downloads_draw(
    UIDownloads *dl,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    TTF_Font *title_font,
    float dt
);

#endif /* DOWNLOADS_H */
