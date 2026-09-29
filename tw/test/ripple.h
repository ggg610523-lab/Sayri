#ifndef RIPPLE_H
#define RIPPLE_H

#include "util.h"

typedef struct Ripple {
  float originX, originY;
  Uint64 startMs;
  float amplitude, frequency, decay, speed, duration;
  float corner;
} Ripple;

typedef struct RippleProg {
  GLuint prog;
  GLint loc_viewport, loc_origin, loc_res, loc_time, loc_amplitude,
      loc_frequency, loc_decay, loc_speed, loc_corner, loc_texture;
} RippleProg;

RippleProg build_ripple_prog(void);
float ripple_time(const Ripple *r, Uint64 now);
void ripple_tap(const Box *b, Ripple *r, int mx, int my);

GLuint ripple_card_texture(TTF_Font *font);
void ripple_draw(const RippleProg *p, GLuint tex, Box b, float time, float ox,
                 float oy, float amp, float corner, int winH);

#endif