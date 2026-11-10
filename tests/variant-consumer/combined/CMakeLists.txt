cmake_minimum_required(VERSION 3.22)
project(cdin_frontend_combined_consumer LANGUAGES C)
include(CTest)
find_package(cdinFrontendCore 1.1 CONFIG REQUIRED)
find_package(cdinFrontendTUI 1.1 CONFIG REQUIRED)
find_package(cdinFrontendGUISDL 1.1 CONFIG REQUIRED)
foreach(required_target IN ITEMS cdin::frontend_core cdin::frontend_ui_tui cdin::frontend_ui_gui_sdl)
    if(NOT TARGET ${required_target})
        message(FATAL_ERROR "Missing installed Frontend variant target: ${required_target}")
    endif()
endforeach()
add_executable(frontend_combined_consumer main.c)
target_compile_features(frontend_combined_consumer PRIVATE c_std_11)
target_link_libraries(frontend_combined_consumer PRIVATE
    cdin::frontend_ui_gui_sdl
    cdin::frontend_ui_tui
    cdin::frontend_core)
add_test(NAME frontend_combined_installed_consumer COMMAND frontend_combined_consumer)
