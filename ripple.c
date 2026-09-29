/*
    Widget tap ripple.

    Faithful CPU port of test/ripple.c: a widget is warped
    pixel-by-pixel by a damped radial sine wave.

      ripple    = amplitude * sin(freq * t') * exp(-decay * t')
      t'        = max(0, u_time - dist / u_speed)
      output    = texture(local + ripple * n)          (displacement)
      color.rgb += 0.3 * (ripple / amplitude) * a       (brightness)

    The already-rendered widget is read back from the
    framebuffer, warped, and drawn back in its place. Works
    for any rect: chat bubbles (tagged by message) and
    static panels/popups/buttons (matched by rect).
*/

#include "ripple.h"

#include <math.h>
#include <stdlib.h>

static UIRipple g_ripples[RIPPLE_MAX];

static bool same_rect(SDL_Rect a, SDL_Rect b)
{
    return a.x == b.x && a.y == b.y &&
           a.w == b.w && a.h == b.h;
}

static void ripple_take_slot(
    UIRipple *r,
    SDL_Rect region,
    float corner,
    int tag,
    int mx, int my,
    float amplitude)
{
    r->active = true;
    r->tag = tag;
    r->region = region;
    r->corner = corner;
    r->ox = (float)mx;
    r->oy = (float)my;
    r->amplitude = amplitude;
    r->startMs = SDL_GetTicks64();
    r->snap = NULL;
    r->snap_w = 0;
    r->snap_h = 0;
}

void ripple_tap(
    SDL_Rect region,
    float corner,
    int tag,
    int mx, int my)
{
    Uint64 now = SDL_GetTicks64();

    int slot = -1;
    Uint64 oldest = now;

    for (int k = 0; k < RIPPLE_MAX; k++) {
        if (!g_ripples[k].active) {
            slot = k;
            break;
        }
        if (g_ripples[k].startMs < oldest) {
            oldest = g_ripples[k].startMs;
            slot = k;
        }
    }

    if (slot < 0) return;

    ripple_take_slot(
        &g_ripples[slot],
        region, corner, tag, mx, my,
        28.0f);
}

void ripple_clear(void)
{
    for (int k = 0; k < RIPPLE_MAX; k++) {
        free(g_ripples[k].snap);
        g_ripples[k].snap = NULL;
        g_ripples[k].active = false;
    }
}

/*
    Coverage ramp for the widget's rounded-corner
    shape, mirroring the u_corner SDF in
    test/ripple.c: output pixels outside the rounded
    rect carry zero alpha.
*/
static float rounded_rect_cov(
    SDL_Rect r,
    float radius,
    float px, float py)
{
    float hw = (float)r.w * 0.5f;
    float hh = (float)r.h * 0.5f;
    float cx = (float)r.x + hw;
    float cy = (float)r.y + hh;

    float qx = fabsf(px - cx) - (hw - radius);
    float qy = fabsf(py - cy) - (hh - radius);

    float ox = qx > 0.0f ? qx : 0.0f;
    float oy = qy > 0.0f ? qy : 0.0f;

    float outside =
        sqrtf(ox * ox + oy * oy) +
        fminf(fmaxf(qx, qy), 0.0f) - radius;

    float cov = 0.5f - outside;
    if (cov > 1.0f) cov = 1.0f;
    if (cov < 0.0f) cov = 0.0f;
    return cov;
}

/*
    Reusable storage for the ripple warp (grows to
    the biggest widget seen).
*/
static SDL_Texture *g_rip_tex = NULL;
static int g_rip_tex_w = 0;
static int g_rip_tex_h = 0;
static Uint32 *g_rip_out = NULL;
static int g_rip_out_alloc = 0;

/*
    Bilinear tap into the read-back widget pixels
    (ARGB8888). Mirrors GL_LINEAR sampling.
*/
static Uint32 ripple_sample_bilinear(
    const Uint32 *px, int w, int h,
    float x, float y)
{
    if (x < 0.0f) x = 0.0f;
    else if (x > (float)(w - 1)) x = (float)(w - 1);
    if (y < 0.0f) y = 0.0f;
    else if (y > (float)(h - 1)) y = (float)(h - 1);

    int x0 = (int)x;
    int y0 = (int)y;
    int x1 = x0 + 1 < w ? x0 + 1 : x0;
    int y1 = y0 + 1 < h ? y0 + 1 : y0;

    float fx = x - (float)x0;
    float fy = y - (float)y0;

    Uint32 c00 = px[y0 * w + x0];
    Uint32 c01 = px[y0 * w + x1];
    Uint32 c10 = px[y1 * w + x0];
    Uint32 c11 = px[y1 * w + x1];

    float r =
        (float)((c00 >> 16) & 255) * (1.0f - fx) * (1.0f - fy) +
        (float)((c01 >> 16) & 255) * fx * (1.0f - fy) +
        (float)((c10 >> 16) & 255) * (1.0f - fx) * fy +
        (float)((c11 >> 16) & 255) * fx * fy;
    float g =
        (float)((c00 >> 8) & 255) * (1.0f - fx) * (1.0f - fy) +
        (float)((c01 >> 8) & 255) * fx * (1.0f - fy) +
        (float)((c10 >> 8) & 255) * (1.0f - fx) * fy +
        (float)((c11 >> 8) & 255) * fx * fy;
    float b =
        (float)(c00 & 255) * (1.0f - fx) * (1.0f - fy) +
        (float)(c01 & 255) * fx * (1.0f - fy) +
        (float)(c10 & 255) * (1.0f - fx) * fy +
        (float)(c11 & 255) * fx * fy;

    return 0xFF000000u |
        ((Uint32)(r + 0.5f) << 16) |
        ((Uint32)(g + 0.5f) << 8) |
        (Uint32)(b + 0.5f);
}

/*
    Compute the shader pixel for one ripple.
*/
static Uint32 ripple_pixel(
    const Uint32 *src, int pw, int ph,
    float px, float py,
    int ox, int oy,
    float t, float speedInv,
    SDL_Rect region, float corner,
    float amplitude)
{
    float dx = px - (float)ox;
    float dy = py - (float)oy;
    float dist = sqrtf(dx * dx + dy * dy);

    float local = t - dist * speedInv;

    Uint32 out;

    if (local <= 0.0f) {
        /* Wave has not arrived yet: untouched. */
        int ix = (int)(px - (float)region.x);
        int iy = (int)(py - (float)region.y);
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;
        if (ix > pw - 1) ix = pw - 1;
        if (iy > ph - 1) iy = ph - 1;
        out = src[iy * pw + ix];
    } else {
        float ripple =
            amplitude * sinf(15.0f * local) *
            expf(-8.0f * local);

        float nx = dist > 0.001f ? dx / dist : 0.0f;
        float ny = dist > 0.001f ? dy / dist : 0.0f;

        /* texture(local + ripple * n) */
        Uint32 s = ripple_sample_bilinear(
            src, pw, ph,
            px + ripple * nx - (float)region.x,
            py + ripple * ny - (float)region.y);

        /* color.rgb += 0.3 * (ripple / amplitude) * a */
        float sa = (float)((s >> 24) & 255);
        float bright =
            0.3f * (ripple / amplitude) * (sa / 255.0f);

        float r = (float)((s >> 16) & 255) + bright;
        float g = (float)((s >> 8) & 255) + bright;
        float b = (float)(s & 255) + bright;

        if (r < 0.0f) r = 0.0f; else if (r > 255.0f) r = 255.0f;
        if (g < 0.0f) g = 0.0f; else if (g > 255.0f) g = 255.0f;
        if (b < 0.0f) b = 0.0f; else if (b > 255.0f) b = 255.0f;

        out =
            ((Uint32)(r + 0.5f) << 16) |
            ((Uint32)(g + 0.5f) << 8) |
            (Uint32)(b + 0.5f);
    }

    float cov = rounded_rect_cov(region, corner, px, py);

    Uint32 a = (Uint32)(cov * 255.0f + 0.5f);
    if (a > 255) a = 255;

    return (a << 24) | (out & 0x00FFFFFFu);
}

/*
    Bilinear tap that preserves alpha, for translucent
    sources like the orb texture.
*/
static Uint32 ripple_sample_alpha(
    const Uint32 *px, int w, int h,
    float x, float y)
{
    if (x < 0.0f) x = 0.0f;
    else if (x > (float)(w - 1)) x = (float)(w - 1);
    if (y < 0.0f) y = 0.0f;
    else if (y > (float)(h - 1)) y = (float)(h - 1);

    int x0 = (int)x;
    int y0 = (int)y;
    int x1 = x0 + 1 < w ? x0 + 1 : x0;
    int y1 = y0 + 1 < h ? y0 + 1 : y0;

    float fx = x - (float)x0;
    float fy = y - (float)y0;

    Uint32 c00 = px[y0 * w + x0];
    Uint32 c01 = px[y0 * w + x1];
    Uint32 c10 = px[y1 * w + x0];
    Uint32 c11 = px[y1 * w + x1];

    float inv = (1.0f - fx) * (1.0f - fy);
    float r =
        (float)((c00 >> 16) & 255) * inv +
        (float)((c01 >> 16) & 255) * fx * (1.0f - fy) +
        (float)((c10 >> 16) & 255) * (1.0f - fx) * fy +
        (float)((c11 >> 16) & 255) * fx * fy;
    float g =
        (float)((c00 >> 8) & 255) * inv +
        (float)((c01 >> 8) & 255) * fx * (1.0f - fy) +
        (float)((c10 >> 8) & 255) * (1.0f - fx) * fy +
        (float)((c11 >> 8) & 255) * fx * fy;
    float b =
        (float)(c00 & 255) * inv +
        (float)(c01 & 255) * fx * (1.0f - fy) +
        (float)(c10 & 255) * (1.0f - fx) * fy +
        (float)(c11 & 255) * fx * fy;
    float a =
        (float)((c00 >> 24) & 255) * inv +
        (float)((c01 >> 24) & 255) * fx * (1.0f - fy) +
        (float)((c10 >> 24) & 255) * (1.0f - fx) * fy +
        (float)((c11 >> 24) & 255) * fx * fy;

    return
        ((Uint32)(a + 0.5f) << 24) |
        ((Uint32)(r + 0.5f) << 16) |
        ((Uint32)(g + 0.5f) << 8) |
        (Uint32)(b + 0.5f);
}

Uint32 ripple_warp_tex(
    const Uint32 *src, int w, int h,
    float x, float y,
    float ox, float oy,
    float t,
    float amplitude)
{
    const float speed = 1200.0f;
    const float speedInv = 1.0f / speed;

    float dx = x - ox;
    float dy = y - oy;
    float dist = sqrtf(dx * dx + dy * dy);

    float local = t - dist * speedInv;

    if (local <= 0.0f) {
        /* Wave has not arrived yet: untouched. */
        int ix = (int)x;
        int iy = (int)y;
        if (ix < 0) ix = 0;
        else if (ix > w - 1) ix = w - 1;
        if (iy < 0) iy = 0;
        else if (iy > h - 1) iy = h - 1;
        return src[iy * w + ix];
    }

    float ripple =
        amplitude * sinf(15.0f * local) *
        expf(-8.0f * local);

    float nx = dist > 0.001f ? dx / dist : 0.0f;
    float ny = dist > 0.001f ? dy / dist : 0.0f;

    /* texture(local + ripple * n) */
    Uint32 s = ripple_sample_alpha(
        src, w, h,
        x + ripple * nx,
        y + ripple * ny);

    /* color.rgb += 0.3 * (ripple / amplitude) * a */
    float sa = (float)((s >> 24) & 255);
    float bright =
        0.3f * (ripple / amplitude) * (sa / 255.0f);

    float r = (float)((s >> 16) & 255) + bright;
    float g = (float)((s >> 8) & 255) + bright;
    float b = (float)(s & 255) + bright;

    if (r < 0.0f) r = 0.0f; else if (r > 255.0f) r = 255.0f;
    if (g < 0.0f) g = 0.0f; else if (g > 255.0f) g = 255.0f;
    if (b < 0.0f) b = 0.0f; else if (b > 255.0f) b = 255.0f;

    Uint8 aa = (Uint8)(sa + 0.5f);

    return
        ((Uint32)aa << 24) |
        ((Uint32)(r + 0.5f) << 16) |
        ((Uint32)(g + 0.5f) << 8) |
        (Uint32)(b + 0.5f);
}

/*
    Warp the widget for its first matching active ripple
    and draw the result back over it. Exact port of
    ripple_draw() from test/ripple.c: read the rendered
    widget back, displace it, add the wave brightness.
*/
void ripple_draw(
    SDL_Renderer *renderer,
    SDL_Rect region,
    int tag)
{
    Uint64 now = SDL_GetTicks64();

    const float speed = 1200.0f;
    const float speedInv = 1.0f / speed;

    UIRipple *cur = NULL;
    float t = 0.0f;
    int ox = 0, oy = 0;

    for (int k = 0; k < RIPPLE_MAX; k++) {
        UIRipple *r = &g_ripples[k];
        if (!r->active)
            continue;

        if (tag == RIPPLE_TAG_REGION) {
            if (r->tag != RIPPLE_TAG_REGION)
                continue;
            if (!same_rect(r->region, region))
                continue;
        } else {
            if (r->tag != tag)
                continue;
        }

        double ms = (double)(now - r->startMs);
        float tt = (float)(ms / 1000.0);

        if (tt >= RIPPLE_DURATION) {
            /* Expired: reclaim the snapshot. */
            free(r->snap);
            r->snap = NULL;
            r->active = false;
            continue;
        }

        if (tt <= 0.0f)
            continue;

        cur = r;
        t = tt;
        ox = (int)r->ox;
        oy = (int)r->oy;
        break;
    }

    if (!cur)
        return;

    if (region.w < 1 || region.h < 1)
        return;

    int pw = region.w;
    int ph = region.h;

    /*
        Large widgets are warped at half resolution:
        the wave is soft, and this keeps the extra
        CPU cost of the correction bounded.
    */
    int stride = (pw * ph > 90000) ? 2 : 1;
    int ow = (pw + stride - 1) / stride;
    int oh = (ph + stride - 1) / stride;

    if (ow * oh > g_rip_out_alloc) {
        Uint32 *nb = realloc(
            g_rip_out, (size_t)(ow * oh) * sizeof(Uint32));
        if (!nb) return;
        g_rip_out = nb;
        g_rip_out_alloc = ow * oh;
    }

    /*
        First frame only: snapshot the freshly-rendered,
        pristine widget. Every later frame warps this
        snapshot, never the previous warped output.
    */
    if (cur->snap == NULL ||
        cur->snap_w != pw ||
        cur->snap_h != ph) {

        Uint32 *nb = realloc(
            cur->snap,
            (size_t)(pw * ph) * sizeof(Uint32));
        if (!nb) return;

        cur->snap = nb;
        cur->snap_w = pw;
        cur->snap_h = ph;

        if (SDL_RenderReadPixels(
                renderer, &region,
                SDL_PIXELFORMAT_ARGB8888,
                cur->snap,
                pw * 4) != 0)
            return;
    }

    const Uint32 *src = cur->snap;
    Uint32 *dst = g_rip_out;

    for (int sy = 0; sy < oh; sy++) {
        float py = (float)(region.y + sy * stride);

        for (int sx = 0; sx < ow; sx++) {
            float px = (float)(region.x + sx * stride);

            dst[sy * ow + sx] = ripple_pixel(
                src, pw, ph, px, py,
                ox, oy, t, speedInv,
                region, cur->corner,
                cur->amplitude);
        }
    }

    if (!g_rip_tex ||
        g_rip_tex_w != ow || g_rip_tex_h != oh) {
        if (g_rip_tex)
            SDL_DestroyTexture(g_rip_tex);

        g_rip_tex = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, ow, oh);

        g_rip_tex_w = ow;
        g_rip_tex_h = oh;
    }

    if (!g_rip_tex)
        return;

    SDL_UpdateTexture(
        g_rip_tex, NULL, dst, ow * 4);

    SDL_SetTextureBlendMode(
        g_rip_tex, SDL_BLENDMODE_BLEND);

    SDL_SetHint(
        SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_RenderCopy(
        renderer, g_rip_tex, NULL, &region);
    SDL_SetHint(
        SDL_HINT_RENDER_SCALE_QUALITY, "0");
}

void ripple_free(void)
{
    if (g_rip_tex) {
        SDL_DestroyTexture(g_rip_tex);
        g_rip_tex = NULL;
    }
    free(g_rip_out);
    g_rip_out = NULL;
    g_rip_out_alloc = 0;
    g_rip_tex_w = 0;
    g_rip_tex_h = 0;

    for (int k = 0; k < RIPPLE_MAX; k++) {
        free(g_ripples[k].snap);
        g_ripples[k].snap = NULL;
    }
}