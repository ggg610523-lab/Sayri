#include "button.h"

void button_init(
    UIButton *button,
    const char *text)
{
    button->text = text;
    button->hovered = false;
    button->pressed = false;
    button->clicked = false;
    button->hover_anim = 0.0f;
}

void button_layout(
    UIButton *button,
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

void button_event(
    UIButton *button,
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

void button_draw(
    UIButton *button,
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

    ui_glass(
        renderer,
        button->rect,
        radius,
        button->hover_anim > 0.01f,
        ui->dark
    );

    UIColor inactive =
        ui_theme(ui->dark,
            (UIColor){30, 40, 58, 255},
            (UIColor){200, 210, 230, 255});

    UIColor active =
        ui_theme(ui->dark,
            (UIColor){30, 85, 210, 255},
            (UIColor){100, 160, 255, 255});

    UIColor textColor =
        ui_color_lerp(
            inactive,
            active,
            button->hover_anim
        );

    ui_text_center(
        renderer,
        font,
        button->text,
        button->rect,
        textColor
    );
}
