/*
    GL orb availability probe.

    Replicates exactly what main.c does (visible
    window + accelerated renderer first), then runs
    the orb GL init sequence and reports whether the
    GL 3.2 core context + entry points load. Use it
    to diagnose the shader-but-software-orb fallback.
*/
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <stdio.h>

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL init failed: %s\n",
               SDL_GetError());
        return 1;
    }

    /*
        Exactly what main.c does: main window +
        accelerated renderer before any orb GL work.
    */
    SDL_Window *mainw =
        SDL_CreateWindow(
            "Sayri", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            520, 720,
            SDL_WINDOW_HIDDEN |
            SDL_WINDOW_RESIZABLE);

    SDL_Renderer *renderer =
        SDL_CreateRenderer(
            mainw, -1,
            SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC);

    if (!mainw || !renderer) {
        printf("main window/renderer failed\n");
        return 1;
    }

    /*
        The orb_gl_init() sequence: core 3.2
        forward-compatible hidden GL window.
    */
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS,
        SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_SetHint(
        SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");

    SDL_Window *orbw =
        SDL_CreateWindow(
            "sayri-orb",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            256, 256,
            SDL_WINDOW_HIDDEN |
            SDL_WINDOW_OPENGL);

    if (!orbw) {
        printf("orb window failed: %s\n",
               SDL_GetError());
        return 1;
    }

    SDL_GLContext oc =
        SDL_GL_CreateContext(orbw);

    if (!oc) {
        printf("orb context failed: %s\n",
               SDL_GetError());
        return 1;
    }

    printf("makecurrent: %s\n",
           SDL_GL_MakeCurrent(orbw, oc) == 0
               ? "OK" : SDL_GetError());

    static const char *names[] = {
        "glUseProgram",
        "glCreateShader",
        "glReadPixels",
        "glDrawArrays",
        "glUniform4f",
        "glGetUniformLocation",
        "glViewport",
        "glGenVertexArrays",
        "glBindVertexArray",
        "glCreateProgram",
        "glShaderSource",
        "glClear",
        "glGetString",
        NULL
    };

    int missing = 0;

    for (int i = 0; names[i]; i++) {
        const char *ok = SDL_GL_GetProcAddress(
            names[i]) ? "OK" : "MISSING";

        if (ok[0] == 'M')
            missing++;

        printf("%-24s %s\n", names[i], ok);
    }

    if (missing)
        printf("GL 3.x unavailable "
               "(%d missing), software orb is used\n",
               missing);
    else
        printf("GL orb looks ready\n");

    SDL_GL_DeleteContext(oc);
    SDL_DestroyWindow(orbw);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(mainw);
    SDL_Quit();
    return 0;
}