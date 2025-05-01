cmake_minimum_required(VERSION 3.10)
project(ecewo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# System modules
add_subdirectory(ecewo)

# Dev modules
if(EXISTS "${CMAKE_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

add_executable(server ${APP_SRC})

target_link_libraries(server PRIVATE ecewo)

target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ecewo
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

# Installation targets
install(TARGETS server DESTINATION bin)

# Run target
add_custom_target(run
    COMMAND server
    DEPENDS server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
