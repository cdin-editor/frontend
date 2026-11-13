#include "cdin/frontend/core/version.h"
#include "cdin/frontend/core/protocol/wire.h"

#include <assert.h>
#include <string.h>

#if CDIN_FRONTEND_FOUNDATION_VERSION_MAJOR != CDIN_FRONTEND_PROJECT_VERSION_MAJOR
#error "Foundation header and CMake package major versions differ"
#endif
#if CDIN_FRONTEND_FOUNDATION_VERSION_MINOR != CDIN_FRONTEND_PROJECT_VERSION_MINOR
#error "Foundation header and CMake package minor versions differ"
#endif
#if CDIN_FRONTEND_FOUNDATION_VERSION_PATCH != CDIN_FRONTEND_PROJECT_VERSION_PATCH
#error "Foundation header and CMake package patch versions differ"
#endif
#if CDIN_FRONTEND_FOUNDATION_PROTOCOL_MAJOR != CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR
#error "Foundation Protocol version declaration and wire codec differ"
#endif
#if CDIN_FRONTEND_FOUNDATION_PROTOCOL_MINOR != CDIN_FRONTEND_PROTOCOL_VERSION_MINOR
#error "Foundation Protocol version declaration and wire codec differ"
#endif
#if CDIN_FRONTEND_FOUNDATION_PROTOCOL_PATCH != CDIN_FRONTEND_PROTOCOL_VERSION_PATCH
#error "Foundation Protocol version declaration and wire codec differ"
#endif

int main(void)
{
    assert(strcmp(CDIN_FRONTEND_FOUNDATION_VERSION_STRING, "1.2.0") == 0);
    return 0;
}
