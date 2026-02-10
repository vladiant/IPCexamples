#include <mqueue.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

constexpr const char* MQ_NAME = "/automotive_mq";
constexpr size_t MAX_MSG_SIZE = 256;
constexpr size_t MAX_MESSAGES = 10;

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

  mq_unlink(MQ_NAME);

  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = MAX_MESSAGES;
  attr.mq_msgsize = MAX_MSG_SIZE;
  attr.mq_curmsgs = 0;

  mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_WRONLY, 0666, &attr);
  if (mq == (mqd_t)-1) {
    std::cerr << "Failed to create message queue: " << strerror(errno)
              << std::endl;
    return 1;
  }

  std::cout << "Message queue sender started" << std::endl;
  std::cout << "Sending messages... (Press Ctrl+C to stop)" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  uint32_t sequence = 0;
  const MessageType types[] = {MessageType::STATUS, MessageType::DIAGNOSTIC,
                               MessageType::CONTROL, MessageType::ALERT};

  while (running) {
    Message msg;
    msg.type = types[sequence % 4];
    msg.sequence = sequence;
    msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    switch (msg.type) {
      case MessageType::STATUS:
        snprintf(msg.payload, sizeof(msg.payload),
                 "Vehicle status: Speed=%.1f km/h, Fuel=%.1f%%",
                 50.0f + (sequence % 50), 75.0f - (sequence % 50) * 0.5f);
        break;
      case MessageType::DIAGNOSTIC:
        snprintf(msg.payload, sizeof(msg.payload),
                 "Diagnostic code: DTC-%04d, Module: ECU-%d",
                 1000 + (sequence % 100), (sequence % 5) + 1);
        break;
      case MessageType::CONTROL:
        snprintf(msg.payload, sizeof(msg.payload),
                 "Control command: SET_MODE=%d, PARAM=%d", (sequence % 3),
                 sequence % 100);
        break;
      case MessageType::ALERT:
        snprintf(
            msg.payload, sizeof(msg.payload), "Alert: %s - Priority: %d",
            (sequence % 2 == 0) ? "Low fuel warning" : "Maintenance required",
            (sequence % 3) + 1);
        break;
    }

    unsigned int priority = static_cast<unsigned int>(msg.type);

    if (mq_send(mq, reinterpret_cast<const char*>(&msg), sizeof(msg),
                priority) < 0) {
      if (errno == EAGAIN) {
        std::cout << "[WARNING] Queue full, waiting..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      } else {
        std::cerr << "\nFailed to send message: " << strerror(errno)
                  << std::endl;
        break;
      }
    }

    std::cout << "[SEQ: " << msg.sequence << "] "
              << "Type: " << message_type_to_string(msg.type) << " | "
              << "Priority: " << priority << " | " << "Payload: " << msg.payload
              << std::endl;

    sequence++;
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
  }

  mq_close(mq);
  mq_unlink(MQ_NAME);

  std::cout << "\nSender stopped (sent " << sequence << " messages)"
            << std::endl;

  return 0;
}
