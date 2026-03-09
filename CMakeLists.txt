cmake_minimum_required(VERSION 3.30)
project(physics)

set(CMAKE_CXX_STANDARD 20)

add_executable(physics main.cpp)

# 1. Tell Dawn to automatically download its own dependencies (Tint, SPIR-V, etc.)
set(DAWN_FETCH_DEPENDENCIES ON)

# 2. Add the Dawn submodule to our build path
add_subdirectory("dawn" EXCLUDE_FROM_ALL)

# 3. Find GLFW (installed via Homebrew)
find_package(glfw3 REQUIRED)

# 4. Define our executable
add_executable(Renderer main.cpp)

# 5. Link Dawn and GLFW to our executable
# Dawn provides 'webgpu_dawn' (the core API) and 'webgpu_glfw' (helpers to map WebGPU to a GLFW window)
target_link_libraries(Renderer PRIVATE
        webgpu_dawn
        webgpu_glfw
        glfw
)