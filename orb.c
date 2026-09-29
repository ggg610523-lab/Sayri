#include "orb.h"
#include "orb1.h"
#include "ripple.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <SDL2/SDL_opengl.h>

/*
    ============================================================
    GL orb: the Siri-style shader orb from test/orb.c, vendored
    here so the running app uses it instead of the software
    renderer above.

    Geometry: a hidden ORB_RES window with its own GL 3.2 core
    context renders the full-screen-triangle fragment shader
    into its default framebuffer; we glReadPixels the result
    and upload it to the same streaming SDL texture the old
    orb used. If any GL step fails we fall back to the CPU
    renderer, so the app never breaks.
    ============================================================
*/

#define ORB_BIND(ret, name, ...) \
    typedef ret (*orb_gl_##name##_fn)(__VA_ARGS__); \
    static orb_gl_##name##_fn orb_gl_##name;

ORB_BIND(void, UseProgram, GLuint)
ORB_BIND(void, Uniform1f, GLint, GLfloat)
ORB_BIND(void, Uniform2f, GLint, GLfloat, GLfloat)
ORB_BIND(void, Uniform3f, GLint, GLfloat, GLfloat, GLfloat)
ORB_BIND(void, Uniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat)
ORB_BIND(GLint, GetUniformLocation, GLuint, const char *)
ORB_BIND(void, Viewport, GLint, GLint, GLsizei, GLsizei)
ORB_BIND(void, DrawArrays, GLenum, GLint, GLsizei)
ORB_BIND(void, Enable, GLenum)
ORB_BIND(void, Disable, GLenum)
ORB_BIND(void, BlendFunc, GLenum, GLenum)
ORB_BIND(void, ClearColor, GLfloat, GLfloat, GLfloat, GLfloat)
ORB_BIND(void, Clear, unsigned int)
ORB_BIND(void, GenVertexArrays, GLsizei, GLuint *)
ORB_BIND(void, BindVertexArray, GLuint)
ORB_BIND(void, DeleteVertexArrays, GLsizei, const GLuint *)
ORB_BIND(GLuint, CreateShader, GLenum)
ORB_BIND(void, ShaderSource, GLuint, GLsizei, const char *const *, const GLint *)
ORB_BIND(void, CompileShader, GLuint)
ORB_BIND(void, GetShaderiv, GLuint, GLenum, GLint *)
ORB_BIND(void, GetShaderInfoLog, GLuint, GLsizei, GLsizei *, char *)
ORB_BIND(void, DeleteShader, GLuint)
ORB_BIND(GLuint, CreateProgram, void)
ORB_BIND(void, AttachShader, GLuint, GLuint)
ORB_BIND(void, LinkProgram, GLuint)
ORB_BIND(void, GetProgramiv, GLuint, GLenum, GLint *)
ORB_BIND(void, GetProgramInfoLog, GLuint, GLsizei, GLsizei *, char *)
ORB_BIND(void, DeleteProgram, GLuint)
ORB_BIND(void, ReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)

#undef ORB_BIND

static SDL_Window *orb_gl_win = NULL;
static SDL_GLContext orb_gl_ctx = NULL;
static GLuint orb_gl_vao = 0;
static GLuint orb_gl_prog = 0;
static GLint orb_gl_loc_viewport = -1;
static GLint orb_gl_loc_resolution = -1;
static GLint orb_gl_loc_time = -1;
static GLint orb_gl_loc_primary = -1;
static GLint orb_gl_loc_secondary = -1;
static GLint orb_gl_loc_noise = -1;
static GLint orb_gl_loc_glow = -1;
static GLint orb_gl_loc_sat = -1;
static GLint orb_gl_loc_bright = -1;
static GLint orb_gl_loc_rot = -1;
static GLint orb_gl_loc_nscale = -1;
static GLint orb_gl_loc_core = -1;
static GLint orb_gl_loc_edge = -1;
static float orb_gl_primary_c[3];
static float orb_gl_secondary_c[3];
static unsigned char *orb_gl_read = NULL;
static int orb_gl_ok = 0;

static const char *ORB_GL_VS =
    "#version 150\n"
    "void main() {\n"
    "  vec2 p;\n"
    "  if (gl_VertexID == 0) p = vec2(-1.0, -1.0);\n"
    "  else if (gl_VertexID == 1) p = vec2(3.0, -1.0);\n"
    "  else p = vec2(-1.0, 3.0);\n"
    "  gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n";

static const char *ORB_GL_FS =
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

static GLuint orb_gl_compile(GLenum type, const char *src)
{
    GLuint sh = orb_gl_CreateShader(type);
    const char *s[] = {src};
    orb_gl_ShaderSource(sh, 1, s, NULL);
    orb_gl_CompileShader(sh);

    GLint ok = 0;
    orb_gl_GetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        char log[4096];
        GLsizei n = 0;
        orb_gl_GetShaderInfoLog(
            sh, sizeof(log) - 1, &n, log);
        log[n > 0 ? n : 0] = '\0';
        fprintf(stderr, "orb GL shader: %s\n", log);
        orb_gl_DeleteShader(sh);
        return 0;
    }

    return sh;
}

static GLuint orb_gl_program(void)
{
    GLuint v = orb_gl_compile(
        GL_VERTEX_SHADER, ORB_GL_VS);
    GLuint f = orb_gl_compile(
        GL_FRAGMENT_SHADER, ORB_GL_FS);

    if (!v || !f)
        return 0;

    GLuint p = orb_gl_CreateProgram();
    orb_gl_AttachShader(p, v);
    orb_gl_AttachShader(p, f);
    orb_gl_LinkProgram(p);

    GLint ok = 0;
    orb_gl_GetProgramiv(p, GL_LINK_STATUS, &ok);

    if (!ok) {
        char log[4096];
        GLsizei n = 0;
        orb_gl_GetProgramInfoLog(
            p, sizeof(log) - 1, &n, log);
        log[n > 0 ? n : 0] = '\0';
        fprintf(stderr, "orb GL program: %s\n", log);
    }

    orb_gl_DeleteShader(v);
    orb_gl_DeleteShader(f);

    return ok ? p : 0;
}

static float orb_randf(void)
{
    return rand() / (float)RAND_MAX;
}

static void orb_hsv2rgb(
    float h, float s, float v,
    float *r, float *g, float *b)
{
    float c = v * s;
    float x = c * (1.0f - fabsf(
        fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rr, gg, bb;

    if (h < 60)      { rr = c; gg = x; bb = 0; }
    else if (h < 120){ rr = x; gg = c; bb = 0; }
    else if (h < 180){ rr = 0; gg = c; bb = x; }
    else if (h < 240){ rr = 0; gg = x; bb = c; }
    else if (h < 300){ rr = x; gg = 0; bb = c; }
    else             { rr = c; gg = 0; bb = x; }

    *r = rr + m; *g = gg + m; *b = bb + m;
}

static void orb_gl_roll_colors(void)
{
    float hue = orb_randf() * 360.0f;
    float r, g, b;

    orb_hsv2rgb(
        hue, 0.8f, 1.0f, &r, &g, &b);
    orb_gl_primary_c[0] = r;
    orb_gl_primary_c[1] = g;
    orb_gl_primary_c[2] = b;

    orb_hsv2rgb(
        fmodf(hue + 110.0f, 360.0f),
        0.9f, 0.9f, &r, &g, &b);
    orb_gl_secondary_c[0] = r;
    orb_gl_secondary_c[1] = g;
    orb_gl_secondary_c[2] = b;
}

static int orb_gl_init(void)
{
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS,
        SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

#if defined(__linux__)
    /*
        Linux patch: X11/Wayland give us a default
        framebuffer with no alpha channel unless we
        ask for one. Without this the shader's radial
        alpha is forced to 1.0 by the time glReadPixels
        returns, so the whole ORB_RES square reads back
        opaque and the region outside the orb renders
        as a black box over the app background.
    */
    SDL_GL_SetAttribute(
        SDL_GL_ALPHA_SIZE, 8);
#endif

    SDL_SetHint(
        SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");

    orb_gl_win = SDL_CreateWindow(
        "sayri-orb",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ORB_RES, ORB_RES,
        SDL_WINDOW_HIDDEN |
        SDL_WINDOW_OPENGL);

    if (!orb_gl_win) {
        fprintf(stderr,
            "orb GL window: %s\n",
            SDL_GetError());
        return 0;
    }

    orb_gl_ctx =
        SDL_GL_CreateContext(orb_gl_win);

    if (!orb_gl_ctx) {
        fprintf(stderr,
            "orb GL context: %s\n",
            SDL_GetError());
        SDL_DestroyWindow(orb_gl_win);
        orb_gl_win = NULL;
        return 0;
    }

    /*
        The attributes and HIDPI hint above are
        global to SDL; put the defaults back so
        windows/contexts created later (e.g. the
        dialog box) are unaffected.
    */
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, 0);
#if defined(__linux__)
    SDL_GL_SetAttribute(
        SDL_GL_ALPHA_SIZE, 0);
#endif
    SDL_SetHint(
        SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");

    SDL_GL_MakeCurrent(orb_gl_win, orb_gl_ctx);

#define LOAD(name) \
    orb_gl_##name = (orb_gl_##name##_fn) \
        SDL_GL_GetProcAddress("gl" #name)
    LOAD(UseProgram);
    LOAD(Uniform1f);
    LOAD(Uniform2f);
    LOAD(Uniform3f);
    LOAD(Uniform4f);
    LOAD(GetUniformLocation);
    LOAD(Viewport);
    LOAD(DrawArrays);
    LOAD(Enable);
    LOAD(Disable);
    LOAD(BlendFunc);
    LOAD(ClearColor);
    LOAD(Clear);
    LOAD(GenVertexArrays);
    LOAD(BindVertexArray);
    LOAD(DeleteVertexArrays);
    LOAD(CreateShader);
    LOAD(ShaderSource);
    LOAD(CompileShader);
    LOAD(GetShaderiv);
    LOAD(GetShaderInfoLog);
    LOAD(DeleteShader);
    LOAD(CreateProgram);
    LOAD(AttachShader);
    LOAD(LinkProgram);
    LOAD(GetProgramiv);
    LOAD(GetProgramInfoLog);
    LOAD(DeleteProgram);
    LOAD(ReadPixels);
#undef LOAD

    if (!orb_gl_UseProgram ||
        !orb_gl_CreateShader ||
        !orb_gl_ReadPixels ||
        !orb_gl_DrawArrays) {
        fprintf(stderr,
            "orb: GL 3.x unavailable, "
            "using software orb\n");
        goto fail;
    }

    orb_gl_GenVertexArrays(1, &orb_gl_vao);
    orb_gl_BindVertexArray(orb_gl_vao);

    orb_gl_prog = orb_gl_program();

    if (!orb_gl_prog)
        goto fail;

    orb_gl_loc_viewport =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_viewport");
    orb_gl_loc_resolution =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_resolution");
    orb_gl_loc_time =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_time");
    orb_gl_loc_primary =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_primary");
    orb_gl_loc_secondary =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_secondary");
    orb_gl_loc_noise =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_noiseIntensity");
    orb_gl_loc_glow =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_glowIntensity");
    orb_gl_loc_sat =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_saturation");
    orb_gl_loc_bright =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_brightness");
    orb_gl_loc_rot =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_rotationSpeed");
    orb_gl_loc_nscale =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_noiseScale");
    orb_gl_loc_core =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_coreIntensity");
    orb_gl_loc_edge =
        orb_gl_GetUniformLocation(
            orb_gl_prog, "u_edgeSoftness");

    orb_gl_read = (unsigned char *)malloc(
        ORB_RES * ORB_RES * 4);

    if (!orb_gl_read)
        goto fail;

    srand((unsigned)time(NULL));
    orb_gl_roll_colors();

    orb_gl_ok = 1;

    return 1;

fail:
    if (orb_gl_prog) {
        orb_gl_DeleteProgram(orb_gl_prog);
        orb_gl_prog = 0;
    }
    if (orb_gl_vao) {
        orb_gl_DeleteVertexArrays(
            1, &orb_gl_vao);
        orb_gl_vao = 0;
    }
    free(orb_gl_read);
    orb_gl_read = NULL;
    SDL_GL_DeleteContext(orb_gl_ctx);
    SDL_DestroyWindow(orb_gl_win);
    orb_gl_win = NULL;
    orb_gl_ctx = NULL;
    return 0;
}

static void orb_gl_render(Orb *orb)
{
    if (!orb_gl_ok)
        return;

    SDL_GL_MakeCurrent(orb_gl_win, orb_gl_ctx);

    orb_gl_Disable(GL_BLEND);
    orb_gl_ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    orb_gl_Clear(GL_COLOR_BUFFER_BIT);

    orb_gl_UseProgram(orb_gl_prog);

    orb_gl_Uniform4f(
        orb_gl_loc_viewport,
        0.0f, 0.0f,
        (float)ORB_RES, (float)ORB_RES);
    orb_gl_Uniform2f(
        orb_gl_loc_resolution,
        (float)ORB_RES, (float)ORB_RES);
    orb_gl_Uniform1f(
        orb_gl_loc_time, orb->time);
    orb_gl_Uniform3f(
        orb_gl_loc_primary,
        orb_gl_primary_c[0],
        orb_gl_primary_c[1],
        orb_gl_primary_c[2]);
    orb_gl_Uniform3f(
        orb_gl_loc_secondary,
        orb_gl_secondary_c[0],
        orb_gl_secondary_c[1],
        orb_gl_secondary_c[2]);
    orb_gl_Uniform1f(
        orb_gl_loc_noise, 1.0f);
    orb_gl_Uniform1f(
        orb_gl_loc_glow, 1.4f);
    orb_gl_Uniform1f(
        orb_gl_loc_sat, 2.0f);
    orb_gl_Uniform1f(
        orb_gl_loc_bright, 1.0f);
    orb_gl_Uniform1f(
        orb_gl_loc_rot, 1.0f);
    orb_gl_Uniform1f(
        orb_gl_loc_nscale, 3.0f);
    orb_gl_Uniform1f(
        orb_gl_loc_core, 0.55f);
    orb_gl_Uniform1f(
        orb_gl_loc_edge, 0.045f);

    orb_gl_Viewport(
        0, 0, ORB_RES, ORB_RES);
    orb_gl_DrawArrays(
        GL_TRIANGLES, 0, 3);

    orb_gl_ReadPixels(
        0, 0, ORB_RES, ORB_RES,
        GL_RGBA, GL_UNSIGNED_BYTE,
        orb_gl_read);

    /*
        GL rows come bottom-up; SDL textures
        are top-down, so mirror when packing.
    */
    for (int y = 0; y < ORB_RES; y++) {
        const unsigned char *src =
            orb_gl_read +
            (ORB_RES - 1 - y) *
            ORB_RES * 4;
        Uint32 *dst = orb->pixels + y * ORB_RES;

        for (int x = 0; x < ORB_RES; x++) {
            int r = src[x * 4 + 0];
            int g = src[x * 4 + 1];
            int b = src[x * 4 + 2];
            int a = src[x * 4 + 3];

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            if (a > 255) a = 255;

            dst[x] =
                ((Uint32)a << 24) |
                ((Uint32)r << 16) |
                ((Uint32)g << 8) |
                (Uint32)b;
        }
    }
}



static int g_gl_up = 0;

/*
    Pristine orb pixels (refreshed every orb_update)
    that the tap ripple warps from — never the previous
    frame's warped output.
*/
static Uint32 *g_orb_snap = NULL;

void orb_tap(
    Orb *orb,
    float tex_x,
    float tex_y)
{
    orb->tap_active = true;
    orb->tap_ox = tex_x;
    orb->tap_oy = tex_y;
    orb->tap_start = SDL_GetTicks64();

    if (!g_orb_snap) {
        g_orb_snap = (Uint32 *)malloc(
            sizeof(Uint32) *
            ORB_RES * ORB_RES);
    }
}

/*
    Warp the freshly-rendered orb pixels for the active
    tap ripple. Runs inside orb_draw's GL path only.
*/
static void orb_warp_tap(Orb *orb)
{
    if (!orb->tap_active)
        return;

    if (!g_orb_snap)
        return;

    Uint64 now = SDL_GetTicks64();
    float t = (float)(
        (double)(now - orb->tap_start) / 1000.0);

    if (t >= RIPPLE_DURATION) {
        orb->tap_active = false;
        return;
    }

    if (t <= 0.0f)
        return;

    const float amp = 28.0f;

    for (int y = 0; y < ORB_RES; y++) {
        for (int x = 0; x < ORB_RES; x++) {
            orb->pixels[y * ORB_RES + x] =
                ripple_warp_tex(
                    g_orb_snap, ORB_RES, ORB_RES,
                    (float)x, (float)y,
                    orb->tap_ox, orb->tap_oy,
                    t, amp);
        }
    }

    orb->dirty = true;
}

void orb_init(
    Orb *orb,
    SDL_Renderer *renderer)
{
    orb->texture =
        SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            ORB_RES,
            ORB_RES);

    SDL_SetTextureBlendMode(
        orb->texture,
        SDL_BLENDMODE_BLEND);

    orb->time = 0.0f;
    orb->dirty = true;
    orb->visible = true;
    orb->fallback = NULL;

    memset(
        orb->pixels,
        0,
        sizeof(orb->pixels));

    if (orb_gl_init()) {
        g_gl_up = 1;
        orb_gl_render(orb);

        if (!g_orb_snap) {
            g_orb_snap = (Uint32 *)malloc(
                sizeof(Uint32) *
                ORB_RES * ORB_RES);
        }
        if (g_orb_snap) {
            memcpy(g_orb_snap, orb->pixels,
                   sizeof(Uint32) *
                   ORB_RES * ORB_RES);
        }
    } else {
        Orb1 *fb = (Orb1 *)malloc(sizeof(Orb1));

        if (fb) {
            orb1_init(fb, renderer);
            orb->fallback = fb;
        }
    }
}

void orb_free(Orb *orb)
{
    if (orb->fallback) {
        orb1_free((Orb1 *)orb->fallback);
        free(orb->fallback);
        orb->fallback = NULL;

        free(g_orb_snap);
        g_orb_snap = NULL;
        return;
    }

    if (g_gl_up) {
        g_gl_up = 0;
        orb_gl_ok = 0;

        SDL_GL_MakeCurrent(
            orb_gl_win, orb_gl_ctx);

        if (orb_gl_prog) {
            orb_gl_DeleteProgram(
                orb_gl_prog);
            orb_gl_prog = 0;
        }

        if (orb_gl_vao) {
            orb_gl_DeleteVertexArrays(
                1, &orb_gl_vao);
            orb_gl_vao = 0;
        }

        free(orb_gl_read);
        orb_gl_read = NULL;

        SDL_GL_DeleteContext(orb_gl_ctx);
        SDL_DestroyWindow(orb_gl_win);

        orb_gl_ctx = NULL;
        orb_gl_win = NULL;
    }

    free(g_orb_snap);
    g_orb_snap = NULL;

    if (orb->texture) {
        SDL_DestroyTexture(orb->texture);
        orb->texture = NULL;
    }
}

void orb_update(Orb *orb, float dt)
{
    orb->time += dt;

    if (orb->fallback) {
        orb1_update((Orb1 *)orb->fallback, dt);
        return;
    }

    orb_gl_render(orb);
    orb->dirty = true;

    /*
        Refresh the pristine warp source from the
        freshly rendered orb.
    */
    if (g_orb_snap) {
        memcpy(g_orb_snap, orb->pixels,
               sizeof(Uint32) *
               ORB_RES * ORB_RES);
    }
}

void orb_draw(
    Orb *orb,
    SDL_Renderer *renderer,
    SDL_Rect dst)
{
    if (orb->fallback) {
        orb1_draw((Orb1 *)orb->fallback,
            renderer, dst);
        return;
    }

    if (!orb->texture || !orb->visible)
        return;

    orb_warp_tap(orb);

    if (orb->dirty) {
        void *pixels;
        int pitch;

        SDL_LockTexture(
            orb->texture,
            NULL,
            &pixels,
            &pitch);

        memcpy(
            pixels,
            orb->pixels,
            ORB_RES * ORB_RES * 4);

        SDL_UnlockTexture(orb->texture);
        orb->dirty = false;
    }

    SDL_RenderCopy(
        renderer,
        orb->texture,
        NULL,
        &dst);
}
