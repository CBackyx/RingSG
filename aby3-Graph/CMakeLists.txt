file(GLOB_RECURSE SRCS *.cpp)
 

add_library(aby3-graph ${SRCS})


target_link_libraries(aby3-graph com-psi)

