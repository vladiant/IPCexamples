#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>

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

const char* severity_to_string(uint8_t severity) {
  switch (severity) {
    case 1:
      return "LOW";
    case 2:
      return "MEDIUM";
    case 3:
      return "HIGH";
    default:
      return "UNKNOWN";
  }
}

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::cout << "Named Pipe Reader - Diagnostic Event Subscriber" << std::endl;
  std::cout << "Waiting for FIFO at: " << FIFO_PATH << std::endl;

  for (int i = 0; i < 10 && running; ++i) {
    struct stat st;
    if (stat(FIFO_PATH, &st) == 0) {
      break;
    }
    sleep(1);
  }

  std::cout << "Opening FIFO..." << std::endl;
  int fd = open(FIFO_PATH, O_RDONLY);
  if (fd < 0) {
    std::cerr << "Failed to open FIFO: " << strerror(errno) << std::endl;
    std::cerr << "Make sure the writer is running first" << std::endl;
    return 1;
  }

  std::cout << "Connected to writer. Receiving diagnostic events..."
            << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  DiagnosticEvent event;
  uint32_t event_count = 0;
  uint32_t high_severity_count = 0;

  while (running) {
    ssize_t bytes_read = read(fd, &event, sizeof(event));

    if (bytes_read == 0) {
      std::cout << "\nWriter closed pipe" << std::endl;
      break;
    }

    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      std::cerr << "\nRead error: " << strerror(errno) << std::endl;
      break;
    }

    if (bytes_read != sizeof(event)) {
      std::cerr << "\nIncomplete event received" << std::endl;
      continue;
    }

    event_count++;
    if (event.severity == 3) {
      high_severity_count++;
    }

    std::cout << "[Event #" << std::setw(4) << event_count << "] " << "DTC: 0x"
              << std::hex << std::setw(4) << std::setfill('0') << event.dtc_code
              << std::dec << " | " << "Module: " << std::setw(8)
              << event.module_name << " | " << "Severity: " << std::setw(6)
              << severity_to_string(event.severity) << " | "
              << "Desc: " << event.description;

    if (event.severity == 3) {
      std::cout << " [CRITICAL!]";
    }

    std::cout << std::endl;

    if (event.severity == 3) {
      std::cout
          << "         >> Logging high-severity event to persistent storage"
          << std::endl;
    }
  }

  close(fd);

  std::cout << "\nReader stopped" << std::endl;
  std::cout << "Total events received: " << event_count << std::endl;
  std::cout << "High-severity events: " << high_severity_count << std::endl;

  return 0;
}
