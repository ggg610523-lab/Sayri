/*
    QR surface renderer for the Sayri pairing panel.

    Wraps qrcodegen (Nayuki, MIT) into a tiny cache: we keep
    the last encoded payload and its module grid, and only
    re-encode when text changes. Rendering walks the grid and
    draws filled squares on a light background with a quiet zone.
*/

#include "qrsurface.h"
#include "qrcodegen.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define QR_MAX_VERSION 6   /* plenty for sayri://ip:port?code=### */
#define QR_BUF_LEN      qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)

static char g_text[192] = "";
static uint8_t g_qr[QR_BUF_LEN];
static int g_size = 0;

bool qrsurface_set(const char *text)
{
    if (!text) {
        g_text[0] = '\0';
        g_size = 0;
        return false;
    }

    if (strcmp(g_text, text) == 0 && g_size > 0)
        return true;

    uint8_t temp[QR_BUF_LEN];
    memset(temp, 0, sizeof(temp));
    memset(g_qr, 0, sizeof(g_qr));

    bool ok = qrcodegen_encodeText(
        text, temp, g_qr,
        qrcodegen_Ecc_MEDIUM,
        1, QR_MAX_VERSION,
        qrcodegen_Mask_AUTO,
        true);

    if (ok) {
        snprintf(g_text, sizeof(g_text), "%s", text);
        g_size = qrcodegen_getSize(g_qr);
    } else {
        g_text[0] = '\0';
        g_size = 0;
    }
    return ok;
}

int qrsurface_size(void)
{
    return g_size;
}

void qrsurface_draw(
    SDL_Renderer *renderer,
    int x,
    int y,
    int px,
    UIColor dark,
    UIColor light)
{
    if (g_size <= 0 || px <= 0) return;

    /* Quiet zone: 4 modules on each side. */
    int total = g_size + 8;
    int cell = px / total;
    if (cell < 1) cell = 1;
    int drawn = cell * total;

    /* Light background square. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, light.r, light.g, light.b, 255);
    SDL_Rect bg = {x, y, drawn, drawn};
    SDL_RenderFillRect(renderer, &bg);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, dark.r, dark.g, dark.b, dark.a);
    for (int gy = 0; gy < g_size; gy++) {
        for (int gx = 0; gx < g_size; gx++) {
            if (!qrcodegen_getModule(g_qr, gx, gy)) continue;
            SDL_Rect m = {
                x + cell * (gx + 4),
                y + cell * (gy + 4),
                cell, cell
            };
            SDL_RenderFillRect(renderer, &m);
        }
    }
}