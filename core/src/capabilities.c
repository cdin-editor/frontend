/* Stable Frontend capability identifiers and negotiation-set validation. */
#include "cdin/frontend/core/capabilities.h"

#include <string.h>

static const char *const capability_names[CDIN_FRONTEND_CAP_COUNT] = {
    "clipboard", "dialogs", "canvas", "mouse", "hit_regions",
    "rich_text", "images", "truecolor", "ime", "driver-role-events/1"
};

static int valid_utf8_name(const unsigned char *bytes, size_t length)
{
    size_t index = 0u;
    while (index < length) {
        unsigned char first = bytes[index++];
        if (first <= 0x7fu) continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            if (length - index < 1u || (bytes[index] & 0xc0u) != 0x80u) return 0;
            index++;
        } else if (first >= 0xe0u && first <= 0xefu) {
            unsigned char second;
            if (length - index < 2u) return 0;
            second = bytes[index++];
            if ((second & 0xc0u) != 0x80u ||
                (first == 0xe0u && second < 0xa0u) ||
                (first == 0xedu && second >= 0xa0u) ||
                (bytes[index] & 0xc0u) != 0x80u) return 0;
            index++;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            unsigned char second;
            if (length - index < 3u) return 0;
            second = bytes[index++];
            if ((second & 0xc0u) != 0x80u ||
                (first == 0xf0u && second < 0x90u) ||
                (first == 0xf4u && second > 0x8fu) ||
                (bytes[index] & 0xc0u) != 0x80u ||
                (bytes[index + 1u] & 0xc0u) != 0x80u) return 0;
            index += 2u;
        } else {
            return 0;
        }
    }
    return 1;
}

const char *cdin_frontend_capability_name(cdin_frontend_capability capability)
{
    if ((unsigned)capability >= (unsigned)CDIN_FRONTEND_CAP_COUNT) return NULL;
    return capability_names[(unsigned)capability];
}

int cdin_frontend_capability_from_name(const char *name,
                                       cdin_frontend_capability *out_capability)
{
    size_t index;
    if (name == NULL || out_capability == NULL) return 0;
    for (index = 0u; index < (size_t)CDIN_FRONTEND_CAP_COUNT; index++) {
        if (strcmp(name, capability_names[index]) == 0) {
            *out_capability = (cdin_frontend_capability)index;
            return 1;
        }
    }
    return 0;
}

int cdin_frontend_capability_mask_valid(cdin_frontend_capability_mask mask)
{
    return (mask & ~CDIN_FRONTEND_CAP_MASK_ALL) == 0u;
}

int cdin_frontend_capability_has(cdin_frontend_capability_mask mask,
                                 cdin_frontend_capability capability)
{
    if (!cdin_frontend_capability_mask_valid(mask) ||
        (unsigned)capability >= (unsigned)CDIN_FRONTEND_CAP_COUNT) return 0;
    return (mask & CDIN_FRONTEND_CAP_BIT(capability)) != 0u;
}

size_t cdin_frontend_capability_count(cdin_frontend_capability_mask mask)
{
    size_t count = 0u;
    unsigned index;
    if (!cdin_frontend_capability_mask_valid(mask)) return 0u;
    for (index = 0u; index < (unsigned)CDIN_FRONTEND_CAP_COUNT; index++) {
        if ((mask & (UINT64_C(1) << index)) != 0u) count++;
    }
    return count;
}

cdin_frontend_capability_status cdin_frontend_capability_name_at(
    cdin_frontend_capability_mask mask, size_t ordinal, const char **out_name)
{
    unsigned index;
    size_t found = 0u;
    if (out_name == NULL || !cdin_frontend_capability_mask_valid(mask)) {
        return CDIN_FRONTEND_CAPABILITY_INVALID_ARGUMENT;
    }
    for (index = 0u; index < (unsigned)CDIN_FRONTEND_CAP_COUNT; index++) {
        if ((mask & (UINT64_C(1) << index)) == 0u) continue;
        if (found == ordinal) {
            *out_name = capability_names[index];
            return CDIN_FRONTEND_CAPABILITY_OK;
        }
        found++;
    }
    return CDIN_FRONTEND_CAPABILITY_LIMIT_EXCEEDED;
}

cdin_frontend_capability_status cdin_frontend_capabilities_parse_names(
    const char *const *names, size_t count, cdin_frontend_capability_mask *out_mask)
{
    cdin_frontend_capability_mask result = 0u;
    size_t index;
    if (out_mask == NULL || (names == NULL && count != 0u)) {
        return CDIN_FRONTEND_CAPABILITY_INVALID_ARGUMENT;
    }
    if (count > 4096u) return CDIN_FRONTEND_CAPABILITY_LIMIT_EXCEEDED;
    for (index = 0u; index < count; index++) {
        size_t length;
        size_t prior;
        cdin_frontend_capability capability;
        if (names[index] == NULL) return CDIN_FRONTEND_CAPABILITY_INVALID_NAME;
        length = strlen(names[index]);
        if (length == 0u || !valid_utf8_name((const unsigned char *)names[index], length)) {
            return CDIN_FRONTEND_CAPABILITY_INVALID_NAME;
        }
        for (prior = 0u; prior < index; prior++) {
            if (strcmp(names[prior], names[index]) == 0) {
                return CDIN_FRONTEND_CAPABILITY_DUPLICATE_NAME;
            }
        }
        if (length <= CDIN_FRONTEND_CAPABILITY_NAME_MAX &&
            cdin_frontend_capability_from_name(names[index], &capability)) {
            result |= CDIN_FRONTEND_CAP_BIT(capability);
        }
    }
    *out_mask = result;
    return CDIN_FRONTEND_CAPABILITY_OK;
}

cdin_frontend_capability_status cdin_frontend_capabilities_parse_cbor_array(
    const uint8_t *array, size_t array_length, cdin_frontend_capability_mask *out_mask)
{
    cdin_frontend_capability_mask result = 0u;
    size_t count = 0u;
    size_t index;
    if (array == NULL || out_mask == NULL) return CDIN_FRONTEND_CAPABILITY_INVALID_ARGUMENT;
    if (cdin_frontend_protocol_array_count(array, array_length, &count) !=
        CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_CAPABILITY_INVALID_ARGUMENT;
    for (index = 0u; index < count; index++) {
        char name[CDIN_FRONTEND_CAPABILITY_NAME_MAX + 1u];
        size_t name_length = 0u;
        size_t previous;
        cdin_frontend_capability capability;
        cdin_frontend_protocol_status wire_status;
        wire_status = cdin_frontend_protocol_array_get_text(
            array, array_length, index, name, sizeof(name), &name_length);
        if (wire_status == CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL) {
            /* Longer valid UTF-8 identifiers are unknown to this registry. */
            for (previous = 0u; previous < index; previous++) {
                const uint8_t *a;
                const uint8_t *b;
                size_t a_length;
                size_t b_length;
                if (cdin_frontend_protocol_array_get_raw(array, array_length, previous,
                                                          &a, &a_length) != CDIN_FRONTEND_PROTOCOL_OK ||
                    cdin_frontend_protocol_array_get_raw(array, array_length, index,
                                                          &b, &b_length) != CDIN_FRONTEND_PROTOCOL_OK) {
                    return CDIN_FRONTEND_CAPABILITY_INVALID_NAME;
                }
                if (a_length == b_length && memcmp(a, b, a_length) == 0) {
                    return CDIN_FRONTEND_CAPABILITY_DUPLICATE_NAME;
                }
            }
            continue;
        }
        if (wire_status != CDIN_FRONTEND_PROTOCOL_OK || name_length == 0u ||
            memchr(name, '\0', name_length) != NULL) {
            return CDIN_FRONTEND_CAPABILITY_INVALID_NAME;
        }
        for (previous = 0u; previous < index; previous++) {
            const uint8_t *a;
            const uint8_t *b;
            size_t a_length;
            size_t b_length;
            if (cdin_frontend_protocol_array_get_raw(array, array_length, previous,
                                                      &a, &a_length) != CDIN_FRONTEND_PROTOCOL_OK ||
                cdin_frontend_protocol_array_get_raw(array, array_length, index,
                                                      &b, &b_length) != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_CAPABILITY_INVALID_NAME;
            }
            if (a_length == b_length && memcmp(a, b, a_length) == 0) {
                return CDIN_FRONTEND_CAPABILITY_DUPLICATE_NAME;
            }
        }
        if (cdin_frontend_capability_from_name(name, &capability)) {
            result |= CDIN_FRONTEND_CAP_BIT(capability);
        }
    }
    *out_mask = result;
    return CDIN_FRONTEND_CAPABILITY_OK;
}

cdin_frontend_capability_status cdin_frontend_capabilities_validate_accepted(
    cdin_frontend_capability_mask requested,
    cdin_frontend_capability_mask accepted,
    cdin_frontend_capability_mask *out_accepted)
{
    if (out_accepted == NULL || !cdin_frontend_capability_mask_valid(requested) ||
        !cdin_frontend_capability_mask_valid(accepted)) {
        return CDIN_FRONTEND_CAPABILITY_INVALID_ARGUMENT;
    }
    if ((accepted & ~requested) != 0u) return CDIN_FRONTEND_CAPABILITY_NOT_REQUESTED;
    *out_accepted = accepted;
    return CDIN_FRONTEND_CAPABILITY_OK;
}
