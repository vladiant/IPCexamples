#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

constexpr const char* SOCKET_PATH = "/tmp/automotive_ipc_socket";
constexpr size_t BUFFER_SIZE = 1024;

std::atomic<bool> running{true};

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    running = false;
  }
}

struct VehicleData {
  float speed;
  float rpm;
  float fuel_level;
  int gear;
  bool engine_on;
  uint64_t timestamp;
};

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
    return 1;
  }

  unlink(SOCKET_PATH);

  struct sockaddr_un server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

  if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
      0) {
    std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 5) < 0) {
    std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
    close(server_fd);
    unlink(SOCKET_PATH);
    return 1;
  }

  std::cout << "Socket server listening on " << SOCKET_PATH << std::endl;
  std::cout << "Press Ctrl+C to stop the server" << std::endl;

  while (running) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int activity = select(server_fd + 1, &read_fds, nullptr, nullptr, &timeout);

    if (activity < 0 && errno != EINTR) {
      std::cerr << "Select error: " << strerror(errno) << std::endl;
      break;
    }

    if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
      int client_fd = accept(server_fd, nullptr, nullptr);
      if (client_fd < 0) {
        if (errno != EINTR) {
          std::cerr << "Failed to accept connection: " << strerror(errno)
                    << std::endl;
        }
        continue;
      }

      std::cout << "Client connected" << std::endl;

      VehicleData vehicle_data;
      vehicle_data.speed = 0.0f;
      vehicle_data.rpm = 800.0f;
      vehicle_data.fuel_level = 75.0f;
      vehicle_data.gear = 0;
      vehicle_data.engine_on = true;

      while (running) {
        vehicle_data.speed += 5.0f;
        if (vehicle_data.speed > 120.0f) vehicle_data.speed = 0.0f;

        vehicle_data.rpm = 800.0f + (vehicle_data.speed * 30.0f);
        vehicle_data.fuel_level -= 0.1f;
        if (vehicle_data.fuel_level < 0.0f) vehicle_data.fuel_level = 100.0f;

        vehicle_data.gear = static_cast<int>(vehicle_data.speed / 20.0f);
        vehicle_data.timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        ssize_t sent =
            send(client_fd, &vehicle_data, sizeof(vehicle_data), MSG_NOSIGNAL);
        if (sent < 0) {
          std::cout << "Client disconnected" << std::endl;
          break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }

      close(client_fd);
    }
  }

  close(server_fd);
  unlink(SOCKET_PATH);
  std::cout << "\nServer stopped" << std::endl;

  return 0;
}
