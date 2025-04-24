cmake_minimum_required(VERSION 3.20)
project(ExpressionSolver VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else ()
    add_compile_options(-Wall -Wpedantic -Wfatal-errors)
endif ()

include(FetchContent)

option(getmdspan "Build kokkss MDSpan?" ON)
set(getmdspan ON CACHE BOOL "Build kokkss MDSpan?" FORCE)

if (getmdspan)
    FetchContent_Declare(
            mdspan
            GIT_REPOSITORY https://github.com/kokkos/mdspan.git
            GIT_TAG stable
    )
    FetchContent_MakeAvailable(mdspan)
endif ()

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(expression_template INTERFACE
        ${CMAKE_SOURCE_DIR}/include/expression_template/values.hpp
        ${CMAKE_SOURCE_DIR}/include/expression_template/operations.hpp
        ${CMAKE_SOURCE_DIR}/include/expression_template/equation.hpp
        ${CMAKE_SOURCE_DIR}/include/expression_template/traits.hpp
        ${CMAKE_SOURCE_DIR}/include/expression_template/procvar.hpp
        ${CMAKE_SOURCE_DIR}/include/expression_template/derivative.hpp
        ${CMAKE_SOURCE_DIR}/include/expression_template/soequations.hpp
)

add_library(expression_solver SHARED src/solver.cpp)
target_include_directories(expression_solver PRIVATE
        ${mdspan_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/expression_template
)

target_link_libraries(expression_solver PRIVATE
        expression_template
)

add_executable(main src/main.cpp)

target_include_directories(main PRIVATE
        ${mdspan_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/solver
        ${CMAKE_SOURCE_DIR}/include/expression_template
)
target_link_libraries(main PUBLIC expression_template
        expression_solver
)

add_executable(tests tests.cpp)
target_link_libraries(tests gtest_main expression_solver)
target_include_directories(tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include/expression_template
        ${CMAKE_SOURCE_DIR}/include/solver
)

add_test(NAME tests COMMAND tests)
enable_testing()
