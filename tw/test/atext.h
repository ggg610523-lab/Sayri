#ifndef ATEXT_H_GUARD
#define ATEXT_H_GUARD

#include <SDL_ttf.h>

#include "util.h"

typedef struct ATextProg {
  GLuint prog;
  GLint loc_rect, loc_center, loc_scale, loc_rot, loc_win, loc_alpha, loc_tex;
} ATextProg;

typedef struct AText AText;

ATextProg build_atext_prog(void);
AText *atext_create(TTF_Font *font, const char **phrases, int n, int x, int y);
void atext_destroy(AText *a);
void atext_update(AText *a, Uint64 now);
void atext_draw(const AText *a, const ATextProg *p, int winW, int winH);

#endif