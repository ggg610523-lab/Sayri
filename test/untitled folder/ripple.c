#include "ripple.h"

static const char *RIPPLE_FS =
    "#version 150\n"
    "uniform vec4 u_viewport;\n"
    "uniform vec2 u_origin;\n"
    "uniform vec2 u_res;\n"
    "uniform float u_time;\n"
    "uniform float u_amplitude;\n"
    "uniform float u_frequency;\n"
    "uniform float u_decay;\n"
    "uniform float u_speed;\n"
    "uniform float u_corner;\n"
    "uniform sampler2D u_texture;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  vec2 local = gl_FragCoord.xy - u_viewport.xy;\n"
    "  float dist = length(local - u_origin);\n"
    "  float delay = dist / u_speed;\n"
    "  float time = max(0.0, u_time - delay);\n"
    "  float ripple = u_amplitude * sin(u_frequency * time) * "
    "exp(-u_decay * time);\n"
    "  vec2 n = dist > 0.001 ? normalize(local - u_origin) : vec2(0.0, 0.0);\n"
    "  vec2 np = (local + ripple * n) / u_res;\n"
    "  vec4 color = texture(u_texture, np);\n"
    "  float brightness = 0.3 * (ripple / max(u_amplitude, 0.001)) * "
    "color.a;\n"
    "  color.rgb += brightness;\n"
    "  if (u_corner > 0.0) {\n"
    "    vec2 halfsize = 0.5 * u_res;\n"
    "    vec2 q = abs(local - halfsize) - (halfsize - vec2(u_corner));\n"
    "    float cov = length(max(q, vec2(0.0))) - u_corner;\n"
    "    if (cov > 0.0) color.a = 0.0;\n"
    "  }\n"
    "  fragColor = color;\n"
    "}\n";

RippleProg build_ripple_prog(void) {
  RippleProg p;
  p.prog = glcore_make_program(glcore_fs_tri(), RIPPLE_FS);
  p.loc_viewport = glGetUniformLocation(p.prog, "u_viewport");
  p.loc_origin = glGetUniformLocation(p.prog, "u_origin");
  p.loc_res = glGetUniformLocation(p.prog, "u_res");
  p.loc_time = glGetUniformLocation(p.prog, "u_time");
  p.loc_amplitude = glGetUniformLocation(p.prog, "u_amplitude");
  p.loc_frequency = glGetUniformLocation(p.prog, "u_frequency");
  p.loc_decay = glGetUniformLocation(p.prog, "u_decay");
  p.loc_speed = glGetUniformLocation(p.prog, "u_speed");
  p.loc_corner = glGetUniformLocation(p.prog, "u_corner");
  p.loc_texture = glGetUniformLocation(p.prog, "u_texture");
  return p;
}

float ripple_time(const Ripple *r, Uint64 now) {
  double ms = (double)(now - r->startMs);
  double sec = ms / 1000.0;
  if (sec < 0.0 || sec >= r->duration) return 0.0f;
  return (float)sec;
}

void ripple_tap(const Box *b, Ripple *r, int mx, int my) {
  r->originX = (float)(mx - b->x);
  r->originY = (float)(b->h - (my - b->y));
  r->startMs = SDL_GetTicks64();
}

GLuint ripple_card_texture(TTF_Font *font) {
  int s = TEX_SCALE;
  SDL_Surface *card = surface_create(CARD_W * s, CARD_H * s);
  fill_gradient(card, 0, 0, CARD_W * s, 250 * s, 26, 48, 94, 196, 124, 74);
  for (int y = 250 * s; y < CARD_H * s; y++) {
    for (int x = 0; x < CARD_W * s; x++) {
      put_rgba(card, x, y, 8, 8, 12, 60);
    }
  }
  scale_alpha(card, 0, 0, CARD_W * s, CARD_H * s, (float)(CARD_W * s) / 2.0f,
              (float)(CARD_H * s) / 2.0f, (float)(CARD_W * s) / 2.0f,
              (float)(CARD_H * s) / 2.0f, (float)(CARD_R * s));

  TTF_SetFontSize(font, 22 * s);
  SDL_Surface *t = text_surface(font, "Sherliam",
                                (SDL_Color){255, 255, 255, 255});
  blit_src_over(card, t, 22 * s, 264 * s);
  SDL_FreeSurface(t);

  TTF_SetFontSize(font, 14 * s);
  t = text_surface(font, "Carries power he never asked for",
                   (SDL_Color){255, 255, 255, 184});
  blit_src_over(card, t, 22 * s, 300 * s);
  SDL_FreeSurface(t);

  TTF_SetFontSize(font, 13 * s);
  t = text_surface(font, "Tap for a Skia ripple",
                   (SDL_Color){255, 255, 255, 150});
  blit_src_over(card, t, 22 * s, (CARD_H - 34) * s);
  SDL_FreeSurface(t);

  GLuint tex = make_texture_from_surface(card);
  debug_surface("/tmp/card.ppm", card);
  SDL_FreeSurface(card);
  return tex;
}

void ripple_draw(const RippleProg *p, GLuint tex, Box b, float time, float ox,
                 float oy, float amp, float corner, int winH) {
  glUseProgram(p->prog);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(p->loc_texture, 0);
  glUniform4f(p->loc_viewport, (float)b.x, (float)(winH - (b.y + b.h)),
              (float)b.w, (float)b.h);
  glUniform2f(p->loc_res, (float)b.w, (float)b.h);
  glUniform1f(p->loc_time, time);
  glUniform2f(p->loc_origin, ox, oy);
  glUniform1f(p->loc_amplitude, amp);
  glUniform1f(p->loc_frequency, 15.0f);
  glUniform1f(p->loc_decay, 8.0f);
  glUniform1f(p->loc_speed, 1200.0f);
  glUniform1f(p->loc_corner, corner);
  glViewport(b.x, winH - (b.y + b.h), b.w, b.h);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}