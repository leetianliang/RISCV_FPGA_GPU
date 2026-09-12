# Golden GPU — Stage 003

add_library(golden_core STATIC
    src/gpu_math.cpp
    src/memory_image.cpp
    src/surface.cpp
    src/surface_view.cpp
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
golden_add_test_exec(golden_test_mutation tests/directed/test_mutation_authority.cpp)
golden_add_test_exec(golden_test_sprite_ext tests/directed/test_sprite_ext.cpp)
golden_add_test_exec(golden_test_random_diff tests/random/test_random_diff.cpp)
golden_add_test_exec(golden_test_premult_exact tests/directed/test_premult_exact.cpp)
golden_add_test_exec(golden_test_premult_matrix tests/directed/test_premult_matrix.cpp)
golden_add_test_exec(golden_test_sampler_errors tests/directed/test_sampler_errors.cpp)
golden_add_test_exec(golden_test_ext_faults tests/directed/test_ext_faults.cpp)
golden_add_test_exec(golden_test_semantics_pairs tests/directed/test_semantics_pairs.cpp)
golden_add_test_exec(golden_test_oracle_exact tests/directed/test_oracle_exact.cpp)
golden_add_test_exec(golden_test_diff_suites tests/random/test_diff_suites.cpp)

add_test(
    NAME golden_test_integrity
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/check_test_integrity.py
)

add_executable(golden_cli tools/golden_cli.cpp)
target_link_libraries(golden_cli PRIVATE golden_core)
target_compile_options(golden_cli PRIVATE ${GOLDEN_WARN_FLAGS})

set(FRAMES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/tests/frames)
set(FILL_BASIC_DIR ${FRAMES_ROOT}/fill_basic)

add_custom_target(golden_regenerate_fixtures
    COMMAND golden_cli generate-fill-basic ${FILL_BASIC_DIR}
    COMMAND golden_cli generate-stage002-fixtures ${FRAMES_ROOT}
    COMMAND golden_cli generate-stage003-fixtures ${FRAMES_ROOT}
    DEPENDS golden_cli
    COMMENT "Opt-in regeneration of checked-in golden fixtures"
    VERBATIM
)

# Immutable fixture runs via explicit args (no JSON in C++ required)
function(golden_add_fixture name)
    set(dir ${FRAMES_ROOT}/${name})
    set(tex_args "")
    set(tex_base_arg "")
    if(EXISTS ${dir}/textures.bin)
        set(tex_args
            --textures ${dir}/textures.bin
            --texture-base 0x20000
            --texture-width 8 --texture-height 8
        )
        # stride/format from optional companion file texture_meta.txt: "stride format"
        if(EXISTS ${dir}/texture_meta.txt)
            file(READ ${dir}/texture_meta.txt _meta)
            string(STRIP "${_meta}" _meta)
            string(REPLACE " " ";" _meta_list "${_meta}")
            list(GET _meta_list 0 _tstride)
            list(GET _meta_list 1 _tfmt)
            list(APPEND tex_args --texture-stride ${_tstride} --texture-format ${_tfmt})
        else()
            list(APPEND tex_args --texture-stride 16 --texture-format rgb565)
        endif()
    endif()
    if(EXISTS ${dir}/palette.bin)
        list(APPEND tex_args --palette ${dir}/palette.bin --palette-base 0x40000)
    endif()
    if(EXISTS ${dir}/extensions.bin)
        list(APPEND tex_args --extensions ${dir}/extensions.bin --ext-base 0x30000)
    endif()
    if(EXISTS ${dir}/texture_index8.raw AND NOT EXISTS ${dir}/textures.bin)
        list(APPEND tex_args
            --textures ${dir}/texture_index8.raw
            --texture-base 0x20000
            --texture-width 8 --texture-height 8
            --texture-stride 8 --texture-format index8
        )
    endif()
    add_test(
        NAME golden_${name}_run
        COMMAND golden_cli run-stream
            --cmd ${dir}/command.bin
            --initial-fb ${dir}/initial_fb.raw
            ${tex_args}
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

golden_add_fixture(fill_basic)
golden_add_fixture(blit_rgb565_basic)
golden_add_fixture(blit_colorkey)
golden_add_fixture(blit_alpha_argb8888)
golden_add_fixture(blit_overdraw_rgb565)
golden_add_fixture(blit_additive)
golden_add_fixture(blit_ext_scale_nearest)
golden_add_fixture(blit_ext_scale_bilinear)
golden_add_fixture(blit_flip_xy)
golden_add_fixture(blit_clip)
golden_add_fixture(indexed8_palette)
golden_add_fixture(indexed8_bilinear)
golden_add_fixture(color_mod)
golden_add_fixture(premult_alpha)
golden_add_fixture(rgb565_dither)
# argb8888_target uses stride 64 — special-case runner
add_test(
    NAME golden_argb8888_target_run
    COMMAND golden_cli run-stream
        --cmd ${FRAMES_ROOT}/argb8888_target/command.bin
        --initial-fb ${FRAMES_ROOT}/argb8888_target/initial_fb.raw
        --width 16 --height 16 --stride 64 --base 0x10000
        --dst-format argb8888
        --out ${CMAKE_CURRENT_BINARY_DIR}/argb8888_target_out.raw
)
add_test(
    NAME golden_argb8888_target_compare
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py
        ${FRAMES_ROOT}/argb8888_target/golden_fb.raw
        ${CMAKE_CURRENT_BINARY_DIR}/argb8888_target_out.raw
)
set_tests_properties(golden_argb8888_target_compare PROPERTIES DEPENDS golden_argb8888_target_run)

# mismatch detector
add_test(
    NAME golden_fill_basic_frame_compare_mismatch
    COMMAND ${Python3_EXECUTABLE} -c
        "import pathlib,subprocess,sys; g=pathlib.Path(r'${FILL_BASIC_DIR}/golden_fb.raw'); bad=pathlib.Path(r'${CMAKE_CURRENT_BINARY_DIR}/fill_basic_mismatch.raw'); b=bytearray(g.read_bytes()); b[0]^=0xFF; bad.write_bytes(b); r=subprocess.run([sys.executable, r'${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py', str(g), str(bad)]); sys.exit(0 if r.returncode!=0 else 1)"
)

add_test(
    NAME golden_fixture_validate
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/fixture_validate/fixture_validate.py
        ${FRAMES_ROOT}
)
