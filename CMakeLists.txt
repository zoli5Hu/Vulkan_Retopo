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

# --- GLM (3D Matematika könyvtár) ---
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
)
FetchContent_MakeAvailable(glm)

# ---> ÚJ: TinyObjLoader betöltése <---
FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG release
)
FetchContent_MakeAvailable(tinyobjloader)

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
        src/RetopoApp.h
        src/Pipeline.cpp
        src/Pipeline.h
        src/Vertex.h
)
# Ez biztosítja, hogy a shaderek leforduljanak az exe előtt
add_dependencies(Pro CompileShaders)

# 4. Hozzácsatolás
target_include_directories(Pro PRIVATE ${Vulkan_INCLUDE_DIRS})

# ==========================================
# 5. GOOGLE TEST (UNIT TESZTEK) BEÁLLÍTÁSA
# ==========================================
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)
# Ez letölti a GTest könyvtárat a háttérben
FetchContent_MakeAvailable(googletest)

# Új "program" létrehozása csak a teszteknek
add_executable(VulkanTests
        tests/test_main.cpp
        src/Pipeline.cpp
        src/RetopoApp.cpp   # <--- Ezt hozzá kell adni!
)

# Kötések (Include mappák)
target_include_directories(VulkanTests PRIVATE src ${Vulkan_INCLUDE_DIRS})

# Itt kötjük hozzá a GLFW-t és a GLM-et a Pro-hoz ÉS a VulkanTests-hez is!
if(TARGET glfw)
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw glm tinyobjloader)
    target_link_libraries(VulkanTests gtest_main ${Vulkan_LIBRARIES} glfw glm tinyobjloader)
else()
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw3 glm tinyobjloader)
    target_link_libraries(VulkanTests gtest_main ${Vulkan_LIBRARIES} glfw3 glm tinyobjloader)
endif()

