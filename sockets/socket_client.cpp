#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>

constexpr const char* SOCKET_PATH = "/tmp/automotive_ipc_socket";

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

  int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client_fd < 0) {
    std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
    return 1;
  }

  struct sockaddr_un server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

  std::cout << "Connecting to server..." << std::endl;
  if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
      0) {
    std::cerr << "Failed to connect to server: " << strerror(errno)
              << std::endl;
    std::cerr << "Make sure the server is running first" << std::endl;
    close(client_fd);
    return 1;
  }

  std::cout << "Connected to server" << std::endl;
  std::cout << "Receiving vehicle data... (Press Ctrl+C to stop)" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  VehicleData vehicle_data;
  int packet_count = 0;

  while (running) {
    ssize_t received = recv(client_fd, &vehicle_data, sizeof(vehicle_data), 0);

    if (received < 0) {
      std::cerr << "\nError receiving data: " << strerror(errno) << std::endl;
      break;
    } else if (received == 0) {
      std::cout << "\nServer disconnected" << std::endl;
      break;
    }

    packet_count++;
    std::cout << "\r[Packet #" << std::setw(4) << packet_count << "] "
              << "Speed: " << std::fixed << std::setprecision(1) << std::setw(6)
              << vehicle_data.speed << " km/h | " << "RPM: " << std::setw(7)
              << static_cast<int>(vehicle_data.rpm) << " | "
              << "Fuel: " << std::setw(5) << std::setprecision(1)
              << vehicle_data.fuel_level << "% | "
              << "Gear: " << vehicle_data.gear << " | "
              << "Engine: " << (vehicle_data.engine_on ? "ON " : "OFF") << " | "
              << "TS: " << vehicle_data.timestamp << std::flush;
  }

  close(client_fd);
  std::cout << "\n\nClient stopped" << std::endl;

  return 0;
}
