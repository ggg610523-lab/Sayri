#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "ui.h"

#define SIDEBAR_ITEMS 5

/*
    Item row geometry (design pixels).

    Sized up from the desktop values (210x32, gap 38) so
    the rows are comfortable touch targets on a phone.
*/
#define SIDEBAR_ITEM_X    15.0f
#define SIDEBAR_ITEM_Y    64.0f
#define SIDEBAR_ITEM_W    245.0f
#define SIDEBAR_ITEM_H    44.0f
#define SIDEBAR_ITEM_GAP  54.0f

typedef struct {
    SDL_Rect rect;
    float anim;
    bool open;
    const char *items[SIDEBAR_ITEMS];

    /*
        Last known mouse position (raw window
        coordinates) for hover states.
    */
    int mouse_x;
    int mouse_y;
} UISidebar;

void sidebar_init(
    UISidebar *sb
);

void sidebar_layout(
    UISidebar *sb,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h
);

void sidebar_event(
    UISidebar *sb,
    SDL_Event *event
);

void sidebar_draw(
    UISidebar *sb,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *title_font,
    TTF_Font *item_font,
    float dt
);

#endif /* SIDEBAR_H */
