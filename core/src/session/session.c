/* High-level Frontend Protocol session state machine and typed payload builders. */
#include "cdin/frontend/core/session/session.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct payload_writer {
    uint8_t bytes[4608];
    size_t length;
    int failed;
} payload_writer;

static void write_head(payload_writer *writer, uint8_t major, uint64_t value)
{
    size_t count;
    size_t index;
    if (writer->failed) return;
    if (value < 24u) {
        if (writer->length >= sizeof(writer->bytes)) { writer->failed = 1; return; }
        writer->bytes[writer->length++] = (uint8_t)(((unsigned)major << 5u) | (unsigned)(uint8_t)value);
        return;
    }
    count = value <= UINT8_MAX ? 1u : value <= UINT16_MAX ? 2u : value <= UINT32_MAX ? 4u : 8u;
    if (count + 1u > sizeof(writer->bytes) - writer->length) { writer->failed = 1; return; }
    writer->bytes[writer->length++] = (uint8_t)(((unsigned)major << 5u) |
        (count == 1u ? 24u : count == 2u ? 25u : count == 4u ? 26u : 27u));
    for (index = 0u; index < count; index++) {
        unsigned shift = (unsigned)((count - index - 1u) * 8u);
        writer->bytes[writer->length++] = (uint8_t)(value >> shift);
    }
}

static void write_uint(payload_writer *writer, uint64_t value) { write_head(writer, 0u, value); }

static void write_text(payload_writer *writer, const char *text, size_t length)
{
    if (text == NULL || length > sizeof(writer->bytes) - writer->length) { writer->failed = 1; return; }
    write_head(writer, 3u, (uint64_t)length);
    if (writer->failed || length > sizeof(writer->bytes) - writer->length) { writer->failed = 1; return; }
    memcpy(writer->bytes + writer->length, text, length);
    writer->length += length;
}

static void write_bytes(payload_writer *writer, const uint8_t *bytes, size_t length)
{
    if (bytes == NULL && length != 0u) { writer->failed = 1; return; }
    write_head(writer, 2u, (uint64_t)length);
    if (writer->failed || length > sizeof(writer->bytes) - writer->length) { writer->failed = 1; return; }
    if (length != 0u) memcpy(writer->bytes + writer->length, bytes, length);
    writer->length += length;
}

static void write_bool(payload_writer *writer, int value)
{
    if (writer->length >= sizeof(writer->bytes)) { writer->failed = 1; return; }
    writer->bytes[writer->length++] = value ? 0xf5u : 0xf4u;
}

static void write_key(payload_writer *writer, const char *key)
{
    write_text(writer, key, strlen(key));
}

static cdin_frontend_protocol_status send_payload(cdin_frontend_session *session,
                                                   cdin_frontend_protocol_message_type type,
                                                   payload_writer *writer)
{
    cdin_frontend_protocol_status status;
    if (writer->failed) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    status = cdin_frontend_client_send(session->client, type, writer->bytes, writer->length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK && session->logger != NULL) {
        CDIN_FRONTEND_ERROR(session->logger, "SESSION", "send %s failed: %s",
                            cdin_frontend_protocol_message_name(type),
                            cdin_frontend_protocol_status_name(status));
    }
    return status;
}


static cdin_frontend_protocol_status send_namespaced_payload(
    cdin_frontend_session *session, const char *name_space,
    cdin_frontend_protocol_message_type type, payload_writer *writer)
{
    cdin_frontend_protocol_status status;
    if (writer->failed) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    status = cdin_frontend_client_send_namespaced(
        session->client, name_space, type, writer->bytes, writer->length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK && session->logger != NULL) {
        CDIN_FRONTEND_ERROR(session->logger, "SESSION", "send %s failed: %s",
                            cdin_frontend_protocol_message_name(type),
                            cdin_frontend_protocol_status_name(status));
    }
    return status;
}

static int utf8_byte_boundary(const uint8_t *bytes, size_t length, uint64_t offset)
{
    return offset <= (uint64_t)length &&
           (offset == 0u || offset == (uint64_t)length ||
            (bytes[(size_t)offset] & 0xc0u) != 0x80u);
}

static int frontend_ime_cancel_reason_valid(const char *reason)
{
    return reason != NULL &&
        (strcmp(reason, "ime_cancelled") == 0 ||
         strcmp(reason, "focus_lost") == 0 ||
         strcmp(reason, "view_changed") == 0);
}

static int backend_ime_cancel_reason_valid(const char *reason)
{
    return reason != NULL &&
        (strcmp(reason, "focus_lost") == 0 ||
         strcmp(reason, "stale_document") == 0 ||
         strcmp(reason, "view_changed") == 0 ||
         strcmp(reason, "selection_changed") == 0 ||
         strcmp(reason, "view_removed") == 0 ||
         strcmp(reason, "driver_revoked") == 0 ||
         strcmp(reason, "session_reset") == 0);
}

static int copy_string(char *output, size_t capacity, const char *value)
{
    size_t length;
    if (output == NULL || value == NULL || capacity == 0u) return -1;
    length = strlen(value);
    if (length == 0u || length >= capacity) return -1;
    memcpy(output, value, length + 1u);
    return 0;
}

static int valid_utf8(const uint8_t *bytes, size_t length)
{
    size_t index = 0u;
    while (index < length) {
        uint8_t first = bytes[index];
        size_t count;
        uint32_t codepoint;
        uint32_t minimum;
        size_t offset;
        if (first < 0x80u) { index++; continue; }
        if (first >= 0xc2u && first <= 0xdfu) { count = 1u; codepoint = first & 0x1fu; minimum = 0x80u; }
        else if (first >= 0xe0u && first <= 0xefu) { count = 2u; codepoint = first & 0x0fu; minimum = 0x800u; }
        else if (first >= 0xf0u && first <= 0xf4u) { count = 3u; codepoint = first & 0x07u; minimum = 0x10000u; }
        else return 0;
        if (count > length - index - 1u) return 0;
        for (offset = 1u; offset <= count; offset++) {
            uint8_t next = bytes[index + offset];
            if ((next & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6u) | (uint32_t)(next & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu)) return 0;
        index += count + 1u;
    }
    return 1;
}

cdin_frontend_protocol_status cdin_frontend_session_set_capabilities(
    cdin_frontend_session *session, cdin_frontend_capability_mask supported)
{
    if (session == NULL || session->client == NULL ||
        session->state != CDIN_FRONTEND_SESSION_INITIAL ||
        !cdin_frontend_capability_mask_valid(supported)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    session->requested_capabilities = supported;
    session->accepted_capabilities = 0u;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_capability_mask cdin_frontend_session_accepted_capabilities(
    const cdin_frontend_session *session)
{
    return session == NULL ? 0u : session->accepted_capabilities;
}

int cdin_frontend_session_is_driver(const cdin_frontend_session *session)
{
    /* A negotiated role event may arrive after attach is sent but before the
     * initial snapshot flush commits the session as ATTACHED. Report the
     * lease state as soon as it is known; mutating APIs remain ATTACHED-only. */
    return session != NULL &&
           (session->state == CDIN_FRONTEND_SESSION_ATTACH_SENT ||
            session->state == CDIN_FRONTEND_SESSION_ATTACHED) &&
           session->driver != 0;
}

void cdin_frontend_session_init(cdin_frontend_session *session,
                                cdin_frontend_client *client,
                                cdin_frontend_logger *logger,
                                const char *frontend_name)
{
    if (session == NULL) return;
    memset(session, 0, sizeof(*session));
    session->client = client;
    session->logger = logger;
    session->state = CDIN_FRONTEND_SESSION_INITIAL;
    session->next_ime_composition_id = 1u;
    if (copy_string(session->frontend_name, sizeof(session->frontend_name), frontend_name) != 0) {
        (void)snprintf(session->frontend_name, sizeof(session->frontend_name), "%s", "cdin-frontend");
    }
}

cdin_frontend_protocol_status cdin_frontend_session_hello(cdin_frontend_session *session)
{
    payload_writer writer;
    if (session == NULL || session->client == NULL || session->state != CDIN_FRONTEND_SESSION_INITIAL) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    size_t capability_count = cdin_frontend_capability_count(session->requested_capabilities);
    size_t capability_index;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 6u);
    write_key(&writer, "capabilities"); write_head(&writer, 4u, (uint64_t)capability_count);
    for (capability_index = 0u; capability_index < capability_count; capability_index++) {
        const char *name = NULL;
        if (cdin_frontend_capability_name_at(session->requested_capabilities,
                                             capability_index, &name) != CDIN_FRONTEND_CAPABILITY_OK) {
            writer.failed = 1;
            break;
        }
        write_text(&writer, name, strlen(name));
    }
    write_key(&writer, "frontend_name"); write_text(&writer, session->frontend_name, strlen(session->frontend_name));
    write_key(&writer, "maximum_major"); write_uint(&writer, CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR);
    write_key(&writer, "maximum_minor"); write_uint(&writer, CDIN_FRONTEND_PROTOCOL_VERSION_MINOR);
    write_key(&writer, "minimum_major"); write_uint(&writer, CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR);
    write_key(&writer, "minimum_minor"); write_uint(&writer, CDIN_FRONTEND_PROTOCOL_VERSION_MINOR);
    {
        cdin_frontend_protocol_status status = send_payload(session, CDIN_FRONTEND_PROTOCOL_HELLO, &writer);
        if (status == CDIN_FRONTEND_PROTOCOL_OK) session->state = CDIN_FRONTEND_SESSION_HELLO_SENT;
        return status;
    }
}

cdin_frontend_protocol_status cdin_frontend_session_receive(
    cdin_frontend_session *session,
    cdin_frontend_protocol_message_view *out_message)
{
    cdin_frontend_protocol_status status;
    uint64_t value;
    if (session == NULL || session->client == NULL || out_message == NULL) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    status = cdin_frontend_client_receive(session->client, out_message);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (strcmp(out_message->name_space, "pane") == 0 &&
        !cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE)) {
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    }
    if (strcmp(out_message->name_space, "ime") == 0 &&
        !cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_IME_COMPOSITION)) {
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    }
    if (out_message->type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL) {
        char reason[32];
        size_t reason_length = 0u;
        if (cdin_frontend_protocol_payload_get_text(out_message->payload,
                out_message->payload_length, "reason", reason, sizeof(reason),
                &reason_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            !backend_ime_cancel_reason_valid(reason)) {
            return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        }
    }
    if (out_message->type == CDIN_FRONTEND_PROTOCOL_HELLO_ACK) {
        if (session->state != CDIN_FRONTEND_SESSION_HELLO_SENT) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        if (cdin_frontend_protocol_payload_get_uint(out_message->payload, out_message->payload_length,
                                                     "major", &value) != CDIN_FRONTEND_PROTOCOL_OK ||
            value != CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR) return CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH;
        if (cdin_frontend_protocol_payload_get_uint(out_message->payload, out_message->payload_length,
                                                     "minor", &value) != CDIN_FRONTEND_PROTOCOL_OK ||
            value != CDIN_FRONTEND_PROTOCOL_VERSION_MINOR) return CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH;
        const uint8_t *accepted_array = NULL;
        size_t accepted_array_length = 0u;
        cdin_frontend_capability_mask accepted = 0u;
        cdin_frontend_protocol_status epoch_status;
        if (cdin_frontend_protocol_payload_get_uint(out_message->payload, out_message->payload_length,
                                                     "session_id", &session->session_id) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(out_message->payload, out_message->payload_length,
                                                     "client_id", &session->client_id) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_bool(out_message->payload, out_message->payload_length,
                                                     "driver", &session->driver) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_raw(out_message->payload, out_message->payload_length,
                                                    "accepted_capabilities", &accepted_array,
                                                    &accepted_array_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_capabilities_parse_cbor_array(accepted_array, accepted_array_length,
                                                        &accepted) != CDIN_FRONTEND_CAPABILITY_OK ||
            cdin_frontend_capabilities_validate_accepted(session->requested_capabilities,
                                                         accepted, &session->accepted_capabilities) !=
                CDIN_FRONTEND_CAPABILITY_OK) {
            return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        }
        epoch_status = cdin_frontend_protocol_payload_get_uint(
            out_message->payload, out_message->payload_length, "driver_role_epoch", &value);
        if (epoch_status == CDIN_FRONTEND_PROTOCOL_OK) {
            if (value != out_message->sequence) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
        } else if (epoch_status != CDIN_FRONTEND_PROTOCOL_UNSUPPORTED ||
                   cdin_frontend_capability_has(session->accepted_capabilities,
                                                CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS)) {
            return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        }
        session->state = CDIN_FRONTEND_SESSION_NEGOTIATED;
    } else if (out_message->type == CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED) {
        int driver = 0;
        uint64_t epoch = 0u;
        if ((session->state != CDIN_FRONTEND_SESSION_ATTACH_SENT &&
             session->state != CDIN_FRONTEND_SESSION_ATTACHED) ||
            !cdin_frontend_capability_has(session->accepted_capabilities,
                                          CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS) ||
            cdin_frontend_protocol_payload_get_bool(out_message->payload, out_message->payload_length,
                                                     "driver", &driver) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(out_message->payload, out_message->payload_length,
                                                     "epoch", &epoch) != CDIN_FRONTEND_PROTOCOL_OK ||
            epoch != out_message->sequence) {
            return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
        }
        session->driver = driver;
    } else if (out_message->type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH) {
        char scope[16];
        size_t length = 0u;
        if (cdin_frontend_protocol_payload_get_text(out_message->payload, out_message->payload_length,
                                                    "scope", scope, sizeof(scope), &length) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(out_message->payload, out_message->payload_length,
                                                    "sequence", &value) != CDIN_FRONTEND_PROTOCOL_OK ||
            value <= session->last_flush_sequence) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
        session->last_flush_sequence = value;
        if (session->state == CDIN_FRONTEND_SESSION_ATTACH_SENT && length == 8u &&
            memcmp(scope, "snapshot", 8u) == 0) {
            session->state = CDIN_FRONTEND_SESSION_ATTACHED;
        }
    } else if (out_message->type == CDIN_FRONTEND_PROTOCOL_DETACH_ACK) {
        if (session->state != CDIN_FRONTEND_SESSION_DETACH_SENT) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        session->driver = 0;
        session->state = CDIN_FRONTEND_SESSION_DETACHED;
    }
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_session_attach(
    cdin_frontend_session *session, const char *view_id, const char *units,
    uint32_t width, uint32_t height)
{
    payload_writer writer;
    cdin_frontend_protocol_status status;
    if (session == NULL ||
        (session->state != CDIN_FRONTEND_SESSION_NEGOTIATED &&
         session->state != CDIN_FRONTEND_SESSION_DETACHED) ||
        session->session_id == 0u || width == 0u || height == 0u ||
        width > 32768u || height > 32768u || units == NULL ||
        (strcmp(units, "cells") != 0 && strcmp(units, "pixels") != 0) ||
        copy_string(session->view_id, sizeof(session->view_id), view_id) != 0) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 5u);
    write_key(&writer, "units"); write_text(&writer, units, strlen(units));
    write_key(&writer, "width"); write_uint(&writer, width);
    write_key(&writer, "height"); write_uint(&writer, height);
    write_key(&writer, "view_id"); write_text(&writer, view_id, strlen(view_id));
    write_key(&writer, "session_id"); write_uint(&writer, session->session_id);
    status = send_payload(session, CDIN_FRONTEND_PROTOCOL_ATTACH, &writer);
    if (status == CDIN_FRONTEND_PROTOCOL_OK) {
        session->width = width;
        session->height = height;
        session->units = strcmp(units, "cells") == 0 ? "cells" : "pixels";
        session->state = CDIN_FRONTEND_SESSION_ATTACH_SENT;
    }
    return status;
}

cdin_frontend_protocol_status cdin_frontend_session_ack(cdin_frontend_session *session,
                                                        uint64_t flush_sequence)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED ||
        flush_sequence == 0u || flush_sequence > session->last_flush_sequence) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 1u);
    write_key(&writer, "sequence"); write_uint(&writer, flush_sequence);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_ACK, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_request_lines(
    cdin_frontend_session *session, const char *view_id, uint64_t first_line,
    uint64_t line_count, uint64_t version)
{
    payload_writer writer;
    if (session == NULL || (session->state != CDIN_FRONTEND_SESSION_ATTACHED &&
        session->state != CDIN_FRONTEND_SESSION_ATTACH_SENT) || view_id == NULL ||
        view_id[0] == '\0' || line_count == 0u || line_count > 4096u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 4u);
    write_key(&writer, "version"); write_uint(&writer, version);
    write_key(&writer, "view_id"); write_text(&writer, view_id, strlen(view_id));
    write_key(&writer, "first_line"); write_uint(&writer, first_line);
    write_key(&writer, "line_count"); write_uint(&writer, line_count);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_VIEW_REQUEST_LINES, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_send_pane_splitter_resize(
    cdin_frontend_session *session,
    uint64_t request_id,
    const char *split_id,
    uint64_t layout_revision,
    uint16_t ratio_bp)
{
    payload_writer writer;
    size_t split_id_length;
    cdin_frontend_protocol_status status;
    if (session == NULL || session->client == NULL ||
        session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver ||
        request_id == 0u || layout_revision == 0u ||
        split_id == NULL || split_id[0] == '\0' ||
        ratio_bp < 1000u || ratio_bp > 8999u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (!cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE)) {
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    }
    split_id_length = strlen(split_id);
    if (split_id_length > 128u ||
        !valid_utf8((const uint8_t *)split_id, split_id_length)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(&writer, 0, sizeof(writer));
    /* Deterministic CBOR key order: ratio_bp, split_id, request_id,
     * layout_revision (encoded key length, then bytewise lexical order). */
    write_head(&writer, 5u, 4u);
    write_key(&writer, "ratio_bp"); write_uint(&writer, ratio_bp);
    write_key(&writer, "split_id"); write_text(&writer, split_id, split_id_length);
    write_key(&writer, "request_id"); write_uint(&writer, request_id);
    write_key(&writer, "layout_revision"); write_uint(&writer, layout_revision);
    if (writer.failed) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    status = cdin_frontend_client_send_namespaced(
        session->client, "pane", CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE,
        writer.bytes, writer.length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK && session->logger != NULL) {
        CDIN_FRONTEND_ERROR(session->logger, "SESSION",
            "send pane.splitter.resize failed: %s",
            cdin_frontend_protocol_status_name(status));
    }
    return status;
}

cdin_frontend_protocol_status cdin_frontend_session_send_key(
    cdin_frontend_session *session, const char *key, uint32_t modifiers, const char *action)
{
    payload_writer writer;
    uint64_t count = 0u;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver ||
        key == NULL || key[0] == '\0' || action == NULL || action[0] == '\0') {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if ((modifiers & ~(uint32_t)(CDIN_FRONTEND_MOD_CTRL | CDIN_FRONTEND_MOD_ALT |
                                  CDIN_FRONTEND_MOD_SHIFT | CDIN_FRONTEND_MOD_ALTGR)) != 0u ||
        (strcmp(action, "press") != 0 && strcmp(action, "release") != 0 && strcmp(action, "repeat") != 0)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if ((modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u) count++;
    if ((modifiers & CDIN_FRONTEND_MOD_ALT) != 0u) count++;
    if ((modifiers & CDIN_FRONTEND_MOD_ALTGR) != 0u) count++;
    if ((modifiers & CDIN_FRONTEND_MOD_SHIFT) != 0u) count++;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 3u);
    write_key(&writer, "key"); write_text(&writer, key, strlen(key));
    write_key(&writer, "action"); write_text(&writer, action, strlen(action));
    write_key(&writer, "modifiers"); write_head(&writer, 4u, count);
    if ((modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u) write_text(&writer, "ctrl", 4u);
    if ((modifiers & CDIN_FRONTEND_MOD_ALT) != 0u) write_text(&writer, "alt", 3u);
    if ((modifiers & CDIN_FRONTEND_MOD_ALTGR) != 0u) write_text(&writer, "altgr", 5u);
    if ((modifiers & CDIN_FRONTEND_MOD_SHIFT) != 0u) write_text(&writer, "shift", 5u);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_INPUT_KEY, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_send_mouse(
    cdin_frontend_session *session,
    const char *action,
    const char *button,
    uint32_t modifiers,
    const char *target,
    uint32_t x,
    uint32_t y,
    int has_position)
{
    static const char *const actions[] = { "press", "release", "move", "scroll" };
    static const char *const buttons[] = {
        "none", "left", "middle", "right", "wheel_up", "wheel_down", "x1", "x2"
    };
    payload_writer writer;
    uint64_t modifier_count = 0u;
    size_t index;
    int action_valid = 0;
    int button_valid = 0;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED ||
        !session->driver || action == NULL || button == NULL ||
        (modifiers & ~(uint32_t)(CDIN_FRONTEND_MOD_CTRL | CDIN_FRONTEND_MOD_ALT |
                                  CDIN_FRONTEND_MOD_SHIFT | CDIN_FRONTEND_MOD_ALTGR)) != 0u ||
        (target != NULL && (target[0] == '\0' || strlen(target) > 128u)) ||
        (has_position != 0 && (x > 32768u || y > 32768u))) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (!cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_MOUSE)) {
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    }
    for (index = 0u; index < sizeof(actions) / sizeof(actions[0]); ++index) {
        if (strcmp(action, actions[index]) == 0) action_valid = 1;
    }
    for (index = 0u; index < sizeof(buttons) / sizeof(buttons[0]); ++index) {
        if (strcmp(button, buttons[index]) == 0) button_valid = 1;
    }
    if (!action_valid || !button_valid) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;

    if ((modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u) modifier_count++;
    if ((modifiers & CDIN_FRONTEND_MOD_ALT) != 0u) modifier_count++;
    if ((modifiers & CDIN_FRONTEND_MOD_ALTGR) != 0u) modifier_count++;
    if ((modifiers & CDIN_FRONTEND_MOD_SHIFT) != 0u) modifier_count++;

    /* Canonical CBOR key ordering: action, button, target, position, modifiers. */
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 3u + (target != NULL ? 1u : 0u) +
                            (has_position != 0 ? 1u : 0u));
    write_key(&writer, "action"); write_text(&writer, action, strlen(action));
    write_key(&writer, "button"); write_text(&writer, button, strlen(button));
    if (target != NULL) {
        write_key(&writer, "target"); write_text(&writer, target, strlen(target));
    }
    if (has_position != 0) {
        write_key(&writer, "position");
        write_head(&writer, 5u, 2u);
        write_key(&writer, "x"); write_uint(&writer, x);
        write_key(&writer, "y"); write_uint(&writer, y);
    }
    write_key(&writer, "modifiers"); write_head(&writer, 4u, modifier_count);
    if ((modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u) write_text(&writer, "ctrl", 4u);
    if ((modifiers & CDIN_FRONTEND_MOD_ALT) != 0u) write_text(&writer, "alt", 3u);
    if ((modifiers & CDIN_FRONTEND_MOD_ALTGR) != 0u) write_text(&writer, "altgr", 5u);
    if ((modifiers & CDIN_FRONTEND_MOD_SHIFT) != 0u) write_text(&writer, "shift", 5u);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE, &writer);
}


uint64_t cdin_frontend_session_next_ime_composition_id(
    const cdin_frontend_session *session)
{
    return session != NULL ? session->next_ime_composition_id : 0u;
}

cdin_frontend_protocol_status cdin_frontend_session_ime_start(
    cdin_frontend_session *session, uint64_t composition_id,
    const char *view_id, uint64_t document_version)
{
    payload_writer writer;
    size_t view_length;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED ||
        !session->driver || composition_id == 0u ||
        session->next_ime_composition_id == 0u ||
        composition_id < session->next_ime_composition_id ||
        view_id == NULL || view_id[0] == '\0')
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (!cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_IME_COMPOSITION))
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    view_length = strlen(view_id);
    if (view_length > 128u || !valid_utf8((const uint8_t *)view_id, view_length))
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 3u);
    write_key(&writer, "id"); write_uint(&writer, composition_id);
    write_key(&writer, "view_id"); write_text(&writer, view_id, view_length);
    write_key(&writer, "document_version"); write_uint(&writer, document_version);
    {
        cdin_frontend_protocol_status status = send_namespaced_payload(
            session, "ime", CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START, &writer);
        if (status == CDIN_FRONTEND_PROTOCOL_OK) {
            session->next_ime_composition_id =
                composition_id == UINT64_MAX ? 0u : composition_id + 1u;
        }
        return status;
    }
}

cdin_frontend_protocol_status cdin_frontend_session_ime_update(
    cdin_frontend_session *session, uint64_t composition_id,
    const uint8_t *preedit, size_t preedit_length, uint64_t cursor_byte,
    uint64_t selection_start_byte, uint64_t selection_end_byte)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED ||
        !session->driver || composition_id == 0u || preedit == NULL)
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (!cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_IME_COMPOSITION))
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    if (preedit_length > 4096u) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    if (!valid_utf8(preedit, preedit_length) ||
        selection_start_byte > selection_end_byte ||
        !utf8_byte_boundary(preedit, preedit_length, cursor_byte) ||
        !utf8_byte_boundary(preedit, preedit_length, selection_start_byte) ||
        !utf8_byte_boundary(preedit, preedit_length, selection_end_byte))
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 5u);
    write_key(&writer, "id"); write_uint(&writer, composition_id);
    write_key(&writer, "preedit"); write_text(&writer, (const char *)preedit, preedit_length);
    write_key(&writer, "cursor_byte"); write_uint(&writer, cursor_byte);
    write_key(&writer, "selection_end_byte"); write_uint(&writer, selection_end_byte);
    write_key(&writer, "selection_start_byte"); write_uint(&writer, selection_start_byte);
    return send_namespaced_payload(session, "ime",
        CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_ime_cancel(
    cdin_frontend_session *session, uint64_t composition_id, const char *reason)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED ||
        !session->driver || composition_id == 0u ||
        !frontend_ime_cancel_reason_valid(reason))
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (!cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_IME_COMPOSITION))
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 2u);
    write_key(&writer, "id"); write_uint(&writer, composition_id);
    write_key(&writer, "reason"); write_text(&writer, reason, strlen(reason));
    return send_namespaced_payload(session, "ime",
        CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_ime_commit(
    cdin_frontend_session *session, uint64_t composition_id,
    const uint8_t *text, size_t text_length)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED ||
        !session->driver || composition_id == 0u || text == NULL ||
        text_length == 0u || !valid_utf8(text, text_length))
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (!cdin_frontend_capability_has(session->accepted_capabilities,
                                      CDIN_FRONTEND_CAP_IME_COMPOSITION))
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 2u);
    write_key(&writer, "text"); write_text(&writer, (const char *)text, text_length);
    write_key(&writer, "composition_id"); write_uint(&writer, composition_id);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_send_text(
    cdin_frontend_session *session, const uint8_t *text, size_t text_length)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver ||
        text == NULL || text_length == 0u || !valid_utf8(text, text_length)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 1u);
    write_key(&writer, "text"); write_text(&writer, (const char *)text, text_length);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_send_paste(
    cdin_frontend_session *session, const uint8_t *bytes, size_t length)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver ||
        (bytes == NULL && length != 0u)) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 1u);
    write_key(&writer, "bytes"); write_bytes(&writer, bytes, length);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_INPUT_PASTE, &writer);
}


cdin_frontend_protocol_status cdin_frontend_session_send_input_event(
    cdin_frontend_session *session,
    const cdin_frontend_input_event *event)
{
    if (session == NULL || event == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    switch (event->kind) {
        case CDIN_FRONTEND_INPUT_KEY:
            if (event->key[0] == '\0') return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
            return cdin_frontend_session_send_key(session, event->key, event->modifiers, "press");
        case CDIN_FRONTEND_INPUT_TEXT:
            if (event->text_length == 0u || event->text_length > sizeof(event->text)) {
                return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
            }
            return cdin_frontend_session_send_text(session, event->text, event->text_length);
        case CDIN_FRONTEND_INPUT_PASTE:
            if (event->text_length > sizeof(event->text)) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
            return cdin_frontend_session_send_paste(session, event->text, event->text_length);
        case CDIN_FRONTEND_INPUT_UNKNOWN:
        default:
            return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    }
}

cdin_frontend_protocol_status cdin_frontend_session_resize(
    cdin_frontend_session *session, uint32_t width, uint32_t height)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver ||
        session->units == NULL ||
        (strcmp(session->units, "cells") != 0 && strcmp(session->units, "pixels") != 0) ||
        width == 0u || height == 0u || width > 32768u || height > 32768u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 3u);
    write_key(&writer, "units"); write_text(&writer, session->units, strlen(session->units));
    write_key(&writer, "width"); write_uint(&writer, width);
    write_key(&writer, "height"); write_uint(&writer, height);
    session->width = width;
    session->height = height;
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_WINDOW_RESIZE, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_focus(cdin_frontend_session *session, int focused)
{
    payload_writer writer;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 1u);
    write_key(&writer, "focused"); write_bool(&writer, focused != 0);
    return send_payload(session, CDIN_FRONTEND_PROTOCOL_FOCUS, &writer);
}

cdin_frontend_protocol_status cdin_frontend_session_detach(cdin_frontend_session *session)
{
    payload_writer writer;
    cdin_frontend_protocol_status status;
    if (session == NULL || session->state != CDIN_FRONTEND_SESSION_ATTACHED) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    memset(&writer, 0, sizeof(writer));
    write_head(&writer, 5u, 0u);
    status = send_payload(session, CDIN_FRONTEND_PROTOCOL_DETACH, &writer);
    if (status == CDIN_FRONTEND_PROTOCOL_OK) session->state = CDIN_FRONTEND_SESSION_DETACH_SENT;
    return status;
}
