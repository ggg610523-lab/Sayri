#include "sidebar.h"
#include <math.h>

void sidebar_init(
    UISidebar *sb)
{
    sb->open = false;
    sb->anim = 0.0f;

    button_init(&sb->buttons[0], "New Chat");
    button_init(&sb->buttons[1], "Recent");
    button_init(&sb->buttons[2], "Search");
    button_init(&sb->buttons[3], "Settings");
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

    if (sb->open) {
        for (int i = 0; i < SIDEBAR_ITEMS; i++) {
            button_event(
                &sb->buttons[i], event);
        }
    }
}

void sidebar_draw(
    UISidebar *sb,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    TTF_Font *title_font,
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

    ui_fill_rounded_rect(
        renderer, vis,
        0,
        (UIColor){240, 245, 252, 175});

    if (sb->anim > 0.4f) {

        float alpha =
            (sb->anim - 0.4f) /
            0.6f;

        if (alpha > 1.0f)
            alpha = 1.0f;

        ui_text(
            renderer,
            title_font,
            "Sayri",
            (int)roundf(25.0f * ui->scale),
            (int)roundf(60.0f * ui->scale),
            (UIColor){
                35, 45, 62,
                (Uint8)(255 * alpha)}
        );

        float btnX = 15.0f;
        float btnW = currentW - 30.0f;
        float btnH = 40.0f;
        float startY = 100.0f;
        float gap = 8.0f;

        for (int i = 0;
             i < SIDEBAR_ITEMS;
             i++)
        {
            float btnY =
                startY + i * (btnH + gap);

            button_layout(
                &sb->buttons[i],
                ui,
                btnX,
                btnY,
                btnW,
                btnH);

            button_draw(
                &sb->buttons[i],
                ui,
                renderer,
                font,
                dt);
        }
    }
}
