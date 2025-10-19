# acore Tests CMakeLists.txt

# Option to build acore tests
option(BUILD_acore_TESTS "Build acore tests" ON)

if(NOT BUILD_acore_TESTS)
    return()
endif()

# Test executables for acore components
add_executable(test_async_event test_async_event.cpp)
target_link_libraries(test_async_event PRIVATE acore pthread)
target_compile_features(test_async_event PRIVATE cxx_std_20)
target_include_directories(test_async_event PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_async_queue test_async_queue.cpp)
target_link_libraries(test_async_queue PRIVATE acore pthread)
target_compile_features(test_async_queue PRIVATE cxx_std_20)
target_include_directories(test_async_queue PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_async_semaphore test_async_semaphore.cpp)
target_link_libraries(test_async_semaphore PRIVATE acore pthread)
target_compile_features(test_async_semaphore PRIVATE cxx_std_20)
target_include_directories(test_async_semaphore PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_dispatcher test_dispatcher.cpp)
target_link_libraries(test_dispatcher PRIVATE acore pthread)
target_compile_features(test_dispatcher PRIVATE cxx_std_20)
target_include_directories(test_dispatcher PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_waitgroup test_waitgroup.cpp)
target_link_libraries(test_waitgroup PRIVATE acore pthread)
target_compile_features(test_waitgroup PRIVATE cxx_std_20)
target_include_directories(test_waitgroup PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_waitgroup_race test_waitgroup_race.cpp)
target_link_libraries(test_waitgroup_race PRIVATE acore pthread)
target_compile_features(test_waitgroup_race PRIVATE cxx_std_20)
target_include_directories(test_waitgroup_race PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_cancellation test_cancellation.cpp)
target_link_libraries(test_cancellation PRIVATE acore pthread)
target_compile_features(test_cancellation PRIVATE cxx_std_20)
target_include_directories(test_cancellation PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Add tests to CTest
add_test(NAME AsyncEventTests COMMAND test_async_event)
add_test(NAME AsyncQueueTests COMMAND test_async_queue)
add_test(NAME AsyncSemaphoreTests COMMAND test_async_semaphore)
add_test(NAME DispatcherTests COMMAND test_dispatcher)
add_test(NAME WaitGroupTests COMMAND test_waitgroup)
add_test(NAME WaitGroupRaceTests COMMAND test_waitgroup_race)
add_test(NAME CancellationTests COMMAND test_cancellation)

message(STATUS "acore tests configured")

