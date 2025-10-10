
# Header-only library for publish-subscribe pattern
add_library(bcast INTERFACE
        async_queue.hpp
        dispatcher.hpp
)


target_include_directories(bcast INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# If you have ASIO installed, link it here
# target_link_libraries(bcast INTERFACE asio::asio)

# Optional: Build examples
option(BUILD_BCAST_EXAMPLES "Build bcast examples" OFF)

if(BUILD_BCAST_EXAMPLES)
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
        
        message(STATUS "Building bcast examples with C++20 coroutine support")
    else()
        message(STATUS "Building bcast examples (C++20 examples disabled)")
    endif()
endif()