#include "orb.h"

#include <math.h>
#include <stdlib.h>

static const char *SIRI_ORB_FS =
    "#version 150\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec4 u_viewport;\n"
    "uniform float u_time;\n"
    "uniform vec3 u_primary;\n"
    "uniform vec3 u_secondary;\n"
    "uniform float u_noiseIntensity;\n"
    "uniform float u_glowIntensity;\n"
    "uniform float u_saturation;\n"
    "uniform float u_brightness;\n"
    "uniform float u_rotationSpeed;\n"
    "uniform float u_noiseScale;\n"
    "uniform float u_coreIntensity;\n"
    "uniform float u_edgeSoftness;\n"
    "out vec4 fragColor;\n"
    "const float TAU = 6.28318530718;\n"
    "float rand(vec2 n) { return fract(sin(dot(n, vec2(12.9898, 4.1414))) * "
    "43758.5453); }\n"
    "float noise(vec2 p) {\n"
    "  vec2 ip = floor(p);\n"
    "  vec2 fp = fract(p);\n"
    "  fp = fp * fp * (3.0 - 2.0 * fp);\n"
    "  float res = mix(\n"
    "    mix(rand(ip), rand(ip + vec2(1.0, 0.0)), fp.x),\n"
    "    mix(rand(ip + vec2(0.0, 1.0)), rand(ip + vec2(1.0, 1.0)), fp.x),\n"
    "    fp.y\n"
    "  );\n"
    "  return res * res;\n"
    "}\n"
    "float fbm(vec2 p, int octaves) {\n"
    "  float s = 0.0;\n"
    "  float m = 0.0;\n"
    "  float a = 0.5;\n"
    "  for (int i = 0; i < 4; i++) {\n"
    "    if (i >= octaves) break;\n"
    "    s += a * noise(p);\n"
    "    m += a;\n"
    "    a *= 0.5;\n"
    "    p *= 2.0;\n"
    "  }\n"
    "  return s / m;\n"
    "}\n"
    "vec3 pal(float t, vec3 a, vec3 b, vec3 c, vec3 d) {\n"
    "  return a + b * cos(TAU * (c * t + d));\n"
    "}\n"
    "float luma(vec3 color) { return dot(color, vec3(0.299, 0.587, 0.114)); }\n"
    "void main() {\n"
    "  vec2 local = gl_FragCoord.xy - u_viewport.xy;\n"
    "  float min_res = min(u_resolution.x, u_resolution.y);\n"
    "  vec2 uv = (local * 2.0 - u_resolution.xy) / min_res * 1.5;\n"
    "  float t = u_time;\n"
    "  float l = dot(uv, uv);\n"
    "  float edgeOuter = 1.0 + u_edgeSoftness;\n"
    "  float edgeInner = 1.0 - u_edgeSoftness;\n"
    "  float sm = smoothstep(edgeOuter, edgeInner, l);\n"
    "  if (sm <= 0.0) { fragColor = vec4(0.0, 0.0, 0.0, 0.0); return; }\n"
    "  float d = sm * l * l * l * 2.0;\n"
    "  vec3 norm = normalize(vec3(uv.x, uv.y, 0.7 - d));\n"
    "  float nx = fbm(uv * 2.0 * u_noiseIntensity + t * 0.4 + 25.69, 4);\n"
    "  float ny = fbm(uv * 2.0 * u_noiseIntensity + t * 0.4 + 86.31, 4);\n"
    "  float n = fbm(uv * u_noiseScale + 2.0 * vec2(nx, ny), 3);\n"
    "  vec3 col = vec3(n * 0.5 + 0.25);\n"
    "  float a = atan(uv.y, uv.x) / TAU + t * 0.1 * u_rotationSpeed;\n"
    "  vec3 palA = mix(vec3(0.3), u_primary * 0.5, 0.5);\n"
    "  vec3 palD = mix(vec3(0.0, 0.8, 0.8), u_secondary, 0.7);\n"
    "  col *= pal(a, palA, vec3(0.5, 0.5, 0.5), vec3(1.0), palD);\n"
    "  col *= u_saturation;\n"
    "  vec3 cd = abs(col);\n"
    "  vec3 c = col * d;\n"
    "  c += (c * 0.5 + vec3(1.0) - luma(c)) * vec3(max(0.0, pow(dot(norm, "
    "vec3(0.0, 0.0, -1.0)), 5.0) * 3.0));\n"
    "  float g = u_glowIntensity * smoothstep(0.6, 1.0, fbm(norm.xy * 3.0 / "
    "(1.0 + norm.z), 2)) * d;\n"
    "  c += g;\n"
    "  col = c + col * pow((1.0 - smoothstep(1.0, 0.98, l) - "
    "pow(max(0.0, length(uv) - 1.0), 0.2)) * 2.0, 4.0);\n"
    "  float f = fbm(normalize(uv) * 2.0 + t, 2) + 0.1;\n"
    "  uv *= f + 0.1;\n"
    "  uv *= 0.5;\n"
    "  l = dot(uv, uv);\n"
    "  vec3 ins = normalize(cd) + 0.1;\n"
    "  float ind = 0.2 + pow(smoothstep(0.0, 1.5, sqrt(l)) * 48.0, 0.25);\n"
    "  ind *= ind * ind * ind;\n"
    "  ind = 1.0 / ind;\n"
    "  ins *= ind;\n"
    "  col += ins * ins * sm * smoothstep(0.7, 1.0, ind) * u_coreIntensity * "
    "2.0;\n"
    "  col += abs(norm) * (1.0 - d) * sm * 0.25;\n"
    "  col *= u_brightness;\n"
    "  fragColor = vec4(col, sm);\n"
    "}\n";

Box orb_box(void) { return (Box){ORB_X, ORB_Y, ORB_SIZE, ORB_SIZE}; }

OrbProg build_orb_prog(void) {
  OrbProg p;
  p.prog = glcore_make_program(glcore_fs_tri(), SIRI_ORB_FS);
  p.loc_viewport = glGetUniformLocation(p.prog, "u_viewport");
  p.loc_resolution = glGetUniformLocation(p.prog, "u_resolution");
  p.loc_time = glGetUniformLocation(p.prog, "u_time");
  p.loc_primary = glGetUniformLocation(p.prog, "u_primary");
  p.loc_secondary = glGetUniformLocation(p.prog, "u_secondary");
  p.loc_noiseIntensity = glGetUniformLocation(p.prog, "u_noiseIntensity");
  p.loc_glowIntensity = glGetUniformLocation(p.prog, "u_glowIntensity");
  p.loc_saturation = glGetUniformLocation(p.prog, "u_saturation");
  p.loc_brightness = glGetUniformLocation(p.prog, "u_brightness");
  p.loc_rotationSpeed = glGetUniformLocation(p.prog, "u_rotationSpeed");
  p.loc_noiseScale = glGetUniformLocation(p.prog, "u_noiseScale");
  p.loc_coreIntensity = glGetUniformLocation(p.prog, "u_coreIntensity");
  p.loc_edgeSoftness = glGetUniformLocation(p.prog, "u_edgeSoftness");
  return p;
}

static float randf(void) { return rand() / (float)RAND_MAX; }

static void hsv2rgb(float h, float s, float v, float *r, float *g, float *b) {
  float c = v * s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;
  float rr, gg, bb;
  if (h < 60) { rr = c; gg = x; bb = 0; }
  else if (h < 120) { rr = x; gg = c; bb = 0; }
  else if (h < 180) { rr = 0; gg = c; bb = x; }
  else if (h < 240) { rr = 0; gg = x; bb = c; }
  else if (h < 300) { rr = x; gg = 0; bb = c; }
  else { rr = c; gg = 0; bb = x; }
  *r = rr + m; *g = gg + m; *b = bb + m;
}

void roll_orb_colors(OrbitState *o) {
  float hue = randf() * 360.0f;
  float r, g, b;
  hsv2rgb(hue, 0.8f, 1.0f, &r, &g, &b);
  o->primary[0] = r; o->primary[1] = g; o->primary[2] = b;
  hsv2rgb(fmodf(hue + 110.0f, 360.0f), 0.9f, 0.9f, &r, &g, &b);
  o->secondary[0] = r; o->secondary[1] = g; o->secondary[2] = b;
}

void orb_draw(const OrbProg *p, const OrbitState *st, float t, int winH) {
  Box b = orb_box();
  glUseProgram(p->prog);
  glUniform4f(p->loc_viewport, (float)b.x, (float)(winH - (b.y + b.h)),
              (float)b.w, (float)b.h);
  glUniform2f(p->loc_resolution, (float)ORB_SIZE, (float)ORB_SIZE);
  glUniform1f(p->loc_time, t);
  glUniform3f(p->loc_primary, st->primary[0], st->primary[1], st->primary[2]);
  glUniform3f(p->loc_secondary, st->secondary[0], st->secondary[1],
              st->secondary[2]);
  glUniform1f(p->loc_noiseIntensity, 1.0f);
  glUniform1f(p->loc_glowIntensity, 1.4f);
  glUniform1f(p->loc_saturation, 2.0f);
  glUniform1f(p->loc_brightness, 1.0f);
  glUniform1f(p->loc_rotationSpeed, 1.0f);
  glUniform1f(p->loc_noiseScale, 3.0f);
  glUniform1f(p->loc_coreIntensity, 0.55f);
  glUniform1f(p->loc_edgeSoftness, 0.045f);
  glViewport(b.x, winH - (b.y + b.h), b.w, b.h);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}