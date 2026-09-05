#ifndef UTIL_H
#define UTIL_H

#include <SDL.h>
#include <SDL_ttf.h>

#include "glcore.h"

#define WIN_W 960
#define WIN_H 720

#define TEX_SCALE 4

#define CARD_X 510
#define CARD_Y 60
#define CARD_W 330
#define CARD_H 420
#define CARD_R 28

#define BTN_X 115
#define BTN_Y 560
#define BTN_W 220
#define BTN_H 46
#define BTN_R 23

typedef struct Box {
  int x, y, w, h;
} Box;

Box box_card(void);
Box box_btn(void);
int inside(const Box *b, int x, int y);

SDL_Surface *surface_create(int w, int h);
void put_rgba(SDL_Surface *s, int x, int y, int r, int g, int b, int a);
void blend_pixel(SDL_Surface *s, int x, int y, int r, int g, int b, float a);
void fill_gradient(SDL_Surface *s, int x0, int y0, int w, int h, float r0,
                   float g0, float b0, float r1, float g1, float b1);
void scale_alpha(SDL_Surface *s, int x0, int y0, int w, int h, float cx,
                 float cy, float hw, float hh, float r);
void fill_rounded(SDL_Surface *s, float cx, float cy, float hw, float hh,
                  float r, int cr, int cg, int cb, float ca);

SDL_Surface *text_surface(TTF_Font *font, const char *text, SDL_Color color);
void blit_src_over(SDL_Surface *dst, SDL_Surface *src, int dx, int dy);

GLuint make_texture_from_surface(SDL_Surface *in);
void debug_surface(const char *path, SDL_Surface *s);
int capture_ppm(const char *path, int w, int h);

#endif