cmake_minimum_required(VERSION 4.2)
project(Pro)

set(CMAKE_CXX_STANDARD 17)

# Csomagok megkeresése
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED) # <--- A GLFW megkeresése

add_executable(Pro main.cpp)

# A Vulkan ÉS a GLFW hozzácsatolása a programodhoz
target_include_directories(Pro PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Pro ${Vulkan_LIBRARIES} glfw) # <--- Itt adtuk hozzá a glfw-t