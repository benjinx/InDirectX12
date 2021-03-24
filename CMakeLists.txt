CMAKE_MINIMUM_REQUIRED(VERSION 3.15 FATAL_ERROR)

PROJECT(InDirectX)

# https://github.com/microsoft/DirectXShaderCompiler for dxc
# or in the vulkanSDK
FIND_PROGRAM(
    dxc_PROGRAM
    NAMES dxc
)

FILE(
    GLOB_RECURSE
    _shaders_in
    "resources/*.hlsl"
)

SET(_shaders_out "")

FOREACH(shader ${_shaders_in})
    GET_FILENAME_COMPONENT(shader_path ${shader} PATH)
    GET_FILENAME_COMPONENT(shader_wle ${shader} NAME_WLE)

    SET(shader_vert_out "${shader_path}/${shader_wle}.vert.cso")
    SET(shader_pixel_out "${shader_path}/${shader_wle}.pixel.cso")

    ADD_CUSTOM_COMMAND(
        DEPENDS ${shader}
        COMMAND ${dxc_PROGRAM} -Fo ${shader_vert_out} -T vs_6_0 -E "VSMain" ${shader}
        COMMAND ${dxc_PROGRAM} -Fo ${shader_pixel_out} -T ps_6_0 -E "PSMain" ${shader}
        OUTPUT ${shader_vert_out} ${shader_pixel_out}
    )

    LIST(APPEND _shaders_out ${shader_vert_out} ${shader_pixel_out})
    #MESSAGE(STATUS "${_shaders_out}")
ENDFOREACH()

FILE(
    GLOB_RECURSE
    _source
    "src/*.h"
    "src/*.cc"
    "src/*.hpp"
    "src/*.cpp"
)

ADD_EXECUTABLE(
    ${PROJECT_NAME}
    ${_source}
    ${_shaders_in}
    ${_shaders_out}
)

TARGET_LINK_LIBRARIES(
    ${PROJECT_NAME}
    PRIVATE
        d3d12.lib
        dxgi.lib
        dxcompiler.lib
)

TARGET_INCLUDE_DIRECTORIES(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

TARGET_COMPILE_OPTIONS(
    ${PROJECT_NAME}
    PRIVATE
        /std:c++latest
)