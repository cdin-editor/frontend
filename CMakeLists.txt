cmake_minimum_required(VERSION 3.22)
project(cdin_frontend VERSION 1.1.1 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(CDIN_FRONTEND_USE_LOG_STUB "Use the test-only logger ABI shim (never for release)" OFF)
option(CDIN_FRONTEND_BUILD_UI_TUI "Build the interactive terminal UI adapter" ON)
option(CDIN_FRONTEND_BUILD_UI_SDL "Build the production GUI-SDL adapter and preview" OFF)
option(CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER "Allow GUI-SDL to request pane-splitter-resize/1 (keep OFF until CDIN product E2E passes)" OFF)
option(CDIN_FRONTEND_GUI_SDL_ENABLE_IME_COMPOSITION "Allow GUI-SDL to request ime-composition/1 (keep OFF until CDIN live IME E2E passes)" OFF)
option(CDIN_FRONTEND_GUI_SDL_ENABLE_BYTE_ANCHORED_LINES "Allow GUI-SDL to request byte-anchored-lines/1 (keep OFF until CDIN byte-anchor E2E passes)" OFF)
option(CDIN_FRONTEND_GUI_SDL_TEST_CAPTURE_OVERRIDE "Enable the deterministic GUI-SDL pointer-capture test seam" OFF)
option(CDIN_FRONTEND_BUILD_APPS "Build runnable adapter preview applications" ON)
option(CDIN_FRONTEND_BUILD_UI_HEADLESS "Build the display-free reference recorder/replayer" ON)
option(CDIN_FRONTEND_BUILD_EXAMPLES "Build small Frontend examples" ON)

if(CDIN_FRONTEND_USE_LOG_STUB)
    add_library(cdin_test_log_stub STATIC tests/support/log_stub.c)
    target_include_directories(cdin_test_log_stub PUBLIC tests/support/include)
    add_library(m-mdy-m::log ALIAS cdin_test_log_stub)
else()
    find_package(m-mdy-m-log 0.1.0 CONFIG REQUIRED)
endif()

# CORE: UI-neutral Frontend services and public contracts only.
# FE-001 / FE-016: the session package encodes typed Protocol peer messages.
# It contains no terminal, SDL, window-system or drawing code.
add_library(cdin_frontend_core STATIC
    core/src/diagnostics/log.c
    core/src/capabilities.c
    core/src/protocol/wire.c
    core/src/protocol/client.c
    core/src/protocol/async_client.c
    core/src/session/session.c
    core/src/session/command.c
    core/src/model/state.c
    core/src/model/semantic.c)
add_library(cdin::frontend_core ALIAS cdin_frontend_core)
set_target_properties(cdin_frontend_core PROPERTIES
    EXPORT_NAME frontend_core
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})
target_compile_features(cdin_frontend_core PUBLIC c_std_11)
target_include_directories(cdin_frontend_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(cdin_frontend_core PUBLIC m-mdy-m::log)

function(cdin_frontend_enable_strict target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wshadow -Wconversion -Wstrict-prototypes
            -Wformat=2 -Wpedantic -Werror)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
endfunction()

function(cdin_frontend_enable_test_strict target)
    cdin_frontend_enable_strict(${target})
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -UNDEBUG)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    endif()
endfunction()

cdin_frontend_enable_strict(cdin_frontend_core)

# UI/TUI: peer adapter. Export the reusable adapter library and its public
# headers when built; the preview executable remains a development-only app.
# Exporting a linkable target does not claim FE-019 or product readiness.
if(CDIN_FRONTEND_BUILD_UI_TUI)
    if(WIN32)
        set(CDIN_FRONTEND_TUI_TERMINAL_SOURCE ui/tui/src/platform/terminal_windows.c)
    else()
        set(CDIN_FRONTEND_TUI_TERMINAL_SOURCE ui/tui/src/platform/terminal_posix.c)
    endif()
    add_library(cdin_frontend_ui_tui STATIC
        ui/tui/src/render/tui.c
        ui/tui/src/layout/layout.c
        ui/tui/src/model/model.c
        ui/tui/src/session/session.c
        ui/tui/src/session/event_loop.c
        ui/tui/src/input/terminal_input.c
        ui/tui/src/input/parser.c
        ${CDIN_FRONTEND_TUI_TERMINAL_SOURCE})
    add_library(cdin::frontend_ui_tui ALIAS cdin_frontend_ui_tui)
    set_target_properties(cdin_frontend_ui_tui PROPERTIES EXPORT_NAME frontend_ui_tui)
    target_compile_features(cdin_frontend_ui_tui PUBLIC c_std_11)
    target_include_directories(cdin_frontend_ui_tui
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ui/tui/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(cdin_frontend_ui_tui PUBLIC cdin::frontend_core)
    cdin_frontend_enable_strict(cdin_frontend_ui_tui)
    if(CDIN_FRONTEND_BUILD_APPS)
        add_executable(cdin_frontend_tui_preview apps/tui/main.c)
        target_compile_features(cdin_frontend_tui_preview PRIVATE c_std_11)
        target_link_libraries(cdin_frontend_tui_preview PRIVATE cdin::frontend_ui_tui)
        cdin_frontend_enable_strict(cdin_frontend_tui_preview)
    endif()
endif()

# GUI-SDL is a peer adapter. SDL usage is confined to this target.
# Its reusable library owns desktop presentation
# and depends only on Frontend Core plus SDL3. The preview remains separate.
if(CDIN_FRONTEND_BUILD_UI_SDL)
    find_package(SDL3 3.2 CONFIG REQUIRED)
    add_library(cdin_frontend_ui_gui_sdl STATIC
        ui/gui-sdl/src/gui_sdl.c
        ui/gui-sdl/src/session/model.c
        ui/gui-sdl/src/session/layout.c
        ui/gui-sdl/src/layout/geometry.c
        ui/gui-sdl/src/text/utf8.c
        ui/gui-sdl/src/render/render.c)
    add_library(cdin::frontend_ui_gui_sdl ALIAS cdin_frontend_ui_gui_sdl)
    set_target_properties(cdin_frontend_ui_gui_sdl PROPERTIES
        EXPORT_NAME frontend_ui_gui_sdl)
    target_compile_features(cdin_frontend_ui_gui_sdl PUBLIC c_std_11)
    target_include_directories(cdin_frontend_ui_gui_sdl
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ui/gui-sdl/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/ui/gui-sdl/src/session)
    target_link_libraries(cdin_frontend_ui_gui_sdl
        PUBLIC cdin::frontend_core
        PRIVATE SDL3::SDL3)
    if(CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER)
        target_compile_definitions(cdin_frontend_ui_gui_sdl PRIVATE
            CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER=1)
    endif()
    if(CDIN_FRONTEND_GUI_SDL_ENABLE_IME_COMPOSITION)
        target_compile_definitions(cdin_frontend_ui_gui_sdl PRIVATE
            CDIN_FRONTEND_GUI_SDL_ENABLE_IME_COMPOSITION=1)
    endif()
    if(CDIN_FRONTEND_GUI_SDL_ENABLE_BYTE_ANCHORED_LINES)
        target_compile_definitions(cdin_frontend_ui_gui_sdl PRIVATE
            CDIN_FRONTEND_GUI_SDL_ENABLE_BYTE_ANCHORED_LINES=1)
    endif()
    if(CDIN_FRONTEND_GUI_SDL_TEST_CAPTURE_OVERRIDE)
        target_compile_definitions(cdin_frontend_ui_gui_sdl PRIVATE
            CDIN_FRONTEND_GUI_SDL_TESTING=1)
    endif()
    cdin_frontend_enable_strict(cdin_frontend_ui_gui_sdl)

    if(CDIN_FRONTEND_BUILD_APPS)
        add_executable(cdin_frontend_gui_sdl_preview ui/gui-sdl/app/main.c)
        target_compile_features(cdin_frontend_gui_sdl_preview PRIVATE c_std_11)
        target_link_libraries(cdin_frontend_gui_sdl_preview PRIVATE
            SDL3::SDL3 cdin::frontend_ui_gui_sdl)
        cdin_frontend_enable_strict(cdin_frontend_gui_sdl_preview)
    endif()
endif()
if(CDIN_FRONTEND_BUILD_UI_HEADLESS)
    add_library(cdin_frontend_ui_headless STATIC
        ui/headless/src/recorder.c)
    add_library(cdin::frontend_ui_headless ALIAS cdin_frontend_ui_headless)
    set_target_properties(cdin_frontend_ui_headless PROPERTIES
        EXPORT_NAME frontend_ui_headless)
    target_compile_features(cdin_frontend_ui_headless PUBLIC c_std_11)
    target_include_directories(cdin_frontend_ui_headless
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ui/headless/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(cdin_frontend_ui_headless PUBLIC cdin::frontend_core)
    cdin_frontend_enable_strict(cdin_frontend_ui_headless)
endif()

if(BUILD_TESTING)
    function(cdin_frontend_core_test target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE cdin::frontend_core)
        target_compile_features(${target} PRIVATE c_std_11)
        cdin_frontend_enable_test_strict(${target})
        add_test(NAME ${target} COMMAND ${target})
    endfunction()
    cdin_frontend_core_test(cdin_frontend_logging_tests tests/core/unit/logging.c)
    cdin_frontend_core_test(cdin_frontend_capability_tests tests/core/unit/capabilities.c)
    add_executable(cdin_frontend_version_tests tests/core/unit/version.c)
    target_link_libraries(cdin_frontend_version_tests PRIVATE cdin::frontend_core)
    target_compile_features(cdin_frontend_version_tests PRIVATE c_std_11)
    target_compile_definitions(cdin_frontend_version_tests PRIVATE
        CDIN_FRONTEND_PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        CDIN_FRONTEND_PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        CDIN_FRONTEND_PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH})
    cdin_frontend_enable_test_strict(cdin_frontend_version_tests)
    add_test(NAME cdin_frontend_version_tests COMMAND cdin_frontend_version_tests)
    cdin_frontend_core_test(cdin_frontend_model_tests tests/core/unit/model_state.c)
    cdin_frontend_core_test(cdin_frontend_semantic_model_tests tests/core/unit/semantic_model.c)
    cdin_frontend_core_test(cdin_frontend_protocol_wire_tests tests/core/unit/protocol_wire.c)
    cdin_frontend_core_test(cdin_frontend_protocol_client_tests tests/core/integration/protocol_client.c)
    cdin_frontend_core_test(cdin_frontend_async_protocol_client_tests tests/core/integration/async_protocol_client.c)
    cdin_frontend_core_test(cdin_frontend_session_tests tests/core/integration/session_state.c)
    cdin_frontend_core_test(cdin_frontend_command_request_tests tests/core/integration/command_request.c)

    if(CDIN_FRONTEND_BUILD_UI_SDL)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME cdin_frontend_pane_splitter_vectors
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-pane-splitter-vectors.py
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pane-splitter-v1.json
                ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    if(CDIN_FRONTEND_BUILD_UI_HEADLESS)
        add_executable(cdin_frontend_headless_recorder_tests
            tests/ui/headless/integration/recorder.c)
        target_link_libraries(cdin_frontend_headless_recorder_tests
            PRIVATE cdin::frontend_ui_headless)
        target_compile_features(cdin_frontend_headless_recorder_tests PRIVATE c_std_11)
        cdin_frontend_enable_test_strict(cdin_frontend_headless_recorder_tests)
        add_test(NAME cdin_frontend_headless_recorder_tests
            COMMAND cdin_frontend_headless_recorder_tests)
    endif()

    add_executable(cdin_frontend_gui_sdl_model_tests
        tests/ui/gui-sdl/integration/model.c
        ui/gui-sdl/src/session/model.c
        ui/gui-sdl/src/session/layout.c)
    target_include_directories(cdin_frontend_gui_sdl_model_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/gui-sdl/src/session")
    target_link_libraries(cdin_frontend_gui_sdl_model_tests PRIVATE cdin::frontend_core)
    target_compile_features(cdin_frontend_gui_sdl_model_tests PRIVATE c_std_11)
    cdin_frontend_enable_test_strict(cdin_frontend_gui_sdl_model_tests)
    add_test(NAME cdin_frontend_gui_sdl_model_tests
        COMMAND cdin_frontend_gui_sdl_model_tests)

    add_executable(cdin_frontend_gui_sdl_geometry_tests
        tests/ui/gui-sdl/integration/geometry.c
        ui/gui-sdl/src/layout/geometry.c)
    target_include_directories(cdin_frontend_gui_sdl_geometry_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/gui-sdl/src/layout")
    target_compile_features(cdin_frontend_gui_sdl_geometry_tests PRIVATE c_std_11)
    cdin_frontend_enable_test_strict(cdin_frontend_gui_sdl_geometry_tests)
    add_test(NAME cdin_frontend_gui_sdl_geometry_tests
        COMMAND cdin_frontend_gui_sdl_geometry_tests)

    add_executable(cdin_frontend_gui_sdl_utf8_tests
        tests/ui/gui-sdl/integration/utf8.c
        ui/gui-sdl/src/text/utf8.c)
    target_include_directories(cdin_frontend_gui_sdl_utf8_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/gui-sdl/src/text")
    target_compile_features(cdin_frontend_gui_sdl_utf8_tests PRIVATE c_std_11)
    cdin_frontend_enable_test_strict(cdin_frontend_gui_sdl_utf8_tests)
    add_test(NAME cdin_frontend_gui_sdl_utf8_tests
        COMMAND cdin_frontend_gui_sdl_utf8_tests)

    if(CDIN_FRONTEND_BUILD_UI_SDL)
        add_executable(cdin_frontend_gui_sdl_adapter_smoke
            tests/ui/gui-sdl/integration/adapter_smoke.c)
        target_link_libraries(cdin_frontend_gui_sdl_adapter_smoke PRIVATE
            cdin::frontend_ui_gui_sdl SDL3::SDL3)
        if(CDIN_FRONTEND_GUI_SDL_TEST_CAPTURE_OVERRIDE)
            target_include_directories(cdin_frontend_gui_sdl_adapter_smoke PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/ui/gui-sdl/src")
            target_compile_definitions(cdin_frontend_gui_sdl_adapter_smoke PRIVATE
                CDIN_FRONTEND_GUI_SDL_TESTING=1)
        endif()
        target_compile_features(cdin_frontend_gui_sdl_adapter_smoke PRIVATE c_std_11)
        cdin_frontend_enable_test_strict(cdin_frontend_gui_sdl_adapter_smoke)
        add_test(NAME cdin_frontend_gui_sdl_adapter_smoke
            COMMAND cdin_frontend_gui_sdl_adapter_smoke)
        set_tests_properties(cdin_frontend_gui_sdl_adapter_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
            TIMEOUT 20)

        add_executable(cdin_frontend_gui_sdl_render_tests
            tests/ui/gui-sdl/integration/render.c)
        target_include_directories(cdin_frontend_gui_sdl_render_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/ui/gui-sdl/src/render")
        target_link_libraries(cdin_frontend_gui_sdl_render_tests PRIVATE
            cdin::frontend_ui_gui_sdl SDL3::SDL3)
        target_compile_features(cdin_frontend_gui_sdl_render_tests PRIVATE c_std_11)
        cdin_frontend_enable_test_strict(cdin_frontend_gui_sdl_render_tests)
        add_test(NAME cdin_frontend_gui_sdl_render_tests
            COMMAND cdin_frontend_gui_sdl_render_tests)
        set_tests_properties(cdin_frontend_gui_sdl_render_tests PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
            TIMEOUT 20)
    endif()

    if(CDIN_FRONTEND_BUILD_UI_TUI)
        function(cdin_frontend_tui_test target source)
            add_executable(${target} ${source})
            target_link_libraries(${target} PRIVATE cdin::frontend_ui_tui)
            target_compile_features(${target} PRIVATE c_std_11)
            cdin_frontend_enable_test_strict(${target})
            add_test(NAME ${target} COMMAND ${target})
        endfunction()
        cdin_frontend_tui_test(cdin_frontend_tui_render_tests tests/ui/tui/integration/tui_render.c)
        cdin_frontend_tui_test(cdin_frontend_tui_layout_tests tests/ui/tui/integration/layout.c)
        cdin_frontend_tui_test(cdin_frontend_tui_model_tests tests/ui/tui/integration/tui_model.c)
        cdin_frontend_tui_test(cdin_frontend_input_parser_tests tests/ui/tui/integration/input_parser.c)
        cdin_frontend_tui_test(cdin_frontend_tui_session_tests tests/ui/tui/integration/tui_session.c)
        cdin_frontend_tui_test(cdin_frontend_tui_scrolling_tests tests/ui/tui/integration/tui_scrolling.c)
        cdin_frontend_tui_test(cdin_frontend_tui_event_loop_tests tests/ui/tui/integration/event_loop.c)
        if(UNIX)
            cdin_frontend_tui_test(cdin_frontend_terminal_nonblocking_tests
                tests/ui/tui/integration/terminal_nonblocking_posix.c)
            set_tests_properties(cdin_frontend_terminal_nonblocking_tests PROPERTIES TIMEOUT 10)
        endif()
    endif()
endif()

if(CDIN_FRONTEND_BUILD_EXAMPLES)
    add_executable(cdin_frontend_logging_example examples/logging/main.c)
    target_link_libraries(cdin_frontend_logging_example PRIVATE cdin::frontend_core)
    target_compile_features(cdin_frontend_logging_example PRIVATE c_std_11)
    cdin_frontend_enable_strict(cdin_frontend_logging_example)
endif()

# Install each public product as a separate Registry package variant.
# Core is a shared dependency; adapters export only their own targets and
# resolve Core through its separately installed CMake package.
if(NOT CDIN_FRONTEND_USE_LOG_STUB)
    set(CDIN_FRONTEND_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake")
    set(CDIN_FRONTEND_PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # Core package: UI-neutral services and public contracts.
    install(TARGETS cdin_frontend_core
        EXPORT cdinFrontendCoreTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT core
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT core
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT core
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(EXPORT cdinFrontendCoreTargets
        FILE cdinFrontendCoreTargets.cmake
        NAMESPACE cdin::
        DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendCore"
        COMPONENT core)
    install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT core
        FILES_MATCHING PATTERN "*.h")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdinFrontendCoreConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendCoreConfig.cmake"
        INSTALL_DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendCore")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendCoreConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendCoreConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendCoreConfigVersion.cmake"
        DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendCore"
        COMPONENT core)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdin-frontend-core.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cdin-frontend-core.pc"
        @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cdin-frontend-core.pc"
        DESTINATION "${CDIN_FRONTEND_PKGCONFIG_INSTALL_DIR}"
        COMPONENT core)

    # TUI package: only the terminal adapter. Core is resolved separately.
    if(CDIN_FRONTEND_BUILD_UI_TUI)
        install(TARGETS cdin_frontend_ui_tui
            EXPORT cdinFrontendTUITargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT tui
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT tui
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT tui
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
        install(EXPORT cdinFrontendTUITargets
            FILE cdinFrontendTUITargets.cmake
            NAMESPACE cdin::
            DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendTUI"
            COMPONENT tui)
        install(DIRECTORY ui/tui/include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            COMPONENT tui
            FILES_MATCHING PATTERN "*.h")

        configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdinFrontendTUIConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendTUIConfig.cmake"
            INSTALL_DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendTUI")
        write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendTUIConfigVersion.cmake"
            VERSION "${PROJECT_VERSION}"
            COMPATIBILITY SameMajorVersion)
        install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendTUIConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendTUIConfigVersion.cmake"
            DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendTUI"
            COMPONENT tui)

        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdin-frontend-tui.pc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cdin-frontend-tui.pc"
            @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cdin-frontend-tui.pc"
            DESTINATION "${CDIN_FRONTEND_PKGCONFIG_INSTALL_DIR}"
            COMPONENT tui)
    endif()

    # GUI-SDL package: only the SDL adapter; SDL3 remains an environment package.
    if(CDIN_FRONTEND_BUILD_UI_SDL)
        install(TARGETS cdin_frontend_ui_gui_sdl
            EXPORT cdinFrontendGUISDLTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gui-sdl
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gui-sdl
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT gui-sdl
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
        install(EXPORT cdinFrontendGUISDLTargets
            FILE cdinFrontendGUISDLTargets.cmake
            NAMESPACE cdin::
            DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendGUISDL"
            COMPONENT gui-sdl)
        install(DIRECTORY ui/gui-sdl/include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            COMPONENT gui-sdl
            FILES_MATCHING PATTERN "*.h")

        configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdinFrontendGUISDLConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendGUISDLConfig.cmake"
            INSTALL_DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendGUISDL")
        write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendGUISDLConfigVersion.cmake"
            VERSION "${PROJECT_VERSION}"
            COMPATIBILITY SameMajorVersion)
        install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendGUISDLConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendGUISDLConfigVersion.cmake"
            DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontendGUISDL"
            COMPONENT gui-sdl)

        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdin-frontend-gui-sdl.pc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cdin-frontend-gui-sdl.pc"
            @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cdin-frontend-gui-sdl.pc"
            DESTINATION "${CDIN_FRONTEND_PKGCONFIG_INSTALL_DIR}"
            COMPONENT gui-sdl)
    endif()

    # Compatibility meta-package for existing consumers. New consumers should
    # request cdinFrontendCore, cdinFrontendTUI or cdinFrontendGUISDL explicitly.
    set(CDIN_FRONTEND_CONFIG_HAS_TUI ${CDIN_FRONTEND_BUILD_UI_TUI})
    set(CDIN_FRONTEND_CONFIG_HAS_GUI_SDL ${CDIN_FRONTEND_BUILD_UI_SDL})
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdinFrontendConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendConfig.cmake"
        INSTALL_DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontend")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cdinFrontendConfigVersion.cmake"
        DESTINATION "${CDIN_FRONTEND_CMAKE_INSTALL_DIR}/cdinFrontend")
endif()
