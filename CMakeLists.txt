cmake_minimum_required(VERSION 3.20)

# Set vcpkg toolchain file if not already set
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/Users/chris/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Force Clang compiler (must be set before project())
if(WIN32)
    # On Windows, prefer clang-cl over clang++ for better MSVC compatibility
    find_program(CLANG_CL_EXECUTABLE NAMES clang-cl clang-cl.exe
        PATHS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
            "$ENV{ProgramFiles}/LLVM/bin"
        DOC "Clang-cl compiler executable"
    )
    
    find_program(CLANG_EXECUTABLE NAMES clang clang.exe
        PATHS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
            "$ENV{ProgramFiles}/LLVM/bin"
        DOC "Clang compiler executable"
    )
    
    find_program(CLANGXX_EXECUTABLE NAMES clang++ clang++.exe
        PATHS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
            "$ENV{ProgramFiles}/LLVM/bin"
        DOC "Clang++ compiler executable"
    )
    
    # Prefer clang-cl for MSVC compatibility, fallback to clang++
    if(CLANG_CL_EXECUTABLE)
        set(CMAKE_C_COMPILER ${CLANG_CL_EXECUTABLE})
        set(CMAKE_CXX_COMPILER ${CLANG_CL_EXECUTABLE})
        message(STATUS "Using Clang-cl compiler for MSVC compatibility: ${CLANG_CL_EXECUTABLE}")
    elseif(CLANG_EXECUTABLE AND CLANGXX_EXECUTABLE)
        set(CMAKE_C_COMPILER ${CLANG_EXECUTABLE})
        set(CMAKE_CXX_COMPILER ${CLANGXX_EXECUTABLE})
        message(STATUS "Using Clang compiler: ${CLANGXX_EXECUTABLE}")
    else()
        message(WARNING "Clang not found. Install LLVM or add it to PATH. Falling back to default compiler.")
    endif()
else()
    # On Unix systems, try to use Clang if available
    find_program(CLANG_EXECUTABLE NAMES clang)
    find_program(CLANGXX_EXECUTABLE NAMES clang++)
    
    if(CLANG_EXECUTABLE AND CLANGXX_EXECUTABLE)
        set(CMAKE_C_COMPILER ${CLANG_EXECUTABLE})
        set(CMAKE_CXX_COMPILER ${CLANGXX_EXECUTABLE})
        message(STATUS "Using Clang compiler: ${CLANGXX_EXECUTABLE}")
    else()
        message(STATUS "Clang not found, using default compiler")
    endif()
endif()

# Project definition
project(Loom 
    VERSION 0.1.0 
    DESCRIPTION "Loom Programming Language Compiler"
    LANGUAGES CXX
)

# Only do these if this is the main project, and not if it is included through add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Ensure -std=c++xx instead of -std=g++xx
    set(CMAKE_CXX_EXTENSIONS OFF)
    
    # Let's nicely support folders in IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    
    # Export compile commands for IDEs
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Project options
option(LOOM_BUILD_TESTING "Build and run tests" ON)
option(LOOM_BUILD_EXAMPLES "Build examples" ON)
option(LOOM_USE_SANITIZERS "Enable sanitizers in debug builds (Linux/macOS only)" ON)
option(LOOM_FORCE_DISABLE_SANITIZERS "Force disable sanitizers on all platforms" OFF)
option(LOOM_USE_LLVM "Enable LLVM support" ON)
option(LOOM_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

# Dependencies
include(FetchContent)



# GoogleTest - Only if testing is enabled
if(LOOM_BUILD_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    TRUE
    )
endif()

# Make dependencies available
if(LOOM_BUILD_TESTING)
    FetchContent_MakeAvailable(googletest)
endif()

# Platform detection
if(WIN32)
    set(LOOM_PLATFORM "Windows")
elseif(APPLE)
    set(LOOM_PLATFORM "macOS")
elseif(UNIX)
    set(LOOM_PLATFORM "Linux")
else()
    set(LOOM_PLATFORM "Unknown")
endif()

# Compiler detection and configuration
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(LOOM_COMPILER "MSVC")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(LOOM_COMPILER "GCC")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Check if we're using clang-cl or regular clang
    get_filename_component(COMPILER_NAME ${CMAKE_CXX_COMPILER} NAME_WE)
    if(COMPILER_NAME STREQUAL "clang-cl")
        set(LOOM_COMPILER "Clang-cl")
    else()
        # Force it to be regular Clang even on Windows if we're using clang++
        set(LOOM_COMPILER "Clang")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(LOOM_COMPILER "Apple Clang")
else()
    set(LOOM_COMPILER "Unknown")
endif()

message(STATUS "Detected compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler simulate ID: ${CMAKE_CXX_SIMULATE_ID}")
message(STATUS "Compiler executable: ${CMAKE_CXX_COMPILER}")
message(STATUS "Loom compiler type: ${LOOM_COMPILER}")

# Additional Clang-specific configurations
if(LOOM_COMPILER STREQUAL "Clang" OR LOOM_COMPILER STREQUAL "Apple Clang")
    # Enable colored diagnostics
    add_compile_options(-fcolor-diagnostics)
    
    # Enable better debugging info format for Windows
    if(WIN32)
        add_compile_options(-gcodeview)
    endif()
    
    # Enable some useful Clang-specific warnings
    add_compile_options(
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-padded
    )
    
    message(STATUS "Clang-specific optimizations enabled")
endif()


# Compiler-specific settings
function(loom_set_compiler_warnings target)
    if(LOOM_COMPILER STREQUAL "MSVC" OR LOOM_COMPILER STREQUAL "Clang-cl")
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
        )
        if(LOOM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall 
            -Wextra 
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
        )
        
        # Additional Clang-specific warnings
        if(LOOM_COMPILER STREQUAL "Clang" OR LOOM_COMPILER STREQUAL "Apple Clang")
            target_compile_options(${target} PRIVATE
                -Wmost
                -Wextra-semi
                -Wno-c++98-compat
                -Wno-c++98-compat-pedantic
                -Wno-padded
                -Wno-exit-time-destructors
                -Wno-global-constructors
            )
        endif()
        
        if(LOOM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Sanitizers function
function(loom_enable_sanitizers target)
    if(LOOM_FORCE_DISABLE_SANITIZERS)
        message(STATUS "Sanitizers force-disabled for ${target}")
        return()
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND LOOM_USE_SANITIZERS)
        if(LOOM_COMPILER STREQUAL "GCC")
            target_compile_options(${target} PRIVATE
                -fsanitize=address
                -fsanitize=undefined
                -fno-omit-frame-pointer
            )
            target_link_options(${target} PRIVATE
                -fsanitize=address
                -fsanitize=undefined
            )
            message(STATUS "Sanitizers (ASan + UBSan) enabled for ${target}")
        elseif(LOOM_COMPILER STREQUAL "Clang" OR LOOM_COMPILER STREQUAL "Apple Clang")
            if(WIN32)
                # On Windows with Clang, sanitizers have compatibility issues with MSVC runtime
                # For now, disable sanitizers and rely on static analysis and other tools
                message(STATUS "Sanitizers disabled for ${target} on Windows due to runtime compatibility issues")
                message(STATUS "Consider using: clang-tidy, clang static analyzer, or external tools like Dr. Memory")
            else()
                # On Unix systems, both ASan and UBSan work well with Clang
                target_compile_options(${target} PRIVATE
                    -fsanitize=address
                    -fsanitize=undefined
                    -fno-omit-frame-pointer
                )
                target_link_options(${target} PRIVATE
                    -fsanitize=address
                    -fsanitize=undefined
                )
                message(STATUS "Sanitizers (ASan + UBSan) enabled for ${target}")
            endif()
        endif()
    endif()
endfunction()

# Additional developer tools configuration
if(WIN32 AND LOOM_COMPILER STREQUAL "Clang")
    # Add compile commands for clang-tidy
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    
    # Optional: Add custom target for running clang-tidy
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
    if(CLANG_TIDY_EXE)
        message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
        message(STATUS "You can run static analysis with: clang-tidy compiler/*.cc")
    endif()
    
    # Optional: Add custom target for running clang static analyzer
    find_program(CLANG_CHECK_EXE NAMES "clang-check" DOC "Path to clang-check executable")
    if(CLANG_CHECK_EXE)
        message(STATUS "Found clang-check: ${CLANG_CHECK_EXE}")
        message(STATUS "You can run static analysis with: clang-check -analyze compiler/*.cc")
    endif()
endif()

# Main subdirectories
add_subdirectory(compiler)

# Testing
if(LOOM_BUILD_TESTING AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(testing)
endif()

# Examples
if(LOOM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Configuration summary
message(STATUS "")
message(STATUS "=== Loom Build Configuration ===")
message(STATUS "Version:      ${PROJECT_VERSION}")
message(STATUS "Platform:     ${LOOM_PLATFORM}")
message(STATUS "Compiler:     ${LOOM_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Testing:           ${LOOM_BUILD_TESTING}")
message(STATUS "  Examples:          ${LOOM_BUILD_EXAMPLES}")
message(STATUS "  Sanitizers:        ${LOOM_USE_SANITIZERS}")
message(STATUS "  LLVM Support:      ${LOOM_USE_LLVM}")
message(STATUS "  Warnings as Errors: ${LOOM_WARNINGS_AS_ERRORS}")
message(STATUS "================================")
message(STATUS "")
