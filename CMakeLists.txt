cmake_minimum_required(VERSION 3.20)
project(SpacetimeSimulator CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache for compilation")
endif()

find_package(glfw3 REQUIRED)

# Use the pre-built Dawn install — do NOT add_subdirectory(dawn)
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dawn/install/Release")
find_package(Dawn REQUIRED CONFIG)

add_executable(spacetime_sim
        main.cpp
        surface_utils.mm
)

target_include_directories(spacetime_sim PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(spacetime_sim PRIVATE
        glfw
        dawn::webgpu_dawn   # brings in Metal/Cocoa frameworks + include paths automatically
)

# Set working directory to project root so "shaders/fullscreen.wgsl" resolves correctly
set_target_properties(spacetime_sim PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
# For CLion / plain cmake --build runs, copy shaders next to the binary as a fallback
add_custom_command(TARGET spacetime_sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:spacetime_sim>/shaders"
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(spacetime_sim PRIVATE -O0 -g)
else()
    target_compile_options(spacetime_sim PRIVATE -O3)
endif()
