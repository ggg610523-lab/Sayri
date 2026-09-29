#include "sidebar.h"

void sidebar_init(
    UISidebar *sb)
{
    sb->open = false;
    sb->anim = 0.0f;

    sb->items[0] = "Settings";
    sb->items[1] = "Profile";
    sb->items[2] = "About";
    sb->items[3] = "Quit";

    sb->mouse_x = -1;
    sb->mouse_y = -1;
}

void sidebar_layout(
    UISidebar *sb,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h)
{
    sb->rect =
        ui_rect(
            ui,
            x,
            y,
            w,
            h
        );
}

void sidebar_event(
    UISidebar *sb,
    SDL_Event *event)
{
    if (
        event->type ==
        SDL_MOUSEMOTION
    ) {
        sb->mouse_x =
            event->motion.x;

        sb->mouse_y =
            event->motion.y;
    }

    if (
        event->type ==
        SDL_MOUSEBUTTONDOWN &&
        event->button.button ==
        SDL_BUTTON_LEFT
    ) {

        if (
            sb->open &&
            !ui_point_in_rect(
                event->button.x,
                event->button.y,
                sb->rect
            )
        ) {
            sb->open = false;
        }
    }
}

void sidebar_draw(
    UISidebar *sb,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *title_font,
    TTF_Font *item_font,
    float dt)
{
    float target = sb->open ? 1.0f : 0.0f;

    sb->anim +=
        (target - sb->anim) *
        6.0f * dt;

    if (sb->anim < 0.005f)
        sb->anim = 0.0f;

    if (sb->anim > 0.995f)
        sb->anim = 1.0f;

    if (sb->anim < 0.001f)
        return;

    float eased =
        ui_ease_out_cubic(sb->anim);

    float maxW =
        (float)sb->rect.w / ui->scale;

    float currentW = eased * maxW;

    SDL_Rect vis =
        ui_rect(
            ui,
            0,
            0,
            currentW,
            sb->rect.h / ui->scale
        );

    ui_glass(
        renderer,
        vis,
        0,
        false,
        ui->dark
    );

    /*
        Brand header.

        Drawn to the RIGHT of the hamburger
        button, which occupies roughly design
        x 15..55 — so start at 68 and center
        vertically against it.
    */
    if (sb->anim > 0.4f) {

        float alpha =
            (sb->anim - 0.4f) /
            0.6f;

        if (alpha > 1.0f)
            alpha = 1.0f;

        int fontH =
            TTF_FontHeight(
                title_font ? title_font
                           : item_font);

        int hb_center =
            (int)roundf(35.0f * ui->scale);

        ui_text(
            renderer,
            title_font ? title_font
                       : item_font,
            "Sayri",
            (int)roundf(68.0f * ui->scale),
            hb_center - fontH / 2,
            (UIColor){
                35, 45, 62,
                (Uint8)(255 * alpha)}
        );
    }

    /*
        Glass buttons.

        Drawn late in the slide so the rows never
        stick out beyond the panel edge.
    */
    if (sb->anim > 0.8f) {

        float alpha =
            (sb->anim - 0.8f) / 0.2f;

        if (alpha > 1.0f)
            alpha = 1.0f;

        int radius =
            (int)roundf(
                SIDEBAR_ITEM_H * 0.5f *
                ui->scale
            );

        for (int i = 0;
             i < SIDEBAR_ITEMS;
             i++) {

            SDL_Rect row =
                ui_rect(
                    ui,
                    SIDEBAR_ITEM_X,
                    SIDEBAR_ITEM_Y +
                        i * SIDEBAR_ITEM_GAP,
                    SIDEBAR_ITEM_W,
                    SIDEBAR_ITEM_H
                );

            bool hovered =
                ui_point_in_rect(
                    sb->mouse_x,
                    sb->mouse_y,
                    row
                );

            ui_glass(
                renderer,
                row,
                radius,
                hovered,
                ui->dark
            );

            UIColor textColor =
                ui_theme(ui->dark,
                    hovered
                    ? (UIColor){25, 55, 130, 255}
                    : (UIColor){30, 40, 58, 255},
                    hovered
                    ? (UIColor){140, 190, 255, 255}
                    : (UIColor){205, 205, 212, 255}
                );

            textColor.a =
                (Uint8)(textColor.a * alpha);

            ui_text_center(
                renderer,
                item_font ? item_font : title_font,
                sb->items[i],
                row,
                textColor
            );
        }
    }
}
