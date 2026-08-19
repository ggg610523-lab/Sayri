#include "hamburger.h"
#include <math.h>

void hamburger_init(
    UIHamburger *hb)
{
    hb->open = false;
    hb->clicked = false;
    hb->anim = 0.0f;
}

void hamburger_layout(
    UIHamburger *hb,
    UIContext *ui,
    float x,
    float y,
    float size)
{
    hb->rect =
        ui_rect(
            ui,
            x,
            y,
            size,
            size
        );
}

void hamburger_event(
    UIHamburger *hb,
    SDL_Event *event)
{
    hb->clicked = false;

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
                hb->rect
            )
        ) {
            hb->open = !hb->open;
            hb->clicked = true;
        }
    }
}

void hamburger_draw(
    UIHamburger *hb,
    UIContext *ui,
    SDL_Renderer *renderer,
    float dt)
{
    float target =
        hb->open ? 1.0f : 0.0f;

    hb->anim +=
        (target - hb->anim) *
        10.0f * dt;

    if (hb->anim < 0.001f)
        hb->anim = 0.0f;

    if (hb->anim > 0.999f)
        hb->anim = 1.0f;

    float t = ui_ease_in_out_cubic(hb->anim);

    int radius =
        (int)roundf(
            12.0f * ui->scale
        );

    ui_glass(
        renderer,
        hb->rect,
        radius,
        hb->anim > 0.01f,
        ui->dark
    );

    int cx =
        hb->rect.x + hb->rect.w / 2;

    int cy =
        hb->rect.y + hb->rect.h / 2;

    int lineW =
        (int)roundf(16.0f * ui->scale);

    int halfW = lineW / 2;

    int gap =
        (int)roundf(5.0f * ui->scale);

    int lw =
        (int)roundf(2.0f * ui->scale);

    if (lw < 2)
        lw = 2;

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(
        renderer,
        50, 60, 85, 220);

    /*
        Animate the three lines from hamburger bars
        to an X shape.
    */

    float bar_y0 = (float)(cy - gap);
    float bar_y1 = (float)cy;
    float bar_y2 = (float)(cy + gap);

    float x_y0 =
        bar_y0 * (1.0f - t) +
        (float)cy * t;

    float x_y1 =
        bar_y1;

    float x_y2 =
        bar_y2 * (1.0f - t) +
        (float)cy * t;

    float x_x0_offset =
        0.0f * (1.0f - t) +
        (float)(-halfW) * t;

    float x_x2_offset =
        0.0f * (1.0f - t) +
        (float)(halfW) * t;

    struct {
        float y;
        float x_off;
    } lines[3] = {
        { x_y0, x_x0_offset },
        { x_y1, 0.0f },
        { x_y2, x_x2_offset }
    };

    for (int i = 0; i < 3; i++) {

        int ly = (int)roundf(lines[i].y);
        int lx_off =
            (int)roundf(lines[i].x_off);

        int x1 = cx - halfW + lx_off;
        int x2 = cx + halfW + lx_off;

        if (i == 0) {
            x1 = cx - halfW + lx_off;
            x2 = cx + halfW + lx_off;
        }

        if (i == 2) {
            x1 = cx - halfW + lx_off;
            x2 = cx + halfW + lx_off;
        }

        SDL_RenderDrawLine(
            renderer,
            x1, ly,
            x2, ly
        );

        if (lw > 2) {

            SDL_RenderDrawLine(
                renderer,
                x1, ly + 1,
                x2, ly + 1
            );
        }
    }
}
