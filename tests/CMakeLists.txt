cmake_minimum_required(VERSION 3.10)
project(worker_thread_tests)

add_executable(wtest test.cc)
target_link_libraries(wtest worker_thread)

