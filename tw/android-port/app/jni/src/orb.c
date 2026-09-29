#include "orb.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void render_orb_line(
    int y,
    Uint32 *pixels,
    int width,
    int height,
    float time,
    float audio)
{
    float radius = 0.285f;
    radius += audio * 0.018f;
    float inv_radius = 1.0f / radius;

    float ld_len = sqrtf(
        0.35f * 0.35f +
        0.45f * 0.45f +
        1.0f);

    float ldx = -0.35f / ld_len;
    float ldy = -0.45f / ld_len;
    float ldz = 1.0f / ld_len;

    float v = (float)y / (float)height;
    float py = v - 0.5f;
    float max_reach = radius + 0.06f;
    float py_abs = py < 0.0f ? -py : py;

    if (py_abs > max_reach)
        return;

    float py2 = py * py;

    for (int x = 0; x < width; x++) {

        float u = (float)x / (float)width;
        float px = u - 0.5f;

        float pr2 = px * px + py2;

        if (pr2 > max_reach * max_reach)
            continue;

        float r = sqrtf(pr2);

        /*
            Anti-aliased sphere coverage.

            The band is ~4.5 texels wide at
            ORB_RES so the boundary stays
            smooth through linear scaling.
        */
        float inside = 1.0f - clampf(
            (r - radius) * 60.0f,
            0.0f, 1.0f);

        /*
            Outside the sphere there is nothing
            to draw — skipping keeps the buffer
            transparent without hard cuts.
        */
        if (inside < 0.001f)
            continue;

        float t = time;
        float qx = px * inv_radius;
        float qy = py * inv_radius;

        float qlen2 = qx * qx + qy * qy;
        float omq = 1.0f - qlen2;
        float sphereZ = sqrtf(omq > 0.0f ? omq : 0.0f);

        float flow_x =
            qx + sinf(qy * 4.0f + t * 0.75f) * 0.10f;
        float flow_y =
            qy + cosf(qx * 5.0f - t * 0.62f) * 0.08f;

        float n1 =
            sinf(flow_x * 3.7f + flow_y * 2.3f + t * 0.4f) * 0.5f +
            sinf(flow_x * 1.9f - flow_y * 4.1f - t * 0.3f) * 0.25f + 0.375f;

        float n2 =
            cosf(flow_x * 5.1f + flow_y * 1.7f - t * 0.5f) * 0.5f +
            cosf(flow_x * 2.3f - flow_y * 3.8f + t * 0.2f) * 0.25f + 0.375f;

        float wave1 = sinf(
            qx * 4.0f + qy * 2.0f +
            t * 1.25f + n1 * 2.2f);

        float ribbon1 =
            expf(-fabsf(wave1) * 2.8f);

        float wave2 = sinf(
            qx * 7.0f - qy * 3.0f -
            t * 1.6f + n2 * 3.0f);

        float ribbon2 =
            expf(-fabsf(wave2) * 4.0f);

        float colorPos =
            qx * 0.38f + qy * 0.25f +
            n1 * 0.55f + t * 0.035f;

        float c1r, c1g, c1b;
        float c2r, c2g, c2b;

        {
            float t2 = colorPos - floorf(colorPos);
            float mr = 1.00f, mg = 0.02f, mb = 0.42f;
            float pr = 0.55f, pg = 0.05f, pb = 1.00f;
            float br = 0.04f, bg2 = 0.20f, bb = 1.00f;
            float cr = 0.00f, cg = 0.85f, cb = 1.00f;
            float pir = 1.00f, pig = 0.30f, pib = 0.75f;

            if (t2 < 0.20f) {
                float s = t2 * 5.0f;
                c1r = mr + (pr - mr) * s;
                c1g = mg + (pg - mg) * s;
                c1b = mb + (pb - mb) * s;
            } else if (t2 < 0.45f) {
                float s = (t2 - 0.20f) * 4.0f;
                c1r = pr + (br - pr) * s;
                c1g = pg + (bg2 - pg) * s;
                c1b = pb + (bb - pb) * s;
            } else if (t2 < 0.70f) {
                float s = (t2 - 0.45f) * 4.0f;
                c1r = br + (cr - br) * s;
                c1g = bg2 + (cg - bg2) * s;
                c1b = bb + (cb - bb) * s;
            } else {
                float s = (t2 - 0.70f) * 3.333f;
                c1r = cr + (pir - cr) * s;
                c1g = cg + (pig - cg) * s;
                c1b = cb + (pib - cb) * s;
            }
        }

        {
            float t2 = colorPos + 0.37f;
            t2 = t2 - floorf(t2);
            float mr = 1.00f, mg = 0.02f, mb = 0.42f;
            float pr = 0.55f, pg = 0.05f, pb = 1.00f;
            float br = 0.04f, bg2 = 0.20f, bb = 1.00f;
            float cr = 0.00f, cg = 0.85f, cb = 1.00f;
            float pir = 1.00f, pig = 0.30f, pib = 0.75f;

            if (t2 < 0.20f) {
                float s = t2 * 5.0f;
                c2r = mr + (pr - mr) * s;
                c2g = mg + (pg - mg) * s;
                c2b = mb + (pb - mb) * s;
            } else if (t2 < 0.45f) {
                float s = (t2 - 0.20f) * 4.0f;
                c2r = pr + (br - pr) * s;
                c2g = pg + (bg2 - pg) * s;
                c2b = pb + (bb - pb) * s;
            } else if (t2 < 0.70f) {
                float s = (t2 - 0.45f) * 4.0f;
                c2r = br + (cr - br) * s;
                c2g = bg2 + (cg - bg2) * s;
                c2b = bb + (cb - bb) * s;
            } else {
                float s = (t2 - 0.70f) * 3.333f;
                c2r = cr + (pir - cr) * s;
                c2g = cg + (pig - cg) * s;
                c2b = cb + (pib - cb) * s;
            }
        }

        float r2b = ribbon2 * 0.50f;
        float color_r = c1r + (c2r - c1r) * r2b;
        float color_g = c1g + (c2g - c1g) * r2b;
        float color_b = c1b + (c2b - c1b) * r2b;

        float intensity =
            (ribbon1 * 0.95f + ribbon2 * 0.45f)
            * (0.75f + audio * 0.90f);

        float inv_len =
            1.0f / sqrtf(
                qlen2 +
                sphereZ * sphereZ +
                0.0001f);

        float nz = sphereZ * inv_len;

        float diffuse =
            qx * inv_len * ldx +
            qy * inv_len * ldy +
            nz * ldz;

        if (diffuse < 0.0f)
            diffuse = 0.0f;

        float lighting = 0.72f + diffuse * 0.55f;
        color_r *= lighting;
        color_g *= lighting;
        color_b *= lighting;

        float nz_c = nz > 0.0f ? nz : 0.0f;
        float f_t = 1.0f - nz_c;
        float fresnel =
            f_t * f_t * f_t * sqrtf(f_t);

        color_r += 0.188f * fresnel;
        color_g += 0.488f * fresnel;
        color_b += 0.750f * fresnel;

        color_r *= intensity;
        color_g *= intensity;
        color_b *= intensity;

        /*
            Soft alpha from the coverage ramp
            only — never punched out by color
            brightness, so the silhouette stays
            smooth while the ribbons move.
        */
        float alpha = inside;

        float final_r = color_r * inside;
        float final_g = color_g * inside;
        float final_b = color_b * inside;

        float edge = clampf(
            (r - (radius - 0.035f)) * 28.571f,
            0.0f, 1.0f);

        float ei = edge * 0.55f * inside;
        final_r += 0.25f * ei;
        final_g += 0.55f * ei;
        final_b += 1.00f * ei;

        float r2 = r * r;

        float glow =
            expf(-r2 * 12.0f) * inside;

        float centerGlow =
            expf(-r2 * 23.0f) * inside;

        final_r += glow * 0.29f + centerGlow * 1.8f;
        final_g += glow * 0.49f + centerGlow * 0.99f;
        final_b += glow * 0.65f + centerGlow * 1.56f;

        final_r *= intensity;
        final_g *= intensity;
        final_b *= intensity;

        if (final_r < 0.0f) final_r = 0.0f;
        if (final_g < 0.0f) final_g = 0.0f;
        if (final_b < 0.0f) final_b = 0.0f;

        if (final_r > 1.0f) final_r = 1.0f;
        if (final_g > 1.0f) final_g = 1.0f;
        if (final_b > 1.0f) final_b = 1.0f;

        Uint8 a =
            (Uint8)(clampf(
                alpha * 255.0f, 0.0f, 255.0f));

        /*
            NOTE: no brightness-based alpha
            culling here — dimming the rim by
            color is what caused a jagged,
            flickering boundary. The coverage
            ramp above owns the edge entirely.
        */
        pixels[y * width + x] =
            (a << 24) |
            ((Uint8)(final_r * 255.0f) << 16) |
            ((Uint8)(final_g * 255.0f) << 8) |
            (Uint8)(final_b * 255.0f);
    }
}

void orb_init(
    Orb *orb,
    SDL_Renderer *renderer)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

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

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    orb->time = 0.0f;
    orb->audio = 0.0f;
    orb->dirty = true;
    orb->visible = true;

    memset(
        orb->pixels,
        0,
        sizeof(orb->pixels));
}

void orb_free(Orb *orb)
{
    if (orb->texture) {
        SDL_DestroyTexture(orb->texture);
        orb->texture = NULL;
    }
}

void orb_update(Orb *orb, float dt)
{
    orb->time += dt;

    orb->audio =
        0.50f
        + 0.25f * sinf(orb->time * 2.1f)
        + 0.15f * sinf(orb->time * 5.7f)
        + 0.08f * sinf(orb->time * 11.0f);

    if (orb->audio < 0.0f)
        orb->audio = 0.0f;

    if (orb->audio > 1.0f)
        orb->audio = 1.0f;

    for (int y = 0; y < ORB_RES; y++) {
        render_orb_line(
            y,
            orb->pixels,
            ORB_RES,
            ORB_RES,
            orb->time,
            orb->audio);
    }

    orb->dirty = true;
}

void orb_draw(
    Orb *orb,
    SDL_Renderer *renderer,
    SDL_Rect dst)
{
    if (!orb->texture || !orb->visible)
        return;

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
