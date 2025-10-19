# Header-only library for async primitives
add_library(acore INTERFACE
        async_queue.hpp
        async_event.hpp
        async_semaphore.hpp
        async_waitgroup.hpp
        async_mutex.hpp
        async_periodic_timer.hpp
        async_rate_limiter.hpp
        async_barrier.hpp
        async_auto_reset_event.hpp
        async_latch.hpp
        dispatcher.hpp
        handler_traits.hpp
)

target_include_directories(acore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# If you have ASIO installed, link it here
# target_link_libraries(acore INTERFACE asio::asio)

# Note: Tests are located in tests/acore/ directory
