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
        Glass button: one frosted translucent
        capsule plus a true signed-distance-field
        stroke. The old version painted the
        outline as a filled ring (border colour
        rect under a 1 px smaller fill rect) —
        the two anti-aliased fringes never line
        up along the corners, so the border
        stepped and looked pixelated.
    */
    int radius = bar->rect.h / 2;

    float s = ui->scale;

    /*
        Frosted material, echoing the popup
        glass but a touch lighter so the field
        reads as a raised element on the panel.
    */
    UIColor fill = ui_theme(ui->dark,
        (UIColor){255, 255, 255, 150},
        (UIColor){255, 255, 255, 26});

    UIColor border =
        bar->focused
            ? ui_theme(ui->dark,
                  (UIColor){0, 0, 0, 110},
                  (UIColor){255, 255, 255, 120})
            : ui_theme(ui->dark,
                  (UIColor){0, 0, 0, 52},
                  (UIColor){255, 255, 255, 56});

    /*
        Hairline at small scales, 2 px once the
        UI is large enough to carry it.
    */
    float stroke_w =
        s >= 1.5f ? 2.0f : 1.0f;

    ui_fill_rounded_rect(
        renderer,
        bar->rect,
        radius,
        fill
    );

    ui_stroke_rounded_rect(
        renderer,
        bar->rect,
        radius,
        stroke_w,
        border
    );

    /*
        Magnifier icon: neutral SDF ring +
        handle. No accent colour — the only
        blue in the field is the caret.
    */
    int icx =
        bar->rect.x +
        (int)roundf(17.0f * s);

    int icy = bar->rect.y +
        bar->rect.h / 2;

    int ir =
        (int)roundf(4.2f * s);

    if (ir < 3) ir = 3;

    UIColor icon = ui_theme(ui->dark,
        (UIColor){120, 128, 144, 220},
        (UIColor){150, 156, 170, 220});

    ui_stroke_circle(
        renderer,
        icx, icy, ir,
        stroke_w,
        icon
    );

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
        Text sits right of the icon, optically
        centered on the real glyph height so
        alignment holds at every font size.
    */
    int text_x =
        bar->rect.x +
        (int)roundf(34.0f * s);

    int text_y = bar->rect.y +
        (bar->rect.h -
         TTF_FontHeight(font)) / 2;

    if (bar->len > 0) {

        ui_text(
            renderer,
            font,
            bar->text,
            text_x,
            text_y,
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
            text_y,
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
