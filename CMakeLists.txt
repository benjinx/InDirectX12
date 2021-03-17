CMAKE_MINIMUM_REQUIRED(VERSION 3.15 FATAL_ERROR)

PROJECT(InDirectX)

FILE(
    GLOB_RECURSE
    _source
    "src/*.hpp"
    "src/*.cpp"
)

ADD_EXECUTABLE(
    ${PROJECT_NAME}
    ${_source}
)

TARGET_LINK_LIBRARIES(
    ${PROJECT_NAME}
    d3d12.lib
    dxgi.lib
    dxcompiler.lib
)

TARGET_COMPILE_OPTIONS(
    ${PROJECT_NAME}
    PRIVATE
        /std:c++latest
)