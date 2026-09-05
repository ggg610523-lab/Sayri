#include "button.h"

GLuint button_texture(TTF_Font *font) {
  int s = TEX_SCALE;
  SDL_Surface *btn = surface_create(BTN_W * s, BTN_H * s);
  fill_rounded(btn, (float)(BTN_W * s) / 2.0f, (float)(BTN_H * s) / 2.0f,
               (float)(BTN_W * s) / 2.0f, (float)(BTN_H * s) / 2.0f,
               (float)(BTN_R * s), 255, 255, 255, 0.08f);
  TTF_SetFontSize(font, 15 * s);
  SDL_Surface *t = text_surface(font, "Reacticx is awesome!",
                                (SDL_Color){255, 255, 255, 255});
  blit_src_over(btn, t, (BTN_W * s - t->w) / 2, (BTN_H * s - t->h) / 2);
  SDL_FreeSurface(t);
  GLuint tex = make_texture_from_surface(btn);
  debug_surface("/tmp/btn.ppm", btn);
  SDL_FreeSurface(btn);
  return tex;
}