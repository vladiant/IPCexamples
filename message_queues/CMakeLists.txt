cmake_minimum_required(VERSION 3.15)
project(message_queue VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(mq_sender mq_sender.cpp)
target_link_libraries(mq_sender PRIVATE Threads::Threads rt)

add_executable(mq_receiver mq_receiver.cpp)
target_link_libraries(mq_receiver PRIVATE Threads::Threads rt)
