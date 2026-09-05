#ifndef ORB1_H
#define ORB1_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define ORB1_RES 256

typedef struct {
    SDL_Texture *texture;
    Uint32 pixels[ORB1_RES * ORB1_RES];
    float time;
    float audio;
    bool dirty;
    bool visible;
    float dx, dy, dw, dh;
    SDL_Rect rect;
} Orb1;

void orb1_init(
    Orb1 *orb,
    SDL_Renderer *renderer
);

void orb1_free(Orb1 *orb);

void orb1_update(
    Orb1 *orb,
    float dt
);

void orb1_draw(
    Orb1 *orb,
    SDL_Renderer *renderer,
    SDL_Rect dst
);

#endif /* ORB1_H */
