
# Header-only library for publish-subscribe pattern
add_library(bcast INTERFACE)

target_include_directories(bcast INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# If you have ASIO installed, link it here
# target_link_libraries(bcast INTERFACE asio::asio)
