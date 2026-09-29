#ifndef CHROMA_H_GUARD
#define CHROMA_H_GUARD

#include <SDL_ttf.h>

#include "util.h"

#define CHROMA_W 300
#define CHROMA_H 56
#define CHROMA_R 28
#define CHROMA_BORDER 2

typedef struct ChromaProg {
  GLuint prog;
  GLint loc_resolution, loc_time, loc_borderWidth, loc_borderRadius,
      loc_speed, loc_base, loc_glow, loc_viewport;
} ChromaProg;

ChromaProg build_chroma_prog(void);
void chroma_draw(const ChromaProg *p, float t, Box b, float borderWidth,
                 float borderRadius, float speed, const float base[3],
                 const float glow[3], int winH);
GLuint chroma_pill_texture(TTF_Font *font);

#endif
