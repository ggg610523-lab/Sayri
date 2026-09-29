#ifndef RIPPLE_H
#define RIPPLE_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define RIPPLE_MAX 8
#define RIPPLE_DURATION 1.2f

/*
    Tag for ripples that must be matched by an
    exact widget rect (popups, panels, buttons)
    instead of a moving chat message.
*/
#define RIPPLE_TAG_REGION (-1)

typedef struct {
    bool active;
    int tag;
    SDL_Rect region;
    float corner;
    float ox, oy;
    float amplitude;
    Uint64 startMs;

    /*
        Pristine snapshot of the widget taken on the
        ripple's first frame; it is the warp source
        for every later frame, mirroring the static
        card texture in test/ripple.c. Without this,
        each read-back re-warped the previous frame's
        output and the ripple compounded.
    */
    Uint32 *snap;
    int snap_w, snap_h;
} UIRipple;

/*
    Spawn a ripple at (mx, my) centered over `region`.

    tag >= 0: the ripple is drawn whenever the caller
    draws with the same tag (chat bubbles, which move
    with scroll).

    tag == RIPPLE_TAG_REGION: the ripple is drawn only
    when the caller passes an identical rect (static
    widgets like popups and buttons).
*/
void ripple_tap(
    SDL_Rect region,
    float corner,
    int tag,
    int mx, int my);

/*
    Warp and redraw the first active ripple matching
    region/tag over the given rect, or do nothing.
    Reclaims expired ripples.
*/
void ripple_draw(
    SDL_Renderer *renderer,
    SDL_Rect region,
    int tag);

/*
    Compute one ripple-warped pixel for a translucent
    source (e.g. the orb texture). Coordinates are in
    the source's own pixel space; alpha is preserved,
    so the source's natural shape clips the wave.
*/
Uint32 ripple_warp_tex(
    const Uint32 *src, int w, int h,
    float x, float y,
    float ox, float oy,
    float t,
    float amplitude);

/* Deactivate all ripples and drop their snapshots. */
void ripple_clear(void);

/* Free snapshots and the warp working buffers. */
void ripple_free(void);

#endif /* RIPPLE_H */