#include "cdin/frontend/core/capabilities.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    const char *names[] = { "mouse", "clipboard", "driver-role-events/1", "future.optional" };
    const char *duplicate_names[] = { "mouse", "mouse" };
    const char *invalid_names[] = { "bad\xc0\xaf" };
    static const uint8_t valid_array[] = {
        0x83u, 0x65u, 'm','o','u','s','e',
        0x74u, 'd','r','i','v','e','r','-','r','o','l','e','-','e','v','e','n','t','s','/','1',
        0x66u, 'f','u','t','u','r','e'
    };
    static const uint8_t duplicate_array[] = { 0x82u, 0x65u, 'm','o','u','s','e', 0x65u, 'm','o','u','s','e' };
    static const uint8_t invalid_item_array[] = { 0x81u, 0x01u };
    cdin_frontend_capability_mask parsed = 0u;
    cdin_frontend_capability_mask accepted = 0u;
    cdin_frontend_capability capability = CDIN_FRONTEND_CAP_MOUSE;
    const char *name = NULL;

    assert(cdin_frontend_capability_mask_valid(0u));
    assert(cdin_frontend_capability_mask_valid(CDIN_FRONTEND_CAP_MASK_ALL));
    assert(!cdin_frontend_capability_mask_valid(UINT64_MAX));
    assert(cdin_frontend_capability_name(CDIN_FRONTEND_CAP_MOUSE) != NULL);
    assert(strcmp(cdin_frontend_capability_name(CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS),
                  "driver-role-events/1") == 0);
    assert(strcmp(cdin_frontend_capability_name(CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE),
                  "pane-splitter-resize/1") == 0);
    assert(cdin_frontend_capability_from_name("pane-splitter-resize/1", &capability));
    assert(capability == CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE);
    assert(cdin_frontend_capability_from_name("mouse", &capability));
    assert(capability == CDIN_FRONTEND_CAP_MOUSE);
    assert(cdin_frontend_capabilities_parse_names(names, 3u, &parsed) ==
           CDIN_FRONTEND_CAPABILITY_OK);
    assert(parsed == (CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE) |
                      CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_CLIPBOARD) |
                      CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS)));
    assert(cdin_frontend_capability_count(parsed) == 3u);
    assert(cdin_frontend_capabilities_parse_cbor_array(valid_array, sizeof(valid_array),
                                                        &parsed) == CDIN_FRONTEND_CAPABILITY_OK);
    assert(parsed == (CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE) |
                      CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS)));
    assert(cdin_frontend_capabilities_parse_cbor_array(duplicate_array, sizeof(duplicate_array),
                                                        &parsed) == CDIN_FRONTEND_CAPABILITY_DUPLICATE_NAME);
    assert(cdin_frontend_capabilities_parse_cbor_array(invalid_item_array, sizeof(invalid_item_array),
                                                        &parsed) == CDIN_FRONTEND_CAPABILITY_INVALID_NAME);
    assert(cdin_frontend_capability_name_at(parsed, 0u, &name) == CDIN_FRONTEND_CAPABILITY_OK);
    assert(name != NULL);
    assert(cdin_frontend_capabilities_parse_names(duplicate_names, 2u, &parsed) ==
           CDIN_FRONTEND_CAPABILITY_DUPLICATE_NAME);
    assert(cdin_frontend_capabilities_parse_names(invalid_names, 1u, &parsed) ==
           CDIN_FRONTEND_CAPABILITY_INVALID_NAME);
    assert(cdin_frontend_capabilities_validate_accepted(
        CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE),
        CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE), &accepted) ==
        CDIN_FRONTEND_CAPABILITY_OK);
    assert(accepted == CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE));
    assert(cdin_frontend_capabilities_validate_accepted(
        CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE),
        CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_DIALOGS), &accepted) ==
        CDIN_FRONTEND_CAPABILITY_NOT_REQUESTED);
    return 0;
}
