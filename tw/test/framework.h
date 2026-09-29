#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <SDL.h>
#include <SDL_ttf.h>

#include "canvas.h"
#include "ripple.h"

typedef struct Comp {
  const char *name;
  const char *cat;
  int gl_stage; /* 1 = component renders via its own GL, else through canvas */
  void *(*create)(void);
  void (*destroy)(void *s);
  void (*update)(void *s, Uint64 now, float dt);
  void (*tap)(void *s, int x, int y);
  void (*draw)(void *s);
} Comp;

extern Canvas *g_cv;
extern RippleProg g_rip;
extern TTF_Font *g_font;

extern const Comp *comps[];
extern int comps_count;

extern const Comp *comp_current;
extern void *comp_state;
extern int comp_index;

void framework_draw_name(Canvas *cv);
void framework_draw_name_gl(void);

#endif