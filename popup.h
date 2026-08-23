#ifndef POPUP_H
#define POPUP_H

#include "ui.h"
#include "toggle.h"

/*
    Default panel size (design pixels).
*/
#define POPUP_DEFAULT_W 264.0f
#define POPUP_DEFAULT_H 112.0f

/*
    Optional item-list geometry (design pixels).
*/
#define POPUP_VALUE_Y    86.0f
#define POPUP_ITEMS_Y   114.0f
#define POPUP_ITEM_STEP  28.0f
#define POPUP_MAX_ITEMS 12

/*
    Panel height needed for an item list.
*/
#define POPUP_HEIGHT_FOR(n) \
    (POPUP_ITEMS_Y + (n) * POPUP_ITEM_STEP + 14.0f)

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

    const char *title;
    const char *row_label;

    /*
        Second interactive row (e.g. the model
        picker). NULL hides it. The string is
        owned by the caller.
    */
    const char *value_label;

    /*
        Clickable list rows (owned by caller).
    */
    const char *items[POPUP_MAX_ITEMS];
    int item_count;

    UIToggle *toggle;

    bool open;

    /*
        True for a single frame after a click
        outside closed the popup.
    */
    bool clicked_outside;

    /*
        Internal click flags (consumed via the
        popup_consume_* accessors).
    */
    int _item_clicked;
    bool _value_clicked;

    float anim;

    /*
        Last known mouse position (raw window
        coordinates).
    */
    int mouse_x;
    int mouse_y;
} UIPopup;

void popup_init(
    UIPopup *pop,
    const char *title
);

void popup_set_title(
    UIPopup *pop,
    const char *title
);

void popup_set_row_label(
    UIPopup *pop,
    const char *label
);

void popup_link_toggle(
    UIPopup *pop,
    UIToggle *toggle
);

void popup_set_value(
    UIPopup *pop,
    const char *label
);

void popup_set_items(
    UIPopup *pop,
    const char **items,
    int count
);

/*
    Returns the clicked row index and clears
    the flag (-1 when nothing was clicked).
*/
int popup_consume_item_click(
    UIPopup *pop
);

bool popup_consume_value_click(
    UIPopup *pop
);

void popup_open(
    UIPopup *pop
);

void popup_close(
    UIPopup *pop
);

void popup_toggle(
    UIPopup *pop
);

void popup_layout(
    UIPopup *pop,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h
);

void popup_event(
    UIPopup *pop,
    UIContext *ui,
    SDL_Event *event
);

void popup_draw(
    UIPopup *pop,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    TTF_Font *title_font,
    float dt
);

#endif /* POPUP_H */
