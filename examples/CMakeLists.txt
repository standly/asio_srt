# Examples

# Find OpenSSL (required by SRT)
find_package(OpenSSL REQUIRED)

# SRT Server Example
add_executable(srt_server_example
    srt_server_example.cpp
)

target_link_libraries(srt_server_example
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# SRT Client Example
add_executable(srt_client_example
    srt_client_example.cpp
)

target_link_libraries(srt_client_example
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# SRT Advanced Server Example
add_executable(srt_advanced_server
    srt_advanced_server.cpp
)

target_link_libraries(srt_advanced_server
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# SRT Server V2 Example
add_executable(srt_server_v2_example
    srt_server_v2_example.cpp
)

target_link_libraries(srt_server_v2_example
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# SRT Client V2 Example
add_executable(srt_client_v2_example
    srt_client_v2_example.cpp
)

target_link_libraries(srt_client_v2_example
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Option Test Example
add_executable(option_test_example
    option_test_example.cpp
)

target_link_libraries(option_test_example
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Custom Logging Example (if exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/custom_logging_example.cpp)
    add_executable(custom_logging_example
        custom_logging_example.cpp
    )
    
    target_link_libraries(custom_logging_example
        PRIVATE
        asrt
        SRT::srt
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
    )
endif()

# SRT Streaming Example
add_executable(srt_streaming_example
    srt_streaming_example.cpp
)

target_link_libraries(srt_streaming_example
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# SRT Resilient Client Example
add_executable(srt_resilient_client_example
    srt_resilient_client_example.cpp
)

target_link_libraries(srt_resilient_client_example
    PRIVATE
    asrt
    SRT::srt
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

