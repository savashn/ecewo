add_library(unity STATIC
    ${CMAKE_SOURCE_DIR}/vendors/unity/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_SOURCE_DIR}/vendors/unity
)

add_library(mock STATIC
    ${CMAKE_SOURCE_DIR}/tests/mock.c
)

target_include_directories(mock PUBLIC
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendors
)

target_link_libraries(mock PUBLIC
    ecewo
)

add_executable(ecewo_test
    test.c
)

target_link_libraries(ecewo_test PRIVATE
    ecewo
    unity
    mock
)

target_include_directories(ecewo_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendors/unity
    ${CMAKE_SOURCE_DIR}/tests
)

message(STATUS "Tests configured. Run with: ctest")
