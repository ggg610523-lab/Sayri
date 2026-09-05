#ifndef CANVAS_H
#define CANVAS_H

#include <SDL_ttf.h>

#include "glcore.h"

typedef struct Canvas Canvas;
struct RippleProg;

typedef struct {
  int r, g, b, a;
} CColor;

Canvas *canvas_create(int w, int h);
void canvas_destroy(Canvas *c);
int canvas_w(Canvas *c);
int canvas_h(Canvas *c);

void canvas_clear(Canvas *c, int r, int g, int b);
void canvas_clear_rgba(Canvas *c, int r, int g, int b, int a);
void canvas_fill_rect(Canvas *c, int x, int y, int w, int h, CColor col);
void canvas_fill_round_rect(Canvas *c, int x, int y, int w, int h, int rad,
                            CColor col);
void canvas_round_rect(Canvas *c, int x, int y, int w, int h, int rad,
                       CColor col);
void canvas_fill_circle(Canvas *c, int cx, int cy, int r, CColor col);
void canvas_fill_ellipse(Canvas *c, int cx, int cy, int rx, int ry,
                         CColor col);
void canvas_circle(Canvas *c, int cx, int cy, int r, CColor col);
void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, int width,
                 CColor col);
void canvas_poly(Canvas *c, const int *xs, const int *ys, int n, CColor col);
void canvas_gradient_v(Canvas *c, int x, int y, int w, int h, CColor a,
                       CColor b);
void canvas_gradient_h(Canvas *c, int x, int y, int w, int h, CColor a,
                       CColor b);
void canvas_gradient_box(Canvas *c, int x, int y, int w, int h, CColor tl,
                         CColor tr, CColor bl, CColor br);

GLuint canvas_texture(Canvas *c);
/* draw helper text into canvas at x,y (top-left anchor); returns text height */
int canvas_text(Canvas *c, TTF_Font *font, const char *s, int x, int y,
                CColor col);
int canvas_text_center(Canvas *c, TTF_Font *font, const char *s, int cx,
                       int y, CColor col);
int canvas_text_w(TTF_Font *font, const char *s, int *w, int *h);
void canvas_present(Canvas *c, const struct RippleProg *rip);

const unsigned char *canvas_raw(Canvas *c);

#endif
