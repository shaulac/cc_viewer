cmake_minimum_required(VERSION 3.20)

project(cc_viewer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set_source_files_properties(assets/eagle_viewer.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/eagle_viewer.ico"
)

add_executable(cc_viewer WIN32
    assets/eagle_viewer.rc
    src/bayer.c
    src/ccm_window.c
    src/image.c
    src/main.c
    src/pixel_render.c
    src/raw.c
    src/raw_toolbar.c
    src/save_name.c
    src/view_math.c
    src/viewer.c
    src/yuv.c
)

target_include_directories(cc_viewer PRIVATE include assets)

target_compile_definitions(cc_viewer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(cc_viewer PRIVATE
    comdlg32
    gdi32
    shell32
    user32
)

if(MINGW)
    target_link_options(cc_viewer PRIVATE -static -static-libgcc)
endif()

if(MSVC)
    target_compile_options(cc_viewer PRIVATE /W4)
else()
    target_compile_options(cc_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/pixel_render_tests.c")
    add_executable(pixel_render_tests tests/pixel_render_tests.c src/pixel_render.c)
    target_include_directories(pixel_render_tests PRIVATE include)
    if(MINGW)
        target_link_options(pixel_render_tests PRIVATE -static -static-libgcc)
    endif()
    add_test(NAME pixel_render_tests COMMAND pixel_render_tests)
    add_executable(save_name_tests tests/save_name_tests.c src/save_name.c)
    target_include_directories(save_name_tests PRIVATE include)
    target_compile_definitions(save_name_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MINGW)
        target_link_options(save_name_tests PRIVATE -static -static-libgcc)
    endif()
    add_test(NAME save_name_tests COMMAND save_name_tests)
    add_executable(image_tests tests/image_tests.c src/image.c src/raw.c src/bayer.c src/yuv.c)
    target_include_directories(image_tests PRIVATE include)
    target_compile_definitions(image_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MINGW)
        target_link_options(image_tests PRIVATE -static -static-libgcc)
    endif()
    add_test(NAME image_tests COMMAND image_tests)
    add_executable(bayer_tests tests/bayer_tests.c src/raw.c src/bayer.c src/image.c src/yuv.c)
    target_include_directories(bayer_tests PRIVATE include)
    target_compile_definitions(bayer_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MINGW)
        target_link_options(bayer_tests PRIVATE -static -static-libgcc)
    endif()
    add_test(NAME bayer_tests COMMAND bayer_tests)
    add_executable(yuv_tests tests/yuv_tests.c src/raw.c src/bayer.c src/image.c src/yuv.c)
    target_include_directories(yuv_tests PRIVATE include)
    target_compile_definitions(yuv_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MINGW)
        target_link_options(yuv_tests PRIVATE -static -static-libgcc)
    endif()
    add_test(NAME yuv_tests COMMAND yuv_tests)
    add_executable(raw_toolbar_tests tests/raw_toolbar_tests.c src/ccm_window.c src/raw_toolbar.c src/raw.c src/bayer.c src/yuv.c)
    target_include_directories(raw_toolbar_tests PRIVATE include)
    target_compile_definitions(raw_toolbar_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(raw_toolbar_tests PRIVATE user32 gdi32)
    if(MINGW)
        target_link_options(raw_toolbar_tests PRIVATE -static -static-libgcc)
    endif()
    add_test(NAME raw_toolbar_tests COMMAND raw_toolbar_tests)
    set_tests_properties(raw_toolbar_tests PROPERTIES TIMEOUT 20)
    add_executable(view_math_tests tests/view_math_tests.c src/view_math.c)
    target_include_directories(view_math_tests PRIVATE include)
    add_test(NAME view_math_tests COMMAND view_math_tests)
endif()
