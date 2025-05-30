file(GLOB_RECURSE SRCS *.cpp)
add_library(aby3-graph ${SRCS})
target_link_libraries(aby3-graph com-psi)

file(GLOB_RECURSE SRCS_REMOVE_OGA *.cpp)
add_library(aby3-graph-remove-oga ${SRCS_REMOVE_OGA})
target_compile_definitions(aby3-graph-remove-oga PUBLIC REMOVE_OGA)
target_link_libraries(aby3-graph-remove-oga com-psi)

file(GLOB_RECURSE SRCS_REMOVE_OEP *.cpp)
add_library(aby3-graph-remove-oep ${SRCS_REMOVE_OEP})
target_compile_definitions(aby3-graph-remove-oep PUBLIC REMOVE_OEP)
target_link_libraries(aby3-graph-remove-oep com-psi)

