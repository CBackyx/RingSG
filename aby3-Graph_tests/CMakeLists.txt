file(GLOB_RECURSE SRCS *.cpp)
add_library(aby3-graph_Tests STATIC ${SRCS})

target_include_directories(aby3-graph_Tests PUBLIC ${CMAKE_SOURCE_DIR}) 
target_link_libraries(aby3-graph_Tests aby3-graph)

file(GLOB_RECURSE SRCS_REMOVE_OGA *.cpp)
add_library(aby3-graph_Tests-remove-oga STATIC ${SRCS_REMOVE_OGA})

target_include_directories(aby3-graph_Tests-remove-oga PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_definitions(aby3-graph_Tests-remove-oga PUBLIC REMOVE_OGA)
target_link_libraries(aby3-graph_Tests-remove-oga aby3-graph-remove-oga)

file(GLOB_RECURSE SRCS_REMOVE_OEP *.cpp)
add_library(aby3-graph_Tests-remove-oep STATIC ${SRCS_REMOVE_OEP})

target_include_directories(aby3-graph_Tests-remove-oep PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_definitions(aby3-graph_Tests-remove-oep PUBLIC REMOVE_OEP)
target_link_libraries(aby3-graph_Tests-remove-oep aby3-graph-remove-oep)
