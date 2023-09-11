project(libcurl-wrapper-tests)

find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

set(SOURCES
    curlmultiasync_tests.cpp
    curlhttptransfer_tests.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::GTest
    GTest::Main
    libcurl-wrapper
    httpmockserver
)

add_test(${PROJECT_NAME} ${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME} DESTINATION .)

# We create a separate application here, because the other transfer tests contain longer timeout tests
set(TEST_URLUTILS_PROJECT "libcurl-wrapper-curlurl-tests")
add_executable(${TEST_URLUTILS_PROJECT} curlurl_tests.cpp)
target_link_libraries(${TEST_URLUTILS_PROJECT} PRIVATE
    GTest::GTest
    GTest::Main
    ${PC_LIBCURL_LDFLAGS}
    libcurl-wrapper
)
add_test(${TEST_URLUTILS_PROJECT} ${TEST_URLUTILS_PROJECT})
install(TARGETS ${TEST_URLUTILS_PROJECT} DESTINATION .)
