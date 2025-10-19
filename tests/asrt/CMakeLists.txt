# asrt (SRT Integration) Tests CMakeLists.txt

# Option to build asrt tests
option(BUILD_asrt_TESTS "Build asrt tests" ON)

if(NOT BUILD_asrt_TESTS)
    return()
endif()

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

# Test executable for custom log
add_executable(test_custom_log
    test_custom_log.cpp
)

# Test executable for default messageapi
add_executable(test_check_default_messageapi
    test_check_default_messageapi.cpp
)

# Link against libraries  
# Note: Order matters for static libraries - dependencies must come after the libraries that use them
target_link_libraries(test_srt_reactor
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    FMT::fmt
    Threads::Threads
)

target_link_libraries(test_logging_enhancement
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    FMT::fmt
    Threads::Threads
)

target_link_libraries(test_srt_socket_acceptor
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    FMT::fmt
    Threads::Threads
)

target_link_libraries(test_custom_log
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    FMT::fmt
    Threads::Threads
)

target_link_libraries(test_check_default_messageapi
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    asrt
    SRT_LIB
    FMT::fmt
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

target_include_directories(test_custom_log
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rdparty/asio-1.36.0/include
)

target_include_directories(test_check_default_messageapi
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rdparty/asio-1.36.0/include
)

# Discover tests
gtest_discover_tests(test_srt_reactor)
gtest_discover_tests(test_logging_enhancement)
gtest_discover_tests(test_srt_socket_acceptor)
gtest_discover_tests(test_custom_log)
gtest_discover_tests(test_check_default_messageapi)

# Add the tests
add_test(NAME SrtReactorTests COMMAND test_srt_reactor)
add_test(NAME LoggingEnhancementTests COMMAND test_logging_enhancement)
add_test(NAME SrtSocketAcceptorTests COMMAND test_srt_socket_acceptor)
add_test(NAME CustomLogTests COMMAND test_custom_log)
add_test(NAME CheckDefaultMessageApiTests COMMAND test_check_default_messageapi)

message(STATUS "asrt tests configured")

