#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>

struct CANMessage {
  uint32_t can_id;
  uint8_t data_length;
  uint8_t data[8];
  uint64_t timestamp;
};

void parent_process(int write_fd, int read_fd) {
  close(read_fd);

  std::cout << "[Parent] ECU Simulator - Sending CAN messages to child process"
            << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  for (int i = 0; i < 10; ++i) {
    CANMessage msg;
    msg.can_id = 0x100 + i;
    msg.data_length = 8;

    for (int j = 0; j < 8; ++j) {
      msg.data[j] = static_cast<uint8_t>((i * 10 + j) % 256);
    }

    msg.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    ssize_t written = write(write_fd, &msg, sizeof(msg));
    if (written < 0) {
      std::cerr << "[Parent] Write error: " << strerror(errno) << std::endl;
      break;
    }

    std::cout << "[Parent] Sent CAN ID: 0x" << std::hex << std::setw(3)
              << std::setfill('0') << msg.can_id << std::dec << " | Data: ";
    for (int j = 0; j < msg.data_length; ++j) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(msg.data[j]) << " ";
    }
    std::cout << std::dec << "| TS: " << msg.timestamp << std::endl;

    usleep(500000);
  }

  close(write_fd);
  std::cout << "[Parent] Finished sending messages" << std::endl;
}

void child_process(int write_fd, int read_fd) {
  close(write_fd);

  std::cout << "[Child] Gateway Process - Receiving CAN messages from parent"
            << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  CANMessage msg;
  int count = 0;

  while (true) {
    ssize_t bytes_read = read(read_fd, &msg, sizeof(msg));

    if (bytes_read == 0) {
      std::cout << "[Child] Parent closed pipe, exiting" << std::endl;
      break;
    }

    if (bytes_read < 0) {
      std::cerr << "[Child] Read error: " << strerror(errno) << std::endl;
      break;
    }

    if (bytes_read != sizeof(msg)) {
      std::cerr << "[Child] Incomplete message received" << std::endl;
      continue;
    }

    count++;
    std::cout << "[Child] Received CAN ID: 0x" << std::hex << std::setw(3)
              << std::setfill('0') << msg.can_id << std::dec << " | Data: ";
    for (int j = 0; j < msg.data_length; ++j) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(msg.data[j]) << " ";
    }
    std::cout << std::dec << "| TS: " << msg.timestamp << std::endl;

    uint32_t checksum = 0;
    for (int j = 0; j < msg.data_length; ++j) {
      checksum += msg.data[j];
    }
    std::cout << "[Child] Processed message #" << count
              << " | Checksum: " << checksum << std::endl;
  }

  close(read_fd);
  std::cout << "[Child] Total messages received: " << count << std::endl;
}

int main() {
  int pipefd[2];

  if (pipe(pipefd) < 0) {
    std::cerr << "Failed to create pipe: " << strerror(errno) << std::endl;
    return 1;
  }

  std::cout << "Anonymous Pipe Example - Parent/Child CAN Communication"
            << std::endl;
  std::cout << "Pipe created: read_fd=" << pipefd[0]
            << ", write_fd=" << pipefd[1] << std::endl;
  std::cout << std::string(80, '=') << std::endl;

  pid_t pid = fork();

  if (pid < 0) {
    std::cerr << "Fork failed: " << strerror(errno) << std::endl;
    close(pipefd[0]);
    close(pipefd[1]);
    return 1;
  }

  if (pid == 0) {
    child_process(pipefd[1], pipefd[0]);
    return 0;
  } else {
    parent_process(pipefd[1], pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Parent process completed. Child exit status: "
              << WEXITSTATUS(status) << std::endl;
  }

  return 0;
}
