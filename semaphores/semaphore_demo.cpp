#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

constexpr const char* SEM_RESOURCE = "/automotive_resource_sem";
constexpr const char* SEM_MUTEX = "/automotive_mutex_sem";
constexpr const char* SEM_BARRIER = "/automotive_barrier_sem";
constexpr int NUM_WORKERS = 3;

std::atomic<bool> running{true};

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    running = false;
  }
}

void worker_process(int worker_id) {
  std::cout << "[Worker " << worker_id << "] Started (PID: " << getpid() << ")"
            << std::endl;

  sem_t* sem_resource = sem_open(SEM_RESOURCE, 0);
  sem_t* sem_mutex = sem_open(SEM_MUTEX, 0);
  sem_t* sem_barrier = sem_open(SEM_BARRIER, 0);

  if (sem_resource == SEM_FAILED || sem_mutex == SEM_FAILED ||
      sem_barrier == SEM_FAILED) {
    std::cerr << "[Worker " << worker_id << "] Failed to open semaphores"
              << std::endl;
    return;
  }

  std::cout << "[Worker " << worker_id << "] Waiting at barrier..."
            << std::endl;
  sem_wait(sem_barrier);
  std::cout << "[Worker " << worker_id << "] Passed barrier, starting work"
            << std::endl;

  for (int task = 0; task < 5 && running; ++task) {
    std::cout << "[Worker " << worker_id << "] Task " << task
              << " - Waiting for resource..." << std::endl;

    sem_wait(sem_resource);

    std::cout << "[Worker " << worker_id << "] Task " << task
              << " - Acquired resource, processing..." << std::endl;

    sem_wait(sem_mutex);
    std::cout << "[Worker " << worker_id << "] Task " << task
              << " - Critical section: Reading sensor data" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "[Worker " << worker_id << "] Task " << task
              << " - Critical section: Writing to log" << std::endl;
    sem_post(sem_mutex);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[Worker " << worker_id << "] Task " << task
              << " - Releasing resource" << std::endl;
    sem_post(sem_resource);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  sem_close(sem_resource);
  sem_close(sem_mutex);
  sem_close(sem_barrier);

  std::cout << "[Worker " << worker_id << "] Completed all tasks" << std::endl;
}

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::cout << "Semaphore Demo - Automotive Resource Management" << std::endl;
  std::cout << "Main process PID: " << getpid() << std::endl;
  std::cout << std::string(80, '=') << std::endl;

  sem_unlink(SEM_RESOURCE);
  sem_unlink(SEM_MUTEX);
  sem_unlink(SEM_BARRIER);

  sem_t* sem_resource = sem_open(SEM_RESOURCE, O_CREAT | O_EXCL, 0666, 2);
  if (sem_resource == SEM_FAILED) {
    std::cerr << "Failed to create resource semaphore: " << strerror(errno)
              << std::endl;
    return 1;
  }

  sem_t* sem_mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0666, 1);
  if (sem_mutex == SEM_FAILED) {
    std::cerr << "Failed to create mutex semaphore: " << strerror(errno)
              << std::endl;
    sem_close(sem_resource);
    sem_unlink(SEM_RESOURCE);
    return 1;
  }

  sem_t* sem_barrier = sem_open(SEM_BARRIER, O_CREAT | O_EXCL, 0666, 0);
  if (sem_barrier == SEM_FAILED) {
    std::cerr << "Failed to create barrier semaphore: " << strerror(errno)
              << std::endl;
    sem_close(sem_resource);
    sem_close(sem_mutex);
    sem_unlink(SEM_RESOURCE);
    sem_unlink(SEM_MUTEX);
    return 1;
  }

  std::cout << "Semaphores created:" << std::endl;
  std::cout << "  - Resource semaphore (initial value: 2) - Limits concurrent "
               "resource access"
            << std::endl;
  std::cout
      << "  - Mutex semaphore (initial value: 1) - Protects critical section"
      << std::endl;
  std::cout
      << "  - Barrier semaphore (initial value: 0) - Synchronizes worker start"
      << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  pid_t worker_pids[NUM_WORKERS];

  for (int i = 0; i < NUM_WORKERS; ++i) {
    pid_t pid = fork();

    if (pid < 0) {
      std::cerr << "Failed to fork worker " << i << ": " << strerror(errno)
                << std::endl;
      for (int j = 0; j < i; ++j) {
        kill(worker_pids[j], SIGTERM);
      }
      return 1;
    }

    if (pid == 0) {
      worker_process(i + 1);
      return 0;
    }

    worker_pids[i] = pid;
  }

  std::cout << "[Main] All workers spawned, waiting 2 seconds..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "[Main] Releasing barrier - all workers can now proceed"
            << std::endl;
  for (int i = 0; i < NUM_WORKERS; ++i) {
    sem_post(sem_barrier);
  }

  std::cout << "[Main] Monitoring worker progress..." << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  for (int i = 0; i < NUM_WORKERS; ++i) {
    int status;
    pid_t pid = waitpid(worker_pids[i], &status, 0);
    if (pid > 0) {
      std::cout << "[Main] Worker " << (i + 1) << " (PID: " << pid
                << ") finished with status: " << WEXITSTATUS(status)
                << std::endl;
    }
  }

  sem_close(sem_resource);
  sem_close(sem_mutex);
  sem_close(sem_barrier);
  sem_unlink(SEM_RESOURCE);
  sem_unlink(SEM_MUTEX);
  sem_unlink(SEM_BARRIER);

  std::cout << std::string(80, '=') << std::endl;
  std::cout << "[Main] All workers completed. Semaphores cleaned up."
            << std::endl;
  std::cout << "\nDemonstrated concepts:" << std::endl;
  std::cout << "  1. Resource limiting (max 2 concurrent accesses)"
            << std::endl;
  std::cout << "  2. Mutual exclusion (critical section protection)"
            << std::endl;
  std::cout << "  3. Barrier synchronization (coordinated start)" << std::endl;

  return 0;
}
