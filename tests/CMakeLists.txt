# Tests CMakeLists.txt

# Enable testing
include(CTest)
enable_testing()

# Find required packages
find_package(OpenSSL REQUIRED)

# Find or fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include Google Test's CMake helpers
include(GoogleTest)

# Test executable for reactor
add_executable(test_srt_reactor
    test_srt_reactor.cpp
)

# Test executable for logging enhancement
add_executable(test_logging_enhancement
    test_logging_enhancement.cpp
)

# Test executable for socket and acceptor
add_executable(test_srt_socket_acceptor
    test_srt_socket_acceptor.cpp
)

# Link against libraries  
# Note: Order matters for static libraries - dependencies must come after the libraries that use them
target_link_libraries(test_srt_reactor
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    Threads::Threads
)

target_link_libraries(test_logging_enhancement
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    Threads::Threads
)

target_link_libraries(test_srt_socket_acceptor
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    Threads::Threads
)

# Include directories
target_include_directories(test_srt_reactor
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rdparty/asio-1.36.0/include
)

target_include_directories(test_logging_enhancement
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rdparty/asio-1.36.0/include
)

target_include_directories(test_srt_socket_acceptor
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rdparty/asio-1.36.0/include
)

# Discover tests
gtest_discover_tests(test_srt_reactor)
gtest_discover_tests(test_logging_enhancement)
gtest_discover_tests(test_srt_socket_acceptor)

# Add the test
add_test(NAME SrtReactorTests COMMAND test_srt_reactor)
add_test(NAME LoggingEnhancementTests COMMAND test_logging_enhancement)
add_test(NAME SrtSocketAcceptorTests COMMAND test_srt_socket_acceptor)


