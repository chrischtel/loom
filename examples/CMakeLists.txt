# Loom Examples CMakeLists.txt

if(NOT LOOM_BUILD_EXAMPLES)
    return()
endif()

# Collect example Loom source files
file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS
    "*.loom"
)

# For now, examples are just data files since we don't have a working compiler yet
# When the compiler is ready, these can be compiled to executables

if(EXAMPLE_SOURCES)
    # Create a custom target that lists all examples
    add_custom_target(examples
        COMMENT "Loom example programs"
        SOURCES ${EXAMPLE_SOURCES}
    )
    
    # Install examples to share directory
    if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        include(GNUInstallDirs)
        install(FILES ${EXAMPLE_SOURCES}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/loom/examples
        )
    endif()
    
    list(LENGTH EXAMPLE_SOURCES EXAMPLE_SOURCES_COUNT)
    message(STATUS "Found ${EXAMPLE_SOURCES_COUNT} example programs")
    
    # When the compiler is working, we can add rules like this:
    # foreach(example_source ${EXAMPLE_SOURCES})
    #     get_filename_component(example_name ${example_source} NAME_WE)
    #     add_custom_command(
    #         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${example_name}
    #         COMMAND loom ${CMAKE_CURRENT_SOURCE_DIR}/${example_source} -o ${example_name}
    #         DEPENDS loom ${example_source}
    #         COMMENT "Compiling ${example_source}"
    #     )
    #     list(APPEND COMPILED_EXAMPLES ${CMAKE_CURRENT_BINARY_DIR}/${example_name})
    # endforeach()
    # 
    # add_custom_target(compile_examples ALL DEPENDS ${COMPILED_EXAMPLES})
    
else()
    message(STATUS "No example programs found")
endif()

# Print examples summary
message(STATUS "")
message(STATUS "=== Examples Module ===")
list(LENGTH EXAMPLE_SOURCES EXAMPLE_SOURCES_COUNT)
message(STATUS "Example files: ${EXAMPLE_SOURCES_COUNT}")
message(STATUS "=======================")
message(STATUS "")
