cmake_minimum_required(VERSION 3.20)
project(ExpressionSolver VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
add_compile_options(-Wall -Wpedantic -Wfatal-errors)

option(getmdspan "Build kokkss MDSpan?" OFF)
option(getgenerator "Build Generator?" OFF)

include(FetchContent)

if (getmdspan)
    FetchContent_Declare(
            mdspan
            GIT_REPOSITORY https://github.com/kokkos/mdspan.git
            GIT_TAG stable
    )
    FetchContent_MakeAvailable(mdspan)
endif ()

if (getgenerator)
    FetchContent_Declare(
            generator
            GIT_REPOSITORY https://github.com/lewissbaker/generator.git
            GIT_TAG main
    )
    FetchContent_MakeAvailable(generator)
endif ()

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(expression_solver INTERFACE
        include/values.hpp
        include/operations.hpp
        include/procvar.hpp
        include/equation.hpp
        include/traits.hpp
)

target_include_directories(expression_solver INTERFACE
        ${CMAKE_SOURCE_DIR}/include
)

add_executable(ExpressionSolver src/main.cpp)
target_include_directories(ExpressionSolver PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(ExpressionSolver PUBLIC expression_solver)

add_executable(tests tests.cpp)
target_link_libraries(tests gtest_main expression_solver)
target_include_directories(tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME tests COMMAND tests)
enable_testing()
