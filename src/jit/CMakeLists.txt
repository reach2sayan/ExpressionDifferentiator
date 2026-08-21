# The only LLVM-aware code in the project.  include/jit/kernel.hpp names no
# LLVM type, so nothing here escapes into a consumer's translation unit.
add_library(ddx_jit STATIC
        codegen.cpp
        compiler.cpp
)
add_library(ddx::jit ALIAS ddx_jit)

target_include_directories(ddx_jit
        PUBLIC ${CMAKE_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ddx_jit SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(ddx_jit PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(ddx_jit PUBLIC ddx_rt PRIVATE ${DDX_LLVM_LIBS})
# PUBLIC: DDX_HAS_JIT unlocks the std::formatter for jit::Ir in expr/format.hpp,
# and the two defaults are read by Options in a consumer's own translation unit.
target_compile_definitions(ddx_jit PUBLIC
        DDX_HAS_JIT
        DDX_JIT_DEFAULT_OPT=${DDX_JIT_DEFAULT_OPT}
        DDX_JIT_DEFAULT_CONTRACT=${DDX_JIT_DEFAULT_CONTRACT})

# Deliberately no -fno-rtti, even against an LLVM built without it.  The flag is
# only needed by code that derives from an LLVM polymorphic class or applies
# typeid/dynamic_cast to one; isa<>/cast<> use LLVM's own RTTI, not the
# language's, and nothing here inherits from LLVM.
ddx_target_flags(ddx_jit)
