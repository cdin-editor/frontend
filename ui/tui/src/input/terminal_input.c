/* Physical terminal key decoding. Editor keymaps remain in Runtime (PROTO-004). */
#include "cdin/frontend/ui/tui/tui.h"

#include <string.h>

static void set_key(cdin_frontend_input_event *event, const char *name)
{
    size_t length = strlen(name);
    if (length >= sizeof(event->key)) length = sizeof(event->key) - 1u;
    memcpy(event->key, name, length);
    event->key[length] = '\0';
    event->kind = CDIN_FRONTEND_INPUT_KEY;
}

static void set_csi_modifiers(cdin_frontend_input_event *event, uint32_t parameter)
{
    uint32_t encoded = parameter > 0u ? parameter - 1u : 0u;
    if ((encoded & 1u) != 0u) event->modifiers |= CDIN_FRONTEND_MOD_SHIFT;
    if ((encoded & 2u) != 0u) event->modifiers |= CDIN_FRONTEND_MOD_ALT;
    if ((encoded & 4u) != 0u) event->modifiers |= CDIN_FRONTEND_MOD_CTRL;
}

static int decode_csi(const uint8_t *bytes, size_t length, cdin_frontend_input_event *event)
{
    uint32_t parameters[3] = { 0u, 0u, 0u };
    size_t parameter_count = 0u;
    size_t index = 2u;
    uint8_t final;
    if (length < 3u || bytes[0] != 0x1bu || bytes[1] != '[') return 0;
    final = bytes[length - 1u];
    while (index + 1u < length) {
        uint8_t value = bytes[index];
        if (value >= '0' && value <= '9') {
            if (parameter_count == 0u) parameter_count = 1u;
            if (parameters[parameter_count - 1u] > 100000u) return 0;
            parameters[parameter_count - 1u] = parameters[parameter_count - 1u] * 10u + (uint32_t)(value - '0');
            index++;
            continue;
        }
        if (value == ';' && parameter_count < 3u) {
            parameter_count++;
            index++;
            continue;
        }
        return 0;
    }
    if (final == 'A' || final == 'B' || final == 'C' || final == 'D' ||
        final == 'H' || final == 'F') {
        if (parameter_count > 2u || (parameter_count != 0u && parameters[0] != 1u)) return 0;
        switch (final) {
            case 'A': set_key(event, "up"); break;
            case 'B': set_key(event, "down"); break;
            case 'C': set_key(event, "right"); break;
            case 'D': set_key(event, "left"); break;
            case 'H': set_key(event, "home"); break;
            case 'F': set_key(event, "end"); break;
            default: return 0;
        }
        if (parameter_count == 2u) set_csi_modifiers(event, parameters[1]);
        return 1;
    }
    if (final == '~' && parameter_count >= 1u) {
        switch (parameters[0]) {
            case 1u: set_key(event, "home"); break;
            case 3u: set_key(event, "delete"); break;
            case 4u: set_key(event, "end"); break;
            case 5u: set_key(event, "page_up"); break;
            case 6u: set_key(event, "page_down"); break;
            case 15u: set_key(event, "f5"); break;
            case 17u: set_key(event, "f6"); break;
            case 18u: set_key(event, "f7"); break;
            case 19u: set_key(event, "f8"); break;
            case 20u: set_key(event, "f9"); break;
            case 21u: set_key(event, "f10"); break;
            case 23u: set_key(event, "f11"); break;
            case 24u: set_key(event, "f12"); break;
            default: return 0;
        }
        if (parameter_count >= 2u) set_csi_modifiers(event, parameters[1]);
        return 1;
    }
    if (final == 'Z' && parameter_count == 0u) {
        set_key(event, "tab");
        event->modifiers = CDIN_FRONTEND_MOD_SHIFT;
        return 1;
    }
    return 0;
}

cdin_frontend_tui_status cdin_frontend_tui_decode_input(
    const uint8_t *bytes, size_t length, cdin_frontend_input_event *out_event)
{
    if (bytes == NULL || length == 0u || out_event == NULL) return CDIN_FRONTEND_TUI_INVALID_ARGUMENT;
    memset(out_event, 0, sizeof(*out_event));
    if (length == 1u) {
        uint8_t value = bytes[0];
        if (value == 27u) { set_key(out_event, "escape"); return CDIN_FRONTEND_TUI_OK; }
        if (value == '\r' || value == '\n') { set_key(out_event, "enter"); return CDIN_FRONTEND_TUI_OK; }
        if (value == '\t') { set_key(out_event, "tab"); return CDIN_FRONTEND_TUI_OK; }
        if (value == 127u || value == 8u) { set_key(out_event, "backspace"); return CDIN_FRONTEND_TUI_OK; }
        if (value == 0u) {
            set_key(out_event, "space");
            out_event->modifiers = CDIN_FRONTEND_MOD_CTRL;
            return CDIN_FRONTEND_TUI_OK;
        }
        if (value >= 1u && value <= 26u) {
            char key_name[2];
            key_name[0] = (char)('a' + value - 1u);
            key_name[1] = '\0';
            set_key(out_event, key_name);
            out_event->modifiers = CDIN_FRONTEND_MOD_CTRL;
            return CDIN_FRONTEND_TUI_OK;
        }
        if (value >= 0x20u) {
            out_event->kind = CDIN_FRONTEND_INPUT_TEXT;
            out_event->text[0] = value;
            out_event->text_length = 1u;
            return CDIN_FRONTEND_TUI_OK;
        }
    }
    if (length >= 3u && bytes[0] == 0x1bu && bytes[1] == '[') {
        if (decode_csi(bytes, length, out_event)) return CDIN_FRONTEND_TUI_OK;
    }
    if (length == 3u && bytes[0] == 0x1bu && bytes[1] == 'O') {
        switch (bytes[2]) {
            case 'P': set_key(out_event, "f1"); return CDIN_FRONTEND_TUI_OK;
            case 'Q': set_key(out_event, "f2"); return CDIN_FRONTEND_TUI_OK;
            case 'R': set_key(out_event, "f3"); return CDIN_FRONTEND_TUI_OK;
            case 'S': set_key(out_event, "f4"); return CDIN_FRONTEND_TUI_OK;
            default: break;
        }
    }
    if (length == 3u && (bytes[0] == 0u || bytes[0] == 0xe0u)) {
        switch (bytes[2]) {
            case 72u: set_key(out_event, "up"); return CDIN_FRONTEND_TUI_OK;
            case 80u: set_key(out_event, "down"); return CDIN_FRONTEND_TUI_OK;
            case 77u: set_key(out_event, "right"); return CDIN_FRONTEND_TUI_OK;
            case 75u: set_key(out_event, "left"); return CDIN_FRONTEND_TUI_OK;
            case 71u: set_key(out_event, "home"); return CDIN_FRONTEND_TUI_OK;
            case 79u: set_key(out_event, "end"); return CDIN_FRONTEND_TUI_OK;
            case 83u: set_key(out_event, "delete"); return CDIN_FRONTEND_TUI_OK;
            default: break;
        }
    }
    if (length >= 6u && bytes[0] == 0x1bu && bytes[1] == '[' &&
        memcmp(bytes + 2u, "200~", 4u) == 0) {
        static const uint8_t paste_end[] = { 0x1bu, '[', '2', '0', '1', '~' };
        if (length < sizeof(paste_end) || memcmp(bytes + length - sizeof(paste_end),
                                                  paste_end, sizeof(paste_end)) != 0) {
            return CDIN_FRONTEND_TUI_INVALID_ARGUMENT;
        }
        length -= 6u + sizeof(paste_end);
        if (length > sizeof(out_event->text)) return CDIN_FRONTEND_TUI_LIMIT_EXCEEDED;
        out_event->kind = CDIN_FRONTEND_INPUT_PASTE;
        if (length != 0u) memcpy(out_event->text, bytes + 6u, length);
        out_event->text_length = length;
        return CDIN_FRONTEND_TUI_OK;
    }
    if (length <= sizeof(out_event->text)) {
        out_event->kind = CDIN_FRONTEND_INPUT_TEXT;
        memcpy(out_event->text, bytes, length);
        out_event->text_length = length;
        return CDIN_FRONTEND_TUI_OK;
    }
    out_event->kind = CDIN_FRONTEND_INPUT_UNKNOWN;
    return CDIN_FRONTEND_TUI_LIMIT_EXCEEDED;
}
