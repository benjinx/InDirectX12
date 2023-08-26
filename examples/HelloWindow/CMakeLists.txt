
include(CompileShaderList)

set(_example HelloWindow)

file(
    GLOB_RECURSE
    _source_list
    "*.h"
    "*.hpp"
    "*.cpp"
)

file(
    GLOB_RECURSE
    _shader_source_list
    "*.hlsl"
)

compile_shader_list("${_shader_source_list}" _shader_list)

add_executable(
    ${_example}
    ${_source_list}
    ${_shader_list}
)

target_link_libraries(
    ${_example}
    PRIVATE
        d3d12.lib
        dxgi.lib
        dxcompiler.lib
        glm::glm
        fmt::fmt
        Microsoft::DirectX-Headers
)

target_include_directories(
    ${_example}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)