# Golden GPU — Stage 002

add_library(golden_core STATIC
    src/gpu_math.cpp
    src/memory_image.cpp
    src/surface.cpp
    src/command_decoder.cpp
    src/golden_gpu.cpp
)

target_include_directories(golden_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(golden_core PUBLIC cxx_std_20)

if(MSVC)
    set(GOLDEN_WARN_FLAGS /W4 /permissive-)
else()
    set(GOLDEN_WARN_FLAGS -Wall -Wextra -Wpedantic)
endif()

target_compile_options(golden_core PRIVATE ${GOLDEN_WARN_FLAGS})

enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

function(golden_add_test_exec name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE golden_core)
    target_compile_options(${name} PRIVATE ${GOLDEN_WARN_FLAGS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

golden_add_test_exec(golden_test_gpu_math tests/unit/test_gpu_math.cpp)
golden_add_test_exec(golden_test_memory_image tests/unit/test_memory_image.cpp)
golden_add_test_exec(golden_test_surface tests/unit/test_surface.cpp)
golden_add_test_exec(golden_test_cmd_io tests/unit/test_cmd_io.cpp)
golden_add_test_exec(golden_test_command_header tests/directed/test_command_header.cpp)
golden_add_test_exec(golden_test_fill_directed tests/directed/test_fill_directed.cpp)
golden_add_test_exec(golden_test_blit_directed tests/directed/test_blit_directed.cpp)
golden_add_test_exec(golden_test_pixel_pipeline tests/unit/test_pixel_pipeline.cpp)
golden_add_test_exec(golden_test_random_stage002 tests/random/test_random_stage002.cpp)

add_executable(golden_cli tools/golden_cli.cpp)
target_link_libraries(golden_cli PRIVATE golden_core)
target_compile_options(golden_cli PRIVATE ${GOLDEN_WARN_FLAGS})

set(FILL_BASIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/frames/fill_basic)
add_custom_target(golden_regenerate_fixtures
    COMMAND golden_cli generate-fill-basic ${FILL_BASIC_DIR}
    COMMAND golden_cli generate-stage002-fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/frames
    DEPENDS golden_cli
    COMMENT "Opt-in regeneration of checked-in golden fixtures"
    VERBATIM
)

add_test(
    NAME golden_fill_basic_fixture_run
    COMMAND golden_cli run-stream
        --cmd ${FILL_BASIC_DIR}/command.bin
        --initial-fb ${FILL_BASIC_DIR}/initial_fb.raw
        --width 16 --height 16 --stride 32 --base 0x10000
        --out ${CMAKE_CURRENT_BINARY_DIR}/fill_basic_out.raw
)
add_test(
    NAME golden_fill_basic_frame_compare
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py
        ${FILL_BASIC_DIR}/golden_fb.raw
        ${CMAKE_CURRENT_BINARY_DIR}/fill_basic_out.raw
)
set_tests_properties(golden_fill_basic_frame_compare PROPERTIES DEPENDS golden_fill_basic_fixture_run)

add_test(
    NAME golden_fill_basic_frame_compare_mismatch
    COMMAND ${Python3_EXECUTABLE} -c
        "import pathlib,subprocess,sys; g=pathlib.Path(r'${FILL_BASIC_DIR}/golden_fb.raw'); bad=pathlib.Path(r'${CMAKE_CURRENT_BINARY_DIR}/fill_basic_mismatch.raw'); b=bytearray(g.read_bytes()); b[0]^=0xFF; bad.write_bytes(b); r=subprocess.run([sys.executable, r'${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py', str(g), str(bad)]); sys.exit(0 if r.returncode!=0 else 1)"
)

function(golden_add_blit_fixture name texfmt)
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/frames/${name})
    add_test(
        NAME golden_${name}_run
        COMMAND golden_cli run-stream
            --cmd ${dir}/command.bin
            --initial-fb ${dir}/initial_fb.raw
            --textures ${dir}/textures.bin
            --texture-base 0x20000
            --texture-width 8 --texture-height 8 --texture-stride ${texfmt}
            --width 16 --height 16 --stride 32 --base 0x10000
            --out ${CMAKE_CURRENT_BINARY_DIR}/${name}_out.raw
    )
    add_test(
        NAME golden_${name}_compare
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py
            ${dir}/golden_fb.raw
            ${CMAKE_CURRENT_BINARY_DIR}/${name}_out.raw
    )
    set_tests_properties(golden_${name}_compare PROPERTIES DEPENDS golden_${name}_run)
endfunction()

# Note: texture stride/format packed as separate args in real calls below
function(golden_add_blit_fixture2 name stride format)
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/frames/${name})
    add_test(
        NAME golden_${name}_run
        COMMAND golden_cli run-stream
            --cmd ${dir}/command.bin
            --initial-fb ${dir}/initial_fb.raw
            --textures ${dir}/textures.bin
            --texture-base 0x20000
            --texture-width 8 --texture-height 8 --texture-stride ${stride}
            --texture-format ${format}
            --width 16 --height 16 --stride 32 --base 0x10000
            --out ${CMAKE_CURRENT_BINARY_DIR}/${name}_out.raw
    )
    add_test(
        NAME golden_${name}_compare
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py
            ${dir}/golden_fb.raw
            ${CMAKE_CURRENT_BINARY_DIR}/${name}_out.raw
    )
    set_tests_properties(golden_${name}_compare PROPERTIES DEPENDS golden_${name}_run)
endfunction()

golden_add_blit_fixture2(blit_rgb565_basic 16 rgb565)
golden_add_blit_fixture2(blit_colorkey 16 rgb565)
golden_add_blit_fixture2(blit_alpha_argb8888 32 argb8888)
golden_add_blit_fixture2(blit_overdraw_rgb565 16 rgb565)
golden_add_blit_fixture2(blit_additive 16 rgb565)
