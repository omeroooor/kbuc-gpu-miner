#pragma once
#include <cuda_runtime.h>
#include <cstdint>

struct MiningHeader {
    // First var slice (32 bytes + 1 byte length)
    uint8_t hash_length;  // 32
    uint8_t hash[32];     // 073ff90209cde3a9ce4ccd23598fd1b50e6a1fe34f30bd7240587f0bde6f65af

    // Second var slice (20 bytes + 1 byte length)
    uint8_t address1_length;  // 20
    uint8_t address1[20];     // 00e72f85c49171825a87a84142bdf02022a75479

    // Fixed uint32
    uint32_t value;          // 3768

    // Third var slice (20 bytes + 1 byte length)
    uint8_t address2_length;  // 20
    uint8_t address2[20];     // 2ecaa57da5fcc9d45bc2d512eeb2b3e98e0393b7

    // Fixed fields
    uint8_t flag;           // 0
    uint32_t timestamp;     // 1737835291
    uint32_t nonce;         // Starting from 13100
};

// Target is stored as 8 uint32_t values for 256-bit comparison
struct Target {
    uint32_t words[8];
};

// Byte swap function declaration
__device__ __host__ __inline__ uint32_t swap32(uint32_t val);

// Parse target hash string into Target structure
__host__ Target parse_target_hash(const char* target_str);

// Convert compact target format to actual target
__host__ Target decode_compact_target(uint32_t compact);

// Save current mining state to a file
bool save_mining_state(const char* filename, const MiningHeader* header, const Target* target);

// Load mining state from a file
bool load_mining_state(const char* filename, MiningHeader* header, Target* target);

// Hex string to bytes conversion utility
__host__ bool hex_to_bytes(const char* hex_str, uint8_t* bytes, size_t len);

// GPU mining functions
__global__ void sha256_gpu(MiningHeader* input, uint8_t* output, Target target, uint32_t* found);
bool mine_block(MiningHeader* header, Target target, float time_limit = 60.0f);
