#ifndef SLIDER_H
#define SLIDER_H

#include "ui.h"

typedef struct {
    SDL_Rect rect;

    float value;

    bool dragging;

    float glow_anim;
} UISlider;

void slider_init(
    UISlider *slider,
    float value
);

void slider_layout(
    UISlider *slider,
    UIContext *ui,
    float x,
    float y,
    float w
);

void slider_event(
    UISlider *slider,
    SDL_Event *event
);

void slider_draw(
    UISlider *slider,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    float dt
);

#endif /* SLIDER_H */
