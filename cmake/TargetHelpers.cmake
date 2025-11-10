# TargetHelpers.cmake
#
# Helper functions for creating and configuring executable targets
# with consistent compile and link options.

# Function to reduce duplication when creating cpdn executable targets
# 
# Parameters:
#   target_name   - The CMake target name
#   output_name   - The output executable name
#   compile_opts  - List of compile options (can be empty)
#   link_opts     - List of link options (can be empty)
#
function(add_cpdn_executable target_name output_name compile_opts link_opts)
    add_executable(${target_name} ./src/openifs.cpp)
    set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${output_name})
    target_link_libraries(${target_name} PRIVATE cpdn_control Threads::Threads)
    
    if(compile_opts)
        target_compile_options(${target_name} PRIVATE ${compile_opts})
    endif()
    
    if(link_opts)
        target_link_options(${target_name} PRIVATE ${link_opts})
    endif()
endfunction()
