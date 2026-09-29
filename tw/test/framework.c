#include <string.h>

#include "framework.h"
#include "util.h"

Canvas *g_cv;
RippleProg g_rip;
TTF_Font *g_font;

const Comp *comp_current = NULL;
void *comp_state = NULL;
int comp_index = 0;

#define XX(n) extern const Comp comp_##n;
#include "comps_registry.inc"
#undef XX

const Comp *comps[] = {
#define XX(n) &comp_##n,
#include "comps_registry.inc"
#undef XX
};
int comps_count = sizeof(comps) / sizeof(comps[0]);

void framework_draw_name(Canvas *cv) {
  if (!comp_current) return;
  char label[160];
  snprintf(label, sizeof(label), "%s / %s  (%d/%d)", comp_current->cat,
           comp_current->name, comp_index + 1, comps_count);
  if (g_font && TTF_SetFontSize(g_font, 13) == 0) {
    int tw, th;
    TTF_SizeUTF8(g_font, label, &tw, &th);
    CColor pill = {20, 20, 28, 180};
    canvas_fill_round_rect(cv, 12, 12, tw + 24, th + 12, 9, pill);
    CColor tc = {255, 255, 255, 230};
    canvas_text(cv, g_font, label, 24, 18, tc);
  }
}

void framework_draw_name_gl(void) {
  if (!comp_current) return;
  char label[160];
  snprintf(label, sizeof(label), "%s / %s  (%d/%d)", comp_current->cat,
           comp_current->name, comp_index + 1, comps_count);
  if (!g_font) return;
  TTF_SetFontSize(g_font, 13);
  SDL_Surface *sur = TTF_RenderUTF8_Blended(
      g_font, label, (SDL_Color){255, 255, 255, 230});
  if (!sur) return;
  GLuint tex = make_texture_from_surface(sur);
  int tw = sur->w, th = sur->h;
  SDL_FreeSurface(sur);
  Box pill = {12, 12, tw + 24, th + 12};
  /* pill background: dark rounded rect drawn as canvas is not possible here;
     instead draw a simple translucent black box via a tiny solid texture. */
  static GLuint bgtex = 0;
  if (!bgtex) {
    SDL_Surface *bs = surface_create(1, 1);
    put_rgba(bs, 0, 0, 20, 20, 28, 180);
    bgtex = make_texture_from_surface(bs);
    SDL_FreeSurface(bs);
  }
  ripple_draw(&g_rip, bgtex, pill, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 720);
  Box tb = {24, 18, tw, th};
  ripple_draw(&g_rip, tex, tb, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 720);
  glDeleteTextures(1, &tex);
}