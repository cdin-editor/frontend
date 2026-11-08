#include "utf8.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static cdin_frontend_gui_sdl_utf8_scalar next_scalar(
    const uint8_t *bytes, size_t length, size_t *offset)
{
    cdin_frontend_gui_sdl_utf8_scalar scalar;
    assert(cdin_frontend_gui_sdl_utf8_decode_next(
        bytes, length, offset, &scalar) == CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    return scalar;
}

static void expect_scalar(
    const cdin_frontend_gui_sdl_utf8_scalar *scalar,
    uint32_t value, size_t start, size_t end, int valid)
{
    assert(scalar->value == value);
    assert(scalar->byte_start == start);
    assert(scalar->byte_end == end);
    assert(scalar->valid_utf8 == valid);
}

static void test_valid_scalars_preserve_source_byte_ranges(void)
{
    static const uint8_t input[] = {
        0x41u, 0xdau, 0x8cu, 0xccu, 0x81u,
        0xf0u, 0x9fu, 0x91u, 0xa9u,
        0xe2u, 0x80u, 0x8du,
        0xf0u, 0x9fu, 0x92u, 0xbbu, 0x00u
    };
    size_t offset = 0u;
    cdin_frontend_gui_sdl_utf8_scalar scalar;

    scalar = next_scalar(input, sizeof(input), &offset);
    expect_scalar(&scalar, 0x41u, 0u, 1u, 1);
    scalar = next_scalar(input, sizeof(input), &offset);
    expect_scalar(&scalar, 0x06ccu, 1u, 3u, 1); /* Persian yeh */
    scalar = next_scalar(input, sizeof(input), &offset);
    expect_scalar(&scalar, 0x0301u, 3u, 5u, 1); /* combining acute */
    scalar = next_scalar(input, sizeof(input), &offset);
    expect_scalar(&scalar, 0x1f469u, 5u, 9u, 1); /* woman */
    scalar = next_scalar(input, sizeof(input), &offset);
    expect_scalar(&scalar, 0x200du, 9u, 12u, 1); /* ZWJ */
    scalar = next_scalar(input, sizeof(input), &offset);
    expect_scalar(&scalar, 0x1f4bbu, 12u, 16u, 1); /* laptop */
    scalar = next_scalar(input, sizeof(input), &offset);
    expect_scalar(&scalar, 0u, 16u, 17u, 1); /* embedded NUL */
    assert(offset == sizeof(input));
    assert(cdin_frontend_gui_sdl_utf8_decode_next(
        input, sizeof(input), &offset, &scalar) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_END);
}

static void expect_all_invalid_one_byte(const uint8_t *input, size_t length)
{
    size_t offset = 0u;
    while (offset < length) {
        size_t start = offset;
        cdin_frontend_gui_sdl_utf8_scalar scalar =
            next_scalar(input, length, &offset);
        expect_scalar(&scalar, 0xfffdu, start, start + 1u, 0);
    }
    assert(offset == length);
}

static void test_malformed_sequences_consume_one_source_byte(void)
{
    static const uint8_t stray_continuation[] = { 0x80u };
    static const uint8_t invalid_lead_and_overlong[] = { 0xc0u, 0xafu };
    static const uint8_t three_byte_overlong[] = { 0xe0u, 0x80u, 0x80u };
    static const uint8_t surrogate[] = { 0xedu, 0xa0u, 0x80u };
    static const uint8_t beyond_unicode_max[] = { 0xf4u, 0x90u, 0x80u, 0x80u };
    static const uint8_t truncated[] = { 0xe2u, 0x82u };
    static const uint8_t bad_middle[] = { 0xe2u, 0x28u, 0xa1u };

    expect_all_invalid_one_byte(stray_continuation, sizeof(stray_continuation));
    expect_all_invalid_one_byte(invalid_lead_and_overlong,
                                sizeof(invalid_lead_and_overlong));
    expect_all_invalid_one_byte(three_byte_overlong, sizeof(three_byte_overlong));
    expect_all_invalid_one_byte(surrogate, sizeof(surrogate));
    expect_all_invalid_one_byte(beyond_unicode_max, sizeof(beyond_unicode_max));
    expect_all_invalid_one_byte(truncated, sizeof(truncated));

    {
        size_t offset = 0u;
        cdin_frontend_gui_sdl_utf8_scalar scalar;
        scalar = next_scalar(bad_middle, sizeof(bad_middle), &offset);
        expect_scalar(&scalar, 0xfffdu, 0u, 1u, 0);
        scalar = next_scalar(bad_middle, sizeof(bad_middle), &offset);
        expect_scalar(&scalar, 0x28u, 1u, 2u, 1);
        scalar = next_scalar(bad_middle, sizeof(bad_middle), &offset);
        expect_scalar(&scalar, 0xfffdu, 2u, 3u, 0);
    }
}

static void test_empty_input_and_invalid_arguments(void)
{
    cdin_frontend_gui_sdl_utf8_scalar scalar;
    size_t offset = 0u;

    assert(cdin_frontend_gui_sdl_utf8_decode_next(
        NULL, 0u, &offset, &scalar) == CDIN_FRONTEND_GUI_SDL_UTF8_END);
    assert(cdin_frontend_gui_sdl_utf8_decode_next(
        NULL, 1u, &offset, &scalar) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT);
    assert(cdin_frontend_gui_sdl_utf8_decode_next(
        (const uint8_t *)"x", 1u, NULL, &scalar) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT);
    assert(cdin_frontend_gui_sdl_utf8_decode_next(
        (const uint8_t *)"x", 1u, &offset, NULL) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT);

    offset = 2u;
    assert(cdin_frontend_gui_sdl_utf8_decode_next(
        (const uint8_t *)"x", 1u, &offset, &scalar) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT);
    assert(offset == 2u);
}

int main(void)
{
    test_valid_scalars_preserve_source_byte_ranges();
    test_malformed_sequences_consume_one_source_byte();
    test_empty_input_and_invalid_arguments();
    return 0;
}
