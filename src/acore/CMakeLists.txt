
# Header-only library for publish-subscribe pattern
add_library(acore INTERFACE
        async_queue.hpp
        dispatcher.hpp
        async_event.hpp
        async_semaphore.hpp
        async_waitgroup.hpp
)

target_include_directories(acore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# If you have ASIO installed, link it here
# target_link_libraries(acore INTERFACE asio::asio)
