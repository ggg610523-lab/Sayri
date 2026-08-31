#include "popup.h"

void popup_init(
    UIPopup *pop,
    const char *title)
{
    pop->rect.x = 0;
    pop->rect.y = 0;
    pop->rect.w = 0;
    pop->rect.h = 0;

    pop->x = 0.0f;
    pop->y = 0.0f;
    pop->w = POPUP_DEFAULT_W;
    pop->h = POPUP_DEFAULT_H;

    pop->title = title;
    pop->row_label = "Option";
    pop->value_label = NULL;

    pop->item_count = 0;

    for (int i = 0; i < POPUP_MAX_ITEMS; i++)
        pop->items[i] = NULL;

    pop->toggle = NULL;
    pop->dropdown = NULL;
    pop->search = NULL;

    pop->open = false;
    pop->clicked_outside = false;
    pop->_item_clicked = -1;
    pop->_value_clicked = false;
    pop->anim = 0.0f;

    pop->mouse_x = -1;
    pop->mouse_y = -1;
}

void popup_set_title(
    UIPopup *pop,
    const char *title)
{
    pop->title = title;
}

void popup_set_row_label(
    UIPopup *pop,
    const char *label)
{
    pop->row_label = label;
}

void popup_link_toggle(
    UIPopup *pop,
    UIToggle *toggle)
{
    pop->toggle = toggle;
}

void popup_link_dropdown(
    UIPopup *pop,
    UIDropDown *dd)
{
    pop->dropdown = dd;
}

void popup_link_search(
    UIPopup *pop,
    UISearchBar *bar)
{
    pop->search = bar;
}

void popup_set_value(
    UIPopup *pop,
    const char *label)
{
    pop->value_label = label;
}

void popup_set_items(
    UIPopup *pop,
    const char **items,
    int count)
{
    pop->item_count = 0;

    for (int i = 0;
         i < count && i < POPUP_MAX_ITEMS;
         i++) {
        pop->items[i] =
            items ? items[i] : NULL;
        pop->item_count++;
    }

    for (int i = pop->item_count;
         i < POPUP_MAX_ITEMS;
         i++)
        pop->items[i] = NULL;
}

int popup_consume_item_click(
    UIPopup *pop)
{
    int idx = pop->_item_clicked;
    pop->_item_clicked = -1;
    return idx;
}

bool popup_consume_value_click(
    UIPopup *pop)
{
    bool v = pop->_value_clicked;
    pop->_value_clicked = false;
    return v;
}

void popup_open(
    UIPopup *pop)
{
    pop->open = true;
}

void popup_close(
    UIPopup *pop)
{
    pop->open = false;
}

void popup_toggle(
    UIPopup *pop)
{
    pop->open = !pop->open;
}

void popup_layout(
    UIPopup *pop,
    UIContext *ui,
    float x,
    float y,
    float w,
    float h)
{
    pop->x = x;
    pop->y = y;
    pop->w = w;
    pop->h = h;

    pop->rect =
        ui_rect(ui, x, y, w, h);

    /*
        Same placement as the draw path below
        so the toggle's click rect is valid
        from the moment the popup opens —
        previously it was only laid out while
        drawing, and clicks before the first
        settled frame landed on an
        uninitialized rect.
    */
    if (!pop->search && pop->toggle)

        toggle_layout(
            pop->toggle, ui,
            pop->x + pop->w -
                14.0f - 46.0f - 54.0f,
            pop->y + 54.0f,
            46.0f, 26.0f);

    if (pop->dropdown)

        dropdown_layout(
            pop->dropdown,
            ui,
            pop->x + 14.0f,
            pop->y + POPUP_DD_Y,
            pop->w - 28.0f);

    if (pop->search)

        searchbar_layout(
            pop->search,
            ui,
            pop->x + 14.0f,
            pop->y + POPUP_SEARCH_Y,
            pop->w - 28.0f,
            POPUP_SEARCH_H);
}

/* ============================================================
   Row geometry helpers
   ============================================================ */

static SDL_Rect value_row_rect(
    UIPopup *pop,
    UIContext *ui)
{
    return
        ui_rect(
            ui,
            pop->x + 14.0f,
            pop->y + POPUP_VALUE_Y - 4.0f,
            pop->w - 28.0f,
            24.0f
        );
}

static SDL_Rect item_row_rect(
    UIPopup *pop,
    UIContext *ui,
    int index)
{
    float base = pop->search
        ? POPUP_ITEMS_Y_SEARCH
        : POPUP_ITEMS_Y;

    return
        ui_rect(
            ui,
            pop->x + 14.0f,
            pop->y + base +
                index * POPUP_ITEM_STEP - 3.0f,
            pop->w - 28.0f,
            24.0f
        );
}

void popup_event(
    UIPopup *pop,
    UIContext *ui,
    SDL_Event *event)
{
    pop->clicked_outside = false;

    if (
        event->type ==
        SDL_MOUSEMOTION
    ) {
        pop->mouse_x =
            event->motion.x;

        pop->mouse_y =
            event->motion.y;

        if (pop->open && pop->search)
            searchbar_event(
                pop->search,
                event);

        if (pop->open && pop->dropdown)
            dropdown_event(
                pop->dropdown,
                event);

        return;
    }

    if (
        event->type ==
        SDL_MOUSEBUTTONDOWN &&
        event->button.button ==
        SDL_BUTTON_LEFT
    ) {
        bool inside =
            ui_point_in_rect(
                event->button.x,
                event->button.y,
                pop->rect
            );

        /*
            Clicked outside: dismiss. The search
            bar still sees the click so it can
            unfocus before the panel hides.
        */
        if (
            !inside &&
            pop->open
        ) {
            /*
                The expanded dropdown list hangs
                below the panel; those clicks must
                reach it instead of dismissing.
            */
            bool dd_zone = false;

            if (pop->dropdown) {
                UIDropDown *dd =
                    pop->dropdown;

                dd_zone =
                    ui_point_in_rect(
                        event->button.x,
                        event->button.y,
                        dd->rect
                    ) || (
                        dd->open &&
                        ui_point_in_rect(
                            event->button.x,
                            event->button.y,
                            dd->listRect
                        )
                    );

                dropdown_event(
                    dd,
                    event);
            }

            if (pop->search)
                searchbar_event(
                    pop->search,
                    event);

            if (!dd_zone) {
                pop->open = false;
                pop->clicked_outside = true;
            }

            return;
        }

        if (!inside || !pop->open)
            return;

        int mx = event->button.x;
        int my = event->button.y;

        /*
            Search bar owns clicks in its rect
            (focus) and the unfocus click
            elsewhere in the panel.
        */
        if (pop->search) {

            searchbar_event(
                pop->search,
                event);

            if (ui_point_in_rect(
                    mx, my,
                    pop->search->rect))
                return;
        }

        if (pop->dropdown) {

            bool in_dd =
                ui_point_in_rect(
                    mx, my,
                    pop->dropdown->rect);

            bool in_list =
                pop->dropdown->open &&
                ui_point_in_rect(
                    mx, my,
                    pop->dropdown->
                        listRect);

            dropdown_event(
                pop->dropdown,
                event);

            if (in_dd || in_list)
                return;
        }

        if (
            pop->toggle &&
            ui_point_in_rect(mx, my,
                pop->toggle->rect)
        ) {
            toggle_event(pop->toggle, event);
            return;
        }

        if (
            pop->value_label &&
            ui_point_in_rect(
                mx, my,
                value_row_rect(pop, ui))
        ) {
            pop->_value_clicked = true;
            return;
        }

        for (int i = 0;
             i < pop->item_count;
             i++) {

            if (!pop->items[i])
                continue;

            if (ui_point_in_rect(
                    mx, my,
                    item_row_rect(pop, ui, i))) {

                pop->_item_clicked = i;
                return;
            }
        }
    }
}

void popup_draw(
    UIPopup *pop,
    UIContext *ui,
    SDL_Renderer *renderer,
    TTF_Font *font,
    TTF_Font *title_font,
    float dt)
{
    float target =
        pop->open ? 1.0f : 0.0f;

    pop->anim +=
        (target - pop->anim) *
        8.0f * dt;

    if (pop->anim < 0.005f)
        pop->anim = 0.0f;

    if (pop->anim > 0.995f)
        pop->anim = 1.0f;

    if (pop->anim < 0.01f)
        return;

    float eased =
        ui_ease_out_cubic(pop->anim);

    SDL_Rect vis = pop->rect;

    /*
        Slide in from slightly above.
    */
    vis.y -= (int)roundf(
        (1.0f - eased) *
        14.0f * ui->scale);

    int radius =
        (int)roundf(16.0f * ui->scale);

    ui_glass(
        renderer,
        vis,
        radius,
        true,
        ui->dark
    );

    int pad =
        (int)roundf(14.0f * ui->scale);

    Uint8 a =
        (Uint8)(255.0f * eased);

    ui_text(
        renderer,
        title_font ? title_font : font,
        pop->title,
        vis.x + pad,
        vis.y + pad - 2,
        ui_theme(ui->dark,
            (UIColor){35, 45, 62, a},
            (UIColor){228, 228, 233, a}));

    /*
        Divider.
    */
    SDL_SetRenderDrawBlendMode(
        renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(
        renderer,
        ui->dark ? 255 : 45,
        ui->dark ? 255 : 55,
        ui->dark ? 255 : 75,
        ui->dark ? 26 : 32);

    int div_y =
        vis.y + (int)roundf(42.0f * ui->scale);

    SDL_RenderDrawLine(
        renderer,
        vis.x + pad,
        div_y,
        vis.x + vis.w - pad,
        div_y);

    /*
        Embedded search bar sits just under
        the divider.
    */
    if (pop->search) {

        searchbar_layout(
            pop->search,
            ui,
            pop->x + 14.0f,
            pop->y + POPUP_SEARCH_Y,
            pop->w - 28.0f,
            POPUP_SEARCH_H);

        searchbar_draw(
            pop->search,
            ui,
            renderer,
            font,
            dt);
    }

    /*
        Option row. Suppressed when the popup
        embeds a search bar (the field owns
        that vertical band instead).
    */
    if (!pop->search && pop->row_label)

        ui_text(
            renderer, font,
            pop->row_label,
            vis.x + pad,
            vis.y +
                (int)roundf(58.0f * ui->scale),
            ui_theme(ui->dark,
                (UIColor){45, 55, 75, a},
                (UIColor){206, 206, 213, a}));

    if (!pop->search && pop->toggle) {

        toggle_layout(
            pop->toggle, ui,
            pop->x + pop->w -
                14.0f - 46.0f - 54.0f,
            pop->y + 54.0f,
            46.0f, 26.0f);

        toggle_draw(
            pop->toggle,
            ui,
            renderer,
            font
        );
    }

    /*
        Dropdown control (expanded list draws
        over whatever sits below the panel).
    */
    if (pop->dropdown)

        dropdown_draw(
            pop->dropdown,
            ui,
            renderer,
            font,
            dt);

    /*
        Value row (e.g. model picker).
    */
    if (pop->value_label) {

        SDL_Rect vr =
            value_row_rect(pop, ui);

        bool hov =
            ui_point_in_rect(
                pop->mouse_x,
                pop->mouse_y,
                vr);

        ui_fill_rounded_rect(
            renderer, vr,
            (int)roundf(6.0f * ui->scale),
            ui_theme(ui->dark,
                hov
                ? (UIColor){255,255,255,70}
                : (UIColor){255,255,255,30},
                hov
                ? (UIColor){255,255,255,22}
                : (UIColor){255,255,255,10}));

        ui_text(
            renderer, font,
            pop->value_label,
            vr.x + (int)roundf(8.0f*ui->scale),
            vr.y + (vr.h -
                TTF_FontHeight(font)) / 2,
            ui_theme(ui->dark,
                (UIColor){40,50,72,a},
                (UIColor){210,218,236,a}));
    }

    /*
        Item rows.
    */
    for (int i = 0;
         i < pop->item_count;
         i++) {

        if (!pop->items[i])
            continue;

        SDL_Rect ir =
            item_row_rect(pop, ui, i);

        bool hov =
            ui_point_in_rect(
                pop->mouse_x,
                pop->mouse_y,
                ir);

        if (hov) {
            ui_fill_rounded_rect(
                renderer, ir,
                (int)roundf(6.0f*ui->scale),
                ui_theme(ui->dark,
                    (UIColor){255,255,255,60},
                    (UIColor){255,255,255,18}));
        }

        ui_text(
            renderer, font,
            pop->items[i],
            ir.x + (int)roundf(8.0f*ui->scale),
            ir.y + (ir.h -
                TTF_FontHeight(font)) / 2,
            ui_theme(ui->dark,
                hov
                ? (UIColor){25,55,130,a}
                : (UIColor){45,55,75,a},
                hov
                ? (UIColor){150,195,255,a}
                : (UIColor){200,208,228,a}));
    }
}
