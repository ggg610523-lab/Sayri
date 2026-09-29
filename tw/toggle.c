#include "toggle.h"
#include <math.h>

void toggle_init(
    UIToggle *toggle,
    bool on)
{
    toggle->on = on;
    toggle->clicked = false;
    toggle->thumb_x = on ? 1.0f : 0.0f;
    toggle->anim = on ? 1.0f : 0.0f;
}

void toggle_layout(
    UIToggle *toggle,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h)
{
    toggle->rect =
        ui_rect(ui, x, y, w, h);
}

void toggle_event(
    UIToggle *toggle,
    SDL_Event *event)
{
    toggle->clicked = false;

    if (
        event->type ==
        SDL_MOUSEBUTTONDOWN &&
        event->button.button ==
        SDL_BUTTON_LEFT
    ) {
        if (
            ui_point_in_rect(
                event->button.x,
                event->button.y,
                toggle->rect
            )
        ) {
            toggle->on = !toggle->on;
            toggle->clicked = true;
        }
    }
}

void toggle_draw(
    UIToggle *toggle,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font)
{
    float target = toggle->on ? 1.0f : 0.0f;
    float diff = target - toggle->anim;

    if (fabsf(diff) < 0.001f)
        toggle->anim = target;
    else
        toggle->anim += diff * 0.25f;

    toggle->thumb_x = toggle->anim;

    int rx = toggle->rect.x;
    int ry = toggle->rect.y;
    int rw = toggle->rect.w;
    int rh = toggle->rect.h;

    int radius = rh / 2;

    SDL_Rect track = {
        rx, ry, rw, rh
    };

    UIColor trackColor =
        ui_theme(ui->dark,
            toggle->on
                ? (UIColor){80, 160, 255, 255}
                : (UIColor){200, 205, 215, 255},
            toggle->on
                ? (UIColor){70, 140, 240, 255}
                : (UIColor){55, 60, 80, 255}
        );

    /*
        SDF-covered fills: the plain scanline
        versions quantize the corner arcs into
        visible stairs at this widget's size.
    */
    ui_fill_rounded_rect_smooth(
        renderer,
        track,
        radius,
        trackColor
    );

    if (toggle->on) {

        ui_fill_rounded_rect_smooth(
            renderer,
            track,
            radius,
            (UIColor){255, 255, 255, 25}
        );
    }

    float thumbPad =
        roundf(3.0f * ui->scale);

    float thumbR =
        (float)radius - thumbPad;

    float thumbCX =
        (float)rx + thumbPad + thumbR +
        toggle->thumb_x *
        ((float)(rw - 2 * (int)thumbPad) -
         2.0f * thumbR);

    float thumbCY =
        (float)ry + (float)rh * 0.5f;

    int tcx = (int)roundf(thumbCX);
    int tcy = (int)roundf(thumbCY);
    int tr  = (int)roundf(thumbR);

    ui_fill_circle_smooth(
        renderer,
        tcx, tcy, tr,
        (UIColor){255, 255, 255, 255}
    );

    /*
        Glossy sheen only on large thumbs —
        on small ones it reads as smear.

        Painted as a per-pixel intersection of
        two SDF coverages — highlight disc and
        thumb interior — so its lower edge is a
        soft gradient and it never bleeds past
        the anti-aliased rim of the thumb.
    */
    if (thumbR > 9.0f) {

        float hlR = thumbR - 1.5f;

        float hlCY =
            thumbCY - hlR * 0.55f;

        SDL_SetRenderDrawBlendMode(
            renderer,
            SDL_BLENDMODE_BLEND);

        for (int y = tcy - tr;
             y <= tcy; y++) {
            for (int x = tcx - tr;
                 x <= tcx + tr; x++) {

                float hdx =
                    (float)(x) - thumbCX;

                float hdy =
                    (float)(y) - hlCY;

                float dhl =
                    sqrtf(hdx * hdx +
                          hdy * hdy) - hlR;

                float cov_hl = 0.5f - dhl;

                if (cov_hl <= 0.0f)
                    continue;

                float tdx =
                    (float)(x) - thumbCX;

                float tdy =
                    (float)(y) - thumbCY;

                float dtm =
                    sqrtf(tdx * tdx +
                          tdy * tdy) -
                    thumbR;

                float cov_tm = 0.5f - dtm;

                if (cov_tm <= 0.0f)
                    continue;

                float cov =
                    cov_hl < cov_tm
                        ? cov_hl : cov_tm;

                if (cov > 1.0f)
                    cov = 1.0f;

                SDL_SetRenderDrawColor(
                    renderer,
                    255, 255, 255,
                    (Uint8)(60.0f * cov));

                SDL_RenderDrawPoint(
                    renderer, x, y);
            }
        }
    }

    ui_text(
        renderer,
        font,
        toggle->on ? "Dark" : "Light",
        rx + rw + (int)(12.0f * ui->scale),
        ry + (int)(5.0f * ui->scale),
        ui_theme(ui->dark,
            (UIColor){25, 35, 55, 255},
            (UIColor){195, 205, 225, 255}
        ));
}
