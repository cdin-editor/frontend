#include "utf8.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_ascii_and_nul(void)
{
    const uint8_t bytes[] = { 0x41u, 0x00u, 0x42u };
    cdin_frontend_gui_sdl_utf8_scalar scalars[3];
    size_t count = 0u;

    assert(cdin_frontend_gui_sdl_utf8_decode(
        bytes, sizeof(bytes), scalars, 3u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 3u);
    assert(scalars[0].codepoint == 0x41u);
    assert(scalars[1].codepoint == 0x00u);
    assert(scalars[2].codepoint == 0x42u);
    assert(scalars[0].byte_offset == 0u && scalars[0].byte_length == 1u);
    assert(scalars[1].byte_offset == 1u && scalars[1].byte_length == 1u);
    assert(scalars[2].byte_offset == 2u && scalars[2].byte_length == 1u);
    assert(scalars[0].valid == 1u && scalars[1].valid == 1u &&
           scalars[2].valid == 1u);
}

static void test_persian_and_arabic_letters_remain_distinct(void)
{
    /* Persian Yeh U+06CC and Arabic Yeh U+064A. */
    const uint8_t bytes[] = { 0xdbu, 0x8cu, 0xd9u, 0x8au };
    cdin_frontend_gui_sdl_utf8_scalar scalars[2];
    size_t count = 0u;

    assert(cdin_frontend_gui_sdl_utf8_decode(
        bytes, sizeof(bytes), scalars, 2u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 2u);
    assert(scalars[0].codepoint == 0x06ccu);
    assert(scalars[1].codepoint == 0x064au);
    assert(scalars[0].byte_offset == 0u && scalars[0].byte_length == 2u);
    assert(scalars[1].byte_offset == 2u && scalars[1].byte_length == 2u);
}

static void test_combining_and_zwj_sequences_are_not_rewritten(void)
{
    /* e + COMBINING ACUTE ACCENT: two scalars, three original bytes. */
    const uint8_t combining[] = { 0x65u, 0xccu, 0x81u };
    /* WOMAN + ZERO WIDTH JOINER + LAPTOP: three scalars, eleven bytes. */
    const uint8_t zwj[] = {
        0xf0u, 0x9fu, 0x91u, 0xa9u,
        0xe2u, 0x80u, 0x8du,
        0xf0u, 0x9fu, 0x92u, 0xbbu
    };
    cdin_frontend_gui_sdl_utf8_scalar combining_scalars[2];
    cdin_frontend_gui_sdl_utf8_scalar zwj_scalars[3];
    size_t count = 0u;

    assert(cdin_frontend_gui_sdl_utf8_decode(
        combining, sizeof(combining), combining_scalars, 2u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 2u);
    assert(combining_scalars[0].codepoint == 0x65u);
    assert(combining_scalars[1].codepoint == 0x0301u);
    assert(combining_scalars[1].byte_offset == 1u);
    assert(combining_scalars[1].byte_length == 2u);

    assert(cdin_frontend_gui_sdl_utf8_decode(
        zwj, sizeof(zwj), zwj_scalars, 3u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 3u);
    assert(zwj_scalars[0].codepoint == 0x1f469u);
    assert(zwj_scalars[1].codepoint == 0x200du);
    assert(zwj_scalars[2].codepoint == 0x1f4bbu);
    assert(zwj_scalars[0].byte_offset == 0u &&
           zwj_scalars[1].byte_offset == 4u &&
           zwj_scalars[2].byte_offset == 7u);
    assert(zwj_scalars[2].byte_length == 4u);
}

static void test_invalid_bytes_are_individually_addressable(void)
{
    const uint8_t malformed[] = { 0xe0u, 0x80u, 0xafu, 0x61u };
    const uint8_t surrogate[] = { 0xedu, 0xa0u, 0x80u };
    const uint8_t truncated[] = { 0xf0u, 0x9fu, 0x92u };
    cdin_frontend_gui_sdl_utf8_scalar scalars[4];
    size_t count = 0u;
    size_t index;

    assert(cdin_frontend_gui_sdl_utf8_decode(
        malformed, sizeof(malformed), scalars, 4u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 4u);
    for (index = 0u; index < 3u; ++index) {
        assert(scalars[index].codepoint == 0xfffdu);
        assert(scalars[index].byte_offset == index);
        assert(scalars[index].byte_length == 1u);
        assert(scalars[index].valid == 0u);
    }
    assert(scalars[3].codepoint == 0x61u && scalars[3].valid == 1u);

    assert(cdin_frontend_gui_sdl_utf8_decode(
        surrogate, sizeof(surrogate), scalars, 4u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 3u);
    for (index = 0u; index < count; ++index) {
        assert(scalars[index].codepoint == 0xfffdu);
        assert(scalars[index].byte_offset == index);
        assert(scalars[index].valid == 0u);
    }

    assert(cdin_frontend_gui_sdl_utf8_decode(
        truncated, sizeof(truncated), scalars, 4u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 3u);
    for (index = 0u; index < count; ++index) {
        assert(scalars[index].codepoint == 0xfffdu);
        assert(scalars[index].byte_offset == index);
        assert(scalars[index].byte_length == 1u);
    }
}

static void test_rejects_overlong_surrogate_and_out_of_range_sequences(void)
{
    const uint8_t invalid_values[][4] = {
        { 0xc0u, 0x80u, 0x00u, 0x00u },
        { 0xc1u, 0xbfu, 0x00u, 0x00u },
        { 0xf4u, 0x90u, 0x80u, 0x80u },
        { 0xf5u, 0x80u, 0x80u, 0x80u }
    };
    cdin_frontend_gui_sdl_utf8_scalar scalars[4];
    size_t case_index;

    for (case_index = 0u; case_index < 4u; ++case_index) {
        size_t count = 0u;
        assert(cdin_frontend_gui_sdl_utf8_decode(
            invalid_values[case_index], 2u, scalars, 4u, &count) ==
            CDIN_FRONTEND_GUI_SDL_UTF8_OK);
        assert(count == 2u);
        assert(scalars[0].valid == 0u && scalars[1].valid == 0u);
        assert(scalars[0].byte_length == 1u &&
               scalars[1].byte_length == 1u);
    }
}

static void test_byte_offset_mapping(void)
{
    const uint8_t bytes[] = { 0x41u, 0xdbu, 0x8cu, 0x42u };
    size_t scalar_index = SIZE_MAX;

    assert(cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
        bytes, sizeof(bytes), 0u, &scalar_index) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(scalar_index == 0u);
    assert(cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
        bytes, sizeof(bytes), 1u, &scalar_index) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(scalar_index == 1u);
    assert(cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
        bytes, sizeof(bytes), 2u, &scalar_index) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_BOUNDARY);
    assert(scalar_index == 0u);
    assert(cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
        bytes, sizeof(bytes), 3u, &scalar_index) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(scalar_index == 2u);
    assert(cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
        bytes, sizeof(bytes), 4u, &scalar_index) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(scalar_index == 3u);
    assert(cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
        bytes, sizeof(bytes), 5u, &scalar_index) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_OUT_OF_RANGE);
}

static void test_empty_input_and_failure_atomicity(void)
{
    const uint8_t bytes[] = { 0x41u, 0xd9u, 0x8cu };
    cdin_frontend_gui_sdl_utf8_scalar output[3];
    cdin_frontend_gui_sdl_utf8_scalar unchanged[3];
    size_t count = 99u;

    memset(output, 0xa5, sizeof(output));
    memcpy(unchanged, output, sizeof(output));
    assert(cdin_frontend_gui_sdl_utf8_decode(
        NULL, 0u, NULL, 0u, &count) == CDIN_FRONTEND_GUI_SDL_UTF8_OK);
    assert(count == 0u);

    count = 0u;
    assert(cdin_frontend_gui_sdl_utf8_decode(
        bytes, sizeof(bytes), output, 2u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_BUFFER_TOO_SMALL);
    assert(count == 2u);
    assert(memcmp(output, unchanged, sizeof(output)) == 0);

    assert(cdin_frontend_gui_sdl_utf8_decode(
        bytes, sizeof(bytes), NULL, 0u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_BUFFER_TOO_SMALL);
    assert(count == 2u);

    assert(cdin_frontend_gui_sdl_utf8_decode(
        NULL, 1u, output, 3u, &count) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT);
    assert(cdin_frontend_gui_sdl_utf8_decode(
        bytes, sizeof(bytes), output, 3u, NULL) ==
        CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT);
}

int main(void)
{
    test_ascii_and_nul();
    test_persian_and_arabic_letters_remain_distinct();
    test_combining_and_zwj_sequences_are_not_rewritten();
    test_invalid_bytes_are_individually_addressable();
    test_rejects_overlong_surrogate_and_out_of_range_sequences();
    test_byte_offset_mapping();
    test_empty_input_and_failure_atomicity();
    return 0;
}
