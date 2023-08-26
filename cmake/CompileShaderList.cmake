# https://github.com/microsoft/DirectXShaderCompiler for dxc
# or in the vulkanSDK
find_program(
    dxc_PROGRAM
    NAMES dxc
    REQUIRED
)

function(compile_shader_list _shader_source_list _shader_list_out)

    foreach(_shader_source ${_shader_source_list})
        cmake_path(GET _shader_source PARENT_PATH _shader_source_path)
        cmake_path(GET _shader_source STEM LAST_ONLY _shader_source_name)

        set(_shader_vert_out "${_shader_source_path}/${_shader_source_name}.vert.cso")
        set(_shader_pixel_out "${_shader_source_path}/${_shader_source_name}.pixel.cso")

        add_custom_command(
            DEPENDS ${_shader_source}
            COMMAND ${dxc_PROGRAM} -Fo ${_shader_vert_out} -T vs_6_0 -E "VSMain" ${_shader_source}
            COMMAND ${dxc_PROGRAM} -Fo ${_shader_pixel_out} -T ps_6_0 -E "PSMain" ${_shader_source}
            OUTPUT ${_shader_vert_out} ${_shader_pixel_out}
        )

        list(APPEND _shaders_out ${_shader_vert_out} ${_shader_pixel_out})

    endforeach()

    set(${_shader_list_out} "${_shaders_out}" PARENT_SCOPE)

endfunction()