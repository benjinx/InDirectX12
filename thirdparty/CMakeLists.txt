cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

# Determine if this file has been included by ADD_SUBDIRECTORY or run directly from cmake
# If the file was included, it will then run a sub-cmake in order to configure and build
# the required dependencies during the main project's configure step.
if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

    set(glm_ROOT             ${CMAKE_CURRENT_BINARY_DIR}/glm/cmake PARENT_SCOPE)
    set(fmt_ROOT             ${CMAKE_CURRENT_BINARY_DIR}/fmt PARENT_SCOPE)
    set(directx-headers_ROOT ${CMAKE_CURRENT_BINARY_DIR}/directx-headers PARENT_SCOPE)

    ###
    ### Configure
    ###

    if(CMAKE_GENERATOR_PLATFORM)
        set(_generatorPlatform -A ${CMAKE_GENERATOR_PLATFORM})
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -G "${CMAKE_GENERATOR}"
            ${_generatorPlatform}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        RESULT_VARIABLE _result
    )

    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Failed to configure thirdparty")
    endif()

    ###
    ### Build
    ###

    execute_process(
        COMMAND ${CMAKE_COMMAND} --build .
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        RESULT_VARIABLE _result
    )

    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Failed to build thirdparty")
    endif()

else()

    project(thirdparty)

    include(ExternalProject)

    ###
    ### glm
    ###

    set(glm_ROOT        ${CMAKE_BINARY_DIR}/glm/cmake)
    set(glm_SOURCE_DIR  ${CMAKE_BINARY_DIR}/glm)

    find_package(glm CONFIG QUIET)

    if (NOT glm_FOUND OR NOT TARGET glm::glm)
        ExternalProject_Add(
            thirdparty_glm
            URL                 "https://codeload.github.com/g-truc/glm/zip/refs/tags/0.9.9.8"
            URL_HASH            MD5=a421a37d1896ab74765c3f94431ca0f6
            SOURCE_DIR          ${glm_SOURCE_DIR}
            CONFIGURE_COMMAND   ""
            BUILD_COMMAND       ""
            INSTALL_COMMAND     ""
            UPDATE_COMMAND      ""
        )
    endif()

    ###
    ### fmt
    ###

    set(fmt_ROOT        ${CMAKE_BINARY_DIR}/fmt)
    set(fmt_SOURCE_DIR  ${CMAKE_BINARY_DIR}/fmt_src)
    set(fmt_BINARY_DIR  ${CMAKE_BINARY_DIR}/fmt_bin)

    find_package(fmt 10.0.0 CONFIG QUIET)

    if (NOT fmt_FOUND)
        ExternalProject_Add(
            thirdparty_fmt
            URL                 "https://codeload.github.com/fmtlib/fmt/zip/refs/tags/10.1.0"
            URL_HASH            MD5=843ebaae55a64a1bff6078e1ead30f1f
            SOURCE_DIR          ${fmt_SOURCE_DIR}
            BINARY_DIR          ${fmt_BINARY_DIR}
            CMAKE_ARGS          -DCMAKE_INSTALL_PREFIX=${fmt_ROOT}
                                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                                -DFMT_DOC=OFF
                                -DFMT_TEST=OFF
            BUILD_COMMAND       ""
            INSTALL_COMMAND     ${CMAKE_COMMAND} --build . --target install
            UPDATE_COMMAND      ""
        )
    endif()
    
    ###
    ### directx-headers
    ###

    set(directx-headers_ROOT        ${CMAKE_BINARY_DIR}/directx-headers)
    set(directx-headers_SOURCE_DIR  ${CMAKE_BINARY_DIR}/directx-headers_src)
    set(directx-headers_BINARY_DIR  ${CMAKE_BINARY_DIR}/directx-headers_bin)

    find_package(directx-headers CONFIG QUIET)

    if (NOT directx-headers_FOUND)
        ExternalProject_Add(
            thirdparty_directx-headers
            URL                 "https://codeload.github.com/microsoft/DirectX-Headers/zip/refs/tags/v1.610.0"
            URL_HASH            MD5=f4a9443bd7630c585ac851a9be46fe97
            SOURCE_DIR          ${directx-headers_SOURCE_DIR}
            BINARY_DIR          ${directx-headers_BINARY_DIR}
            CMAKE_ARGS          -DCMAKE_INSTALL_PREFIX=${directx-headers_ROOT}
                                -DDXHEADERS_BUILD_TEST=OFF
                                -DDXHEADERS_BUILD_GOOGLE_TEST=OFF
            BUILD_COMMAND       ""
            INSTALL_COMMAND     ${CMAKE_COMMAND} --build . --target install
            UPDATE_COMMAND      ""
        )
    endif()

endif()