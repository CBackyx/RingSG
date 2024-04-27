file(GLOB_RECURSE SRCS *.cpp)
add_library(aby3-graph_Tests STATIC ${SRCS})

target_include_directories(aby3-graph_Tests PUBLIC ${CMAKE_SOURCE_DIR}) 
target_link_libraries(aby3-graph_Tests aby3-graph)
