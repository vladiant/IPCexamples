add_executable(anonymous_pipe anonymous_pipe.cpp)
target_link_libraries(anonymous_pipe PRIVATE Threads::Threads)

add_executable(named_pipe_writer named_pipe_writer.cpp)
target_link_libraries(named_pipe_writer PRIVATE Threads::Threads)

add_executable(named_pipe_reader named_pipe_reader.cpp)
target_link_libraries(named_pipe_reader PRIVATE Threads::Threads)
