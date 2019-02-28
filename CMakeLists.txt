cmake_minimum_required(VERSION 3.13)
project(metricq-source-hdeem)

include(cmake/DefaultBuildType.cmake)
include(cmake/GitSubmoduleUpdate.cmake)

git_submodule_update()

add_subdirectory(lib)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(metricq-source-hdeem)
target_sources(metricq-source-hdeem PUBLIC
    src/main.cpp
    src/hdeem_source.cpp
    src/hdeem_connection.cpp
)
target_compile_features(metricq-source-hdeem PUBLIC cxx_std_17)
target_link_libraries(metricq-source-hdeem
    PUBLIC
        asio
        metricq::source
        metricq::logger-nitro
        Threads::Threads
        json::json
        hdeem::cxx
)
target_compile_options(metricq-source-hdeem
    PUBLIC
        $<$<CONFIG:Debug>:-Wall -pedantic -Wextra>
)

install(TARGETS metricq-source-hdeem
    RUNTIME DESTINATION bin
)
