# Loom Testing CMakeLists.txt

if(NOT LOOM_BUILD_TESTING)
    return()
endif()

# Collect test source files
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "*_test.cpp"
    "*_test.cc" 
    "test_*.cpp"
    "test_*.cc"
)

# Create test executable for each test file
foreach(test_source ${TEST_SOURCES})
    # Get test name from filename
    get_filename_component(test_name ${test_source} NAME_WE)
    
    # Create test executable
    add_executable(${test_name} ${test_source})
    
    # Set target properties
    set_target_properties(${test_name} PROPERTIES
        CXX_STANDARD ${CMAKE_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # Link with GoogleTest
    target_link_libraries(${test_name} PRIVATE
        gtest
        gtest_main
        gmock
        gmock_main
    )
    
    # Link with compiler library if it exists
    if(TARGET loom_compiler_lib)
        target_link_libraries(${test_name} PRIVATE loom_compiler_lib)
    endif()
    
    # Apply compiler warnings
    loom_set_compiler_warnings(${test_name})
    
    # Enable sanitizers
    loom_enable_sanitizers(${test_name})
    
    # Set output directory
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    )
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Set test properties
    set_tests_properties(${test_name} PROPERTIES
        LABELS "unit"
    )
    
    message(STATUS "Added test: ${test_name}")
endforeach()

# Create a custom target to run all tests
if(TEST_SOURCES)
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel ${CMAKE_BUILD_PARALLEL_LEVEL}
        DEPENDS ${TEST_SOURCES}
        COMMENT "Running all tests"
        VERBATIM
    )
    
    message(STATUS "Created run_tests target")
endif()

# Benchmark tests (if any)
file(GLOB_RECURSE BENCHMARK_SOURCES CONFIGURE_DEPENDS
    "*_benchmark.cpp"
    "*_benchmark.cc"
    "benchmark_*.cpp" 
    "benchmark_*.cc"
)

foreach(benchmark_source ${BENCHMARK_SOURCES})
    get_filename_component(benchmark_name ${benchmark_source} NAME_WE)
    
    add_executable(${benchmark_name} ${benchmark_source})
    
    set_target_properties(${benchmark_name} PROPERTIES
        CXX_STANDARD ${CMAKE_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
    )
    
    # Link with compiler library if it exists
    if(TARGET loom_compiler_lib)
        target_link_libraries(${benchmark_name} PRIVATE loom_compiler_lib)
    endif()
    
    # Apply compiler warnings
    loom_set_compiler_warnings(${benchmark_name})
    
    # Add to benchmarks
    add_test(NAME ${benchmark_name} COMMAND ${benchmark_name})
    set_tests_properties(${benchmark_name} PROPERTIES LABELS "benchmark")
    
    message(STATUS "Added benchmark: ${benchmark_name}")
endforeach()

# Print testing summary
message(STATUS "")
message(STATUS "=== Testing Module ===")
list(LENGTH TEST_SOURCES TEST_SOURCES_COUNT)
list(LENGTH BENCHMARK_SOURCES BENCHMARK_SOURCES_COUNT)
message(STATUS "Unit tests: ${TEST_SOURCES_COUNT}")
message(STATUS "Benchmarks: ${BENCHMARK_SOURCES_COUNT}")
message(STATUS "======================")
message(STATUS "")
