cmake_minimum_required(VERSION 3.20)
project(fpga_2d_gpu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_GOLDEN "Build the PC Golden GPU model" ON)
option(BUILD_PC_DEMO "Build Stage 004.5 PC interactive application" ON)

enable_testing()

if(BUILD_GOLDEN)
    add_subdirectory(model/golden)
endif()

if(BUILD_GOLDEN AND BUILD_PC_DEMO)
    add_subdirectory(model/pc_demo)
endif()
