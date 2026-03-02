cmake_minimum_required(VERSION 3.15)
project(shared_memory_demo VERSION 1.0.0 LANGUAGES CXX)

find_package(Threads REQUIRED)
add_executable(shm_producer shm_producer.cpp)
target_link_libraries(shm_producer PRIVATE Threads::Threads rt)

add_executable(shm_consumer shm_consumer.cpp)
target_link_libraries(shm_consumer PRIVATE Threads::Threads rt)
