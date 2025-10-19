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

# Add module-specific tests subdirectories
add_subdirectory(acore)
add_subdirectory(asrt)

