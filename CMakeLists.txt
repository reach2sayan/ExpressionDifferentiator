cmake_minimum_required(VERSION 3.20)
project(ExpressionSolver VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
include(FetchContent)
FetchContent_Declare(
        mdspan
        GIT_REPOSITORY https://github.com/kokkos/mdspan.git
        GIT_TAG stable
)
FetchContent_Declare(
        generator
        GIT_REPOSITORY https://github.com/lewissbaker/generator.git
        GIT_TAG main
)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(mdspan)
FetchContent_MakeAvailable(generator)

add_executable(ExpressionSolver src/main.cpp
        include/values.hpp
        include/operations.hpp
        include/procvar.hpp)

target_include_directories(ExpressionSolver PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${mdspan_SOURCE_DIR}/include
        ${generator_SOURCE_DIR}/include)

add_executable(tests tests.cpp)

target_link_libraries(tests gtest_main)
target_include_directories(tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${mdspan_SOURCE_DIR}/include
        ${generator_SOURCE_DIR}/include)

add_test(NAME alltest COMMAND tests)
enable_testing()
