#include "checkbox.h"
#include <math.h>

void checkbox_init(
    UICheckBox *checkbox,
    const char *text,
    bool checked)
{
    checkbox->text = text;
    checkbox->checked = checked;
    checkbox->toggle_anim =
        checked ? 1.0f : 0.0f;
}

void checkbox_layout(
    UICheckBox *checkbox,
    UIContext *ui,
    float x,
    float y)
{
    checkbox->rect =
        ui_rect(
            ui,
            x,
            y,
            260,
            36
        );
}

void checkbox_event(
    UICheckBox *checkbox,
    SDL_Event *event)
{
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
                checkbox->rect
            )
        ) {
            checkbox->checked =
                !checkbox->checked;
        }
    }
}

void checkbox_draw(
    UICheckBox *checkbox,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    float dt)
{
    float target =
        checkbox->checked ? 1.0f : 0.0f;

    checkbox->toggle_anim +=
        (target - checkbox->toggle_anim) *
        12.0f * dt;

    if (checkbox->toggle_anim < 0.001f)
        checkbox->toggle_anim = 0.0f;

    if (checkbox->toggle_anim > 0.999f)
        checkbox->toggle_anim = 1.0f;

    float t = ui_ease_out_cubic(
        checkbox->toggle_anim);

    int size =
        (int)roundf(26.0f * ui->scale);

    if (size < 18)
        size = 18;

    SDL_Rect box = {
        checkbox->rect.x,
        checkbox->rect.y +
            (checkbox->rect.h - size) / 2,
        size,
        size
    };

    ui_glass(
        renderer,
        box,
        (int)roundf(7.0f * ui->scale),
        t > 0.01f,
        ui->dark);

    if (t > 0.01f) {

        SDL_SetRenderDrawBlendMode(
            renderer,
            SDL_BLENDMODE_BLEND);

        /*
            Checkmark glow.
        */
        Uint8 glow_a =
            (Uint8)(60 * t);

        SDL_SetRenderDrawColor(
            renderer,
            50, 105, 235, glow_a);

        int gx1 =
            box.x + (int)(size * 0.18f);
        int gy1 =
            box.y + (int)(size * 0.48f);
        int gx2 =
            box.x + (int)(size * 0.45f);
        int gy2 =
            box.y + (int)(size * 0.76f);
        int gx3 =
            box.x + (int)(size * 0.82f);
        int gy3 =
            box.y + (int)(size * 0.24f);

        /*
            Animate checkmark as a wipe from left
            to right using t as progress.
        */

        float seg1_end = 0.55f;
        float seg2_end = 1.0f;

        if (t > 0.01f) {

            float seg1_t =
                t < seg1_end
                ? t / seg1_end
                : 1.0f;

            int mx =
                gx1 +
                (int)((gx2 - gx1) * seg1_t);
            int my =
                gy1 +
                (int)((gy2 - gy1) * seg1_t);

            SDL_RenderDrawLine(
                renderer,
                gx1 - 1, gy1,
                mx, my + 1);
            SDL_RenderDrawLine(
                renderer,
                gx1 + 1, gy1 + 1,
                mx + 1, my);

            if (t > seg1_end) {

                float seg2_t =
                    (t - seg1_end) /
                    (seg2_end - seg1_end);

                int sx =
                    gx2 +
                    (int)(
                        (gx3 - gx2) *
                        seg2_t);
                int sy =
                    gy2 +
                    (int)(
                        (gy3 - gy2) *
                        seg2_t);

                SDL_RenderDrawLine(
                    renderer,
                    gx2, gy2 + 1,
                    sx, sy - 1);
                SDL_RenderDrawLine(
                    renderer,
                    gx2 + 1, gy2,
                    sx - 1, sy + 1);
            }
        }

        /*
            Checkmark core (two-pass for thickness).
        */
        Uint8 core_a =
            (Uint8)(255 * t);

        SDL_SetRenderDrawColor(
            renderer,
            45, 100, 230, core_a);

        int x1 =
            box.x + (int)(size * 0.22f);
        int y1 =
            box.y + (int)(size * 0.52f);
        int x2 =
            box.x + (int)(size * 0.43f);
        int y2 =
            box.y + (int)(size * 0.73f);
        int x3 =
            box.x + (int)(size * 0.78f);
        int y3 =
            box.y + (int)(size * 0.27f);

        if (t > 0.01f) {

            float seg1_t =
                t < seg1_end
                ? t / seg1_end
                : 1.0f;

            int mx =
                x1 +
                (int)((x2 - x1) * seg1_t);
            int my =
                y1 +
                (int)((y2 - y1) * seg1_t);

            SDL_RenderDrawLine(
                renderer, x1, y1, mx, my);

            if (t > seg1_end) {

                float seg2_t =
                    (t - seg1_end) /
                    (seg2_end - seg1_end);

                int sx =
                    x2 +
                    (int)(
                        (x3 - x2) *
                        seg2_t);
                int sy =
                    y2 +
                    (int)(
                        (y3 - y2) *
                        seg2_t);

                SDL_RenderDrawLine(
                    renderer,
                    x2, y2, sx, sy);
            }

            Uint8 hi_a =
                (Uint8)(200 * t);

            SDL_SetRenderDrawColor(
                renderer,
                90, 140, 250, hi_a);

            SDL_RenderDrawLine(
                renderer,
                x1 + 1, y1 - 1,
                mx, my);

            if (t > seg1_end) {

                float seg2_t =
                    (t - seg1_end) /
                    (seg2_end - seg1_end);

                int sx =
                    x2 +
                    (int)(
                        (x3 - x2) *
                        seg2_t);
                int sy =
                    y2 +
                    (int)(
                        (y3 - y2) *
                        seg2_t);

                SDL_RenderDrawLine(
                    renderer,
                    x2, y2,
                    sx - 1, sy + 1);
            }
        }
    }

    ui_text(
        renderer,
        font,
        checkbox->text,
        checkbox->rect.x +
            size +
            (int)roundf(10.0f * ui->scale),
        checkbox->rect.y +
            (int)roundf(6.0f * ui->scale),
        ui_theme(ui->dark,
            (UIColor){25, 35, 55, 255},
            (UIColor){195, 205, 225, 255}
        ));
}
