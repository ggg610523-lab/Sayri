#ifndef ORB_H
#define ORB_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define ORB_RES 256

typedef struct {
    SDL_Texture *texture;
    Uint32 pixels[ORB_RES * ORB_RES];
    float time;
    bool dirty;
    bool visible;
    float dx, dy, dw, dh;
    SDL_Rect rect;
    void *fallback;

    /*
        Tap ripple: a single damped sine wave warping the
        orb texture from the tap point (texture-space
        coordinates, 0..ORB_RES).
    */
    bool tap_active;
    float tap_ox, tap_oy;
    Uint64 tap_start;
} Orb;

void orb_init(
    Orb *orb,
    SDL_Renderer *renderer
);

void orb_free(Orb *orb);

void orb_update(
    Orb *orb,
    float dt
);

void orb_draw(
    Orb *orb,
    SDL_Renderer *renderer,
    SDL_Rect dst
);

/*
    Start a ripple on the orb from a tap in texture
    space (0..ORB_RES). Any previous orb ripple is
    replaced.
*/
void orb_tap(
    Orb *orb,
    float tex_x,
    float tex_y
);

#endif /* ORB_H */
