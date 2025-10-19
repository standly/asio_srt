# acore Examples

# Option to build acore examples
option(BUILD_acore_EXAMPLES "Build acore examples" ON)

if(NOT BUILD_acore_EXAMPLES)
    return()
endif()

# Basic example (C++17)
add_executable(acore_example example.cpp)
target_link_libraries(acore_example PRIVATE acore pthread)
target_compile_features(acore_example PRIVATE cxx_std_17)

# Advanced example (C++17)
add_executable(acore_advanced_example advanced_example.cpp)
target_link_libraries(acore_advanced_example PRIVATE acore pthread)
target_compile_features(acore_advanced_example PRIVATE cxx_std_17)

# Benchmark (C++17)
add_executable(acore_benchmark benchmark.cpp)
target_link_libraries(acore_benchmark PRIVATE acore pthread)
target_compile_features(acore_benchmark PRIVATE cxx_std_17)

# Coroutine examples (C++20) - only build if C++20 is available
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
    add_executable(acore_coroutine_example coroutine_example.cpp)
    target_link_libraries(acore_coroutine_example PRIVATE acore pthread)
    target_compile_features(acore_coroutine_example PRIVATE cxx_std_20)
    
    add_executable(acore_real_world_example real_world_example.cpp)
    target_link_libraries(acore_real_world_example PRIVATE acore pthread)
    target_compile_features(acore_real_world_example PRIVATE cxx_std_20)
    
    add_executable(acore_timeout_example timeout_example.cpp)
    target_link_libraries(acore_timeout_example PRIVATE acore pthread)
    target_compile_features(acore_timeout_example PRIVATE cxx_std_20)
    
    add_executable(acore_batch_example batch_example.cpp)
    target_link_libraries(acore_batch_example PRIVATE acore pthread)
    target_compile_features(acore_batch_example PRIVATE cxx_std_20)
    
    add_executable(acore_semaphore_test semaphore_test.cpp)
    target_link_libraries(acore_semaphore_test PRIVATE acore pthread)
    target_compile_features(acore_semaphore_test PRIVATE cxx_std_20)
    
    add_executable(acore_event_test event_test.cpp)
    target_link_libraries(acore_event_test PRIVATE acore pthread)
    target_compile_features(acore_event_test PRIVATE cxx_std_20)
    
    add_executable(acore_queue_benchmark queue_benchmark.cpp)
    target_link_libraries(acore_queue_benchmark PRIVATE acore pthread)
    target_compile_features(acore_queue_benchmark PRIVATE cxx_std_20)
    
    add_executable(acore_waitgroup_simple example_waitgroup_simple.cpp)
    target_link_libraries(acore_waitgroup_simple PRIVATE acore pthread)
    target_compile_features(acore_waitgroup_simple PRIVATE cxx_std_20)
    
    # New async components examples
    add_executable(acore_mutex_example mutex_example.cpp)
    target_link_libraries(acore_mutex_example PRIVATE acore pthread)
    target_compile_features(acore_mutex_example PRIVATE cxx_std_20)
    
    add_executable(acore_timer_example timer_example.cpp)
    target_link_libraries(acore_timer_example PRIVATE acore pthread)
    target_compile_features(acore_timer_example PRIVATE cxx_std_20)
    
    add_executable(acore_rate_limiter_example rate_limiter_example.cpp)
    target_link_libraries(acore_rate_limiter_example PRIVATE acore pthread)
    target_compile_features(acore_rate_limiter_example PRIVATE cxx_std_20)
    
    add_executable(acore_barrier_latch_example barrier_latch_example.cpp)
    target_link_libraries(acore_barrier_latch_example PRIVATE acore pthread)
    target_compile_features(acore_barrier_latch_example PRIVATE cxx_std_20)
    
    message(STATUS "Building acore examples with C++20 coroutine support")
else()
    message(STATUS "Building acore examples (C++20 coroutine examples disabled, need GCC 10+ or Clang 10+)")
endif()

message(STATUS "acore examples configured")

