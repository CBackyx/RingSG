file(GLOB_RECURSE SRC_EVAL ${CMAKE_SOURCE_DIR}/eval/*.cpp)
include_directories(${CMAKE_SOURCE_DIR}/eval/)

add_executable(eval ${SRC_EVAL})

target_link_libraries(eval com-psi)
target_link_libraries(eval aby3-ML)
target_link_libraries(eval com-psi_Tests)
target_link_libraries(eval aby3_Tests)
target_link_libraries(eval aby3-graph_Tests)
target_link_libraries(eval oc::tests_cryptoTools)