#include "util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

Box box_card(void) { return (Box){CARD_X, CARD_Y, CARD_W, CARD_H}; }
Box box_btn(void) { return (Box){BTN_X, BTN_Y, BTN_W, BTN_H}; }

int inside(const Box *b, int x, int y) {
  return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h;
}

static Uint8 u8(float v) {
  return (Uint8)(v < 0.0f ? 0 : v > 255.0f ? 255 : v);
}

void put_rgba(SDL_Surface *s, int x, int y, int r, int g, int b, int a) {
  if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
  Uint32 val = SDL_MapRGBA(s->format, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
  *(Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch + (size_t)x * 4) = val;
}

SDL_Surface *surface_create(int w, int h) {
  return SDL_CreateRGBSurfaceWithFormat(0, w, h, 32,
                                        SDL_PIXELFORMAT_RGBA8888);
}

void blend_pixel(SDL_Surface *s, int x, int y, int r, int g, int b, float a) {
  if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
  Uint32 dpx = *(Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch +
                           (size_t)x * 4);
  Uint8 dr, dg, db, da;
  SDL_GetRGBA(dpx, s->format, &dr, &dg, &db, &da);
  float sa = a;
  float de = da / 255.0f;
  float oa = sa + de * (1.0f - sa);
  if (oa <= 0.0f) {
    put_rgba(s, x, y, 0, 0, 0, 0);
    return;
  }
  float fr = (r * sa + dr / 255.0f * de * (1.0f - sa)) / oa;
  float fg = (g * sa + dg / 255.0f * de * (1.0f - sa)) / oa;
  float fb = (b * sa + db / 255.0f * de * (1.0f - sa)) / oa;
  put_rgba(s, x, y, u8(fr * 255.0f), u8(fg * 255.0f), u8(fb * 255.0f),
           u8(oa * 255.0f));
}

void fill_gradient(SDL_Surface *s, int x0, int y0, int w, int h, float r0,
                   float g0, float b0, float r1, float g1, float b1) {
  for (int y = 0; y < h; y++) {
    float t = h > 1 ? (float)y / (float)(h - 1) : 0.0f;
    int rr = (int)(r0 + (r1 - r0) * t);
    int gg = (int)(g0 + (g1 - g0) * t);
    int bb = (int)(b0 + (b1 - b0) * t);
    for (int x = 0; x < w; x++) {
      put_rgba(s, x0 + x, y0 + y, rr, gg, bb, 255);
    }
  }
}

static float rounded_cov(float x, float y, float cx, float cy, float hw,
                         float hh, float r) {
  float qx = fabsf(x - cx) - (hw - r);
  float qy = fabsf(y - cy) - (hh - r);
  float dx = fmaxf(qx, 0.0f);
  float dy = fmaxf(qy, 0.0f);
  float d = sqrtf(dx * dx + dy * dy) + fminf(fmaxf(qx, qy), 0.0f) - r;
  float cov = 1.0f - (d + 0.75f);
  if (cov < 0.0f) cov = 0.0f;
  if (cov > 1.0f) cov = 1.0f;
  return cov;
}

void scale_alpha(SDL_Surface *s, int x0, int y0, int w, int h, float cx,
                 float cy, float hw, float hh, float r) {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float cov = rounded_cov((float)(x0 + x), (float)(y0 + y), cx, cy, hw, hh,
                              r);
      if (cov >= 1.0f) continue;
      Uint32 px = *(Uint32 *)((Uint8 *)s->pixels +
                              (size_t)(y0 + y) * s->pitch +
                              (size_t)(x0 + x) * 4);
      Uint8 r8, g8, b8, a8;
      SDL_GetRGBA(px, s->format, &r8, &g8, &b8, &a8);
      put_rgba(s, x0 + x, y0 + y, r8, g8, b8, u8(a8 * cov));
    }
  }
}

void fill_rounded(SDL_Surface *s, float cx, float cy, float hw, float hh,
                  float r, int cr, int cg, int cb, float ca) {
  int x0 = (int)(cx - hw);
  int y0 = (int)(cy - hh);
  int w = (int)(hw * 2.0f + 1.0f);
  int h = (int)(hh * 2.0f + 1.0f);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float cov = rounded_cov((float)(x0 + x), (float)(y0 + y), cx, cy, hw, hh,
                              r);
      if (cov <= 0.0f) continue;
      blend_pixel(s, x0 + x, y0 + y, cr, cg, cb, ca * cov);
    }
  }
}

SDL_Surface *text_surface(TTF_Font *font, const char *text, SDL_Color color) {
  return TTF_RenderUTF8_Blended(font, text, color);
}

void blit_src_over(SDL_Surface *dst, SDL_Surface *src, int dx, int dy) {
  for (int y = 0; y < src->h; y++) {
    for (int x = 0; x < src->w; x++) {
      Uint32 spx = *(Uint32 *)((Uint8 *)src->pixels + (size_t)y * src->pitch +
                               (size_t)x * 4);
      Uint8 r, g, b, a;
      SDL_GetRGBA(spx, src->format, &r, &g, &b, &a);
      if (a == 0) continue;
      blend_pixel(dst, dx + x, dy + y, r, g, b, a / 255.0f);
    }
  }
}

GLuint make_texture_from_surface(SDL_Surface *in) {
  int w = in->w, h = in->h;
  int rowbytes = w * 4;
  unsigned char *data = malloc((size_t)h * (size_t)rowbytes);
  SDL_PixelFormat *fmt = in->format;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      Uint32 px = *(Uint32 *)((Uint8 *)in->pixels + (size_t)y * in->pitch +
                              (size_t)x * 4);
      Uint8 r, g, b, a;
      SDL_GetRGBA(px, fmt, &r, &g, &b, &a);
      unsigned char *d = data + (size_t)(h - 1 - y) * rowbytes + (size_t)x * 4;
      d[0] = r;
      d[1] = g;
      d[2] = b;
      d[3] = a;
    }
  }
  GLuint tex;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(data);
  return tex;
}

void debug_surface(const char *path, SDL_Surface *s) {
  if (!SDL_getenv("REACTICX_DBG")) return;
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
  for (int y = 0; y < s->h; y++) {
    for (int x = 0; x < s->w; x++) {
      Uint32 px = *(Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch +
                              (size_t)x * 4);
      Uint8 r, g, b, a;
      SDL_GetRGBA(px, s->format, &r, &g, &b, &a);
      fputc(r, f);
      fputc(g, f);
      fputc(b, f);
    }
  }
  fclose(f);
}

int capture_ppm(const char *path, int w, int h) {
  unsigned char *px = malloc((size_t)w * h * 4);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
  FILE *f = fopen(path, "wb");
  if (!f) {
    free(px);
    return 0;
  }
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      fputc(px[(y * w + x) * 4 + 0], f);
      fputc(px[(y * w + x) * 4 + 1], f);
      fputc(px[(y * w + x) * 4 + 2], f);
    }
  }
  fclose(f);
  free(px);
  return 1;
}