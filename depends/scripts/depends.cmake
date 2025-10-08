# Dependencies.cmake

# --- Path Configuration ---

# Get the directory where this script resides.
get_filename_component(DEPENDS_DIR "${CMAKE_CURRENT_LIST_DIR}" PATH)

# Define the base directory for all installed (resolved) dependencies.
set(DEP_RESOLVED_DIR ${DEPENDS_DIR}/resolved)

message(STATUS "Dependencies resolved directory: ${DEP_RESOLVED_DIR}")


# --- Function for Creating Imported Targets ---

# Function to create imported targets with explicit control over paths.
# This is used for libraries that have been pre-compiled and installed locally.
# Usage: create_imported_target(PREFIX <ns> NAME <lib> LIBNAME <file> LIBBASE <path> [INCLUDE <path>] [PATH <full_path>])
function(create_imported_target)
    cmake_parse_arguments(ARG "" "PREFIX;NAME;LIBNAME;LIBBASE;PATH;INCLUDE" "" ${ARGN})

    # Only create the target if it doesn't already exist.
    if(NOT TARGET ${ARG_PREFIX}::${ARG_NAME})
        # Verify required arguments
        if(NOT ARG_PREFIX OR NOT ARG_NAME OR NOT ARG_LIBNAME OR NOT ARG_LIBBASE)
            message(FATAL_ERROR "PREFIX, NAME, LIBNAME, and LIBBASE are required arguments")
        endif()

        # Handle library path (where the .a, .so, or .lib file is)
        if(ARG_PATH)
            # Use explicitly specified path
            set(LIB_PATH ${ARG_PATH})
        else()
            # Search in standard install locations (lib or lib64)
            set(LIB_PATHS
                    "${ARG_LIBBASE}/lib/${ARG_LIBNAME}"
                    "${ARG_LIBBASE}/lib64/${ARG_LIBNAME}"
            )

            unset(FOUND_LIB_PATH)
            foreach(PATH ${LIB_PATHS})
                if(EXISTS ${PATH})
                    set(FOUND_LIB_PATH ${PATH})
                    break()
                endif()
            endforeach()

            if(NOT FOUND_LIB_PATH)
                message(FATAL_ERROR "Could not find ${ARG_LIBNAME} in: ${LIB_PATHS}")
            endif()
            set(LIB_PATH ${FOUND_LIB_PATH})
        endif()

        # Handle include path
        if(ARG_INCLUDE)
            set(INCLUDE_PATH ${ARG_INCLUDE})
        else()
            set(INCLUDE_PATH "${ARG_LIBBASE}/include")
        endif()

        # Create the Imported Target
        add_library(${ARG_PREFIX}::${ARG_NAME} STATIC IMPORTED)

        # Set the location of the library and its public header files
        set_target_properties(${ARG_PREFIX}::${ARG_NAME} PROPERTIES
                IMPORTED_LOCATION ${LIB_PATH}
                INTERFACE_INCLUDE_DIRECTORIES ${INCLUDE_PATH}
        )

        message(STATUS "Created target: ${ARG_PREFIX}::${ARG_NAME}")
        message(STATUS "  Library: ${LIB_PATH}")
        message(STATUS "  Includes: ${INCLUDE_PATH}")
    endif()
endfunction()


# --- Individual Dependency Configuration ---

# 1. Secure Reliable Transport (SRT) Library
# The target is created as SRT::srt
create_imported_target(
        PREFIX SRT
        NAME srt
        LIBNAME libsrt.a
        LIBBASE ${DEP_RESOLVED_DIR}/srt
        INCLUDE ${DEP_RESOLVED_DIR}/srt/include
)


# --- Dependency Interface (Meta) Targets ---

# Find OpenSSL (required by SRT)
find_package(OpenSSL REQUIRED)

# SRT Interface Target: Bundles the library and its required compile definitions.
add_library(SRT_LIB INTERFACE)
target_link_libraries(SRT_LIB INTERFACE
        SRT::srt
)
# Link OpenSSL after SRT (dependency order matters for static linking)
target_link_libraries(SRT_LIB INTERFACE
        OpenSSL::Crypto
        OpenSSL::SSL
)
# Often required when compiling against external C++ libraries on Linux
target_compile_definitions(SRT_LIB INTERFACE _GLIBCXX_USE_CXX11_ABI=1)


# Project Dependencies Meta Target: A single target to link all dependencies.
# Projects should link against ProjectDependencies to get all requirements.
add_library(ProjectDependencies INTERFACE)

target_link_libraries(ProjectDependencies INTERFACE
        SRT_LIB
        # Add other dependency targets here as they are defined (e.g., FOLLY_LIB)
)

message(STATUS "All dependencies configured as CMake targets")

# NOTE: The commented-out sections below suggest future work using standard
# CMake mechanisms like CMAKE_PREFIX_PATH and find_package().

#set(CMAKE_PREFIX_PATH ${DEP_RESOLVED_DIR})
#set(CMAKE_MODULE_PATH ${DEP_RESOLVED_DIR})
#message("CMAKE_FRAMEWORK_PATH.\n")
#message("${CMAKE_FRAMEWORK_PATH}")
#message("CMAKE_FRAMEWORK_PATH.done")