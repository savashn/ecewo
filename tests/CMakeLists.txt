add_executable(ecewo_test
    runner.c
    ecewo-mock.c
    test-body.c
    test-blocking.c
    test-concurrent-request.c
    test-context.c
    test-fire-and-forget.c
    test-headers.c
    test-methods.c
    test-middleware.c
    test-params.c
    test-query.c
    test-redirect.c
    test-response.c
    test-root.c
    test-task-parallel.c
    test-task.c
)

target_link_libraries(ecewo_test PRIVATE
    ecewo
)

target_include_directories(ecewo_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
)
