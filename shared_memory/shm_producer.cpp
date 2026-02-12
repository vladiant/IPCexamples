#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

constexpr const char* SHM_NAME = "/automotive_shm";
constexpr const char* SEM_WRITE_NAME = "/automotive_sem_write";
constexpr const char* SEM_READ_NAME = "/automotive_sem_read";

std::atomic<bool> running{true};

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    running = false;
  }
}

struct SensorData {
  float temperature;
  float pressure;
  float voltage;
  int error_code;
  uint64_t timestamp;
  uint32_t sequence_number;
  bool valid;
};

struct SharedMemory {
  SensorData data;
  bool producer_active;
};

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  shm_unlink(SHM_NAME);
  sem_unlink(SEM_WRITE_NAME);
  sem_unlink(SEM_READ_NAME);

  int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd < 0) {
    std::cerr << "Failed to create shared memory: " << strerror(errno)
              << std::endl;
    return 1;
  }

  if (ftruncate(shm_fd, sizeof(SharedMemory)) < 0) {
    std::cerr << "Failed to set shared memory size: " << strerror(errno)
              << std::endl;
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return 1;
  }

  SharedMemory* shared_mem = static_cast<SharedMemory*>(
      mmap(nullptr, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED,
           shm_fd, 0));

  if (shared_mem == MAP_FAILED) {
    std::cerr << "Failed to map shared memory: " << strerror(errno)
              << std::endl;
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return 1;
  }

  sem_t* sem_write = sem_open(SEM_WRITE_NAME, O_CREAT, 0666, 1);
  if (sem_write == SEM_FAILED) {
    std::cerr << "Failed to create write semaphore: " << strerror(errno)
              << std::endl;
    munmap(shared_mem, sizeof(SharedMemory));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return 1;
  }

  sem_t* sem_read = sem_open(SEM_READ_NAME, O_CREAT, 0666, 0);
  if (sem_read == SEM_FAILED) {
    std::cerr << "Failed to create read semaphore: " << strerror(errno)
              << std::endl;
    sem_close(sem_write);
    sem_unlink(SEM_WRITE_NAME);
    munmap(shared_mem, sizeof(SharedMemory));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return 1;
  }

  memset(shared_mem, 0, sizeof(SharedMemory));
  shared_mem->producer_active = true;

  std::cout << "Shared memory producer started" << std::endl;
  std::cout << "Writing sensor data... (Press Ctrl+C to stop)" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  uint32_t sequence = 0;
  float temp_base = 20.0f;

  while (running) {
    sem_wait(sem_write);

    shared_mem->data.temperature =
        temp_base +
        (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 10.0f;
    shared_mem->data.pressure =
        1.0f +
        (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.5f;
    shared_mem->data.voltage =
        12.0f +
        (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f;
    shared_mem->data.error_code = (rand() % 100 < 5) ? (rand() % 10) : 0;
    shared_mem->data.timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    shared_mem->data.sequence_number = sequence++;
    shared_mem->data.valid = true;

    std::cout << "[SEQ: " << shared_mem->data.sequence_number << "] "
              << "Temp: " << shared_mem->data.temperature << "°C | "
              << "Pressure: " << shared_mem->data.pressure << " bar | "
              << "Voltage: " << shared_mem->data.voltage << "V | "
              << "Error: " << shared_mem->data.error_code << std::endl;

    sem_post(sem_read);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  shared_mem->producer_active = false;
  sem_post(sem_read);

  sem_close(sem_write);
  sem_close(sem_read);
  sem_unlink(SEM_WRITE_NAME);
  sem_unlink(SEM_READ_NAME);
  munmap(shared_mem, sizeof(SharedMemory));
  close(shm_fd);
  shm_unlink(SHM_NAME);

  std::cout << "\nProducer stopped" << std::endl;

  return 0;
}
