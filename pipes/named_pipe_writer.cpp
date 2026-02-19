#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

constexpr const char* FIFO_PATH = "/tmp/automotive_fifo";

std::atomic<bool> running{true};

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    running = false;
  }
}

struct DiagnosticEvent {
  uint32_t dtc_code;
  uint8_t severity;
  char module_name[32];
  char description[128];
  uint64_t timestamp;
};

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  unlink(FIFO_PATH);

  if (mkfifo(FIFO_PATH, 0666) < 0) {
    std::cerr << "Failed to create FIFO: " << strerror(errno) << std::endl;
    return 1;
  }

  std::cout << "Named Pipe Writer - Diagnostic Event Publisher" << std::endl;
  std::cout << "FIFO created at: " << FIFO_PATH << std::endl;
  std::cout << "Waiting for reader to connect..." << std::endl;

  int fd = open(FIFO_PATH, O_WRONLY);
  if (fd < 0) {
    std::cerr << "Failed to open FIFO: " << strerror(errno) << std::endl;
    unlink(FIFO_PATH);
    return 1;
  }

  std::cout << "Reader connected. Sending diagnostic events..." << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  const char* modules[] = {"ECU", "TCU", "ABS", "BCM", "ADAS"};
  const char* descriptions[] = {"Sensor malfunction detected",
                                "Communication timeout", "Voltage out of range",
                                "Temperature threshold exceeded",
                                "Calibration data invalid"};

  uint32_t event_count = 0;

  while (running) {
    DiagnosticEvent event;
    event.dtc_code = 0x0100 + (event_count % 500);
    event.severity = (event_count % 3) + 1;

    int module_idx = event_count % 5;
    strncpy(event.module_name, modules[module_idx],
            sizeof(event.module_name) - 1);
    event.module_name[sizeof(event.module_name) - 1] = '\0';

    int desc_idx = event_count % 5;
    strncpy(event.description, descriptions[desc_idx],
            sizeof(event.description) - 1);
    event.description[sizeof(event.description) - 1] = '\0';

    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

    ssize_t written = write(fd, &event, sizeof(event));
    if (written < 0) {
      if (errno == EPIPE) {
        std::cout << "\nReader disconnected" << std::endl;
        break;
      }
      std::cerr << "\nWrite error: " << strerror(errno) << std::endl;
      break;
    }

    std::cout << "[Event #" << event_count << "] " << "DTC: 0x" << std::hex
              << event.dtc_code << std::dec << " | "
              << "Module: " << event.module_name << " | "
              << "Severity: " << static_cast<int>(event.severity) << " | "
              << "Desc: " << event.description << std::endl;

    event_count++;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  close(fd);
  unlink(FIFO_PATH);

  std::cout << "\nWriter stopped (sent " << event_count << " events)"
            << std::endl;

  return 0;
}
