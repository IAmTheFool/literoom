cmake_minimum_required(VERSION 3.21)
project(literoom VERSION 0.1.0 LANGUAGES CXX)

# Set before anything is fetched so Catch2 is built with the same settings.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC: link the C++ runtime statically (no Visual C++ Redistributable needed).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT WIN32)
    enable_language(C)   # only for liteui's vendored Wayland protocol .c files
else()
    enable_language(RC)  # compiles packaging/windows/literoom.rc (the exe's icon)
endif()

# ------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------
option(LITEROOM_ONNX     "literoom: object selection via ONNX Runtime" OFF)
option(LITEROOM_TESTS    "literoom: build unit tests (Catch2)" ON)
option(LITEROOM_SANITIZE "literoom: build the tests with AddressSanitizer" OFF)
set(ONNXRUNTIME_DIR "" CACHE PATH
    "Unpacked onnxruntime release (the folder containing include/ and lib/)")

# MSVC's ASan is incompatible with the default /RTC1 runtime checks.
if(LITEROOM_SANITIZE AND MSVC)
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

# ------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------
add_subdirectory(third_party/liteui)     # provides liteui::liteui
find_package(Threads REQUIRED)           # workers/*.hpp use std::jthread

# ------------------------------------------------------------------------
# Application
# ------------------------------------------------------------------------
# All sources live under src/. That folder is the include root, so any file can
# write  #include "mask/mask.hpp"  no matter which folder it sits in.
set(LITEROOM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Headers are listed only so they show up in the IDE; the compiler finds them
# through #include either way. CONFIGURE_DEPENDS picks up new headers.
file(GLOB_RECURSE LITEROOM_HEADERS CONFIGURE_DEPENDS ${LITEROOM_SRC}/*.hpp)

add_executable(literoom ${LITEROOM_SRC}/main.cpp ${LITEROOM_HEADERS})
target_include_directories(literoom PRIVATE ${LITEROOM_SRC})
target_link_libraries(literoom PRIVATE liteui::liteui Threads::Threads)

# Windows: embed the icon (Explorer, taskbar, Alt-Tab) via the resource file.
# OBJECT_DEPENDS makes a changed .ico trigger a rebuild of the resource.
if(WIN32)
    set(LITEROOM_RC ${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/literoom.rc)
    target_sources(literoom PRIVATE ${LITEROOM_RC})
    set_source_files_properties(${LITEROOM_RC} PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/literoom.ico)
endif()

# Same folder tree in Visual Studio's Solution Explorer.
source_group(TREE ${LITEROOM_SRC} FILES ${LITEROOM_SRC}/main.cpp ${LITEROOM_HEADERS})

if(MSVC)
    target_compile_options(literoom PRIVATE /utf-8 /bigobj)
endif()

# ------------------------------------------------------------------------
# Object selection (MobileSAM via ONNX Runtime), optional
# ------------------------------------------------------------------------
if(LITEROOM_ONNX)
    # When a directory is given, search ONLY there so a system copy is never
    # picked up by accident. Results are cached: delete ORT_INCLUDE_DIR /
    # ORT_LIBRARY from the cache if you switch ONNXRUNTIME_DIR.
    set(_ort_search "")
    if(ONNXRUNTIME_DIR)
        set(_ort_search NO_DEFAULT_PATH)
    endif()

    find_path(ORT_INCLUDE_DIR onnxruntime_cxx_api.h
        HINTS ${ONNXRUNTIME_DIR}
        PATH_SUFFIXES include include/onnxruntime
        ${_ort_search})
    find_library(ORT_LIBRARY onnxruntime
        HINTS ${ONNXRUNTIME_DIR}
        PATH_SUFFIXES lib
        ${_ort_search})
    if(NOT ORT_INCLUDE_DIR OR NOT ORT_LIBRARY)
        message(FATAL_ERROR
            "LITEROOM_ONNX is ON but onnxruntime was not found. "
            "Set -DONNXRUNTIME_DIR=<unpacked onnxruntime folder>.")
    endif()

    get_filename_component(_ort_lib_dir "${ORT_LIBRARY}" DIRECTORY)

    # One imported target carries include path, library and the define. Only the
    # app links it: the tests compile without LITEROOM_ONNX.
    add_library(literoom_onnxruntime UNKNOWN IMPORTED)
    set_target_properties(literoom_onnxruntime PROPERTIES
        IMPORTED_LOCATION             "${ORT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS LITEROOM_ONNX)
    target_link_libraries(literoom PRIVATE literoom_onnxruntime)

    if(WIN32)
        # onnxruntime.dll (and the providers DLL, if shipped) must sit next to the exe.
        foreach(_dll onnxruntime.dll onnxruntime_providers_shared.dll)
            if(EXISTS "${_ort_lib_dir}/${_dll}")
                add_custom_command(TARGET literoom POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_ort_lib_dir}/${_dll}"
                            $<TARGET_FILE_DIR:literoom>)
            endif()
        endforeach()
    else()
        # Find libonnxruntime.so when running from the build folder.
        set_target_properties(literoom PROPERTIES BUILD_RPATH "${_ort_lib_dir}")
    endif()
    message(STATUS "literoom: ONNX Runtime enabled (${ORT_LIBRARY})")

    # Models: copy models/ next to the executable (ONNX builds only).
    # The app looks in <exe folder>/models (or $LITEROOM_MODELS).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/models)
        add_custom_command(TARGET literoom POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/models
                    $<TARGET_FILE_DIR:literoom>/models
            COMMENT "literoom: copying models/")
    endif()
else()
    message(STATUS "literoom: built WITHOUT ONNX (basic object selection only)")
endif()

# ------------------------------------------------------------------------
# Install
# ------------------------------------------------------------------------
include(GNUInstallDirs)

if(LITEROOM_ONNX AND UNIX AND NOT APPLE)
    # ONNX build on Linux: the app needs libonnxruntime and the models next to
    # it, so it gets a private folder instead of sitting alone in bin/:
    #   bin/literoom                          launcher script (what users run)
    #   lib/literoom/literoom                 the real binary
    #   lib/literoom/libonnxruntime.so*       bundled ONNX Runtime
    #   share/literoom/models/*.onnx          MobileSAM models
    # The launcher finds everything relative to itself, so the same tree works
    # from /usr (the .deb) and from an extracted tarball.
    set(LITEROOM_PRIVATE_DIR lib/literoom)
    install(TARGETS literoom RUNTIME DESTINATION ${LITEROOM_PRIVATE_DIR})
    # Look for libonnxruntime next to the binary. (BUILD_RPATH above points at
    # the build machine's copy; CMake swaps it for this one at install time.)
    set_target_properties(literoom PROPERTIES INSTALL_RPATH "$ORIGIN")

    # GLOB matches the whole symlink chain (libonnxruntime.so -> .so.1 -> .so.1.x.y)
    # and install(FILES) keeps the links as links.
    file(GLOB _ort_libs "${_ort_lib_dir}/libonnxruntime*.so*")
    install(FILES ${_ort_libs} DESTINATION ${LITEROOM_PRIVATE_DIR})
    install(DIRECTORY models/ DESTINATION share/literoom/models FILES_MATCHING PATTERN "*.onnx")

    # The app looks for models in <exe folder>/models or $LITEROOM_MODELS; the
    # launcher sets the variable unless the user already did.
    set(_launcher "${CMAKE_CURRENT_BINARY_DIR}/literoom-launcher")
    file(WRITE "${_launcher}"
"#!/bin/sh\n"
"here=\"$(dirname \"$(readlink -f \"$0\")\")\"\n"
"if [ -z \"$LITEROOM_MODELS\" ]; then\n"
"    LITEROOM_MODELS=\"$here/../share/literoom/models\"\n"
"    export LITEROOM_MODELS\n"
"fi\n"
"exec \"$here/../lib/literoom/literoom\" \"$@\"\n")
    install(PROGRAMS "${_launcher}" RENAME literoom DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(TARGETS literoom RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Linux desktop integration: the menu entry and its icon. The icon name in
# literoom.desktop ("Icon=literoom") is looked up in the hicolor theme, so the
# file must be called literoom.png and sit in a <size>x<size>/apps folder.
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/literoom.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/icons/literoom.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()

# ------------------------------------------------------------------------
# Packaging (.deb via CPack). The Windows installer is built by Inno Setup
# (packaging/windows/literoom.iss), not CPack.
#   cpack --config build/release/CPackConfig.cmake -G DEB
# The release workflow overrides CPACK_PACKAGE_VERSION with the git tag.
# ------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR                    "DEB")
    set(CPACK_PACKAGE_NAME                 "literoom")
    set(CPACK_PACKAGE_VENDOR               "Roshan")
    set(CPACK_PACKAGE_CONTACT              "Roshan <chaudharyroshan2020@gmail.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY  "Lightweight photo editor")
    # Debian packages belong in /usr, not CMake's default /usr/local.
    set(CPACK_PACKAGING_INSTALL_PREFIX     "/usr")
    set(CPACK_DEBIAN_PACKAGE_SECTION       "graphics")
    # Let dpkg-shlibdeps work out "Depends:" (libwayland, cairo, pango, ...)
    # from the libraries the built binary actually links against.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS     ON)
    # Name the file <name>_<version>_<arch>.deb
    set(CPACK_DEBIAN_FILE_NAME             DEB-DEFAULT)
    include(CPack)   # must come after the install() rules and CPACK_* variables
endif()

# ------------------------------------------------------------------------
# Tests (Catch2 v3)
# ------------------------------------------------------------------------
if(LITEROOM_TESTS)
    include(CTest)
    include(FetchContent)

    # Prefer an installed Catch2 v3, otherwise download a pinned release.
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "literoom tests: Catch2 not installed, fetching v3.8.1")
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v3.8.1)
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
    include(Catch)

    # Optional AddressSanitizer for a test target. Sets <env_var> (in the caller)
    # to the ENVIRONMENT list the CTest tests need.
    function(literoom_enable_sanitizers target env_var)
        cmake_parse_arguments(ARG "NO_LEAK_CHECK" "" "" ${ARGN})
        set(${env_var} "" PARENT_SCOPE)
        if(NOT LITEROOM_SANITIZE)
            return()
        endif()
        if(MSVC)
            target_compile_options(${target} PRIVATE /fsanitize=address)
            target_link_options(${target} PRIVATE /INCREMENTAL:NO)
        else()
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
            if(ARG_NO_LEAK_CHECK)
                set(${env_var} "ASAN_OPTIONS=detect_leaks=0" PARENT_SCOPE)
            endif()
        endif()
    endfunction()

    # ---- literoom_test: one TU that #includes every tests/cases/*.inc ----
    # To add a test: create tests/cases/<name>.inc and #include it from
    # literoom_test.cpp. Nothing to change here.
    file(GLOB LITEROOM_CASES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/*.inc)

    add_executable(literoom_test tests/literoom_test.cpp ${LITEROOM_CASES})
    # Cases include headers by src-relative path ("core/pixel_math.hpp") and each
    # other's files from tests/ ("cases/pixel_math.inc").
    target_include_directories(literoom_test PRIVATE ${LITEROOM_SRC}
                                                     ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_features(literoom_test PRIVATE cxx_std_23)
    # literoom_test.cpp defines LITEUI_IMPLEMENTATION once for all cases.
    target_link_libraries(literoom_test PRIVATE Catch2::Catch2WithMain liteui::liteui Threads::Threads)
    if(MSVC)
        target_compile_options(literoom_test PRIVATE /utf-8 /bigobj)
    endif()

    set(_catch_env "")
    literoom_enable_sanitizers(literoom_test _catch_env NO_LEAK_CHECK)

    # CTest names: literoom_<Catch2 test case name>. Catch2 tags become labels:
    #   ctest -L threaded --repeat until-fail:50
    set(_catch_props TIMEOUT 120)
    if(_catch_env)
        list(APPEND _catch_props ENVIRONMENT "${_catch_env}")
    endif()
    catch_discover_tests(literoom_test TEST_PREFIX "literoom_"
        ADD_TAGS_AS_LABELS
        PROPERTIES ${_catch_props})

    # ---- Golden render test: the refactoring safety net (not Catch2) ----
    # Own executable: compiles src/main.cpp itself (main() renamed) and has its
    # own main() with --record / --dump. One golden file per compiler + config.
    #   cmake --build --preset <p> --target literoom_golden_record   (once, BEFORE refactoring)
    #   ctest --preset <p> -R literoom_golden_render                 (after every step)
    # Skipped (exit code 77) until a golden file for this compiler/config exists.
    # CI can leave it out with:  ctest -LE golden
    # Never sanitized: the fingerprints are float-exact.
    set(GOLDEN_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/render_${CMAKE_CXX_COMPILER_ID}_$<CONFIG>.txt)

    add_executable(literoom_golden_render_test tests/golden_render_test.cpp)
    target_include_directories(literoom_golden_render_test PRIVATE ${LITEROOM_SRC})
    target_compile_features(literoom_golden_render_test PRIVATE cxx_std_23)
    target_link_libraries(literoom_golden_render_test PRIVATE liteui::liteui Threads::Threads)
    if(MSVC)
        target_compile_options(literoom_golden_render_test PRIVATE /utf-8 /bigobj)
    endif()

    add_test(NAME literoom_golden_render COMMAND literoom_golden_render_test ${GOLDEN_FILE})
    set_tests_properties(literoom_golden_render PROPERTIES
        LABELS golden TIMEOUT 300 SKIP_RETURN_CODE 77)

    add_custom_target(literoom_golden_record
        COMMAND literoom_golden_render_test --record ${GOLDEN_FILE}
        DEPENDS literoom_golden_render_test
        COMMENT "golden: recording render_${CMAKE_CXX_COMPILER_ID}_<config>.txt")

    # ---- lut_pane: the LUTs pane's logic without a window (not Catch2) ----
    # Own executable (compiles src/main.cpp itself, own main() on check.hpp).
    add_executable(literoom_lut_pane_test tests/lut_pane_test.cpp)
    target_include_directories(literoom_lut_pane_test PRIVATE ${LITEROOM_SRC}
                                                              ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_features(literoom_lut_pane_test PRIVATE cxx_std_23)
    target_link_libraries(literoom_lut_pane_test PRIVATE liteui::liteui Threads::Threads)
    if(MSVC)
        target_compile_options(literoom_lut_pane_test PRIVATE /utf-8 /bigobj)
    endif()

    set(_lut_pane_env "")
    literoom_enable_sanitizers(literoom_lut_pane_test _lut_pane_env NO_LEAK_CHECK)

    add_test(NAME literoom_lut_pane COMMAND literoom_lut_pane_test)
    set_tests_properties(literoom_lut_pane PROPERTIES LABELS threaded TIMEOUT 120)
    if(_lut_pane_env)
        set_tests_properties(literoom_lut_pane PROPERTIES ENVIRONMENT "${_lut_pane_env}")
    endif()
endif()