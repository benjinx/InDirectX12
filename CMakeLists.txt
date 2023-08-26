cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

# Allow for custom cmake modules
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(InDirectX)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

###
### Dependencies
###

include(FetchContent)

FetchContent_Declare(
    glm
    URL         "https://codeload.github.com/g-truc/glm/zip/refs/tags/0.9.9.8"
    URL_HASH    MD5=a421a37d1896ab74765c3f94431ca0f6
)

FetchContent_Declare(
    fmt
    URL         "https://codeload.github.com/fmtlib/fmt/zip/refs/tags/10.1.0"
    URL_HASH    MD5=843ebaae55a64a1bff6078e1ead30f1f
)

FetchContent_Declare(
    directx-headers
    URL         "https://codeload.github.com/microsoft/DirectX-Headers/zip/refs/tags/v1.610.0"
    URL_HASH    MD5=f4a9443bd7630c585ac851a9be46fe97
)

FetchContent_MakeAvailable(glm fmt directx-headers)

###
### Examples
###

add_subdirectory(examples/HelloWindow)
# add_subdirectory(examples/HelloTriangle)