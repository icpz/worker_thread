cmake_minimum_required(VERSION 3.10)
project(worker_thread)

find_library(LIBEV NAMES libev.a libev.lib ev.lib ev)

option(WORKER_THREAD_BUILD_TESTS "Build tests" OFF)

add_library(${PROJECT_NAME} src/worker_thread.cc)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBEV})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

if (WORKER_THREAD_BUILD_TESTS)
    add_subdirectory(tests)
endif (WORKER_THREAD_BUILD_TESTS)

