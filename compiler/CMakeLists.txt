# Loom Compiler CMakeLists.txt

# Collect all source files
file(GLOB_RECURSE COMPILER_SOURCES CONFIGURE_DEPENDS
    "*.cpp"
    "*.cc" 
    "*.cxx"
)

file(GLOB_RECURSE COMPILER_HEADERS CONFIGURE_DEPENDS
    "*.hpp"
    "*.h"
    "*.hxx"
    "*.hh"
)

# Separate main.cc/cpp from library sources
set(MAIN_SOURCES)
set(LIB_SOURCES)

foreach(source ${COMPILER_SOURCES})
    get_filename_component(filename ${source} NAME_WE)
    if(filename STREQUAL "main")
        list(APPEND MAIN_SOURCES ${source})
    else()
        list(APPEND LIB_SOURCES ${source})
    endif()
endforeach()

# Create compiler library if we have non-main sources
if(LIB_SOURCES)
    add_library(loom_compiler_lib ${LIB_SOURCES} ${COMPILER_HEADERS})
    
    # Set target properties
    set_target_properties(loom_compiler_lib PROPERTIES
        CXX_STANDARD ${CMAKE_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # Include directories
    target_include_directories(loom_compiler_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Link dependencies

      # Apply compiler warnings
    loom_set_compiler_warnings(loom_compiler_lib)
    
    # Enable sanitizers in debug builds
    loom_enable_sanitizers(loom_compiler_lib)
    
    # Fix for LLVM/MSVC iterator debug level mismatch on Windows
    if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        # LLVM libraries are typically built with _ITERATOR_DEBUG_LEVEL=0 even in debug builds
        # This fixes compatibility issues when linking with LLVM
        target_compile_definitions(loom_compiler_lib PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    endif()
    
      # Alias for easier linking
    add_library(Loom::CompilerLib ALIAS loom_compiler_lib)
    
    list(LENGTH LIB_SOURCES LIB_SOURCES_COUNT)
    message(STATUS "Created loom_compiler_lib with ${LIB_SOURCES_COUNT} source files")
endif()

# Create main executable
if(MAIN_SOURCES)
    add_executable(loom ${MAIN_SOURCES})
    
    # Set target properties
    set_target_properties(loom PROPERTIES
        CXX_STANDARD ${CMAKE_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_NAME "loom"
    )
    
    # Link with compiler library if it exists
    if(TARGET loom_compiler_lib)
        target_link_libraries(loom PRIVATE loom_compiler_lib)
    else()

    endif()    # Apply compiler warnings
    loom_set_compiler_warnings(loom)
    
    # Enable sanitizers in debug builds
    loom_enable_sanitizers(loom)
    
    # Fix for LLVM/MSVC iterator debug level mismatch on Windows
    if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        # LLVM libraries are typically built with _ITERATOR_DEBUG_LEVEL=0 even in debug builds
        # This fixes compatibility issues when linking with LLVM
        target_compile_definitions(loom PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    endif()
    
    # Set output directory
    set_target_properties(loom PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    message(STATUS "Created loom executable")
else()
    message(WARNING "No main.cpp/main.cc found - no executable will be built")
endif()

# LLVM integration (optional)
if(LOOM_USE_LLVM)
    # Try to find LLVM using the config approach first
    # Add custom LLVM build path
    if(NOT DEFINED LLVM_DIR AND EXISTS "C:/dev/llvm-project/$installDir/lib/cmake/llvm")
        set(LLVM_DIR "C:/dev/llvm-project/$installDir/lib/cmake/llvm" CACHE PATH "LLVM installation directory")
        message(STATUS "Using custom LLVM build at: ${LLVM_DIR}")
    endif()
    
    find_package(LLVM CONFIG QUIET)
    
    # If that fails, try manual detection on Windows
    if(NOT LLVM_FOUND AND WIN32)
        message(STATUS "LLVM CONFIG not found, attempting manual detection...")
          # Find llvm-config executable
        find_program(LLVM_CONFIG_EXECUTABLE NAMES llvm-config llvm-config.exe
            PATHS
                "C:/dev/llvm-project/$installDir/bin"
                "C:/Program Files/LLVM/bin"
                "C:/Program Files (x86)/LLVM/bin"
            DOC "LLVM configuration tool"
        )
        
        if(LLVM_CONFIG_EXECUTABLE)
            message(STATUS "Found llvm-config: ${LLVM_CONFIG_EXECUTABLE}")
            
            # Get LLVM version
            execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --version
                OUTPUT_VARIABLE LLVM_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            # Get LLVM include directories
            execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
                OUTPUT_VARIABLE LLVM_INCLUDE_DIRS
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            # Get LLVM library directories
            execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --libdir
                OUTPUT_VARIABLE LLVM_LIBRARY_DIRS
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            # Get LLVM libraries for our components
            execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs core support target mc mcparser asmprinter
                OUTPUT_VARIABLE LLVM_LIBRARIES_RAW
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            # Get LLVM system libraries
            execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --system-libs
                OUTPUT_VARIABLE LLVM_SYSTEM_LIBS_RAW
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET  # Some versions don't support --system-libs
            )
            
            # Convert library strings to lists
            string(REPLACE " " ";" LLVM_LIBRARIES "${LLVM_LIBRARIES_RAW}")
            string(REPLACE " " ";" LLVM_SYSTEM_LIBS "${LLVM_SYSTEM_LIBS_RAW}")
            
            # Mark as found
            set(LLVM_FOUND TRUE)
            set(LLVM_PACKAGE_VERSION ${LLVM_VERSION})
            
            message(STATUS "Manual LLVM detection successful:")
            message(STATUS "  Version: ${LLVM_VERSION}")
            message(STATUS "  Include: ${LLVM_INCLUDE_DIRS}")
            message(STATUS "  LibDir:  ${LLVM_LIBRARY_DIRS}")
        endif()
    endif()
      if(LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
          # Add LLVM include directories and definitions
        if(TARGET loom_compiler_lib)
            # Use SYSTEM to suppress warnings from LLVM headers
            target_include_directories(loom_compiler_lib SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
            if(DEFINED LLVM_DEFINITIONS)
                target_compile_definitions(loom_compiler_lib PRIVATE ${LLVM_DEFINITIONS})
            endif()
            
            # Add specific warning suppressions for LLVM on Clang
            if(LOOM_COMPILER STREQUAL "Clang" OR LOOM_COMPILER STREQUAL "Apple Clang")
                target_compile_options(loom_compiler_lib PRIVATE
                    -Wno-old-style-cast
                    -Wno-sign-conversion
                    -Wno-shorten-64-to-32
                    -Wno-language-extension-token
                )
            endif()
        endif()
        
        if(TARGET loom)
            # Use SYSTEM to suppress warnings from LLVM headers
            target_include_directories(loom SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
            if(DEFINED LLVM_DEFINITIONS)
                target_compile_definitions(loom PRIVATE ${LLVM_DEFINITIONS})
            endif()
            
            # Add specific warning suppressions for LLVM on Clang
            if(LOOM_COMPILER STREQUAL "Clang" OR LOOM_COMPILER STREQUAL "Apple Clang")
                target_compile_options(loom PRIVATE
                    -Wno-old-style-cast
                    -Wno-sign-conversion
                    -Wno-shorten-64-to-32
                    -Wno-language-extension-token
                )
            endif()
        endif()
        
        # Link LLVM libraries
        if(COMMAND llvm_map_components_to_libnames)
            # Use CMake config approach if available
            llvm_map_components_to_libnames(llvm_libs 
                core 
                support 
                irreader 
                codegen 
                target 
                mc 
                mcparser 
                asmprinter 
                x86codegen  # Add other targets as needed (aarch64codegen, etc.)
            )
            set(FINAL_LLVM_LIBS ${llvm_libs})
        else()
            # Use manual approach - libraries from llvm-config
            set(FINAL_LLVM_LIBS ${LLVM_LIBRARIES})
            if(LLVM_SYSTEM_LIBS)
                list(APPEND FINAL_LLVM_LIBS ${LLVM_SYSTEM_LIBS})
            endif()            
            # Add library search path
            if(LLVM_LIBRARY_DIRS)
                link_directories(${LLVM_LIBRARY_DIRS})
            endif()
        endif()
        
        if(TARGET loom_compiler_lib)
            target_link_libraries(loom_compiler_lib PUBLIC ${FINAL_LLVM_LIBS})
        elseif(TARGET loom)
            target_link_libraries(loom PRIVATE ${FINAL_LLVM_LIBS})
        endif()
        
        message(STATUS "LLVM integration enabled")
    else()
        message(STATUS "LLVM not found - building without LLVM support")
        message(STATUS "To use LLVM, install it and set LLVM_DIR to the LLVM CMake directory")
        message(STATUS "Or ensure llvm-config is in your PATH")
    endif()
endif()

# Install rules
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(GNUInstallDirs)
    
    if(TARGET loom)
        install(TARGETS loom
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
    
    if(TARGET loom_compiler_lib)
        install(TARGETS loom_compiler_lib
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
        
        # Install headers
        install(FILES ${COMPILER_HEADERS}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/loom
        )
    endif()
endif()

# Print compiler module summary
message(STATUS "")
message(STATUS "=== Compiler Module ===")
list(LENGTH COMPILER_SOURCES COMPILER_SOURCES_COUNT)
list(LENGTH COMPILER_HEADERS COMPILER_HEADERS_COUNT)
message(STATUS "Sources found: ${COMPILER_SOURCES_COUNT}")
message(STATUS "Headers found: ${COMPILER_HEADERS_COUNT}")
if(TARGET loom_compiler_lib)
    message(STATUS "Library: loom_compiler_lib")
endif()
if(TARGET loom)
    message(STATUS "Executable: loom")
endif()
if(LOOM_USE_LLVM AND LLVM_FOUND)
    message(STATUS "LLVM: Enabled (${LLVM_PACKAGE_VERSION})")
else()
    message(STATUS "LLVM: Disabled")
endif()
message(STATUS "======================")
message(STATUS "")
