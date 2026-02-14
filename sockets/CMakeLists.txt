add_executable(socket_server socket_server.cpp)
target_link_libraries(socket_server PRIVATE Threads::Threads)

add_executable(socket_client socket_client.cpp)
target_link_libraries(socket_client PRIVATE Threads::Threads)
