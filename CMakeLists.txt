cmake_minimum_required(VERSION 3.20)
project(Pro)

set(CMAKE_CXX_STANDARD 17)

# ==========================================
# 1. VULKAN ÉS SHADEREK BEÁLLÍTÁSA
# ==========================================
find_package(Vulkan REQUIRED glslc)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(VERT_SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert)
set(VERT_SHADER_SPV ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vert.spv)
set(FRAG_SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag)
set(FRAG_SHADER_SPV ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frag.spv)

add_custom_command(
        OUTPUT ${VERT_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND Vulkan::glslc ${VERT_SHADER_SRC} -o ${VERT_SHADER_SPV}
        DEPENDS ${VERT_SHADER_SRC}
        COMMENT "Compiling vertex shader"
)

add_custom_command(
        OUTPUT ${FRAG_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND Vulkan::glslc ${FRAG_SHADER_SRC} -o ${FRAG_SHADER_SPV}
        DEPENDS ${FRAG_SHADER_SRC}
        COMMENT "Compiling fragment shader"
)

add_custom_target(
        CompileShaders ALL
        DEPENDS ${VERT_SHADER_SPV} ${FRAG_SHADER_SPV}
)

# ==========================================
# 2. KÜLSŐ KÖNYVTÁRAK LETÖLTÉSE (FETCHCONTENT)
# ==========================================
include(FetchContent)

# GLFW (Ablakkezelő)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)

# GLM (3D Matematika)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG master)
FetchContent_MakeAvailable(glm)

# TinyObjLoader (Modell betöltő)
FetchContent_Declare(tinyobjloader GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git GIT_TAG release)
FetchContent_MakeAvailable(tinyobjloader)

# Dear ImGui (Felhasználói felület)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG master)
FetchContent_MakeAvailable(imgui)

# ImGui C++ forrásfájlok összegyűjtése
set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_SOURCES}   
)

# Megpróbáljuk megkeresni a GLFW-t a gépen, ha nincs, letöltjük
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW nem talalhato a rendszeren, letoltes inditasa...")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# ==========================================
# 3. A FŐ PROGRAM (Pro) BEÁLLÍTÁSA
# ==========================================
add_executable(Pro
        main.cpp
        src/RetopoApp.cpp
        src/RetopoApp.h
        src/Pipeline.cpp
        src/Pipeline.h
        src/Vertex.h
        src/VulkanContext.cpp
        src/VulkanContext.h
        src/Renderer.cpp
        src/Renderer.h
        src/ResourceManager.cpp
        src/ResourceManager.h
        src/VulkanUtils.cpp
        src/VulkanUtils.h
        ${IMGUI_SOURCES} # <--- ImGui forrásfájlok hozzáfűzve!
)

# Shaderek lefordítása az exe előtt
add_dependencies(Pro CompileShaders)

# Mappák, amiket a fő program láthat (Vulkan + ImGui)
target_include_directories(Pro PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

# ==========================================
# 4. TESZTELŐ PROGRAM (VulkanTests) BEÁLLÍTÁSA
# ==========================================
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG release-1.12.1)
FetchContent_MakeAvailable(googletest)

add_executable(VulkanTests
        tests/test_main.cpp
        src/Pipeline.cpp
        src/RetopoApp.cpp
        src/VulkanContext.cpp
        src/Renderer.cpp
        src/ResourceManager.cpp
        src/VulkanUtils.cpp
)

# Mappák, amiket a tesztelő láthat (A saját src mappád + Vulkan + ImGui)
target_include_directories(VulkanTests PRIVATE
        src
        ${Vulkan_INCLUDE_DIRS}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

# ==========================================
# 5. KÖNYVTÁRAK ÖSSZELINKELÉSE (A VÉGSŐ LÉPÉS)
# ==========================================
# Ide kötjük be az összes letöltött modult mindkét programba!
if(TARGET glfw)
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw glm tinyobjloader)
    target_link_libraries(VulkanTests gtest_main ${Vulkan_LIBRARIES} glfw glm tinyobjloader)
else()
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw3 glm tinyobjloader)
    target_link_libraries(VulkanTests gtest_main ${Vulkan_LIBRARIES} glfw3 glm tinyobjloader)
endif()