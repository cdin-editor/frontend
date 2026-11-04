#ifndef CDIN_FRONTEND_CORE_CAPABILITIES_H
#define CDIN_FRONTEND_CORE_CAPABILITIES_H

#include <stddef.h>
#include <stdint.h>
#include "cdin/frontend/core/protocol/wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_CAPABILITY_NAME_MAX 63u

typedef uint64_t cdin_frontend_capability_mask;

typedef enum cdin_frontend_capability {
    CDIN_FRONTEND_CAP_CLIPBOARD = 0,
    CDIN_FRONTEND_CAP_DIALOGS,
    CDIN_FRONTEND_CAP_CANVAS,
    CDIN_FRONTEND_CAP_MOUSE,
    CDIN_FRONTEND_CAP_HIT_REGIONS,
    CDIN_FRONTEND_CAP_RICH_TEXT,
    CDIN_FRONTEND_CAP_IMAGES,
    CDIN_FRONTEND_CAP_TRUECOLOR,
    CDIN_FRONTEND_CAP_IME,
    CDIN_FRONTEND_CAP_COUNT
} cdin_frontend_capability;

typedef enum cdin_frontend_capability_status {
    CDIN_FRONTEND_CAPABILITY_OK = 0,
    CDIN_FRONTEND_CAPABILITY_INVALID_ARGUMENT,
    CDIN_FRONTEND_CAPABILITY_INVALID_NAME,
    CDIN_FRONTEND_CAPABILITY_DUPLICATE_NAME,
    CDIN_FRONTEND_CAPABILITY_NOT_REQUESTED,
    CDIN_FRONTEND_CAPABILITY_LIMIT_EXCEEDED
} cdin_frontend_capability_status;

#define CDIN_FRONTEND_CAP_BIT(capability) \
    ((unsigned)(capability) < (unsigned)CDIN_FRONTEND_CAP_COUNT ? \
        (UINT64_C(1) << (unsigned)(capability)) : UINT64_C(0))
#define CDIN_FRONTEND_CAP_MASK_ALL ((UINT64_C(1) << CDIN_FRONTEND_CAP_COUNT) - UINT64_C(1))

/* Names are protocol identifiers, not adapter features. This registry only
 * defines stable spelling and bit identity; it does not activate a service.
 * Returned name pointers refer to immutable process-lifetime strings. */
const char *cdin_frontend_capability_name(cdin_frontend_capability capability);
int cdin_frontend_capability_from_name(const char *name,
                                       cdin_frontend_capability *out_capability);
int cdin_frontend_capability_mask_valid(cdin_frontend_capability_mask mask);
int cdin_frontend_capability_has(cdin_frontend_capability_mask mask,
                                 cdin_frontend_capability capability);
size_t cdin_frontend_capability_count(cdin_frontend_capability_mask mask);
cdin_frontend_capability_status cdin_frontend_capability_name_at(
    cdin_frontend_capability_mask mask, size_t ordinal, const char **out_name);

/* Unknown valid UTF-8 identifiers are ignored for forward compatibility.
 * Empty, malformed UTF-8 and duplicate names are rejected. Output is set on success. */
cdin_frontend_capability_status cdin_frontend_capabilities_parse_names(
    const char *const *names, size_t count, cdin_frontend_capability_mask *out_mask);
cdin_frontend_capability_status cdin_frontend_capabilities_parse_cbor_array(
    const uint8_t *array, size_t array_length, cdin_frontend_capability_mask *out_mask);

/* Accepted capabilities must be a subset of the locally requested set. */
cdin_frontend_capability_status cdin_frontend_capabilities_validate_accepted(
    cdin_frontend_capability_mask requested,
    cdin_frontend_capability_mask accepted,
    cdin_frontend_capability_mask *out_accepted);

#ifdef __cplusplus
}
#endif
#endif
