#include "atext.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHARS 64
#define CYCLE_MS 2000
#define DELAY_MS 40
#define ENTER_MS 350
#define EXIT_MS 250
#define ENTER_TY 25.0f
#define ENTER_SCALE 0.5f
#define EXIT_TY -20.0f
#define EXIT_SCALE 0.8f
#define EXIT_ROT_DEG -15.0f
#define SPRING_M 0.6f
#define SPRING_K 120.0f
#define SPRING_C 14.0f

typedef struct CharGL {
  GLuint tex;
  int w, h;
  float cx, cy;
} CharGL;

struct AText {
  TTF_Font *font;
  const char **phrases;
  int nPhrases;
  int baseX, baseY;
  int phraseIdx;
  Uint64 enterStart;
  CharGL cur[MAX_CHARS];
  int curN;
  CharGL prev[MAX_CHARS];
  int prevN;
  Uint64 exitStart;
};

static const char *QUAD_VS =
    "#version 150\n"
    "uniform vec4 u_rect;\n"
    "uniform vec2 u_center;\n"
    "uniform vec2 u_scale;\n"
    "uniform float u_rot;\n"
    "uniform vec2 u_win;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "  vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));\n"
    "  vec2 p = u_rect.xy + c * u_rect.zw;\n"
    "  v_uv = vec2(c.x, 1.0 - c.y);\n"
    "  vec2 d = p - u_center;\n"
    "  float cr = cos(u_rot), sr = sin(u_rot);\n"
    "  vec2 r = vec2(cr * d.x - sr * d.y, sr * d.x + cr * d.y);\n"
    "  p = u_center + r * u_scale;\n"
    "  vec2 ndc = vec2(p.x / u_win.x, 1.0 - p.y / u_win.y) * 2.0 - 1.0;\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";

static const char *QUAD_FS =
    "#version 150\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_alpha;\n"
    "in vec2 v_uv;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  vec4 c = texture(u_tex, v_uv);\n"
    "  fragColor = vec4(c.rgb, c.a * u_alpha);\n"
    "}\n";

ATextProg build_atext_prog(void) {
  ATextProg p;
  p.prog = glcore_make_program(QUAD_VS, QUAD_FS);
  p.loc_rect = glGetUniformLocation(p.prog, "u_rect");
  p.loc_center = glGetUniformLocation(p.prog, "u_center");
  p.loc_scale = glGetUniformLocation(p.prog, "u_scale");
  p.loc_rot = glGetUniformLocation(p.prog, "u_rot");
  p.loc_win = glGetUniformLocation(p.prog, "u_win");
  p.loc_alpha = glGetUniformLocation(p.prog, "u_alpha");
  p.loc_tex = glGetUniformLocation(p.prog, "u_tex");
  SDL_Log("atext locs rect=%d ctr=%d sc=%d rot=%d win=%d a=%d tex=%d",
          p.loc_rect, p.loc_center, p.loc_scale, p.loc_rot, p.loc_win,
          p.loc_alpha, p.loc_tex);
  return p;
}

static float ease_out(float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }
static float ease_in(float t) { return t * t * t; }

static float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

static float damped_spring(float x0, float t) {
  float w = sqrtf(SPRING_K / SPRING_M);
  float z = SPRING_C / (2.0f * sqrtf(SPRING_K * SPRING_M));
  float wd = w * sqrtf(1.0f - z * z);
  float env = expf(-z * w * t);
  float osc = cosf(wd * t) + (z / sqrtf(1.0f - z * z)) * sinf(wd * t);
  return x0 * env * osc;
}

static void free_chars(CharGL *c, int n) {
  for (int i = 0; i < n; i++) {
    if (c[i].tex) {
      GLuint t = c[i].tex;
      glDeleteTextures(1, &t);
    }
  }
}

static void layout_phrase(TTF_Font *font, const char *text, CharGL *out,
                          int *n, int centerX, int baseY, SDL_Color color);

AText *atext_create(TTF_Font *font, const char **phrases, int n, int x, int y) {
  AText *a = calloc(1, sizeof(*a));
  a->font = font;
  a->phrases = phrases;
  a->nPhrases = n;
  a->baseX = x;
  a->baseY = y;
  a->phraseIdx = 0;
  SDL_Color col = {235, 240, 255, 255};
  layout_phrase(font, phrases[0], a->cur, &a->curN, x, y, col);
  a->enterStart = SDL_GetTicks64();
  (void)CYCLE_MS;
  return a;
}

void atext_destroy(AText *a) {
  if (!a) return;
  free_chars(a->cur, a->curN);
  free_chars(a->prev, a->prevN);
  free(a);
}

static int utf8_len(const char *p) {
  unsigned char c = (unsigned char)*p;
  if (c >= 0xF0) return 4;
  if (c >= 0xE0) return 3;
  if (c >= 0xC0) return 2;
  return 1;
}

static void layout_phrase(TTF_Font *font, const char *text, CharGL *out,
                          int *n, int centerX, int baseY, SDL_Color color) {
  CharGL ch[MAX_CHARS];
  memset(ch, 0, sizeof(ch));
  int count = 0;

  const char *p = text;
  while (*p && count < MAX_CHARS) {
    int len = utf8_len(p);
    char buf[8];
    memcpy(buf, p, (size_t)len);
    buf[len] = 0;

    SDL_Color sc = {color.r, color.g, color.b, color.a};
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, buf, sc);
    if (s) {
      ch[count].tex = make_texture_from_surface(s);
      ch[count].w = s->w;
      ch[count].h = s->h;
      SDL_FreeSurface(s);
    } else {
      ch[count].w = 0;
      ch[count].h = 0;
    }
    count++;
    p += len;
  }

  /* measure the whole phrase width, kerning-aware */
  int lineW = 0, dummy = 0;
  TTF_SizeUTF8(font, text, &lineW, &dummy);

  /* place each glyph using its accumulated advance */
  int accum = 0;
  const char *q = text;
  for (int i = 0; i < count && *q; i++) {
    int len = utf8_len(q);
    char pre[160];
    size_t pl = (size_t)(q - text);
    if (pl + (size_t)len < sizeof(pre)) {
      memcpy(pre, text, pl + (size_t)len);
      pre[pl + (size_t)len] = 0;
      int pw = 0;
      TTF_SizeUTF8(font, pre, &pw, &dummy);
      accum = pw;
    }
    float cx = (float)centerX - (float)lineW / 2.0f + (float)accum -
               (float)ch[i].w / 2.0f;
    ch[i].cx = cx;
    ch[i].cy = (float)baseY;
    q += len;
  }

  *n = count;
  memcpy(out, ch, sizeof(CharGL) * (size_t)count);
}

void atext_update(AText *a, Uint64 now) {
  if (now - a->enterStart > CYCLE_MS) {
    free_chars(a->prev, a->prevN);
    a->prevN = 0;
    memcpy(a->prev, a->cur, sizeof(CharGL) * (size_t)a->curN);
    a->prevN = a->curN;
    memset(a->cur, 0, sizeof(a->cur));
    a->curN = 0;

    a->phraseIdx = (a->phraseIdx + 1) % a->nPhrases;
    SDL_Color col = {235, 240, 255, 255};
    layout_phrase(a->font, a->phrases[a->phraseIdx], a->cur, &a->curN,
                  a->baseX, a->baseY, col);
    a->enterStart = now;
    a->exitStart = now;
  }
}

static void draw_chars(const AText *a, const CharGL *ch, int n, Uint64 now,
                       int entering, const ATextProg *p, int winW, int winH) {
  glUseProgram(p->prog);
  glUniform2f(p->loc_win, (float)winW, (float)winH);
  glUniform1i(p->loc_tex, 0);
  for (int i = 0; i < n; i++) {
    const CharGL *c = &ch[i];
    if (!c->tex) continue;

    float t;
    if (entering) {
      float delay = (float)(i * DELAY_MS);
      t = (float)(now - a->enterStart) - delay;
      float dur = (float)ENTER_MS;
      float tt = clamp01(t / dur);
      float eased = ease_out(tt);
      float ty = damped_spring(ENTER_TY, t / 1000.0f);
      if (t <= 0.0f) ty = ENTER_TY;
      float sc = 1.0f + damped_spring(ENTER_SCALE - 1.0f, t / 1000.0f);
      if (t <= 0.0f) sc = ENTER_SCALE;

      float op = (t <= 0.0f) ? 0.0f : eased;

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, c->tex);
      glUniform4f(p->loc_rect, c->cx - c->w / 2.0f, c->cy - c->h / 2.0f,
                  (float)c->w, (float)c->h);
      glUniform2f(p->loc_center, c->cx, c->cy + ty);
      glUniform2f(p->loc_scale, sc, sc);
      glUniform1f(p->loc_rot, 0.0f);
      glUniform1f(p->loc_alpha, op);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    } else {
      t = (float)(now - a->exitStart);
      float delay = (float)(i * (DELAY_MS / 2));
      float tt = clamp01((t - delay) / (float)EXIT_MS);
      float eased = (t - delay) <= 0.0f ? 0.0f : ease_in(tt);
      float ty = EXIT_TY * eased;
      float sc = 1.0f + (EXIT_SCALE - 1.0f) * eased;
      float rot = EXIT_ROT_DEG * (float)(3.14159265359 / 180.0) * eased;
      float op = 1.0f - eased;
      if ((t - delay) < 0.0f) op = 0.0f;

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, c->tex);
      glUniform4f(p->loc_rect, c->cx - c->w / 2.0f, c->cy - c->h / 2.0f,
                  (float)c->w, (float)c->h);
      glUniform2f(p->loc_center, c->cx, c->cy + ty);
      glUniform2f(p->loc_scale, sc, sc);
      glUniform1f(p->loc_rot, rot);
      glUniform1f(p->loc_alpha, op);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
  }
}

void atext_draw(const AText *a, const ATextProg *p, int winW, int winH) {
  glViewport(0, 0, winW, winH);
  Uint64 now = SDL_GetTicks64();
  draw_chars(a, a->prev, a->prevN, now, 0, p, winW, winH);
  draw_chars(a, a->cur, a->curN, now, 1, p, winW, winH);
}