#include "downloads.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
   Helpers
   ============================================================ */

static void format_size(
    double bytes,
    char *out,
    size_t out_size)
{
    if (bytes >= 1024.0 * 1024.0 * 1024.0)
        snprintf(out, out_size,
                 "%.2f GB",
                 bytes /
                     (1024.0 * 1024.0 *
                      1024.0));
    else
        snprintf(out, out_size,
                 "%.0f MB",
                 bytes / (1024.0 * 1024.0));
}

void downloads_init(
    UIDownloads *dl,
    const char *model)
{
    memset(dl, 0, sizeof(*dl));

    dl->w = DL_DEFAULT_W;
    dl->h = DL_DEFAULT_H;

    snprintf(dl->model,
             sizeof(dl->model),
             "%s", model);

    button_init(&dl->install_btn,
                "Install");

    progress_bar_init(&dl->bar, 0.0f);

    dl->note[0] = '\0';
}

void downloads_layout(
    UIDownloads *dl,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h)
{
    dl->x = x;
    dl->y = y;
    dl->w = w;
    dl->h = h;

    dl->rect = ui_rect(ui, x, y, w, h);
}

bool downloads_consume_install_click(
    UIDownloads *dl)
{
    bool clicked = dl->install_btn.clicked;

    dl->install_btn.clicked = false;

    return clicked;
}

void downloads_set_pull(
    UIDownloads *dl,
    const OllamaPull *pull)
{
    dl->pulling = pull->active;

    if (pull->active) {

        dl->fraction = pull->fraction;

        char done_s[32];
        char total_s[32];

        format_size(pull->completed,
                    done_s, sizeof(done_s));
        format_size(pull->total,
                    total_s,
                    sizeof(total_s));

        snprintf(dl->note,
                 sizeof(dl->note),
                 "%s  %s / %s",
                 pull->status[0]
                 ? pull->status
                 : "Downloading",
                 done_s, total_s);

        return;
    }

    if (pull->done) {

        dl->fraction =
            pull->ok ? 1.0f : dl->fraction;

        if (pull->ok) {
            dl->installed = true;
            dl->failed = false;
            snprintf(dl->note,
                     sizeof(dl->note),
                     "Model installed.");
        } else {
            dl->failed = true;
            snprintf(dl->note,
                     sizeof(dl->note),
                     "%s",
                     pull->error[0]
                     ? pull->error
                     : "Download failed.");
        }

        return;
    }

    /*
        Idle: keep whatever note is showing but
        reflect server state when known.
    */
}

void downloads_set_setup(
    UIDownloads *dl,
    const OllamaSetup *setup,
    bool server_missing)
{
    dl->server_missing = server_missing;

    dl->setting_up = setup->active;

    if (setup->active) {

        dl->fraction = setup->fraction;

        snprintf(dl->note,
                 sizeof(dl->note),
                 "%s",
                 setup->status[0]
                 ? setup->status
                 : "Working\u2026");

        return;
    }

    if (setup->done) {

        if (setup->ok) {

            dl->server_missing = false;
            dl->failed = false;
            dl->fraction = 1.0f;

            snprintf(dl->note,
                     sizeof(dl->note),
                     "Ollama ready.");

        } else {

            dl->failed = true;

            snprintf(dl->note,
                     sizeof(dl->note),
                     "%s",
                     setup->error[0]
                     ? setup->error
                     : "Setup failed.");
        }
    }
}

void downloads_set_installed(
    UIDownloads *dl,
    bool installed)
{
    dl->installed = installed;

    if (installed && !dl->pulling) {
        snprintf(dl->note,
                 sizeof(dl->note),
                 "Already installed.");
    }
}

/* ============================================================
   Events
   ============================================================ */

void downloads_event(
    UIDownloads *dl,
    UIContext *ui,
    SDL_Event *event)
{
    (void)ui;

    dl->clicked_outside = false;

    if (event->type ==
        SDL_MOUSEMOTION) {

        dl->mouse_x = event->motion.x;
        dl->mouse_y = event->motion.y;

        button_event(&dl->install_btn,
                     event);

        return;
    }

    if (event->type ==
            SDL_MOUSEBUTTONDOWN &&
        event->button.button ==
        SDL_BUTTON_LEFT) {

        bool inside =
            ui_point_in_rect(
                event->button.x,
                event->button.y,
                dl->rect);

        if (!inside && dl->open) {
            dl->open = false;
            dl->clicked_outside = true;
            return;
        }

        if (inside && dl->open &&
            !dl->pulling && !dl->setting_up &&
            !dl->installed)
            button_event(&dl->install_btn,
                         event);
    }

    if (event->type ==
            SDL_MOUSEBUTTONUP &&
        event->button.button ==
        SDL_BUTTON_LEFT) {

        if (dl->open &&
            !dl->pulling && !dl->setting_up &&
            !dl->installed)
            button_event(&dl->install_btn,
                         event);
    }
}

/* ============================================================
   Draw
   ============================================================ */

void downloads_draw(
    UIDownloads *dl,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    TTF_Font *title_font,
    float dt)
{
    float target =
        dl->open ? 1.0f : 0.0f;

    dl->anim +=
        (target - dl->anim) * 8.0f * dt;

    if (dl->anim < 0.005f)
        dl->anim = 0.0f;

    if (dl->anim > 0.995f)
        dl->anim = 1.0f;

    if (dl->anim < 0.01f)
        return;

    float eased =
        ui_ease_out_cubic(dl->anim);

    SDL_Rect vis = dl->rect;

    vis.y -= (int)roundf(
        (1.0f - eased) * 14.0f * ui->scale);

    int radius =
        (int)roundf(16.0f * ui->scale);

    ui_glass(renderer, vis, radius,
             true, ui->dark);

    int pad =
        (int)roundf(14.0f * ui->scale);

    Uint8 a = (Uint8)(255.0f * eased);

    ui_text(
        renderer,
        title_font ? title_font : font,
        "Downloads",
        vis.x + pad,
        vis.y + pad - 2,
        ui_theme(ui->dark,
            (UIColor){35, 45, 62, a},
            (UIColor){228, 228, 233, a}));

    SDL_SetRenderDrawBlendMode(
        renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(
        renderer,
        ui->dark ? 255 : 45,
        ui->dark ? 255 : 55,
        ui->dark ? 255 : 75,
        ui->dark ? 26 : 32);

    int div_y =
        vis.y +
        (int)roundf(42.0f * ui->scale);

    SDL_RenderDrawLine(
        renderer,
        vis.x + pad, div_y,
        vis.x + vis.w - pad, div_y);

    /*
        Model row.
    */
    ui_text(
        renderer, font, dl->model,
        vis.x + pad,
        vis.y +
            (int)roundf(56.0f * ui->scale),
        ui_theme(ui->dark,
            (UIColor){45, 55, 75, a},
            (UIColor){206, 206, 213, a}));

    /*
        Progress bar.
    */
    progress_bar_layout(
        &dl->bar, ui,
        dl->x + 14.0f,
        dl->y + 84.0f,
        dl->w - 28.0f,
        8.0f);

    progress_bar_set_value(&dl->bar,
                           dl->fraction);

    progress_bar_draw(&dl->bar, ui,
                      renderer, dt);

    /*
        Status note.
    */
    ui_text(
        renderer, font,
        dl->note[0] ? dl->note : " ",
        vis.x + pad,
        vis.y +
            (int)roundf(100.0f*ui->scale),
        ui_theme(ui->dark,
            dl->failed
            ? (UIColor){190,60,50,a}
            : (UIColor){110,120,140,a},
            dl->failed
            ? (UIColor){255,130,120,a}
            : (UIColor){160,172,196,a}));

    /*
        Install button.

        Label reflects what the next press
        would do.
    */
    const char *label =
        dl->installed ? "Installed"
        : (dl->pulling || dl->setting_up)
                        ? "Working\u2026"
        : dl->server_missing ? "Set up"
        : dl->failed  ? "Retry"
        :               "Install";

    dl->install_btn.text = label;

    button_layout(
        &dl->install_btn, ui,
        dl->x + dl->w - 14.0f - 118.0f,
        dl->y + dl->h - 44.0f,
        118.0f, 30.0f);

    button_draw(&dl->install_btn, ui,
                renderer, font);
}
