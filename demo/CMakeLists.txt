file(GLOB_RECURSE SRC_EVAL ${CMAKE_SOURCE_DIR}/demo/*.cpp)
include_directories(${CMAKE_SOURCE_DIR}/demo/)

add_executable(demo ${SRC_EVAL})

target_link_libraries(demo com-psi)
target_link_libraries(demo aby3-ML)
target_link_libraries(demo com-psi_Tests)
target_link_libraries(demo aby3_Tests)
target_link_libraries(demo aby3-graph_Tests)
target_link_libraries(demo oc::tests_cryptoTools)