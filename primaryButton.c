#include "primaryButton.h"

void primary_button_init(
    UIPrimaryButton *button,
    const char *text)
{
    button->text = text;
    button->hovered = false;
    button->pressed = false;
    button->clicked = false;
    button->hover_anim = 0.0f;
}

void primary_button_layout(
    UIPrimaryButton *button,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h)
{
    button->rect =
        ui_rect(
            ui,
            x,
            y,
            w,
            h
        );
}

void primary_button_event(
    UIPrimaryButton *button,
    SDL_Event *event)
{
    if (event->type == SDL_MOUSEMOTION) {

        button->hovered =
            ui_point_in_rect(
                event->motion.x,
                event->motion.y,
                button->rect
            );
    }

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
                button->rect
            )
        ) {
            button->pressed = true;
        }
    }

    if (
        event->type ==
        SDL_MOUSEBUTTONUP &&
        event->button.button ==
        SDL_BUTTON_LEFT
    ) {

        button->clicked =
            button->pressed &&
            ui_point_in_rect(
                event->button.x,
                event->button.y,
                button->rect
            );

        button->pressed = false;
    }
}

void primary_button_draw(
    UIPrimaryButton *button,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    float dt)
{
    float target =
        (button->hovered ||
         button->pressed)
        ? 1.0f : 0.0f;

    button->hover_anim +=
        (target - button->hover_anim) *
        14.0f * dt;

    if (button->hover_anim < 0.001f)
        button->hover_anim = 0.0f;

    if (button->hover_anim > 0.999f)
        button->hover_anim = 1.0f;

    int radius =
        (int)roundf(
            16.0f * ui->scale
        );

    UIColor rest =
        ui_theme(ui->dark,
            (UIColor){50, 100, 230, 255},
            (UIColor){50, 100, 230, 255});

    UIColor hover =
        ui_theme(ui->dark,
            (UIColor){60, 110, 240, 255},
            (UIColor){60, 110, 240, 255});

    UIColor press =
        ui_theme(ui->dark,
            (UIColor){40, 80, 210, 255},
            (UIColor){40, 80, 210, 255});

    UIColor fillColor;

    if (button->hover_anim > 0.5f) {
        fillColor = ui_color_lerp(
            hover, press,
            (button->hover_anim - 0.5f) * 2.0f);
    } else {
        fillColor = ui_color_lerp(
            rest, hover,
            button->hover_anim * 2.0f);
    }

    ui_fill_rounded_rect(
        renderer,
        button->rect,
        radius,
        fillColor
    );

    ui_outline_rounded_rect(
        renderer,
        button->rect,
        radius,
        ui_theme(ui->dark,
            (UIColor){120, 170, 255,
                (Uint8)(150 +
                    button->hover_anim * 50)},
            (UIColor){80, 130, 220,
                (Uint8)(150 +
                    button->hover_anim * 50)})
    );

    ui_text_center(
        renderer,
        font,
        button->text,
        button->rect,
        (UIColor){255, 255, 255, 255}
    );
}
