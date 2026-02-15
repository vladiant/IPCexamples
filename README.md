# Inter Process Communication examples

## Pipes (Anonymous/Named)

## Shared Memory
- **Producer**: Generates sensor data (temperature, pressure, voltage, error codes)
- **Consumer**: Reads sensor data from shared memory
- Synchronized access using POSIX semaphores
- Sequence number tracking to detect missed packets
- Automatic cleanup on shutdown

## Message Queues
The sender transmits prioritized messages, and the receiver processes them in priority order.
- **Sender**: Sends prioritized automotive messages (diagnostic, control, status, alerts)
- **Receiver**: Receives messages with priority ordering
- Non-blocking operations with timeout handling
- Multiple message types with different priorities
- Queue overflow protection

## Sockets (Unix Domain Sockets)
- **Server**: Simulates vehicle data streaming (speed, RPM, fuel level, gear, engine status)
- **Client**: Receives and displays real-time vehicle data
- Connection-oriented communication with automatic reconnection handling
- Graceful shutdown with signal handling

## Signals

## Semaphores & Mutexes

## Remote Procedure Call (RPC)

## File Mapping/Shared Files

## References
* <https://github.com/shake0/IPC-demo>
