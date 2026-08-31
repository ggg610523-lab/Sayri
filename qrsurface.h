#ifndef SAYRI_QRSURFACE_H
#define SAYRI_QRSURFACE_H

#include <SDL2/SDL.h>
#include "ui.h"

/*
    Minimal QR surface renderer built on the QR Code
    generator (Nayuki, MIT).

    Set the payload once; the module grid is only re-encoded
    when the text actually changes (cheap because the URI is
    tiny). Then draw it as a square of filled modules.

    Desktop-only dependency (qrcodegen.c). The Android port
    only *scans* QRs and does not link these files.
*/

/*
    (Re)encode text if it differs from the last payload.
    Returns true when a valid grid is available.
*/
bool qrsurface_set(const char *text);

/*
    Returns the current QR size in modules (21..177), or 0.
*/
int qrsurface_size(void);

/*
    Draw the QR code, scaled to `px` physical pixels, with its
    top-left at (x, y). Draws dark modules on a light background,
    including the standard quiet-zone border. Uses only simple
    filled rects so it stays crisp at any scale.
*/
void qrsurface_draw(
    SDL_Renderer *renderer,
    int x,
    int y,
    int px,
    UIColor dark,
    UIColor light);

#endif /* SAYRI_QRSURFACE_H */