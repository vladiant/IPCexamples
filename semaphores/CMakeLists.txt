cmake_minimum_required(VERSION 3.15)
project(semaphore_demo VERSION 1.0.0 LANGUAGES CXX)

find_package(Threads REQUIRED)

add_executable(semaphore_demo semaphore_demo.cpp)
target_link_libraries(semaphore_demo PRIVATE Threads::Threads rt)
