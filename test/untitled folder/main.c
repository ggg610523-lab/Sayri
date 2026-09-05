#include <SDL.h>
#include <SDL_ttf.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "orb.h"
#include "ripple.h"
#include "button.h"
#include "util.h"

#define WIN_W 960
#define WIN_H 720

static int snapshot_no = 0;

int main(void) {
  srand((unsigned)time(NULL));

  SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    SDL_Log("SDL_Init: %s", SDL_GetError());
    return 1;
  }
  if (TTF_Init() != 0) {
    SDL_Log("TTF_Init: %s", TTF_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

  SDL_Window *win = SDL_CreateWindow(
      "reacticx x SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W,
      WIN_H, SDL_WINDOW_OPENGL);
  if (!win) {
    SDL_Log("Window: %s", SDL_GetError());
    return 1;
  }
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  if (!ctx) {
    SDL_Log("GL context: %s", SDL_GetError());
    return 1;
  }
  SDL_GL_SetSwapInterval(1);

  if (!glcore_load()) return 1;
  SDL_Log("GL: %s %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  RippleProg rip = build_ripple_prog();
  if (!rip.prog) return 1;
  OrbProg orb = build_orb_prog();
  if (!orb.prog) return 1;

  OrbitState orbSt;
  roll_orb_colors(&orbSt);

  TTF_Font *font = TTF_OpenFont("MuternVF.ttf", 16);
  if (!font) {
    SDL_Log("Font load: %s", TTF_GetError());
    return 1;
  }
  GLuint cardTex = ripple_card_texture(font);
  GLuint btnTex = button_texture(font);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Box cardBox = box_card();
  Box btnBox = box_btn();

  Ripple cardR = {0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.2f, 0.0f};
  Ripple btnR = {0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.2f, 0.0f};

  const char *tapPath = SDL_getenv("REACTICX_TAP");
  const char *capPath = SDL_getenv("REACTICX_CAPTURE");
  Uint64 runStart = SDL_GetTicks64();
  Uint64 tapAt = runStart + 900;
  int initCapturePending = (capPath && capPath[0]) ? 1 : 0;
  Uint64 initCapAt = runStart + 2500;
  const char *capAtEnv = SDL_getenv("REACTICX_CAPAT");
  if (capAtEnv && capAtEnv[0]) initCapAt = runStart + (Uint64)strtoul(capAtEnv, NULL, 10);

  int running = 1;
  while (running) {
    Uint64 now = SDL_GetTicks64();
    float t = now / 1000.0f;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = 0;
      else if (ev.type == SDL_KEYDOWN) {
        switch (ev.key.keysym.sym) {
          case SDLK_ESCAPE:
            running = 0;
            break;
          case SDLK_s: {
            char path[64];
            snprintf(path, sizeof(path), "shot_%03d.ppm", snapshot_no++);
            capture_ppm(path, WIN_W, WIN_H);
            break;
          }
          default:
            break;
        }
      } else if (ev.type == SDL_MOUSEBUTTONDOWN &&
                 ev.button.button == SDL_BUTTON_LEFT) {
        int mx = ev.button.x, my = ev.button.y;
        if (inside(&btnBox, mx, my)) {
          ripple_tap(&btnBox, &btnR, mx, my);
        } else if (inside(&cardBox, mx, my)) {
          ripple_tap(&cardBox, &cardR, mx, my);
        } else {
          roll_orb_colors(&orbSt);
        }
      }
    }

    float ct = ripple_time(&cardR, now);
    float bt = ripple_time(&btnR, now);

    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    orb_draw(&orb, &orbSt, t, WIN_H);
    ripple_draw(&rip, cardTex, cardBox, ct, cardR.originX, cardR.originY,
                ct > 0.0f ? 28.0f : 0.0f, CARD_R, WIN_H);
    ripple_draw(&rip, btnTex, btnBox, bt, btnR.originX, btnR.originY,
                bt > 0.0f ? 30.0f : 0.0f, BTN_R, WIN_H);

    SDL_GL_SwapWindow(win);
    SDL_Delay(1);

    if (initCapturePending && now >= initCapAt) {
      capture_ppm(capPath, WIN_W, WIN_H);
      initCapturePending = 0;
    }

    if (tapPath && now >= tapAt && !tapPath[1]) {
      int mx = cardBox.x + cardBox.w / 2, my = cardBox.y + cardBox.h / 2;
      ripple_tap(&cardBox, &cardR, mx, my);
      tapPath = "";
    }
  }

  TTF_CloseFont(font);
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  TTF_Quit();
  SDL_Quit();
  return 0;
}