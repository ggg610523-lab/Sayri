#include "chroma.h"

#include <SDL.h>

static const char *CHROMA_FS =
    "#version 150\n"
    "uniform vec2 iResolution;\n"
    "uniform vec2 u_viewport;\n"
    "uniform float iTime;\n"
    "uniform float borderWidth;\n"
    "uniform float borderRadius;\n"
    "uniform float speed;\n"
    "uniform vec3 baseColor;\n"
    "uniform vec3 glowColor;\n"
    "out vec4 fragColor;\n"
    "const float PI = 3.14159265359;\n"
    "float sdRoundedBox(vec2 p, vec2 b, float r) {\n"
    "  vec2 q = abs(p) - b + vec2(r);\n"
    "  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "float hash(vec2 p) {\n"
    "  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
    "}\n"
    "float noise(vec2 p) {\n"
    "  vec2 i = floor(p);\n"
    "  vec2 f = fract(p);\n"
    "  f = f * f * (3.0 - 2.0 * f);\n"
    "  float a = hash(i);\n"
    "  float b = hash(i + vec2(1.0, 0.0));\n"
    "  float c = hash(i + vec2(0.0, 1.0));\n"
    "  float d = hash(i + vec2(1.0, 1.0));\n"
    "  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
    "}\n"
    "void main() {\n"
    "  vec2 local = gl_FragCoord.xy - u_viewport;\n"
    "  vec2 fragCoord = vec2(local.x, iResolution.y - local.y);\n"
    "  vec2 uv = fragCoord / iResolution;\n"
    "  vec2 center = vec2(0.5, 0.5);\n"
    "  vec2 pixelPos = fragCoord - iResolution * 0.5;\n"
    "  vec2 halfSize = iResolution * 0.5 - vec2(1.0);\n"
    "  float r = min(borderRadius, min(halfSize.x, halfSize.y));\n"
    "  float dist = sdRoundedBox(pixelPos, halfSize, r);\n"

    "  float borderInner = borderWidth;\n"
    "  float borderOuter = 0.0;\n"
    "  float borderMask = 1.0 - smoothstep(borderOuter, borderOuter + 1.0, "
    "dist)\n"
    "                   - (1.0 - smoothstep(-borderInner - 1.0, -borderInner, "
    "dist));\n"
    "  borderMask = clamp(borderMask, 0.0, 1.0);\n"
    "  float angle = atan(pixelPos.y, pixelPos.x);\n"
    "  float normalizedAngle = (angle + PI) / (2.0 * PI);\n"
    "  float t = iTime * speed;\n"
    "  float perimeter = normalizedAngle;\n"
    "  float lights = 0.0;\n"
    "  float p1 = fract(perimeter - t * 0.15);\n"
    "  lights += pow(max(0.0, 1.0 - abs(p1 - 0.5) * 8.0), 2.0);\n"
    "  float p2 = fract(perimeter - t * 0.12 + 0.33);\n"
    "  lights += pow(max(0.0, 1.0 - abs(p2 - 0.5) * 8.0), 2.0);\n"
    "  float p3 = fract(perimeter - t * 0.1 + 0.66);\n"
    "  lights += pow(max(0.0, 1.0 - abs(p3 - 0.5) * 8.0), 2.0);\n"
    "  float sparkle = noise(vec2(normalizedAngle * 20.0, t * 2.0)) * 0.5;\n"
    "  sparkle += noise(vec2(normalizedAngle * 40.0 + 100.0, t * 3.0)) * 0.3;\n"
    "  vec3 rainbow;\n"
    "  float hueShift = normalizedAngle * 2.0 + t * 0.5;\n"
    "  rainbow.r = 0.5 + 0.5 * sin(hueShift * PI * 2.0);\n"
    "  rainbow.g = 0.5 + 0.5 * sin(hueShift * PI * 2.0 + PI * 0.66);\n"
    "  rainbow.b = 0.5 + 0.5 * sin(hueShift * PI * 2.0 + PI * 1.33);\n"
    "  vec3 borderColor = baseColor;\n"
    "  vec3 lightColor = mix(glowColor, rainbow, 0.6);\n"
    "  borderColor += lightColor * lights * 1.5;\n"
    "  borderColor += glowColor * sparkle * 0.3;\n"
    "  borderColor += baseColor * 0.3;\n"
    "  vec3 finalColor = borderColor * borderMask;\n"
    "  float alpha = borderMask * (0.4 + lights * 0.6 + sparkle * 0.2);\n"
    "  alpha = clamp(alpha, 0.0, 1.0);\n"
    "  fragColor = vec4(finalColor, alpha);\n"
    "}\n";

ChromaProg build_chroma_prog(void) {
  ChromaProg p;
  p.prog = glcore_make_program(glcore_fs_tri(), CHROMA_FS);
  p.loc_resolution = glGetUniformLocation(p.prog, "iResolution");
  p.loc_time = glGetUniformLocation(p.prog, "iTime");
  p.loc_borderWidth = glGetUniformLocation(p.prog, "borderWidth");
  p.loc_borderRadius = glGetUniformLocation(p.prog, "borderRadius");
  p.loc_speed = glGetUniformLocation(p.prog, "speed");
  p.loc_base = glGetUniformLocation(p.prog, "baseColor");
  p.loc_glow = glGetUniformLocation(p.prog, "glowColor");
  p.loc_viewport = glGetUniformLocation(p.prog, "u_viewport");
  return p;
}

void chroma_draw(const ChromaProg *p, float t, Box b, float borderWidth,
                 float borderRadius, float speed, const float base[3],
                 const float glow[3], int winH) {
  glUseProgram(p->prog);
  glUniform2f(p->loc_viewport, (float)b.x,
              (float)(winH - (b.y + b.h)));
  glUniform2f(p->loc_resolution, (float)b.w, (float)b.h);
  glUniform1f(p->loc_time, t);
  glUniform1f(p->loc_borderWidth, borderWidth);
  glUniform1f(p->loc_borderRadius, borderRadius);
  glUniform1f(p->loc_speed, speed);
  glUniform3f(p->loc_base, base[0], base[1], base[2]);
  glUniform3f(p->loc_glow, glow[0], glow[1], glow[2]);
  glViewport(b.x, (int)(winH - (b.y + b.h)), b.w, b.h);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

GLuint chroma_pill_texture(TTF_Font *font) {
  SDL_Surface *pill = surface_create(CHROMA_W, CHROMA_H);
  fill_rounded(pill, (float)CHROMA_W / 2.0f, (float)CHROMA_H / 2.0f,
               (float)CHROMA_W / 2.0f, (float)CHROMA_H / 2.0f,
               (float)(CHROMA_R - CHROMA_BORDER), 10, 10, 12, 1.0f);
  TTF_SetFontSize(font, 16);
  SDL_Surface *t = text_surface(font, "Chroma Ring",
                                (SDL_Color){230, 230, 240, 255});
  blit_src_over(pill, t, (CHROMA_W - t->w) / 2, (CHROMA_H - t->h) / 2);
  SDL_FreeSurface(t);
  GLuint tex = make_texture_from_surface(pill);
  SDL_FreeSurface(pill);
  return tex;
}