# BCAST Examples

# Option to build bcast examples
option(BUILD_BCAST_EXAMPLES "Build bcast examples" ON)

if(NOT BUILD_BCAST_EXAMPLES)
    return()
endif()

# Basic example (C++17)
add_executable(bcast_example example.cpp)
target_link_libraries(bcast_example PRIVATE bcast pthread)
target_compile_features(bcast_example PRIVATE cxx_std_17)

# Advanced example (C++17)
add_executable(bcast_advanced_example advanced_example.cpp)
target_link_libraries(bcast_advanced_example PRIVATE bcast pthread)
target_compile_features(bcast_advanced_example PRIVATE cxx_std_17)

# Benchmark (C++17)
add_executable(bcast_benchmark benchmark.cpp)
target_link_libraries(bcast_benchmark PRIVATE bcast pthread)
target_compile_features(bcast_benchmark PRIVATE cxx_std_17)

# Coroutine examples (C++20) - only build if C++20 is available
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
    add_executable(bcast_coroutine_example coroutine_example.cpp)
    target_link_libraries(bcast_coroutine_example PRIVATE bcast pthread)
    target_compile_features(bcast_coroutine_example PRIVATE cxx_std_20)
    
    add_executable(bcast_real_world_example real_world_example.cpp)
    target_link_libraries(bcast_real_world_example PRIVATE bcast pthread)
    target_compile_features(bcast_real_world_example PRIVATE cxx_std_20)
    
    add_executable(bcast_timeout_example timeout_example.cpp)
    target_link_libraries(bcast_timeout_example PRIVATE bcast pthread)
    target_compile_features(bcast_timeout_example PRIVATE cxx_std_20)
    
    add_executable(bcast_batch_example batch_example.cpp)
    target_link_libraries(bcast_batch_example PRIVATE bcast pthread)
    target_compile_features(bcast_batch_example PRIVATE cxx_std_20)
    
    add_executable(bcast_semaphore_test semaphore_test.cpp)
    target_link_libraries(bcast_semaphore_test PRIVATE bcast pthread)
    target_compile_features(bcast_semaphore_test PRIVATE cxx_std_20)
    
    add_executable(bcast_event_test event_test.cpp)
    target_link_libraries(bcast_event_test PRIVATE bcast pthread)
    target_compile_features(bcast_event_test PRIVATE cxx_std_20)
    
    message(STATUS "Building bcast examples with C++20 coroutine support")
else()
    message(STATUS "Building bcast examples (C++20 coroutine examples disabled, need GCC 10+ or Clang 10+)")
endif()

message(STATUS "BCAST examples configured")

