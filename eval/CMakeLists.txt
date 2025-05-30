file(GLOB_RECURSE SRC_EVAL ${CMAKE_SOURCE_DIR}/eval/*.cpp)
include_directories(${CMAKE_SOURCE_DIR}/eval/)

add_executable(eval ${SRC_EVAL})

target_link_libraries(eval com-psi)
target_link_libraries(eval aby3-ML)
target_link_libraries(eval com-psi_Tests)
target_link_libraries(eval aby3_Tests)
target_link_libraries(eval aby3-graph_Tests)
target_link_libraries(eval oc::tests_cryptoTools)

# ----

file(GLOB_RECURSE SRC_EVAL_REMOVE_OGA ${CMAKE_SOURCE_DIR}/eval/*.cpp)
add_executable(eval-remove-oga ${SRC_EVAL_REMOVE_OGA})

target_compile_definitions(eval-remove-oga PUBLIC REMOVE_OGA)
target_include_directories(eval-remove-oga PUBLIC ${CMAKE_SOURCE_DIR}/eval/)

target_link_libraries(eval-remove-oga com-psi)
target_link_libraries(eval-remove-oga aby3-ML)
target_link_libraries(eval-remove-oga com-psi_Tests)
target_link_libraries(eval-remove-oga aby3_Tests) 
target_link_libraries(eval-remove-oga aby3-graph_Tests-remove-oga)
target_link_libraries(eval-remove-oga oc::tests_cryptoTools)

# ----

file(GLOB_RECURSE SRC_EVAL_REMOVE_OEP ${CMAKE_SOURCE_DIR}/eval/*.cpp)
add_executable(eval-remove-oep ${SRC_EVAL_REMOVE_OEP})

target_compile_definitions(eval-remove-oep PUBLIC REMOVE_OEP)
target_include_directories(eval-remove-oep PUBLIC ${CMAKE_SOURCE_DIR}/eval/)

target_link_libraries(eval-remove-oep com-psi)
target_link_libraries(eval-remove-oep aby3-ML)
target_link_libraries(eval-remove-oep com-psi_Tests)
target_link_libraries(eval-remove-oep aby3_Tests) 
target_link_libraries(eval-remove-oep aby3-graph_Tests-remove-oep)
target_link_libraries(eval-remove-oep oc::tests_cryptoTools)