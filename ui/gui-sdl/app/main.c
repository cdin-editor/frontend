/*
 * SDL3 desktop presentation preview.
 *
 * FE-010/FE-012/FE-020: all SDL usage is isolated to the GUI adapter.
 * FE-023: this preview advertises no SDL-backed editor capability because
 * it is not connected to a Runtime/Backend session.
 *
 * SDL_RenderDebugText is intentionally used as a dependency-light diagnostic
 * font path. A production editor will replace this preview with its own font,
 * shaping, bidi and grapheme pipeline (FE-006/FE-018).
 */
#include <SDL3/SDL.h>
#include "cdin/frontend/core/diagnostics/log.h"

#include <stdbool.h>

int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    cdin_frontend_logger *logger = NULL;
    bool running = true;
    (void)argc;
    (void)argv;

    (void)cdin_frontend_logger_create(MMLLOG_LEVEL_INFO, &logger);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        CDIN_FRONTEND_ERROR(logger, "GUI", "SDL_Init failed: %s", SDL_GetError());
        if (logger != NULL) cdin_frontend_logger_destroy(logger);
        return 1;
    }
    window = SDL_CreateWindow("CDIN Frontend — GUI Preview", 1000, 680,
                              SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        CDIN_FRONTEND_ERROR(logger, "GUI", "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        if (logger != NULL) cdin_frontend_logger_destroy(logger);
        return 1;
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        CDIN_FRONTEND_ERROR(logger, "GUI", "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        if (logger != NULL) cdin_frontend_logger_destroy(logger);
        return 1;
    }

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                running = false;
        }
        (void)SDL_SetRenderDrawColor(renderer, 24u, 27u, 34u, 255u);
        (void)SDL_RenderClear(renderer);
        (void)SDL_SetRenderDrawColor(renderer, 42u, 47u, 57u, 255u);
        {
            SDL_FRect title = { 0.0f, 0.0f, 1000.0f, 42.0f };
            SDL_FRect sidebar = { 0.0f, 42.0f, 220.0f, 638.0f };
            SDL_FRect status = { 220.0f, 648.0f, 780.0f, 32.0f };
            (void)SDL_RenderFillRect(renderer, &title);
            (void)SDL_RenderFillRect(renderer, &sidebar);
            (void)SDL_RenderFillRect(renderer, &status);
        }
        (void)SDL_SetRenderDrawColor(renderer, 225u, 230u, 238u, 255u);
        (void)SDL_RenderDebugText(renderer, 16.0f, 14.0f,
                                  "CDIN Frontend — GUI Preview (Esc to quit)");
        (void)SDL_RenderDebugText(renderer, 18.0f, 64.0f, "WORKSPACE");
        (void)SDL_RenderDebugText(renderer, 30.0f, 92.0f, "No live Backend session");
        (void)SDL_RenderDebugText(renderer, 246.0f, 78.0f,
                                  "GUI adapter is isolated from the TUI renderer.");
        (void)SDL_RenderDebugText(renderer, 246.0f, 104.0f,
                                  "A composition root must provide Protocol transport.");
        (void)SDL_RenderDebugText(renderer, 246.0f, 130.0f,
                                  "This window is a build/run smoke preview, not an editor.");
        (void)SDL_RenderDebugText(renderer, 18.0f, 660.0f,
                                  "SDL3 preview | Runtime owns commands and document state");
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    CDIN_FRONTEND_INFO(logger, "GUI", "desktop preview closed");
    if (logger != NULL) cdin_frontend_logger_destroy(logger);
    return 0;
}
