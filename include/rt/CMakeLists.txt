# rt/ — the runtime expression graph.  Unlike the rest of include/, these
# headers are listed on ddx_rt: opcode/apply/builder/expr/interpret/bridge need
# only ddx itself, but graph.hpp needs NWGraph.
target_sources(ddx_rt INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/opcode.hpp
        ${CMAKE_CURRENT_LIST_DIR}/apply.hpp
        ${CMAKE_CURRENT_LIST_DIR}/builder.hpp
        ${CMAKE_CURRENT_LIST_DIR}/expr.hpp
        ${CMAKE_CURRENT_LIST_DIR}/interpret.hpp
        ${CMAKE_CURRENT_LIST_DIR}/derivative.hpp
        ${CMAKE_CURRENT_LIST_DIR}/bridge.hpp
        ${CMAKE_CURRENT_LIST_DIR}/graph.hpp
)
