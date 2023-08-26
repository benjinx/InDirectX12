cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

# Allow for custom cmake modules
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(InDirectX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_subdirectory(thirdparty)

find_package(glm CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

add_subdirectory(examples/HelloWindow)
# add_subdirectory(examples/HelloTriangle)