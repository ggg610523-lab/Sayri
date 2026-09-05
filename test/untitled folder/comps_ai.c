/* ai/bottom-input-bar and ai/thinking-state (C port) */
#include <math.h>
#include <stdlib.h>

#include "framework.h"
#include "util.h"

static float C01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }
static float EIO(float t) {
  t = C01(t);
  return t < 0.5f ? 4 * t * t * t : 1.0f - powf(-2 * t + 2, 3) / 2.0f;
}

/* ---------------------------------------------------------------- */
/* ai/bottom-input-bar                                               */
/* ---------------------------------------------------------------- */
typedef struct {
  Uint64 start;
  float open; /* 0..1 */
  int message;
} BotBar;

static void *botbar_create(void) {
  BotBar *s = calloc(1, sizeof(*s));
  s->start = SDL_GetTicks64();
  return s;
}

static void botbar_update(void *p, Uint64 now, float dt) {
  (void)dt;
  BotBar *s = p;
  float target = s->message ? 1.0f : 0.0f;
  float speed = 2.4f;
  float diff = target - s->open;
  s->open += diff * (1.0f - expf(-speed * dt));
  if (fabsf(diff) < 0.001f) s->open = target;
  (void)now;
}

static void botbar_tap(void *p, int x, int y) {
  BotBar *s = p;
  int w = canvas_w(g_cv), h = canvas_h(g_cv);
  int bh = (int)(56 + 40 * s->open);
  int bx = 16, by = h - 16 - bh, bw = w - 32;
  if (x >= bx && x <= bx + bw && y >= by && y <= by + bh) {
    s->message = !s->message;
    s->start = SDL_GetTicks64();
  }
}

static void botbar_draw(void *p) {
  BotBar *s = p;
  int w = canvas_w(g_cv), h = canvas_h(g_cv);
  canvas_clear(g_cv, 13, 13, 18);
  TTF_SetFontSize(g_font, 16);
  CColor dim = {235, 235, 245, 210};
  canvas_text_center(g_cv, g_font, "Bottom Input Bar", w / 2, 60, dim);
  TTF_SetFontSize(g_font, 13);
  CColor sub = {150, 150, 165, 180};
  canvas_text_center(g_cv, g_font, "Chat input that grows as you type", w / 2, 86, sub);

  int bh = (int)(56 + 40 * EIO(s->open));
  int bx = 16, by = h - 16 - bh, bw = w - 32;
  CColor bg = {24, 24, 32, 235};
  canvas_fill_round_rect(g_cv, bx, by, bw, bh, 18, bg);
  CColor border = {60, 60, 76, 255};
  canvas_round_rect(g_cv, bx, by, bw, bh, 18, border);

  CColor acc = {70, 70, 92, 255};
  canvas_fill_circle(g_cv, bx + 26, by + bh / 2, 12, acc);
  CColor accin = {130, 130, 160, 255};
  canvas_fill_circle(g_cv, bx + 26, by + bh / 2, 5, accin);

  int ty = by + bh / 2 - 9;
  if (s->message) {
    TTF_SetFontSize(g_font, 15);
    CColor tx = {235, 235, 245, 255};
    canvas_text(g_cv, g_font, "Tell me about Reacticx", bx + 50, ty, tx);
  } else {
    TTF_SetFontSize(g_font, 15);
    CColor ph = {140, 140, 158, 255};
    canvas_text(g_cv, g_font, "Chat with Explore...", bx + 50, ty, ph);
  }
  Uint64 now = SDL_GetTicks64();
  if ((now / 600) % 2 == 0) {
    int cx = bx + 50 + (s->message ? 168 : 150);
    CColor caret = {255, 255, 255, 200};
    canvas_fill_rect(g_cv, cx, ty + 2, 2, 14, caret);
  }

  CColor sbg = {88, 126, 255, 255};
  canvas_fill_circle(g_cv, bx + bw - 30, by + bh / 2, 14, sbg);
  CColor sfg = {255, 255, 255, 255};
  int sx = bx + bw - 36, sy = by + bh / 2 - 6;
  canvas_poly(g_cv, (int[]){sx, sx + 12, sx}, (int[]){sy, sy + 12, sy}, 3, sfg);
  TTF_SetFontSize(g_font, 12);
  CColor hint = {120, 120, 140, 170};
  canvas_text_center(g_cv, g_font, "tap me", w / 2, by - 34, hint);
}

const Comp comp_bottom_input_bar = {
    "bottom-input-bar", "ai", 0, botbar_create, free, botbar_update,
    botbar_tap, botbar_draw};

/* ---------------------------------------------------------------- */
/* ai/thinking-state                                                 */
/* ---------------------------------------------------------------- */
typedef struct {
  Uint64 start;
  int line;
  float pulse;
} ThinkState;

static void *ts_create(void) {
  ThinkState *s = calloc(1, sizeof(*s));
  s->start = SDL_GetTicks64();
  return s;
}

static void ts_update(void *p, Uint64 now, float dt) {
  (void)dt;
  ThinkState *s = p;
  s->line = (int)((now - s->start) / 1500) % 7;
}

static void ts_draw(void *p) {
  ThinkState *s = p;
  int w = canvas_w(g_cv);
  canvas_clear(g_cv, 12, 14, 18);

  int cw = 640, chh = 200, cx = (w - cw) / 2, cy = 120;
  CColor win = {18, 20, 26, 255};
  canvas_fill_round_rect(g_cv, cx, cy, cw, chh, 10, win);
  CColor border = {55, 60, 74, 255};
  canvas_round_rect(g_cv, cx, cy, cw, chh, 10, border);

  /* window chrome */
  CColor dot = {255, 95, 86, 255};
  canvas_fill_circle(g_cv, cx + 22, cy + 18, 5, dot);
  dot = (CColor){255, 189, 46, 255};
  canvas_fill_circle(g_cv, cx + 40, cy + 18, 5, dot);
  dot = (CColor){39, 201, 63, 255};
  canvas_fill_circle(g_cv, cx + 58, cy + 18, 5, dot);

  static const char *lines[] = {"Connecting to API...",
                                "Authenticating user...",
                                "Loading profile data...",
                                "Fetching notifications...",
                                "Syncing preferences...",
                                "Ready!",
                                "Done."};
  int base = s->line > 1 ? s->line - 1 : 0;
  int lh = 34;
  for (int i = 0; i < 4; i++) {
    int li = base + i;
    if (li >= 7) break;
    int y = cy + 44 + i * lh;
    CColor num = {107, 114, 128, 255};
    char numbuf[8];
    snprintf(numbuf, sizeof(numbuf), "%02d", li + 1);
    TTF_SetFontSize(g_font, 12);
    canvas_text(g_cv, g_font, numbuf, cx + 20, y, num);
    CColor tc = {229, 231, 235, 255};
    if (li == s->line) {
      tc = (CColor){255, 255, 255, 255};
    } else if (li < s->line) {
      tc = (CColor){160, 168, 178, 255};
    }
    TTF_SetFontSize(g_font, 13);
    canvas_text(g_cv, g_font, lines[li], cx + 52, y, tc);
  }

  /* cursor bar highlight behind "loading" line */
  int pc = (int)(40 + 30 * sinf((float)(SDL_GetTicks64() % 900) / 900.0f * 6.283f));
  CColor bar = {70, 90, 255, 55};
  canvas_fill_round_rect(g_cv, cx + 12, cy + 44 + (s->line % 7) * lh, cw - 24, lh, 6,
                         bar);

  /* fade overlay toward bottom */
  CColor fade0 = {12, 14, 18, 60};
  CColor fade1 = {12, 14, 18, 0};
  canvas_gradient_v(g_cv, cx, cy + chh - 60, cw, 60, fade0, fade1);
  (void)pc;
}

const Comp comp_thinking_state = {
    "thinking-state", "ai", 0, ts_create, free, ts_update, NULL, ts_draw};