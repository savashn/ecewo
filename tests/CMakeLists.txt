add_executable(ecewo_test
    runner.c
    test-basics.c
    test-request.c
    test-context.c
    test-worker.c
    test-cluster.c
)

target_link_libraries(ecewo_test PRIVATE
    ecewo
)

target_include_directories(ecewo_test PRIVATE
    ${CMAKE_SOURCE_DIR}/vendors/unity
    ${CMAKE_SOURCE_DIR}/tests
)
