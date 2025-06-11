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

    endif()
    
    # Apply compiler warnings
    loom_set_compiler_warnings(loom)
    
    # Enable sanitizers in debug builds
    loom_enable_sanitizers(loom)
    
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
    find_package(LLVM CONFIG QUIET)
    
    if(LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
        message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
        
        # Add LLVM include directories and definitions
        if(TARGET loom_compiler_lib)
            target_include_directories(loom_compiler_lib PRIVATE ${LLVM_INCLUDE_DIRS})
            target_compile_definitions(loom_compiler_lib PRIVATE ${LLVM_DEFINITIONS})
        endif()
        
        if(TARGET loom)
            target_include_directories(loom PRIVATE ${LLVM_INCLUDE_DIRS})
            target_compile_definitions(loom PRIVATE ${LLVM_DEFINITIONS})
        endif()
        
        # Find required LLVM components
        llvm_map_components_to_libnames(llvm_libs core support)
        
        if(TARGET loom_compiler_lib)
            target_link_libraries(loom_compiler_lib PUBLIC ${llvm_libs})
        elseif(TARGET loom)
            target_link_libraries(loom PRIVATE ${llvm_libs})
        endif()
        
        message(STATUS "LLVM integration enabled")
    else()
        message(STATUS "LLVM not found - building without LLVM support")
        message(STATUS "To use LLVM, install it and set LLVM_DIR to the LLVM CMake directory")
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
