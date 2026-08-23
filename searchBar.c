#include "searchBar.h"
#include <math.h>

void searchbar_init(
    UISearchBar *bar)
{
    bar->text[0] = '\0';
    bar->len = 0;
    bar->focused = false;
    bar->cursor_blink = 0.0f;
}

void searchbar_layout(
    UISearchBar *bar,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h)
{
    bar->rect =
        ui_rect(
            ui,
            x,
            y,
            w,
            h
        );
}

void searchbar_event(
    UISearchBar *bar,
    SDL_Event *event)
{
    if (
        event->type ==
        SDL_MOUSEBUTTONDOWN &&
        event->button.button ==
        SDL_BUTTON_LEFT
    ) {
        bar->focused =
            ui_point_in_rect(
                event->button.x,
                event->button.y,
                bar->rect
            );

        if (bar->focused)
            bar->cursor_blink = 0.0f;
    }

    if (
        event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT
    ) {
        if (
            !ui_point_in_rect(
                event->button.x,
                event->button.y,
                bar->rect)
        ) {
            bar->focused = false;
        }
    }

    if (!bar->focused)
        return;

    if (
        event->type == SDL_KEYDOWN
    ) {
        if (
            event->key.keysym.sym ==
            SDLK_BACKSPACE
        ) {
            if (bar->len > 0) {
                bar->len--;
                bar->text[bar->len] = '\0';
                bar->cursor_blink = 0.0f;
            }
        }
    }

    if (
        event->type == SDL_TEXTINPUT &&
        bar->len < SEARCHBAR_MAX - 1
    ) {
        strcat(bar->text, event->text.text);
        bar->len =
            (int)strlen(bar->text);
        bar->cursor_blink = 0.0f;
    }
}

void searchbar_draw(
    UISearchBar *bar,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    float dt)
{
    /*
        Capsule-shaped field: border painted
        as a filled rounded ring (outer rect
        in border colour, inner rect in fill
        colour) instead of a stroked path —
        strokes double-blend where segments
        join and leave patchy seams.
    */
    int radius = bar->rect.h / 2;

    UIColor fill = ui_theme(ui->dark,
        (UIColor){255, 255, 255, 255},
        (UIColor){40, 40, 46, 255});

    UIColor border =
        bar->focused
            ? ui_theme(ui->dark,
                  (UIColor){0, 0, 0, 90},
                  (UIColor){255, 255, 255, 78})
            : ui_theme(ui->dark,
                  (UIColor){0, 0, 0, 46},
                  (UIColor){255, 255, 255, 36});

    SDL_Rect inner = {
        bar->rect.x + 1,
        bar->rect.y + 1,
        bar->rect.w - 2,
        bar->rect.h - 2};

    int inner_radius =
        radius > 2 ? radius - 2 : 0;

    ui_fill_rounded_rect(
        renderer,
        bar->rect,
        radius,
        border
    );

    ui_fill_rounded_rect(
        renderer,
        inner,
        inner_radius,
        fill
    );

    /*
        Magnifier icon: small neutral donut
        ring + handle. No accent colour — the
        only blue in the field is the caret.
    */
    float s = ui->scale;

    int icx =
        bar->rect.x +
        (int)roundf(17.0f * s);

    int icy = bar->rect.y +
        bar->rect.h / 2;

    int ir =
        (int)roundf(4.2f * s);

    /*
        Ring painted as a filled disc over a
        slightly smaller disc of the field
        colour (donut) — no stroked polyline,
        so no gaps or stepping.
    */
    UIColor icon = ui_theme(ui->dark,
        (UIColor){120, 128, 144, 220},
        (UIColor){150, 156, 170, 220});

    if (ir < 3) ir = 3;

    ui_fill_circle(
        renderer,
        icx, icy, ir + 1,
        icon);

    ui_fill_circle(
        renderer,
        icx, icy, ir - 1,
        fill);

    /*
        Handle.
    */
    float hd = 0.7854f;

    int hx0 =
        icx + (int)(ir * cosf(hd));

    int hy0 =
        icy + (int)(ir * sinf(hd));

    int hl =
        (int)roundf(6.0f * s);

    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(
        renderer,
        icon.r, icon.g, icon.b, icon.a);

    SDL_RenderDrawLine(
        renderer,
        hx0, hy0,
        hx0 + (int)(hl * cosf(hd)),
        hy0 + (int)(hl * sinf(hd)));

    /*
        Text sits right of the icon.
    */
    int text_x =
        bar->rect.x +
        (int)roundf(34.0f * s);

    if (bar->len > 0) {

        ui_text(
            renderer,
            font,
            bar->text,
            text_x,
            bar->rect.y +
                (bar->rect.h -
                 (int)roundf(
                     19.0f * ui->scale))
                / 2,
            ui_theme(ui->dark,
                (UIColor){30, 40, 58, 255},
                (UIColor){212, 216, 228, 255})
        );

    } else {

        ui_text(
            renderer,
            font,
            "Search chats...",
            text_x,
            bar->rect.y +
                (bar->rect.h -
                 (int)roundf(
                     19.0f * ui->scale))
                / 2,
            ui_theme(ui->dark,
                (UIColor){104, 114, 138, 255},
                (UIColor){126, 132, 146, 255})
        );
    }

    if (bar->focused) {

        bar->cursor_blink += dt;

        if (fmodf(bar->cursor_blink, 1.0f) <
            0.5f)
        {
            int textW = 0;

            if (bar->len > 0) {

                TTF_SizeUTF8(
                    font,
                    bar->text,
                    &textW,
                    NULL
                );
            }

            int cx =
                text_x + textW +
                (int)(2.0f * ui->scale);

            int cy1 =
                bar->rect.y +
                (int)roundf(8.0f * ui->scale);

            int cy2 =
                bar->rect.y +
                bar->rect.h -
                (int)roundf(8.0f * ui->scale);

            SDL_SetRenderDrawBlendMode(
                renderer,
                SDL_BLENDMODE_BLEND);

            SDL_SetRenderDrawColor(
                renderer,
                50, 100, 235, 200);

            SDL_RenderDrawLine(
                renderer,
                cx, cy1,
                cx, cy2);
        }
    }
}
