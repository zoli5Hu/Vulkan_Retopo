cmake_minimum_required(VERSION 4.1.2)
project(Pro)

set(CMAKE_CXX_STANDARD 17)

# 1. Vulkan megkeresése
find_package(Vulkan REQUIRED)

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

# 3. A programod (Itt mondjuk meg, hogy a main.cpp mellett az src/RetopoApp.cpp is a projekt része)
add_executable(Pro
        main.cpp
        src/RetopoApp.cpp
)

# 4. Hozzácsatolás
target_include_directories(Pro PRIVATE ${Vulkan_INCLUDE_DIRS})

# Itt egy trükk: ha letöltöttük, akkor 'glfw' a neve, ha találtuk, akkor 'glfw3'
if(TARGET glfw)
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw)
else()
    target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw3)
endif()