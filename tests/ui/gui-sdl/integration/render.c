#include "render.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void set_view(
    cdin_frontend_gui_sdl_model *model,
    size_t index,
    const char *view_id,
    int focused)
{
    cdin_frontend_gui_sdl_view *view = &model->views[index];
    memset(view, 0, sizeof(*view));
    assert(strlen(view_id) < sizeof(view->semantic.view_id));
    memcpy(view->semantic.view_id, view_id, strlen(view_id) + 1u);
    view->semantic.version = 1u;
    view->semantic.in_layout = 1;
    view->semantic.visible = 1;
    view->semantic.focused = focused;
    model->view_count = index + 1u;
}

static void set_leaf(
    cdin_frontend_gui_sdl_layout *layout,
    size_t node_index,
    const char *node_id,
    uint8_t view_index)
{
    cdin_frontend_gui_sdl_layout_node *node = &layout->nodes[node_index];
    assert(strlen(node_id) < sizeof(node->id));
    memcpy(node->id, node_id, strlen(node_id) + 1u);
    node->kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF;
    node->first_child = SIZE_MAX;
    node->second_child = SIZE_MAX;
    node->view_indices[0] = view_index;
    node->view_count = 1u;
    node->active_view_index = view_index;
    node->has_active_view = 1;
}

static void make_two_pane_model(cdin_frontend_gui_sdl_model *model)
{
    memset(model, 0, sizeof(*model));
    model->has_layout = 1;
    model->layout.has_root = 1;
    model->layout.has_active_leaf = 1;
    model->layout.root_index = 0u;
    model->layout.active_leaf_index = 1u;
    model->layout.node_count = 3u;
    model->layout.leaf_count = 2u;
    model->layout.view_count = 2u;
    (void)strcpy(model->layout.view_ids[0], "view-left");
    (void)strcpy(model->layout.view_ids[1], "view-right");

    model->layout.nodes[0].kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT;
    model->layout.nodes[0].first_child = 1u;
    model->layout.nodes[0].second_child = 2u;
    model->layout.nodes[0].axis = CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL;
    model->layout.nodes[0].ratio_bp = 5000u;
    set_leaf(&model->layout, 1u, "leaf-left", 0u);
    set_leaf(&model->layout, 2u, "leaf-right", 1u);

    set_view(model, 0u, "view-left", 1);
    set_view(model, 1u, "view-right", 0);
}

static void test_two_panes_render_to_distinct_pixel_regions(
    SDL_Renderer *renderer)
{
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_rect viewport = { 0u, 42u, 640u, 328u };
    cdin_frontend_gui_sdl_render_stats stats;
    cdin_frontend_gui_sdl_render_status status;
    SDL_Surface *pixels;
    uint8_t left_r = 0u;
    uint8_t left_g = 0u;
    uint8_t left_b = 0u;
    uint8_t right_r = 0u;
    uint8_t right_g = 0u;
    uint8_t right_b = 0u;

    make_two_pane_model(&model);
    assert(SDL_SetRenderDrawColor(renderer, 0u, 0u, 0u, 255u));
    assert(SDL_RenderClear(renderer));

    status = cdin_frontend_gui_sdl_render_panes(
        renderer, &model, &viewport, &stats);
    assert(status == CDIN_FRONTEND_GUI_SDL_RENDER_OK);
    assert(stats.pane_count == 2u);
    assert(stats.rendered_view_count == 2u);

    pixels = SDL_RenderReadPixels(renderer, NULL);
    assert(pixels != NULL);
    assert(SDL_ReadSurfacePixel(
        pixels, 160, 200, &left_r, &left_g, &left_b, NULL));
    assert(SDL_ReadSurfacePixel(
        pixels, 480, 200, &right_r, &right_g, &right_b, NULL));
    assert(left_r == 31u && left_g == 36u && left_b == 46u);
    assert(right_r == 25u && right_g == 30u && right_b == 39u);
    assert(left_r != right_r || left_g != right_g || left_b != right_b);
    SDL_DestroySurface(pixels);
}

static void test_nested_layout_renders_every_active_leaf(
    SDL_Renderer *renderer)
{
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_rect viewport = { 0u, 42u, 640u, 328u };
    cdin_frontend_gui_sdl_render_stats stats;
    cdin_frontend_gui_sdl_render_status status;
    SDL_Surface *pixels;
    uint8_t left_r = 0u;
    uint8_t left_g = 0u;
    uint8_t left_b = 0u;
    uint8_t top_r = 0u;
    uint8_t top_g = 0u;
    uint8_t top_b = 0u;
    uint8_t bottom_r = 0u;
    uint8_t bottom_g = 0u;
    uint8_t bottom_b = 0u;

    make_two_pane_model(&model);
    model.layout.node_count = 5u;
    model.layout.leaf_count = 3u;
    model.layout.view_count = 3u;
    (void)strcpy(model.layout.view_ids[1], "view-top-right");
    (void)strcpy(model.layout.view_ids[2], "view-bottom-right");

    model.layout.nodes[0].second_child = 2u;
    model.layout.nodes[0].ratio_bp = 4000u;
    set_leaf(&model.layout, 1u, "leaf-left", 0u);
    memset(&model.layout.nodes[2], 0, sizeof(model.layout.nodes[2]));
    (void)strcpy(model.layout.nodes[2].id, "split-right");
    model.layout.nodes[2].kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT;
    model.layout.nodes[2].first_child = 3u;
    model.layout.nodes[2].second_child = 4u;
    model.layout.nodes[2].axis = CDIN_FRONTEND_GUI_SDL_LAYOUT_VERTICAL;
    model.layout.nodes[2].ratio_bp = 5000u;
    set_leaf(&model.layout, 3u, "leaf-top-right", 1u);
    set_leaf(&model.layout, 4u, "leaf-bottom-right", 2u);

    set_view(&model, 1u, "view-top-right", 1);
    set_view(&model, 2u, "view-bottom-right", 0);
    model.layout.active_leaf_index = 3u;
    model.views[0].semantic.focused = 0;

    assert(SDL_SetRenderDrawColor(renderer, 0u, 0u, 0u, 255u));
    assert(SDL_RenderClear(renderer));
    status = cdin_frontend_gui_sdl_render_panes(
        renderer, &model, &viewport, &stats);
    assert(status == CDIN_FRONTEND_GUI_SDL_RENDER_OK);
    assert(stats.pane_count == 3u);
    assert(stats.rendered_view_count == 3u);

    pixels = SDL_RenderReadPixels(renderer, NULL);
    assert(pixels != NULL);
    assert(SDL_ReadSurfacePixel(
        pixels, 128, 206, &left_r, &left_g, &left_b, NULL));
    assert(SDL_ReadSurfacePixel(
        pixels, 448, 124, &top_r, &top_g, &top_b, NULL));
    assert(SDL_ReadSurfacePixel(
        pixels, 448, 288, &bottom_r, &bottom_g, &bottom_b, NULL));
    assert(left_r == 25u && left_g == 30u && left_b == 39u);
    assert(top_r == 31u && top_g == 36u && top_b == 46u);
    assert(bottom_r == 25u && bottom_g == 30u && bottom_b == 39u);
    SDL_DestroySurface(pixels);
}

static int surface_region_has_rgb(
    SDL_Surface *surface, int left, int top, int right, int bottom,
    uint8_t expected_r, uint8_t expected_g, uint8_t expected_b)
{
    int x;
    int y;
    for (y = top; y < bottom; ++y) {
        for (x = left; x < right; ++x) {
            uint8_t red = 0u;
            uint8_t green = 0u;
            uint8_t blue = 0u;
            if (SDL_ReadSurfacePixel(
                    surface, x, y, &red, &green, &blue, NULL) &&
                red == expected_r && green == expected_g &&
                blue == expected_b) {
                return 1;
            }
        }
    }
    return 0;
}

static void test_line_spans_resolve_semantic_style_roles(
    SDL_Renderer *renderer)
{
    static const uint8_t line_bytes[] = "abcd";
    static const uint8_t spans[] = {
        0x81u,
        0xa3u,
        0x65u, 's', 't', 'a', 'r', 't', 0x02u,
        0x63u, 'e', 'n', 'd', 0x04u,
        0x64u, 'r', 'o', 'l', 'e',
        0x67u, 'k', 'e', 'y', 'w', 'o', 'r', 'd'
    };
    static const uint8_t styles[] = {
        0xa1u,
        0x67u, 'k', 'e', 'y', 'w', 'o', 'r', 'd',
        0xa1u,
        0x6au, 'f', 'o', 'r', 'e', 'g', 'r', 'o', 'u', 'n', 'd',
        0xa4u,
        0x61u, 'r', 0x18u, 0xffu,
        0x61u, 'g', 0x18u, 0x3cu,
        0x61u, 'b', 0x0au,
        0x61u, 'a', 0x18u, 0xffu
    };
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_view *view;
    cdin_frontend_gui_sdl_rect viewport = { 0u, 42u, 640u, 328u };
    cdin_frontend_gui_sdl_render_stats stats;
    SDL_Surface *pixels;

    make_two_pane_model(&model);
    view = &model.views[0];
    view->visible.count = 1u;
    view->visible.lines[0].bytes = (uint8_t *)(uintptr_t)line_bytes;
    view->visible.lines[0].length = sizeof(line_bytes) - 1u;
    view->visible.lines[0].spans_cbor = (uint8_t *)(uintptr_t)spans;
    view->visible.lines[0].spans_cbor_length = sizeof(spans);
    view->visible.lines[0].span_count = 1u;
    assert(cdin_frontend_gui_sdl_style_table_decode(
        styles, sizeof(styles), &model.presentation.decoded_style_table) ==
        CDIN_FRONTEND_GUI_SDL_STYLE_OK);

    assert(SDL_SetRenderDrawColor(renderer, 0u, 0u, 0u, 255u));
    assert(SDL_RenderClear(renderer));
    assert(cdin_frontend_gui_sdl_render_panes(
        renderer, &model, &viewport, &stats) ==
        CDIN_FRONTEND_GUI_SDL_RENDER_OK);
    pixels = SDL_RenderReadPixels(renderer, NULL);
    assert(pixels != NULL);
    /* The span covers the final two ASCII glyphs; the prefix keeps the
     * adapter's default foreground. */
    assert(surface_region_has_rgb(pixels, 72, 68, 88, 78, 255u, 60u, 10u));
    assert(!surface_region_has_rgb(pixels, 56, 68, 72, 78, 255u, 60u, 10u));
    SDL_DestroySurface(pixels);
}

static void test_invalid_layout_fails_closed_and_empty_layout_draws_nothing(
    SDL_Renderer *renderer)
{
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_rect viewport = { 0u, 42u, 640u, 328u };
    cdin_frontend_gui_sdl_render_stats stats;

    make_two_pane_model(&model);
    model.layout.nodes[0].first_child = 0u;
    assert(cdin_frontend_gui_sdl_render_panes(
        renderer, &model, &viewport, &stats) ==
        CDIN_FRONTEND_GUI_SDL_RENDER_INVALID_LAYOUT);
    assert(stats.pane_count == 0u);

    memset(&model, 0, sizeof(model));
    model.has_layout = 1;
    model.layout.root_index = SIZE_MAX;
    assert(SDL_SetRenderDrawColor(renderer, 0u, 0u, 0u, 255u));
    assert(SDL_RenderClear(renderer));
    assert(cdin_frontend_gui_sdl_render_panes(
        renderer, &model, &viewport, &stats) ==
        CDIN_FRONTEND_GUI_SDL_RENDER_OK);
    assert(stats.pane_count == 0u);
    assert(stats.rendered_view_count == 0u);
}

int main(void)
{
    SDL_Window *window;
    SDL_Renderer *renderer;

    assert(SDL_Init(SDL_INIT_VIDEO));
    window = SDL_CreateWindow("CDIN multi-pane render test", 640, 400,
                              SDL_WINDOW_HIDDEN);
    assert(window != NULL);
    renderer = SDL_CreateRenderer(window, NULL);
    assert(renderer != NULL);

    test_two_panes_render_to_distinct_pixel_regions(renderer);
    test_nested_layout_renders_every_active_leaf(renderer);
    test_line_spans_resolve_semantic_style_roles(renderer);
    test_invalid_layout_fails_closed_and_empty_layout_draws_nothing(renderer);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
