cmake_minimum_required(VERSION 3.22)
project(cdin_frontend_installed_consumer LANGUAGES C)

include(CTest)
find_package(cdinFrontend 1.0 CONFIG REQUIRED)

if(NOT TARGET cdin::frontend_core)
    message(FATAL_ERROR "Installed cdinFrontend package did not export cdin::frontend_core")
endif()
if(NOT TARGET cdin::frontend_ui_tui)
    message(FATAL_ERROR "Installed cdinFrontend package did not export cdin::frontend_ui_tui")
endif()

add_executable(cdin_frontend_installed_consumer main.c)
target_compile_features(cdin_frontend_installed_consumer PRIVATE c_std_11)
target_link_libraries(cdin_frontend_installed_consumer PRIVATE cdin::frontend_ui_tui)
add_test(NAME installed_frontend_package_consumer COMMAND cdin_frontend_installed_consumer)
