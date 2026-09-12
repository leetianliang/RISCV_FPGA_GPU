# Golden GPU — Stage 001
# Builds golden_core, unit/directed tests, and golden_cli (no third-party deps).

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

function(golden_add_test_exec name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE golden_core)
    target_compile_options(${name} PRIVATE ${GOLDEN_WARN_FLAGS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

golden_add_test_exec(golden_test_gpu_math tests/unit/test_gpu_math.cpp)
golden_add_test_exec(golden_test_memory_image tests/unit/test_memory_image.cpp)
golden_add_test_exec(golden_test_surface tests/unit/test_surface.cpp)
golden_add_test_exec(golden_test_command_header tests/directed/test_command_header.cpp)
golden_add_test_exec(golden_test_fill_directed tests/directed/test_fill_directed.cpp)

add_executable(golden_cli tools/golden_cli.cpp)
target_link_libraries(golden_cli PRIVATE golden_core)
target_compile_options(golden_cli PRIVATE ${GOLDEN_WARN_FLAGS})

# Generate fill_basic fixture at build time into the source tree for check-in.
set(FILL_BASIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/frames/fill_basic)
add_custom_command(
    OUTPUT
        ${FILL_BASIC_DIR}/command.bin
        ${FILL_BASIC_DIR}/initial_fb.raw
        ${FILL_BASIC_DIR}/golden_fb.raw
        ${FILL_BASIC_DIR}/manifest.json
    COMMAND golden_cli generate-fill-basic ${FILL_BASIC_DIR}
    DEPENDS golden_cli
    COMMENT "Generating fill_basic golden fixture"
    VERBATIM
)
add_custom_target(golden_fill_basic_fixture ALL
    DEPENDS
        ${FILL_BASIC_DIR}/command.bin
        ${FILL_BASIC_DIR}/initial_fb.raw
        ${FILL_BASIC_DIR}/golden_fb.raw
        ${FILL_BASIC_DIR}/manifest.json
)

# Run fixture through architectural path and compare.
add_test(
    NAME golden_fill_basic_fixture_run
    COMMAND golden_cli run-fill
        --cmd ${FILL_BASIC_DIR}/command.bin
        --initial-fb ${FILL_BASIC_DIR}/initial_fb.raw
        --width 16
        --height 16
        --stride 32
        --base 0x10000
        --out ${CMAKE_CURRENT_BINARY_DIR}/fill_basic_out.raw
)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME golden_fill_basic_frame_compare
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py
        ${FILL_BASIC_DIR}/golden_fb.raw
        ${CMAKE_CURRENT_BINARY_DIR}/fill_basic_out.raw
)
set_tests_properties(golden_fill_basic_frame_compare PROPERTIES
    DEPENDS golden_fill_basic_fixture_run
)

# Intentional mismatch must fail frame_compare (test asserts non-zero exit).
add_test(
    NAME golden_fill_basic_frame_compare_mismatch
    COMMAND ${Python3_EXECUTABLE} -c
        "import pathlib,subprocess,sys; g=pathlib.Path(r'${FILL_BASIC_DIR}/golden_fb.raw'); bad=pathlib.Path(r'${CMAKE_CURRENT_BINARY_DIR}/fill_basic_mismatch.raw'); b=bytearray(g.read_bytes()); b[0]^=0xFF; bad.write_bytes(b); r=subprocess.run([sys.executable, r'${CMAKE_CURRENT_SOURCE_DIR}/../../tools/frame_compare/frame_compare.py', str(g), str(bad)]); sys.exit(0 if r.returncode!=0 else 1)"
)
