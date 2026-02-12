add_executable(shm_producer shm_producer.cpp)
target_link_libraries(shm_producer PRIVATE Threads::Threads rt)

add_executable(shm_consumer shm_consumer.cpp)
target_link_libraries(shm_consumer PRIVATE Threads::Threads rt)
