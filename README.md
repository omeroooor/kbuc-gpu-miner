# Bitcoin GPU Miner

This is a CUDA-based Bitcoin miner that implements the SHA-256 hashing algorithm on the GPU. It uses Bitcoin's official HashWriter approach for data serialization and hashing.

## Prerequisites

- CUDA Toolkit (11.0 or later)
- CMake (3.18 or later)
- C++17 compatible compiler
- OpenSSL development libraries

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

After building, you can run the miner with:

```bash
./miner
```

## Features

- GPU-accelerated SHA-256 mining
- Bitcoin block header structure implementation
- Double SHA-256 hashing as per Bitcoin protocol
- Configurable mining difficulty
- Real-time mining status updates

## Implementation Details

The miner implements:
- Bitcoin's block header structure
- SHA-256 compression function on GPU
- Parallel nonce searching
- HashWriter for CPU-side verification

## Performance Notes

The current implementation uses:
- 256 threads per block
- 65536 blocks
- This gives us about 16.7 million nonce attempts per kernel launch

## License

MIT License - See LICENSE file for details
