# rt/ — the runtime expression graph.  Unlike the rest of include/, these
# headers are listed on ddx_rt: opcode/apply/builder/expr/interpret/bridge need
# only ddx itself, but graph.hpp and coupling.hpp need Boost.Graph.
#
# Every header here has to be listed, not just the ones with a dependency: an
# IDE resolves a header's include paths from the target that names it, and one
# left off gets no context at all -- not even the standard library.
target_sources(ddx_rt INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/opcode.hpp
        ${CMAKE_CURRENT_LIST_DIR}/apply.hpp
        ${CMAKE_CURRENT_LIST_DIR}/builder.hpp
        ${CMAKE_CURRENT_LIST_DIR}/expr.hpp
        ${CMAKE_CURRENT_LIST_DIR}/interpret.hpp
        ${CMAKE_CURRENT_LIST_DIR}/derivative.hpp
        ${CMAKE_CURRENT_LIST_DIR}/coupling.hpp
        ${CMAKE_CURRENT_LIST_DIR}/bridge.hpp
        ${CMAKE_CURRENT_LIST_DIR}/graph.hpp
        ${CMAKE_CURRENT_LIST_DIR}/dot.hpp
        ${CMAKE_CURRENT_LIST_DIR}/equation.hpp
)
