cmake_minimum_required(VERSION 3.20)
project(ExpressionDifferentor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else ()
    add_compile_options(-Wall -Wpedantic -Wfatal-errors -Wextra)
endif ()

find_package(Boost 1.74 REQUIRED)

include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(diff INTERFACE
        ${CMAKE_SOURCE_DIR}/include/expressions.hpp
        ${CMAKE_SOURCE_DIR}/include/operations.hpp
        ${CMAKE_SOURCE_DIR}/include/traits.hpp
        ${CMAKE_SOURCE_DIR}/include/values.hpp
        ${CMAKE_SOURCE_DIR}/include/equation.hpp
        ${CMAKE_SOURCE_DIR}/include/dual.hpp
        ${CMAKE_SOURCE_DIR}/include/gradient.hpp
)
target_include_directories(diff INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(diff INTERFACE Boost::headers)

add_executable(main src/main.cpp)
target_link_libraries(main PUBLIC diff)

add_executable(tests tests.cpp)
target_link_libraries(tests gtest_main diff)

add_test(NAME tests COMMAND tests)
enable_testing()
