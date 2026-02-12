#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>

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

  std::cout << "Waiting for producer to start..." << std::endl;

  int shm_fd = -1;
  for (int i = 0; i < 10 && running; ++i) {
    shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd >= 0) break;
    sleep(1);
  }

  if (shm_fd < 0) {
    std::cerr << "Failed to open shared memory: " << strerror(errno)
              << std::endl;
    std::cerr << "Make sure the producer is running first" << std::endl;
    return 1;
  }

  SharedMemory* shared_mem = static_cast<SharedMemory*>(
      mmap(nullptr, sizeof(SharedMemory), PROT_READ, MAP_SHARED, shm_fd, 0));

  if (shared_mem == MAP_FAILED) {
    std::cerr << "Failed to map shared memory: " << strerror(errno)
              << std::endl;
    close(shm_fd);
    return 1;
  }

  sem_t* sem_write = sem_open(SEM_WRITE_NAME, 0);
  if (sem_write == SEM_FAILED) {
    std::cerr << "Failed to open write semaphore: " << strerror(errno)
              << std::endl;
    munmap(shared_mem, sizeof(SharedMemory));
    close(shm_fd);
    return 1;
  }

  sem_t* sem_read = sem_open(SEM_READ_NAME, 0);
  if (sem_read == SEM_FAILED) {
    std::cerr << "Failed to open read semaphore: " << strerror(errno)
              << std::endl;
    sem_close(sem_write);
    munmap(shared_mem, sizeof(SharedMemory));
    close(shm_fd);
    return 1;
  }

  std::cout << "Connected to shared memory" << std::endl;
  std::cout << "Reading sensor data... (Press Ctrl+C to stop)" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  uint32_t last_sequence = 0;
  int packets_received = 0;

  while (running) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 2;

    if (sem_timedwait(sem_read, &timeout) < 0) {
      if (errno == ETIMEDOUT) {
        if (!shared_mem->producer_active) {
          std::cout << "\nProducer has stopped" << std::endl;
          break;
        }
        continue;
      } else if (errno == EINTR) {
        continue;
      } else {
        std::cerr << "\nSemaphore wait error: " << strerror(errno) << std::endl;
        break;
      }
    }

    if (!shared_mem->producer_active) {
      std::cout << "\nProducer has stopped" << std::endl;
      break;
    }

    SensorData data = shared_mem->data;

    sem_post(sem_write);

    if (data.valid) {
      packets_received++;

      if (data.sequence_number != last_sequence + 1 && last_sequence != 0) {
        std::cout << "\n[WARNING] Missed packets! Expected: "
                  << (last_sequence + 1) << ", Got: " << data.sequence_number
                  << std::endl;
      }
      last_sequence = data.sequence_number;

      std::cout << "[SEQ: " << std::setw(5) << data.sequence_number << "] "
                << "Temp: " << std::fixed << std::setprecision(2)
                << std::setw(6) << data.temperature << "°C | "
                << "Pressure: " << std::setw(5) << data.pressure << " bar | "
                << "Voltage: " << std::setw(5) << data.voltage << "V | "
                << "Error: " << std::setw(2) << data.error_code;

      if (data.error_code != 0) {
        std::cout << " [ERROR!]";
      }

      std::cout << std::endl;
    }
  }

  sem_close(sem_write);
  sem_close(sem_read);
  munmap(shared_mem, sizeof(SharedMemory));
  close(shm_fd);

  std::cout << "\nConsumer stopped (received " << packets_received
            << " packets)" << std::endl;

  return 0;
}
