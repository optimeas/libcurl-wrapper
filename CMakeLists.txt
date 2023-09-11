project(libcurl-wrapper)

set(HEADERS
        include/libcurl-wrapper/curlasynctransfer.hpp
        include/libcurl-wrapper/curlhttptransfer.hpp
        include/libcurl-wrapper/curlholder.hpp
        include/libcurl-wrapper/curlmultiasync.hpp
        include/libcurl-wrapper/tracing.hpp
        include/libcurl-wrapper/tracefile.hpp
        include/libcurl-wrapper/traceconfiguration.hpp
        include/libcurl-wrapper/curlurl.hpp
)

set(SOURCES
        curlasynctransfer.cpp
        curlhttptransfer.cpp
        curlholder.cpp
        curlmultiasync.cpp
        tracefile.cpp
        traceconfiguration.cpp
        curlurl.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME}
    ${PC_LIBCURL_LDFLAGS}
    fmt::fmt
    cpp-utils
)

target_include_directories(${PROJECT_NAME}
    PUBLIC include
    PRIVATE .                 # "dot" is redundant, because local headers are always available in C/C++.
)

# We intentionally don't make the unit tests dependent on CMAKE_TESTING_ENABLED: so everyone can decide for themselves which unit tests to build
option(ENABLE_LIBCURL_UTILS_TESTING "unit tests for libcurl-wrapper" FALSE)
if(ENABLE_LIBCURL_UTILS_TESTING)
    add_subdirectory(tests)
endif()
