#include "presentation.h"

#include <assert.h>
#include <string.h>

static const uint8_t valid_styles[] = {
    0xa1u,
    0x67u, 'k', 'e', 'y', 'w', 'o', 'r', 'd',
    0xa3u,
    0x6au, 'f', 'o', 'r', 'e', 'g', 'r', 'o', 'u', 'n', 'd',
    0xa4u,
    0x61u, 'r', 0x18u, 0xffu,
    0x61u, 'g', 0x18u, 0x3cu,
    0x61u, 'b', 0x0au,
    0x61u, 'a', 0x18u, 0xffu,
    0x64u, 'b', 'o', 'l', 'd', 0xf5u,
    0x66u, 'f', 'u', 't', 'u', 'r', 'e', 0x81u, 0x01u
};

static const uint8_t invalid_channel_styles[] = {
    0xa1u,
    0x67u, 'k', 'e', 'y', 'w', 'o', 'r', 'd',
    0xa1u,
    0x6au, 'f', 'o', 'r', 'e', 'g', 'r', 'o', 'u', 'n', 'd',
    0xa4u,
    0x61u, 'r', 0x19u, 0x01u, 0x00u,
    0x61u, 'g', 0x00u,
    0x61u, 'b', 0x00u,
    0x61u, 'a', 0x18u, 0xffu
};

static const uint8_t invalid_boolean_styles[] = {
    0xa1u,
    0x67u, 'k', 'e', 'y', 'w', 'o', 'r', 'd',
    0xa1u,
    0x64u, 'b', 'o', 'l', 'd', 0x01u
};

static const uint8_t invalid_role_styles[] = {
    0xa1u,
    0x68u, 'b', 'a', 'd', ' ', 'r', 'o', 'l', 'e',
    0xa0u
};

static void test_decodes_portable_style_role(void)
{
    cdin_frontend_gui_sdl_style_table table;
    const cdin_frontend_gui_sdl_style_role *role;
    memset(&table, 0, sizeof(table));
    assert(cdin_frontend_gui_sdl_style_table_decode(
        valid_styles, sizeof(valid_styles), &table) ==
        CDIN_FRONTEND_GUI_SDL_STYLE_OK);
    assert(table.role_count == 1u);
    role = cdin_frontend_gui_sdl_style_table_find(&table, "keyword");
    assert(role != NULL);
    assert(role->has_foreground == 1u);
    assert(role->foreground[0] == 255u);
    assert(role->foreground[1] == 60u);
    assert(role->foreground[2] == 10u);
    assert(role->foreground[3] == 255u);
    assert(role->bold == 1u);
    assert(role->italic == 0u);
    assert(role->has_background == 0u);
    assert(cdin_frontend_gui_sdl_style_table_find(&table, "missing") == NULL);
}

static void test_empty_table_is_valid_and_replaces_prior_roles(void)
{
    const uint8_t empty_styles[] = {0xa0u};
    cdin_frontend_gui_sdl_style_table table;
    memset(&table, 0, sizeof(table));
    table.role_count = 1u;
    (void)strcpy(table.roles[0].id, "old");
    assert(cdin_frontend_gui_sdl_style_table_decode(
        empty_styles, sizeof(empty_styles), &table) ==
        CDIN_FRONTEND_GUI_SDL_STYLE_OK);
    assert(table.role_count == 0u);
    assert(cdin_frontend_gui_sdl_style_table_find(&table, "old") == NULL);
}

static void test_malformed_known_attributes_reject_atomically(void)
{
    cdin_frontend_gui_sdl_style_table table;
    cdin_frontend_gui_sdl_style_table previous;
    memset(&table, 0, sizeof(table));
    table.role_count = 1u;
    (void)strcpy(table.roles[0].id, "previous");
    previous = table;

    assert(cdin_frontend_gui_sdl_style_table_decode(
        invalid_channel_styles, sizeof(invalid_channel_styles), &table) ==
        CDIN_FRONTEND_GUI_SDL_STYLE_INVALID);
    assert(memcmp(&table, &previous, sizeof(table)) == 0);
    assert(cdin_frontend_gui_sdl_style_table_decode(
        invalid_boolean_styles, sizeof(invalid_boolean_styles), &table) ==
        CDIN_FRONTEND_GUI_SDL_STYLE_INVALID);
    assert(memcmp(&table, &previous, sizeof(table)) == 0);
}

static void test_invalid_role_id_rejects_table(void)
{
    cdin_frontend_gui_sdl_style_table table;
    memset(&table, 0, sizeof(table));
    assert(cdin_frontend_gui_sdl_style_table_decode(
        invalid_role_styles, sizeof(invalid_role_styles), &table) ==
        CDIN_FRONTEND_GUI_SDL_STYLE_INVALID);
    assert(table.role_count == 0u);
}

int main(void)
{
    test_decodes_portable_style_role();
    test_empty_table_is_valid_and_replaces_prior_roles();
    test_malformed_known_attributes_reject_atomically();
    test_invalid_role_id_rejects_table();
    return 0;
}
