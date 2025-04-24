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
        ${CMAKE_SOURCE_DIR}/include/values.hpp
        ${CMAKE_SOURCE_DIR}/include/operations.hpp
        ${CMAKE_SOURCE_DIR}/include/equation.hpp
        ${CMAKE_SOURCE_DIR}/include/traits.hpp
        ${CMAKE_SOURCE_DIR}/include/procvar.hpp
        ${CMAKE_SOURCE_DIR}/include/derivative.hpp
        ${CMAKE_SOURCE_DIR}/include/soequations.hpp
)

add_executable(main src/main.cpp)

target_include_directories(main PRIVATE
        ${mdspan_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(main PUBLIC expression_template)

add_executable(tests tests.cpp)
target_link_libraries(tests gtest_main expression_template)
target_include_directories(tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${mdspan_SOURCE_DIR}/include
)

add_test(NAME tests COMMAND tests)
enable_testing()
