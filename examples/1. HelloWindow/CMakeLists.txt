
set(_example HelloWindow)

file(
    GLOB_RECURSE
    _source_list
    "*.h"
    "*.hpp"
    "*.cpp"
)

add_executable(
    ${_example}
    ${_source_list}
)

target_link_libraries(
    ${_example}
    PRIVATE
        glm::glm
        fmt::fmt
)