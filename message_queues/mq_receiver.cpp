#include <mqueue.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>

constexpr const char* MQ_NAME = "/automotive_mq";
constexpr size_t MAX_MSG_SIZE = 256;

std::atomic<bool> running{true};

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    running = false;
  }
}

enum class MessageType : uint8_t {
  DIAGNOSTIC = 1,
  CONTROL = 2,
  STATUS = 3,
  ALERT = 4
};

struct Message {
  MessageType type;
  uint32_t sequence;
  uint64_t timestamp;
  char payload[200];
};

const char* message_type_to_string(MessageType type) {
  switch (type) {
    case MessageType::DIAGNOSTIC:
      return "DIAGNOSTIC";
    case MessageType::CONTROL:
      return "CONTROL";
    case MessageType::STATUS:
      return "STATUS";
    case MessageType::ALERT:
      return "ALERT";
    default:
      return "UNKNOWN";
  }
}

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::cout << "Waiting for message queue to be created..." << std::endl;

  mqd_t mq = (mqd_t)-1;
  for (int i = 0; i < 10 && running; ++i) {
    mq = mq_open(MQ_NAME, O_RDONLY | O_NONBLOCK);
    if (mq != (mqd_t)-1) break;
    sleep(1);
  }

  if (mq == (mqd_t)-1) {
    std::cerr << "Failed to open message queue: " << strerror(errno)
              << std::endl;
    std::cerr << "Make sure the sender is running first" << std::endl;
    return 1;
  }

  struct mq_attr attr;
  if (mq_getattr(mq, &attr) < 0) {
    std::cerr << "Failed to get queue attributes: " << strerror(errno)
              << std::endl;
    mq_close(mq);
    return 1;
  }

  std::cout << "Connected to message queue" << std::endl;
  std::cout << "Queue info: max_msgs=" << attr.mq_maxmsg
            << ", max_msgsize=" << attr.mq_msgsize << std::endl;
  std::cout << "Receiving messages... (Press Ctrl+C to stop)" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  char buffer[MAX_MSG_SIZE];
  uint32_t last_sequence = 0;
  int messages_received = 0;
  bool first_message = true;

  while (running) {
    unsigned int priority;
    ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, &priority);

    if (bytes_read < 0) {
      if (errno == EAGAIN) {
        usleep(100000);
        continue;
      } else if (errno == EINTR) {
        continue;
      } else {
        std::cerr << "\nError receiving message: " << strerror(errno)
                  << std::endl;
        break;
      }
    }

    if (bytes_read < static_cast<ssize_t>(sizeof(Message))) {
      std::cout << "\n[WARNING] Received incomplete message" << std::endl;
      continue;
    }

    Message* msg = reinterpret_cast<Message*>(buffer);
    messages_received++;

    if (!first_message && msg->sequence != last_sequence + 1) {
      std::cout << "\n[WARNING] Missed messages! Expected: "
                << (last_sequence + 1) << ", Got: " << msg->sequence
                << std::endl;
    }
    first_message = false;
    last_sequence = msg->sequence;

    std::cout << "[SEQ: " << std::setw(5) << msg->sequence << "] "
              << "Type: " << std::setw(11) << message_type_to_string(msg->type)
              << " | " << "Priority: " << priority << " | "
              << "Payload: " << msg->payload;

    if (msg->type == MessageType::ALERT) {
      std::cout << " [!]";
    }

    std::cout << std::endl;
  }

  mq_close(mq);

  std::cout << "\nReceiver stopped (received " << messages_received
            << " messages)" << std::endl;

  return 0;
}
