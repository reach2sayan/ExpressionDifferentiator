cmake_minimum_required(VERSION 3.20)

# Newest GCC on PATH, unless the caller chose a compiler.  Must precede project().
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
        AND NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    find_program(DDX_PREFERRED_CXX NAMES g++-15 g++-14)
    if (DDX_PREFERRED_CXX)
        set(CMAKE_CXX_COMPILER "${DDX_PREFERRED_CXX}")
    endif ()
endif ()

project(ddx VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c++23")

# --- accessor spelling ------------------------------------------------------
# include/util/config.hpp owns the decision; this only overrides it.
#   auto - the compiler's own answer (__cpp_explicit_this_parameter)
#   on   - deducing this even where the macro is absent
#   off  - the ref-qualified overload set
# Two probes: what `auto` picks up, and what `on` would require.
set(DDX_DEDUCING_THIS "auto" CACHE STRING "Accessor spelling: auto|on|off")
set_property(CACHE DDX_DEDUCING_THIS PROPERTY STRINGS auto on off)

check_cxx_source_compiles(
        "struct S { int f(this S) { return 0; } };
         #ifndef __cpp_explicit_this_parameter
         #error no macro
         #endif
         int main() { return S{}.f(); }"
        DDX_HAS_DEDUCING_THIS)
check_cxx_source_compiles(
        "struct S { int f(this S) { return 0; } };
         int main() { return S{}.f(); }"
        DDX_DEDUCING_THIS_COMPILES)

if (DDX_DEDUCING_THIS STREQUAL "on" AND NOT DDX_DEDUCING_THIS_COMPILES)
    message(FATAL_ERROR
            "DDX_DEDUCING_THIS=on but ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
            "cannot compile an explicit object parameter.  Use auto or off.")
elseif (DDX_DEDUCING_THIS STREQUAL "on")
    set(DDX_DEDUCING_THIS_VALUE 1)
    set(DDX_ACCESSORS "deducing this, forced on")
elseif (DDX_DEDUCING_THIS STREQUAL "off")
    set(DDX_DEDUCING_THIS_VALUE 0)
    set(DDX_ACCESSORS "ref-qualified overload set, forced off")
elseif (NOT DDX_DEDUCING_THIS STREQUAL "auto")
    message(FATAL_ERROR "DDX_DEDUCING_THIS must be auto, on or off (got '${DDX_DEDUCING_THIS}').")
elseif (DDX_HAS_DEDUCING_THIS)
    # auto leaves the value undefined, so a consumer building without this file agrees.
    set(DDX_ACCESSORS "deducing this")
else ()
    set(DDX_ACCESSORS "ref-qualified overload set")
endif ()
message(STATUS "accessors: ${DDX_ACCESSORS} (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")

# --- which mdspan include/md/md.hpp binds to --------------------------------
#   auto     - standard <mdspan> when complete enough, vendored otherwise
#   std      - require the standard one; hard error if incomplete
#   vendored - always the copy under include/md/third_party
set(DDX_MDSPAN_MODE "auto" CACHE STRING "Which mdspan md.hpp binds to: auto|std|vendored")
set_property(CACHE DDX_MDSPAN_MODE PROPERTY STRINGS auto std vendored)

check_cxx_source_compiles(
        "#include <version>
         #if !defined(__cpp_lib_mdspan) || __cpp_lib_mdspan < 202207L
         #error no mdspan
         #endif
         #if !defined(__cpp_lib_submdspan) || __cpp_lib_submdspan < 202306L
         #error no submdspan
         #endif
         #include <mdspan>
         int main() { double a[2]{}; std::mdspan m(a, 2); return (int)m.extent(0) - 2; }"
        DDX_HAS_STD_MDSPAN)
unset(CMAKE_REQUIRED_FLAGS)

if (DDX_MDSPAN_MODE STREQUAL "std" AND NOT DDX_HAS_STD_MDSPAN)
    message(FATAL_ERROR
            "DDX_MDSPAN_MODE=std but ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
            "has no complete <mdspan>/<submdspan>.  Use auto or vendored.")
elseif (DDX_MDSPAN_MODE STREQUAL "vendored" OR NOT DDX_HAS_STD_MDSPAN)
    set(DDX_MDSPAN_VENDORED ON)
    message(STATUS "mdspan: vendored (include/md/third_party/mdspan.hpp)")
else ()
    message(STATUS "mdspan: standard <mdspan>")
endif ()

# --- the library ------------------------------------------------------------
# Header-only and dependency-free.  CMAKE_CURRENT_SOURCE_DIR, so this works
# standalone and via add_subdirectory().
add_library(ddx INTERFACE)
add_library(ddx::ddx ALIAS ddx)
target_include_directories(ddx INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Each header directory lists its own files onto the target, for IDE visibility;
# nothing is compiled from there.
add_subdirectory(include)

if (DDX_MDSPAN_VENDORED)
    target_compile_definitions(ddx INTERFACE DDX_MDSPAN_FORCE_VENDORED)
endif ()
if (DEFINED DDX_DEDUCING_THIS_VALUE)
    target_compile_definitions(ddx INTERFACE DDX_DEDUCING_THIS=${DDX_DEDUCING_THIS_VALUE})
endif ()

option(ENABLE_NATIVE_ARCH "Build optimized for this machine" ON)
# Pin FP contraction, drop errno.  Neither flag is lossy.  Nothing that trades
# accuracy for speed belongs here -- in particular never -ffast-math (nor
# /fp:fast on MSVC, which needs no flags at all).
option(DDX_FP_FLAGS "Pin FP contraction and drop errno on libm calls" ON)

if (MSVC)
    # /bigobj: one TU instantiates past the 2^16 COFF section limit.
    set(DDX_CODEGEN_FLAGS /arch:AVX2 /bigobj)
    # /Wall noise a header-only expression-template library emits by the
    # thousand.  C4365 and C5219 are deliberately left on.
    set(DDX_WARNINGS /Wall /wd4623 /wd4625 /wd4626 /wd5026 /wd5027 /wd4710
                     /wd4711 /wd4868 /wd4820 /wd5045 /wd5246 /wd4514 /wd4324 /wd5266)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else ()
    if (ENABLE_NATIVE_ARCH)
        set(DDX_CODEGEN_FLAGS -march=native)
    else ()
        set(DDX_CODEGEN_FLAGS -march=x86-64-v3)
    endif ()
    if (DDX_FP_FLAGS)
        list(APPEND DDX_CODEGEN_FLAGS -ffp-contract=fast -fno-math-errno)
    endif ()
    set(DDX_WARNINGS -Wall -Wextra -Wpedantic -Wfatal-errors)
endif ()

# Our own targets only -- never the INTERFACE library, which must not push flags
# onto a consumer.
function(ddx_target_flags target)
    target_compile_options(${target} PRIVATE ${DDX_CODEGEN_FLAGS} ${DDX_WARNINGS})
endfunction()

# --- optional runtime graph -------------------------------------------------
# Everything above is header-only and dependency-free, and stays that way.  The
# runtime graph is what an expression built at run time lives in, and it is
# opt-in because it pulls third-party code.
option(DDX_BUILD_RT "Build the runtime expression graph (Boost.Graph)" OFF)
option(DDX_BUILD_JIT "Build the LLVM JIT backend (implies DDX_BUILD_RT)" OFF)
if (DDX_BUILD_JIT AND NOT DDX_BUILD_RT)
    set(DDX_BUILD_RT ON)
endif ()

if (DDX_BUILD_RT)
    # Before include(FetchContent), which defines this itself and would make the
    # guard below a no-op.
    if (NOT DEFINED CACHE{FETCHCONTENT_BASE_DIR})
        set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH
                "Shared download cache for fetched dependencies")
    endif ()
    include(FetchContent)

    # Boost.Graph for the frozen graph's CSR and the Hessian colouring, and
    # Boost.DynamicBitset for the coupling rows.  Header-only, so the compiled
    # Boost libraries are not needed.
    #
    # Fetched rather than taken from the system: a distro Boost is whatever it
    # is, and the colouring's quality is something this project asserts on.
    # BOOST_INCLUDE_LIBRARIES names the two used and the modular build resolves
    # their dependencies -- this is not a fetch of all of Boost.
    #
    # Pinned, not floating: "latest" that moves under you is not reproducible.
    # Bump the one line.
    set(DDX_BOOST_VERSION "1.92.0" CACHE STRING "Boost release to fetch")

    # The -cmake archive is the whole Boost source (~700 MB extracted);
    # BOOST_INCLUDE_LIBRARIES limits what is *configured*, not what is
    # downloaded.  The shared cache above means one copy per checkout rather
    # than one per build tree -- five presets would otherwise be five copies.
    set(BOOST_INCLUDE_LIBRARIES graph dynamic_bitset CACHE STRING "" FORCE)
    set(BOOST_ENABLE_CMAKE ON CACHE BOOL "" FORCE)
    # SYSTEM: a fetched dependency's warnings are not ours to fix, and Boost's
    # smallest_last_ordering.hpp emits several under -Wall -Wextra.
    FetchContent_Declare(Boost
            URL https://github.com/boostorg/boost/releases/download/boost-${DDX_BOOST_VERSION}/boost-${DDX_BOOST_VERSION}-cmake.tar.xz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SYSTEM
    )
    FetchContent_MakeAvailable(Boost)

    add_library(ddx_rt INTERFACE)
    add_library(ddx::rt ALIAS ddx_rt)
    target_link_libraries(ddx_rt INTERFACE ddx Boost::graph Boost::dynamic_bitset)
    add_subdirectory(include/rt)
endif ()

if (DDX_BUILD_JIT)
    # The JIT's defaults follow how the project itself was built.  Contraction
    # especially: a kernel that contracts where the compile-time path does not
    # would return different derivative values for the same expression.
    if (DDX_FP_FLAGS)
        set(DDX_JIT_DEFAULT_CONTRACT 1)
    else ()
        set(DDX_JIT_DEFAULT_CONTRACT 0)
    endif ()

    # No predefined macro carries the optimisation *level* -- GCC and Clang
    # define only __OPTIMIZE__, and only as a yes/no -- so CMake is the one
    # place that knows.  O0 is deliberately unreachable: it turns the loop
    # vectoriser off, and vectorising is the only reason a batch kernel beats
    # the compile-time path.  A debug build gets cheaper compiles, not scalar code.
    string(TOUPPER "${CMAKE_BUILD_TYPE}" DDX_BUILD_TYPE_UPPER)
    if (DDX_BUILD_TYPE_UPPER STREQUAL "DEBUG")
        set(DDX_JIT_DEFAULT_OPT 1)
    elseif (DDX_BUILD_TYPE_UPPER STREQUAL "RELEASE")
        set(DDX_JIT_DEFAULT_OPT 3)
    else ()
        set(DDX_JIT_DEFAULT_OPT 2)
    endif ()

    # LLVMConfig declares itself compatible only with an exact version request,
    # so a find_package version range never matches; check the major ourselves.
    # The ORC C++ API is not stable across releases, hence the range at all.
    find_package(LLVM REQUIRED CONFIG)
    if (LLVM_VERSION_MAJOR LESS 18 OR LLVM_VERSION_MAJOR GREATER 20)
        message(FATAL_ERROR
                "ddx::jit has been built against LLVM 18-20; found ${LLVM_PACKAGE_VERSION}. "
                "Point CMake at a supported one with -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm.")
    endif ()
    message(STATUS "LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_INCLUDE_DIRS})")
    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
    llvm_map_components_to_libnames(DDX_LLVM_LIBS
            core orcjit native passes support analysis target)
    add_subdirectory(src/jit)
endif ()

# Everything below builds the executables, and only when this is the top-level
# project.  A consumer that add_subdirectory()s us gets the library and nothing else.
if (NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    return ()
endif ()

option(DDX_BUILD_BENCHMARKS "Build Google Benchmark benchmarks" ON)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SYSTEM
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE ddx)
ddx_target_flags(main)

# One TU per anonymous-namespace helper block, so `-j` can build them at once.
file(GLOB DDX_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(tests ${DDX_TEST_SOURCES})
target_include_directories(tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(tests PRIVATE gtest_main ddx)
ddx_target_flags(tests)

enable_testing()
gtest_discover_tests(tests)

# The runtime-graph tests live in their own target: they need NWGraph, which
# the dependency-free suite above must not link.
if (DDX_BUILD_RT)
    file(GLOB DDX_RT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/rt/*.cpp")
    add_executable(tests_rt ${DDX_RT_TEST_SOURCES})
    target_link_libraries(tests_rt PRIVATE gtest_main ddx_rt)
    ddx_target_flags(tests_rt)
    gtest_discover_tests(tests_rt)
endif ()

if (DDX_BUILD_JIT)
    file(GLOB DDX_JIT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/*.cpp")
    add_executable(tests_jit ${DDX_JIT_TEST_SOURCES})
    target_link_libraries(tests_jit PRIVATE gtest_main ddx_jit)
    ddx_target_flags(tests_jit)
    gtest_discover_tests(tests_jit)
endif ()

if (DDX_BUILD_BENCHMARKS)
    FetchContent_Declare(
            googlebenchmark
            URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SYSTEM
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(benchmarks benchmarks/benchmark.cpp)
    target_link_libraries(benchmarks PRIVATE benchmark::benchmark ddx)
    ddx_target_flags(benchmarks)

    if (DDX_BUILD_JIT)
        add_executable(benchmarks_jit benchmarks/jit/benchmark_jit.cpp)
        target_link_libraries(benchmarks_jit PRIVATE benchmark::benchmark ddx_jit)
        ddx_target_flags(benchmarks_jit)
    endif ()

    add_custom_target(benchmark_json
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/benchmark-results"
            COMMAND $<TARGET_FILE:benchmarks>
            --benchmark_min_time=0.05s
            --benchmark_out="${CMAKE_BINARY_DIR}/benchmark-results/benchmarks.json"
            --benchmark_out_format=json
            DEPENDS benchmarks
            USES_TERMINAL
            COMMENT "Running benchmarks and exporting JSON results")
endif ()

# Fetched dependencies are not ours to warn about.
if (MSVC)
    foreach (dep gtest gtest_main gmock gmock_main benchmark benchmark_main)
        if (TARGET ${dep})
            target_compile_options(${dep} PRIVATE /W0 /wd4668)
            set_target_properties(${dep} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
        endif ()
    endforeach ()
endif ()
