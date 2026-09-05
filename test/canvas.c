#include "canvas.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ripple.h"

struct Canvas {
  int w, h;
  unsigned char *buf; /* row 0 = top of stage, R,G,B,A per pixel */
  GLuint tex;
};

static unsigned char u8(float v) {
  return (unsigned char)(v < 0.0f ? 0 : v > 255.0f ? 255 : v);
}

static void blend_at(Canvas *c, int x, int y, float r, float g, float b,
                     float a) {
  if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
  r /= 255.0f;
  g /= 255.0f;
  b /= 255.0f;
  int stride = c->w * 4;
  unsigned char *p = c->buf + (size_t)y * stride + (size_t)x * 4;
  if (a <= 0.0f) return;
  float da = p[3] / 255.0f;
  float oa = a + da * (1.0f - a);
  if (oa <= 0.0f) {
    p[0] = p[1] = p[2] = p[3] = 0;
    return;
  }
  float fr = (r * a + (p[0] / 255.0f) * da * (1.0f - a)) / oa;
  float fg = (g * a + (p[1] / 255.0f) * da * (1.0f - a)) / oa;
  float fb = (b * a + (p[2] / 255.0f) * da * (1.0f - a)) / oa;
  p[0] = u8(fr * 255.0f);
  p[1] = u8(fg * 255.0f);
  p[2] = u8(fb * 255.0f);
  p[3] = u8(oa * 255.0f);
}

Canvas *canvas_create(int w, int h) {
  Canvas *c = calloc(1, sizeof(Canvas));
  c->w = w;
  c->h = h;
  c->buf = malloc((size_t)w * h * 4);
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &c->tex);
  glBindTexture(GL_TEXTURE_2D, c->tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return c;
}

void canvas_destroy(Canvas *c) {
  if (!c) return;
  glDeleteTextures(1, &c->tex);
  free(c->buf);
  free(c);
}

int canvas_w(Canvas *c) { return c->w; }
int canvas_h(Canvas *c) { return c->h; }
const unsigned char *canvas_raw(Canvas *c) { return c->buf; }

static void clear_impl(Canvas *c, int r, int g, int b, int a) {
  int n = c->w * c->h;
  for (int i = 0; i < n; i++) {
    c->buf[i * 4 + 0] = (unsigned char)r;
    c->buf[i * 4 + 1] = (unsigned char)g;
    c->buf[i * 4 + 2] = (unsigned char)b;
    c->buf[i * 4 + 3] = (unsigned char)a;
  }
}

void canvas_clear(Canvas *c, int r, int g, int b) {
  clear_impl(c, r, g, b, 255);
}

void canvas_clear_rgba(Canvas *c, int r, int g, int b, int a) {
  clear_impl(c, r, g, b, a);
}

void canvas_fill_rect(Canvas *c, int x, int y, int w, int h, CColor col) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      blend_at(c, xx, yy, col.r, col.g, col.b, col.a / 255.0f);
}

static float rounded_dist(float x, float y, float cx, float cy, float hw,
                          float hh, float r) {
  float qx = fabsf(x - cx) - (hw - r);
  float qy = fabsf(y - cy) - (hh - r);
  float dx = fmaxf(qx, 0.0f);
  float dy = fmaxf(qy, 0.0f);
  return sqrtf(dx * dx + dy * dy) + fminf(fmaxf(qx, qy), 0.0f) - r;
}

void canvas_fill_round_rect(Canvas *c, int x, int y, int w, int h, int rad,
                            CColor col) {
  if (rad > w / 2) rad = w / 2;
  if (rad > h / 2) rad = h / 2;
  float cx = x + w / 2.0f, cy = y + h / 2.0f;
  float hw = w / 2.0f, hh = h / 2.0f;
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      float d = rounded_dist(xx + 0.5f, yy + 0.5f, cx, cy, hw, hh, rad);
      float cov = 0.5f - d;
      if (cov >= 1.0f)
        blend_at(c, xx, yy, col.r, col.g, col.b, col.a / 255.0f);
      else if (cov > 0.0f)
        blend_at(c, xx, yy, col.r, col.g, col.b,
                 (col.a / 255.0f) * cov);
    }
  }
}

void canvas_round_rect(Canvas *c, int x, int y, int w, int h, int rad,
                       CColor col) {
  canvas_fill_round_rect(c, x, y, w, h, rad, col);
}

void canvas_fill_circle(Canvas *c, int cx, int cy, int r, CColor col) {
  for (int yy = cy - r; yy <= cy + r; yy++) {
    for (int xx = cx - r; xx <= cx + r; xx++) {
      float d = sqrtf((float)(xx - cx) * (xx - cx) + (float)(yy - cy) * (yy - cy));
      float cov = r - d;
      if (cov >= 1.0f)
        blend_at(c, xx, yy, col.r, col.g, col.b, col.a / 255.0f);
      else if (cov > 0.0f)
        blend_at(c, xx, yy, col.r, col.g, col.b,
                 (col.a / 255.0f) * cov);
    }
  }
}

void canvas_fill_ellipse(Canvas *c, int cx, int cy, int rx, int ry,
                         CColor col) {
  for (int yy = cy - ry; yy <= cy + ry; yy++) {
    for (int xx = cx - rx; xx <= cx + rx; xx++) {
      float d = (float)(xx - cx) * (xx - cx) / (float)(rx * rx) +
                (float)(yy - cy) * (yy - cy) / (float)(ry * ry);
      if (d <= 1.0f)
        blend_at(c, xx, yy, col.r, col.g, col.b, col.a / 255.0f);
    }
  }
}

void canvas_circle(Canvas *c, int cx, int cy, int r, CColor col) {
  for (int yy = cy - r - 1; yy <= cy + r + 1; yy++) {
    for (int xx = cx - r - 1; xx <= cx + r + 1; xx++) {
      float d = sqrtf((float)(xx - cx) * (xx - cx) + (float)(yy - cy) * (yy - cy));
      float cov = fabsf(d - r);
      if (cov <= 0.9f)
        blend_at(c, xx, yy, col.r, col.g, col.b,
                 (col.a / 255.0f) * (1.0f - cov));
    }
  }
}

void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, int width,
                 CColor col) {
  float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
  float len = sqrtf(dx * dx + dy * dy);
  if (len <= 0.0001f) len = 1.0f;
  dx /= len;
  dy /= len;
  float half = width / 2.0f;
  int minx = x0 < x1 ? x0 : x1;
  int maxx = x0 > x1 ? x0 : x1;
  int miny = y0 < y1 ? y0 : y1;
  int maxy = y0 > y1 ? y0 : y1;
  for (int yy = miny - width; yy <= maxy + width; yy++) {
    for (int xx = minx - width; xx <= maxx + width; xx++) {
      float vx = (float)(xx - x0), vy = (float)(yy - y0);
      float t = vx * dx + vy * dy;
      float px = vx - dx * t, py = vy - dy * t;
      float d = sqrtf(px * px + py * py);
      if (t < 0.0f) {
        d = sqrtf(vx * vx + vy * vy);
      } else if (t > len) {
        d = sqrtf((float)(xx - x1) * (xx - x1) + (float)(yy - y1) * (yy - y1));
      }
      float cov = half - d;
      if (cov > 0.0f) {
        if (cov > 1.0f) cov = 1.0f;
        blend_at(c, xx, yy, col.r, col.g, col.b, col.a / 255.0f * cov);
      }
    }
  }
}

void canvas_poly(Canvas *c, const int *xs, const int *ys, int n, CColor col) {
  int miny = ys[0], maxy = ys[0];
  for (int i = 1; i < n; i++) {
    if (ys[i] < miny) miny = ys[i];
    if (ys[i] > maxy) maxy = ys[i];
  }
  for (int yy = miny; yy <= maxy; yy++) {
    float cross[32];
    int nm = 0;
    for (int i = 0; i < n; i++) {
      int j = (i + 1) % n;
      int yi = ys[i], yj = ys[j];
      if ((yi <= yy && yj > yy) || (yj <= yy && yi > yy)) {
        float t = (float)(yy - yi) / (float)(yj - yi);
        if (nm < 32) cross[nm++] = xs[i] + t * (xs[j] - xs[i]);
      }
    }
    for (int i = 0; i < nm; i++)
      for (int j = i + 1; j < nm; j++)
        if (cross[j] < cross[i]) {
          float t = cross[i];
          cross[i] = cross[j];
          cross[j] = t;
        }
    for (int i = 0; i + 1 < nm; i += 2) {
      int xa = (int)ceilf(cross[i]);
      int xb = (int)floorf(cross[i + 1]);
      for (int xx = xa; xx <= xb; xx++)
        blend_at(c, xx, yy, col.r, col.g, col.b, col.a / 255.0f);
    }
  }
}

void canvas_gradient_v(Canvas *c, int x, int y, int w, int h, CColor a,
                       CColor b) {
  for (int i = 0; i < h; i++) {
    float t = h > 1 ? (float)i / (float)(h - 1) : 0.0f;
    CColor col = {a.r + (int)((b.r - a.r) * t),
                  a.g + (int)((b.g - a.g) * t),
                  a.b + (int)((b.b - a.b) * t), 255};
    canvas_fill_rect(c, x, y + i, w, 1, col);
  }
}

void canvas_gradient_h(Canvas *c, int x, int y, int w, int h, CColor a,
                       CColor b) {
  for (int i = 0; i < w; i++) {
    float t = w > 1 ? (float)i / (float)(w - 1) : 0.0f;
    CColor col = {a.r + (int)((b.r - a.r) * t),
                  a.g + (int)((b.g - a.g) * t),
                  a.b + (int)((b.b - a.b) * t), 255};
    canvas_fill_rect(c, x + i, y, 1, h, col);
  }
}

void canvas_gradient_box(Canvas *c, int x, int y, int w, int h, CColor tl,
                         CColor tr, CColor bl, CColor br) {
  for (int yy = 0; yy < h; yy++) {
    float ty = h > 1 ? (float)yy / (float)(h - 1) : 0.0f;
    for (int xx = 0; xx < w; xx++) {
      float tx = w > 1 ? (float)xx / (float)(w - 1) : 0.0f;
      CColor top = {tl.r + (int)((tr.r - tl.r) * tx),
                    tl.g + (int)((tr.g - tl.g) * tx),
                    tl.b + (int)((tr.b - tl.b) * tx), 255};
      CColor bot = {bl.r + (int)((br.r - bl.r) * tx),
                    bl.g + (int)((br.g - bl.g) * tx),
                    bl.b + (int)((br.b - bl.b) * tx), 255};
      CColor col = {top.r + (int)((bot.r - top.r) * ty),
                    top.g + (int)((bot.g - top.g) * ty),
                    top.b + (int)((bot.b - top.b) * ty), 255};
      blend_at(c, x + xx, y + yy, col.r, col.g, col.b, 1.0f);
    }
  }
}

GLuint canvas_texture(Canvas *c) { return c->tex; }

int canvas_text_w(TTF_Font *font, const char *s, int *w, int *h) {
  if (!font) return 0;
  TTF_SizeUTF8(font, s, w, h);
  return *w;
}

static int blit_text_to(Canvas *c, TTF_Font *font, const char *s, int x,
                        int y, CColor col, int center) {
  if (!font) return 0;
  SDL_Color sc = {col.r, col.g, col.b, col.a};
  SDL_Surface *sur = TTF_RenderUTF8_Blended(font, s, sc);
  if (!sur) return 0;
  if (center) x -= sur->w / 2;
  SDL_PixelFormat *fmt = sur->format;
  for (int sy = 0; sy < sur->h; sy++) {
    for (int sx = 0; sx < sur->w; sx++) {
      Uint32 px = *(Uint32 *)((Uint8 *)sur->pixels + (size_t)sy * sur->pitch +
                              (size_t)sx * 4);
      Uint8 r, g, b, a;
      SDL_GetRGBA(px, fmt, &r, &g, &b, &a);
      blend_at(c, x + sx, y + sy, r, g, b, a / 255.0f);
    }
  }
  int th = sur->h;
  SDL_FreeSurface(sur);
  return th;
}

int canvas_text(Canvas *c, TTF_Font *font, const char *s, int x, int y,
                CColor col) {
  return blit_text_to(c, font, s, x, y, col, 0);
}

int canvas_text_center(Canvas *c, TTF_Font *font, const char *s, int cx,
                       int y, CColor col) {
  return blit_text_to(c, font, s, cx, y, col, 1);
}

void canvas_present(Canvas *c, const struct RippleProg *rip) {
  const char *dump = SDL_getenv("REACTICX_CANVAS");
  if (dump && dump[0]) {
    FILE *f = fopen(dump, "wb");
    if (f) {
      fprintf(f, "P6\n%d %d\n255\n", c->w, c->h);
      for (int y = 0; y < c->h; y++) {
        const unsigned char *row = c->buf + (size_t)y * c->w * 4;
        for (int x = 0; x < c->w; x++)
          fwrite(row + x * 4, 3, 1, f);
      }
      fclose(f);
    }
  }
  int stride = c->w * 4;
  unsigned char *staging = malloc((size_t)c->h * stride);
  for (int i = 0; i < c->h; i++) {
    memcpy(staging + (size_t)i * stride,
           c->buf + (size_t)(c->h - 1 - i) * stride, (size_t)stride);
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, c->tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, c->w, c->h, GL_RGBA,
                  GL_UNSIGNED_BYTE, staging);
  free(staging);
  Box full = {0, 0, c->w, c->h};
  ripple_draw(rip, c->tex, full, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, c->h);
}