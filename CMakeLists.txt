cmake_minimum_required(VERSION 3.13)

########################################################################################################################

project(fty-automatic-group
    VERSION 1.0.0
    DESCRIPTION "Dynamic group management"
)

########################################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
########################################################################################################################

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(rest)

########################################################################################################################
