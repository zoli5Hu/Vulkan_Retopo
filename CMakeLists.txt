cmake_minimum_required(VERSION 4.1.2)
project(Pro)

set(CMAKE_CXX_STANDARD 17)

# 1. Vulkan megkeresése (glslc komponenssel!)
find_package(Vulkan REQUIRED glslc)

# --- SHADEREK BEÁLLÍTÁSA ---
set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

# Vertex Shader
set(VERT_SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert)
set(VERT_SHADER_SPV ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vert.spv)

# Fragment Shader
set(FRAG_SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag)
set(FRAG_SHADER_SPV ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frag.spv)

# shader.vert -> vert.spv
add_custom_command(
        OUTPUT ${VERT_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND Vulkan::glslc ${VERT_SHADER_SRC} -o ${VERT_SHADER_SPV}
        DEPENDS ${VERT_SHADER_SRC}
        COMMENT "Compiling vertex shader"
)

# shader.frag -> frag.spv
add_custom_command(
        OUTPUT ${FRAG_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND Vulkan::glslc ${FRAG_SHADER_SRC} -o ${FRAG_SHADER_SPV}
        DEPENDS ${FRAG_SHADER_SRC}
        COMMENT "Compiling fragment shader"
)

# Target létrehozása a shaderekhez
add_custom_target(
        CompileShaders ALL
        DEPENDS ${VERT_SHADER_SPV} ${FRAG_SHADER_SPV}
)
# --- SHADER BLOKK VÉGE ---

# 2. GLFW kezelése (FetchContent - Automata letöltés, ha nincs meg a gépen)
include(FetchContent)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)

# Megpróbáljuk megkeresni a gépen, ha nincs, letöltjük
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW nem talalhato a rendszeren, letoltes inditasa...")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# 3. A programod
add_executable(Pro
        main.cpp
        src/RetopoApp.cpp
)

# Ez biztosítja, hogy a shaderek leforduljanak az exe előtt
add_dependencies(Pro CompileShaders)

# 4. Hozzácsatolás
target_include_directories(Pro PRIVATE ${Vulkan_INCLUDE_DIRS})

# Itt egy trükk: ha letöltöttük, akkor 'glfw' a neve, ha találtuk, akkor 'glfw3'
if(TARGET glfw)
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw)
else()
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw3)
endif()