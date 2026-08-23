/*
    Renders the Search popup offscreen and
    saves a BMP for visual inspection.
*/
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "ui.h"
#include "popup.h"

int main(void)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL fail: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() != 0) {
        printf("TTF fail\n");
        return 1;
    }

    SDL_Window *window =
        SDL_CreateWindow(
                "shot",
            0, 0, 1100, 700,
            SDL_WINDOW_HIDDEN);

    SDL_Renderer *renderer =
        SDL_CreateRenderer(
            window, -1,
            SDL_RENDERER_SOFTWARE);

    TTF_Font *font =
        TTF_OpenFont("font/default.ttf", 13);
    TTF_Font *boldFont =
        TTF_OpenFont("font/default.ttf", 15);

    if (!font || !boldFont) {
        printf("font fail: %s\n", TTF_GetError());
        return 1;
    }

    UIContext ui;
    ui.dark = true;

    UIToggle tgl;
    toggle_init(&tgl, false);
    tgl.on = true;

    UISearchBar bar;
    searchbar_init(&bar);
    bar.focused = true;

    UIPopup pop;
    popup_init(&pop, "Search");
    popup_link_search(&pop, &bar);

    static char label_buf[3][72];
    snprintf(label_buf[0], 72, "Aug 22 21:04");
    snprintf(label_buf[1], 72, "Aug 22 18:40");
    snprintf(label_buf[2], 72, "Aug 21 12:15");

    const char *labels[3] = {
        label_buf[0], label_buf[1], label_buf[2] };

    popup_set_items(&pop, labels, 3);
    popup_layout(&pop, &ui,
                 70.0f, 64.0f,
                 POPUP_DEFAULT_W,
                 POPUP_HEIGHT_FOR_SEARCH(3));
    popup_open(&pop);
    pop.anim = 1.0f;

    /*
        Fake chat backdrop so contrast against
        the real app background is visible.
    */
    for (int frame = 0; frame < 5; frame++) {

        ui_begin(&ui, 1100, 700);

        SDL_SetRenderDrawColor(
            renderer, 18, 18, 21, 255);
        SDL_RenderClear(renderer);

        float dt = 1.0f / 60.0f;

        popup_draw(&pop, &ui, renderer,
                   font, boldFont, dt);

        SDL_RenderPresent(renderer);
    }

    SDL_Surface *shot =
        SDL_CreateRGBSurfaceWithFormat(
            0, 1100, 700, 32,
            SDL_PIXELFORMAT_ARGB8888);

    SDL_RenderReadPixels(
        renderer, NULL,
        SDL_PIXELFORMAT_ARGB8888,
        shot->pixels, shot->pitch);

    SDL_SaveBMP(shot, "/tmp/opencode/search_dark.bmp");

    /*
        Light theme pass.
    */
    ui.dark = false;
    bar.focused = false;

    for (int frame = 0; frame < 5; frame++) {

        ui_begin(&ui, 1100, 700);

        SDL_SetRenderDrawColor(
            renderer, 242, 242, 245, 255);
        SDL_RenderClear(renderer);

        popup_draw(&pop, &ui, renderer,
                   font, boldFont, 1.0f / 60.0f);

        SDL_RenderPresent(renderer);
    }

    SDL_RenderReadPixels(
        renderer, NULL,
        SDL_PIXELFORMAT_ARGB8888,
        shot->pixels, shot->pitch);

    SDL_SaveBMP(shot, "/tmp/opencode/search_light.bmp");

    printf("saved shots\n");
    return 0;
}
