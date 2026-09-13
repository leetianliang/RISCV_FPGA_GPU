# PC Demo — Stage 004.5 (Golden Immediate/Tile interactive application)
# Window/input live here; golden_core stays GUI-independent.

if(NOT DEFINED GOLDEN_WARN_FLAGS)
    if(MSVC)
        set(GOLDEN_WARN_FLAGS /W4 /permissive-)
    else()
        set(GOLDEN_WARN_FLAGS -Wall -Wextra -Wpedantic)
    endif()
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_library(gpu2d_api INTERFACE)
target_include_directories(gpu2d_api INTERFACE
    ${CMAKE_SOURCE_DIR}/software/graphics/include
)
target_compile_features(gpu2d_api INTERFACE cxx_std_20)

add_library(neon_game STATIC
    ${CMAKE_SOURCE_DIR}/software/applications/neon_survivor/src/sim.cpp
    ${CMAKE_SOURCE_DIR}/software/applications/neon_survivor/src/assets.cpp
)
target_include_directories(neon_game PUBLIC
    ${CMAKE_SOURCE_DIR}/software/applications/neon_survivor/include
)
target_link_libraries(neon_game PUBLIC gpu2d_api)
target_compile_options(neon_game PRIVATE ${GOLDEN_WARN_FLAGS})

add_library(pc_host STATIC
    host/presenter.cpp
)
target_include_directories(pc_host PUBLIC host)
target_compile_features(pc_host PUBLIC cxx_std_20)
if(WIN32)
    target_link_libraries(pc_host PUBLIC user32 gdi32)
endif()

add_library(golden_backend_lib STATIC
    golden_backend/golden_renderer.cpp
)
target_include_directories(golden_backend_lib PUBLIC golden_backend)
target_link_libraries(golden_backend_lib PUBLIC golden_core gpu2d_api)
target_compile_options(golden_backend_lib PRIVATE ${GOLDEN_WARN_FLAGS})

add_executable(gpu2d_demo app/main.cpp)
target_link_libraries(gpu2d_demo PRIVATE golden_backend_lib neon_game pc_host gpu2d_api)
target_compile_options(gpu2d_demo PRIVATE ${GOLDEN_WARN_FLAGS})

# ---- Application-level tests (Stage 004.5) ----
enable_testing()

add_executable(gpu2d_test_api tests/test_api.cpp)
target_link_libraries(gpu2d_test_api PRIVATE gpu2d_api neon_game)
target_compile_options(gpu2d_test_api PRIVATE ${GOLDEN_WARN_FLAGS})
add_test(NAME gpu2d_test_api COMMAND gpu2d_test_api)

add_executable(gpu2d_test_backend tests/test_backend.cpp)
target_link_libraries(gpu2d_test_backend PRIVATE golden_backend_lib neon_game)
target_compile_options(gpu2d_test_backend PRIVATE ${GOLDEN_WARN_FLAGS})
add_test(NAME gpu2d_test_backend COMMAND gpu2d_test_backend)

add_executable(gpu2d_test_sim tests/test_sim.cpp)
target_link_libraries(gpu2d_test_sim PRIVATE neon_game)
target_compile_options(gpu2d_test_sim PRIVATE ${GOLDEN_WARN_FLAGS})
add_test(NAME gpu2d_test_sim COMMAND gpu2d_test_sim)

add_executable(gpu2d_test_presenter tests/test_presenter.cpp)
target_link_libraries(gpu2d_test_presenter PRIVATE pc_host)
target_compile_options(gpu2d_test_presenter PRIVATE ${GOLDEN_WARN_FLAGS})
add_test(NAME gpu2d_test_presenter COMMAND gpu2d_test_presenter)

add_executable(gpu2d_test_system tests/test_system.cpp)
target_link_libraries(gpu2d_test_system PRIVATE golden_backend_lib neon_game)
target_compile_options(gpu2d_test_system PRIVATE ${GOLDEN_WARN_FLAGS})
add_test(NAME gpu2d_test_system COMMAND gpu2d_test_system)

add_test(NAME gpu2d_test_boundary
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_app_boundary.py
)

# Headless demo smoke via CLI
add_test(NAME gpu2d_demo_headless_imm
    COMMAND gpu2d_demo --headless --frames 30 --seed 1234 --backend immediate --scene game
)
add_test(NAME gpu2d_demo_headless_tile
    COMMAND gpu2d_demo --headless --frames 30 --seed 1234 --backend tile --scene game
)
add_test(NAME gpu2d_demo_invalid_cli
    COMMAND gpu2d_demo --not-a-flag
)
set_tests_properties(gpu2d_demo_invalid_cli PROPERTIES WILL_FAIL TRUE)
