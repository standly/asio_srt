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

# Test executable
add_executable(test_srt_reactor
    test_srt_reactor.cpp
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

# Include directories
target_include_directories(test_srt_reactor
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rdparty/asio-1.36.0/include
)

# Discover tests
gtest_discover_tests(test_srt_reactor)

# Add the test
add_test(NAME SrtReactorTests COMMAND test_srt_reactor)


