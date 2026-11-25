add_library(liteui INTERFACE)
add_library(liteui::liteui ALIAS liteui)

target_include_directories(liteui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(liteui INTERFACE cxx_std_23)

option(LITEUI_CANVAS_GL "Use the OpenGL ES 3.0 Canvas backend (Linux)" ON)
if(LITEUI_CANVAS_GL AND NOT WIN32)
    target_compile_definitions(liteui INTERFACE LITEUI_CANVAS_GL)
endif()

if(WIN32)
    target_link_libraries(liteui INTERFACE
        user32 gdi32 d3d11 dxgi d2d1 dwrite windowscodecs ole32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND        REQUIRED IMPORTED_TARGET wayland-client)
    pkg_check_modules(WAYLAND_CURSOR REQUIRED IMPORTED_TARGET wayland-cursor)
    pkg_check_modules(WAYLAND_EGL    REQUIRED IMPORTED_TARGET wayland-egl)
    pkg_check_modules(CAIRO          REQUIRED IMPORTED_TARGET cairo)
    pkg_check_modules(PANGOCAIRO     REQUIRED IMPORTED_TARGET pangocairo)
    pkg_check_modules(EGL            REQUIRED IMPORTED_TARGET egl)
    pkg_check_modules(GLES           REQUIRED IMPORTED_TARGET glesv2)
    pkg_check_modules(XKBCOMMON      REQUIRED IMPORTED_TARGET xkbcommon)
    pkg_check_modules(PNG            REQUIRED IMPORTED_TARGET libpng)
    pkg_check_modules(JPEG           REQUIRED IMPORTED_TARGET libjpeg)

    # Vendored Wayland protocol glue. Every wayland-protocols/<name>-protocol.c
    # is picked up automatically (currently: xdg-shell, xdg-decoration, tablet,
    # fractional-scale, viewporter). To add a protocol, drop its generated
    # <name>-protocol.c and <name>-client-protocol.h into that folder.
    set(_proto "${CMAKE_CURRENT_SOURCE_DIR}/wayland-protocols")
    file(GLOB _proto_sources CONFIGURE_DEPENDS ${_proto}/*-protocol.c)
    if(NOT _proto_sources)
        message(FATAL_ERROR "liteui: no *-protocol.c files found in ${_proto}")
    endif()

    add_library(liteui_wayland_protocols STATIC ${_proto_sources})
    target_include_directories(liteui_wayland_protocols PUBLIC ${_proto})
    target_compile_options(liteui_wayland_protocols PRIVATE -w)

    target_link_libraries(liteui INTERFACE
        liteui_wayland_protocols
        PkgConfig::WAYLAND PkgConfig::WAYLAND_CURSOR PkgConfig::WAYLAND_EGL
        PkgConfig::CAIRO PkgConfig::PANGOCAIRO
        PkgConfig::EGL PkgConfig::GLES
        PkgConfig::XKBCOMMON PkgConfig::PNG PkgConfig::JPEG)
endif()