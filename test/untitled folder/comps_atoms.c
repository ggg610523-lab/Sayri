/* atoms/divider, atoms/pressable, atoms/stacks (C port) */
#include <math.h>
#include <stdlib.h>

#include "framework.h"
#include "util.h"


/* ---------------------------------------------------------------- */
/* atoms/divider                                                     */
/* ---------------------------------------------------------------- */
typedef struct { int dummy; } Divider;

static void *div_create(void) { return calloc(1, sizeof(Divider)); }

static void div_draw(void *p) {
  (void)p;
  int w = canvas_w(g_cv);
  canvas_clear(g_cv, 13, 13, 18);

  CColor dim = {235, 235, 245, 210};
  canvas_text_center(g_cv, g_font, "Divider", w / 2, 60, dim);

  /* horizontal sample: three chips with pill dividers */
  TTF_SetFontSize(g_font, 14);
  const char *chips[] = {"Home", "Library", "Profile"};
  int cw = 110, gap = 24, total = 3 * cw + 2 * gap;
  int x0 = (w - total) / 2, y = 160;
  for (int i = 0; i < 3; i++) {
    CColor chipbg = {34, 34, 46, 255};
    canvas_fill_round_rect(g_cv, x0 + i * (cw + gap), y, cw, 44, 12, chipbg);
    CColor chipbor = {66, 66, 84, 255};
    canvas_round_rect(g_cv, x0 + i * (cw + gap), y, cw, 44, 12, chipbor);
    CColor tc = {235, 235, 245, 255};
    canvas_text_center(g_cv, g_font, chips[i], x0 + i * (cw + gap) + cw / 2,
                       y + 14, tc);
    if (i > 0) {
      CColor pill = {125, 125, 125, 255};
      int px = x0 + i * (cw + gap) - gap / 2;
      canvas_fill_round_rect(g_cv, px - 2, y + 12, 4, 20, 2, pill);
    }
  }

  /* vertical sample */
  const char *vchips[] = {"Sync", "Status", "Battery"};
  int vw = 160, vy0 = 300, vh = 44, vgap = 24;
  int vx = (w - vw) / 2;
  for (int i = 0; i < 3; i++) {
    CColor chipbg = {34, 34, 46, 255};
    canvas_fill_round_rect(g_cv, vx, vy0 + i * (vh + vgap), vw, vh, 12, chipbg);
    CColor chipbor = {66, 66, 84, 255};
    canvas_round_rect(g_cv, vx, vy0 + i * (vh + vgap), vw, vh, 12, chipbor);
    CColor tc = {235, 235, 245, 255};
    canvas_text_center(g_cv, g_font, vchips[i], vx + vw / 2,
                       vy0 + i * (vh + vgap) + 14, tc);
    if (i > 0) {
      CColor pill = {125, 125, 125, 255};
      int py = vy0 + i * (vh + vgap) - vgap / 2;
      canvas_fill_round_rect(g_cv, vx + vw / 2 - 10, py - 2, 20, 4, 2, pill);
    }
  }

  CColor sub = {150, 150, 165, 180};
  TTF_SetFontSize(g_font, 12);
  canvas_text_center(g_cv, g_font, "horizontal  +  vertical", w / 2, 560, sub);
}

const Comp comp_divider = {"divider", "atoms", 0, div_create, free, NULL, NULL,
                           div_draw};

/* ---------------------------------------------------------------- */
/* atoms/pressable                                                   */
/* ---------------------------------------------------------------- */
typedef struct {
  Uint64 downAt;
  float scale;
  int down;
} Pressable;

static void *press_create(void) {
  Pressable *s = calloc(1, sizeof(*s));
  s->scale = 1.0f;
  return s;
}

static void press_update(void *p, Uint64 now, float dt) {
  Pressable *s = p;
  float target = s->down ? 0.96f : 1.0f;
  float k = s->down ? 30.0f : 22.0f;
  (void)now;
  (void)dt;
  s->scale += (target - s->scale) * (1.0f - expf(-k * dt));
  if (fabsf(target - s->scale) < 0.001f) s->scale = target;
}

static void press_tap(void *p, int x, int y) {
  Pressable *s = p;
  int w = canvas_w(g_cv);
  int bw = 240, bx = (w - bw) / 2;
  (void)bx;
  (void)bw;
  s->down = !s->down;
  s->downAt = SDL_GetTicks64();
}

static void press_draw(void *p) {
  Pressable *s = p;
  int w = canvas_w(g_cv), h = canvas_h(g_cv);
  canvas_clear(g_cv, 13, 13, 18);
  CColor dim = {235, 235, 245, 210};
  canvas_text_center(g_cv, g_font, "Pressable", w / 2, 60, dim);

  int bw = 240, bh = 64, bx = (w - bw) / 2, by = 240;
  int scale_bw = (int)(bw * s->scale), scale_bh = (int)(bh * s->scale);
  int sbx = bx + (bw - scale_bw) / 2, sby = by + (bh - scale_bh) / 2;

  CColor glow = {88, 126, 255, 60 + (int)(40 * (1.0f - s->scale))};
  canvas_fill_round_rect(g_cv, sbx - 8, sby - 8, scale_bw + 16, scale_bh + 16,
                         22, glow);
  CColor bg = {88, 126, 255, 255};
  canvas_fill_round_rect(g_cv, sbx, sby, scale_bw, scale_bh, 16, bg);
  CColor tc = {255, 255, 255, 255};
  canvas_text_center(g_cv, g_font, "Press me", w / 2, sby + 22, tc);

  CColor sub = {150, 150, 165, 180};
  TTF_SetFontSize(g_font, 13);
  canvas_text_center(g_cv, g_font,
                     s->down ? "pressed  (springs back)" : "tap to press",
                     w / 2, by + bh + 40, sub);
  (void)h;
}

const Comp comp_pressable = {"pressable", "atoms", 0, press_create, free,
                             press_update, press_tap, press_draw};

/* ---------------------------------------------------------------- */
/* atoms/stacks                                                      */
/* ---------------------------------------------------------------- */
typedef struct { int dummy; } Stacks;

static void *stacks_create(void) { return calloc(1, sizeof(Stacks)); }

static void stacks_draw(void *p) {
  (void)p;
  int w = canvas_w(g_cv), h = canvas_h(g_cv);
  canvas_clear(g_cv, 13, 13, 18);
  CColor dim = {235, 235, 245, 210};
  canvas_text_center(g_cv, g_font, "Stacks / Row / Center", w / 2, 50, dim);

  /* Row: horizontal stack with gap */
  CColor rlbl = {150, 150, 165, 200};
  TTF_SetFontSize(g_font, 12);
  canvas_text_center(g_cv, g_font, "<Row gap>", w / 2, 110, rlbl);
  int bw = 150, gap = 26, total = 3 * bw + 2 * gap;
  int x0 = (w - total) / 2, ry = 140;
  for (int i = 0; i < 3; i++) {
    CColor cols[] = {{88, 126, 255, 255}, {124, 77, 255, 255},
                     {255, 130, 90, 255}};
    canvas_fill_round_rect(g_cv, x0 + i * (bw + gap), ry, bw, 90, 14, cols[i]);
    CColor wt = {255, 255, 255, 255};
    char b[3] = {(char)('A' + i), 0};
    canvas_text_center(g_cv, g_font, b, x0 + i * (bw + gap) + bw / 2, ry + 38,
                       wt);
  }

  /* Center: single sandwiched centered element */
  CColor clbl = {150, 150, 165, 200};
  TTF_SetFontSize(g_font, 12);
  canvas_text_center(g_cv, g_font, "<Center>", w / 2, 290, clbl);
  CColor boxbg = {30, 30, 42, 255};
  canvas_fill_round_rect(g_cv, w / 2 - 170, 320, 340, 190, 18, boxbg);
  CColor boxb = {66, 66, 90, 255};
  canvas_round_rect(g_cv, w / 2 - 170, 320, 340, 190, 18, boxb);
  CColor inner = {255, 200, 90, 255};
  canvas_fill_round_rect(g_cv, w / 2 - 60, 365, 120, 100, 12, inner);
  CColor it = {60, 40, 10, 255};
  canvas_text_center(g_cv, g_font, "centered", w / 2, 410, it);
  (void)h;
}

const Comp comp_stacks = {"stacks", "atoms", 0, stacks_create, free, NULL, NULL,
                          stacks_draw};