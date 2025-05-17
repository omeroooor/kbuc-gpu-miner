# Kbunet GPU Miner

A high-performance Bitcoin mining application with both CLI and GUI interfaces. This miner implements the SHA-256 hashing algorithm on CUDA-enabled GPUs for maximum mining efficiency, along with a comprehensive user interface for easy management of mining operations.

## Architecture

The application consists of several key components:

1. **Core Mining Engine**:
   - CUDA-based implementation of the SHA-256 algorithm for GPU acceleration
   - Optimized parallel nonce searching
   - HashWriter for CPU-side verification
   - Kbunet support ticket structure implementation

2. **Service Layer**:
   - gRPC service for managing mining sessions
   - Kbunet RPC integration for block submission
   - Background worker thread for mining task management

3. **User Interface**:
   - Qt5-based GUI with intuitive controls
   - Real-time mining statistics and status updates
   - Configuration management for mining parameters
   - Mining history tracking

## Prerequisites

- CUDA Toolkit (11.0 or later)
- CMake (3.18 or later)
- C++17 compatible compiler
- OpenSSL development libraries
- Qt5 development libraries (for GUI version)
- gRPC and Protocol Buffers

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

### CLI Version

```bash
./miner
```

### GUI Version

```bash
./miner_ui
```

## Configuration

The miner can be configured through the GUI settings dialog or by editing the `miner_config.json` file with the following options:

- RPC connection settings (host, port, username, password)
- Reward Bitcoin address
- Auto-broadcast setting for found blocks
- Mining difficulty parameters
- GPU selection and thread configuration

## Features

- GPU-accelerated SHA-256 mining with CUDA
- Dual interface: command-line and graphical user interface
- Real-time mining statistics and status updates
- Mining session management (start/pause/resume/stop)
- Mining history tracking
- Configurable mining parameters
- Kbunet RPC integration for automatic block submission

## Implementation Details

The miner implements:
- Kbunet's support ticket structure
- SHA-256 compression function on GPU
- Parallel nonce searching
- HashWriter for CPU-side verification
- Modular architecture with separate miner_lib library

## Performance Notes

The current implementation uses:
- 256 threads per block
- 65536 blocks
- This gives us about 16.7 million nonce attempts per kernel launch

## License

MIT License - See LICENSE file for details
