#include "miner.cuh"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <conio.h>  // For _kbhit() and _getch_
#include <errno.h>  // For errno and strerror

// Byte swap functions
__device__ __host__ __inline__ uint32_t swap32(uint32_t val) {
    return ((val & 0x000000ff) << 24) |
           ((val & 0x0000ff00) << 8)  |
           ((val & 0x00ff0000) >> 8)  |
           ((val & 0xff000000) >> 24);
}

// Parse target hash string into Target structure
__host__ Target parse_target_hash(const char* target_str) {
    Target target;
    
    // Convert hex string to bytes, 8 words of 4 bytes each
    for (int i = 0; i < 8; i++) {
        char word[9];  // 8 chars + null terminator
        strncpy(word, target_str + (i * 8), 8);
        word[8] = '\0';
        
        // Convert hex string to uint32_t and store in big-endian format
        uint32_t value;
        sscanf(word, "%x", &value);
        target.words[i] = value;  // Keep in big-endian for comparison
    }
    
    return target;
}

// Convert compact target format to actual target
__host__ Target decode_compact_target(uint32_t compact) {
    Target target = {0};
    int exp = compact >> 24;
    uint32_t mant = compact & 0x007fffff;
    
    // Add the implicit "1" bit
    if (mant > 0) {
        mant |= 0x00800000;
    }
    
    // For exp=0x1d and mant=0x00ffff:
    // Target should be: 0x00ffff0000000000000000000000000000000000000000000000000000000000
    int shift = 8 * (exp - 3);
    int word_idx = shift / 32;
    int bit_shift = shift % 32;
    
    // Place the mantissa in the correct word, keeping big-endian format
    if (bit_shift == 0) {
        target.words[word_idx] = mant;
    } else {
        target.words[word_idx] = mant << bit_shift;
        if (word_idx < 7) {  // Don't overflow array
            target.words[word_idx + 1] = mant >> (32 - bit_shift);
        }
    }
    
    return target;
}

// SHA-256 Constants
__device__ __constant__ uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// SHA-256 initial state constants
__device__ const uint32_t sha256_init_state[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

// SHA-256 functions
__device__ uint32_t rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

__device__ uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

__device__ uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

__device__ uint32_t sigma0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

__device__ uint32_t sigma1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

__device__ uint32_t gamma0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

__device__ uint32_t gamma1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

__device__ void sha256_transform(uint32_t* state, const uint32_t* block) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;

    // Copy block into first 16 words of w - data is already in big-endian
    for (int i = 0; i < 16; i++) {
        w[i] = block[i];
    }

    // Extend the first 16 words into the remaining 48 words w[16..63]
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    // Initialize working variables
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    // Main loop
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        t1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    // Add the compressed chunk to the current hash value
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

__global__ void sha256_gpu(MiningHeader* header, uint8_t* output, Target target, uint32_t* found) {
    uint32_t tid = blockDim.x * blockIdx.x + threadIdx.x;
    
    // Create a local copy of the header and update nonce
    MiningHeader local_header = *header;
    local_header.nonce = header->nonce + tid;

    // First SHA-256 hash
    uint32_t state[8];
    uint8_t bytes[64] = {0};  // Changed to byte array
    memcpy(state, sha256_init_state, sizeof(state));
    
    // if (tid == 0) {
    //     printf("\nAttempting new batch with base nonce: %08x\n", header->nonce);
    //     printf("Thread 0 nonce: %08x\n", local_header.nonce);
    // }
    
    // Build block data in big-endian format for SHA-256
    uint32_t pos = 0;
    
    // Hash (variable length) with length prefix
    bytes[pos++] = local_header.hash_length;  // Variable length prefix
    for (int i = 0; i < local_header.hash_length; i++) {
        bytes[pos++] = local_header.hash[i];
    }
    
    // Address1 (20 bytes) with length prefix
    bytes[pos++] = 0x14;  // Length prefix (20)
    for (int i = 0; i < 20; i++) {
        bytes[pos++] = local_header.address1[i];
    }
    
    // Value (block height) - 4 bytes little-endian
    bytes[pos++] = local_header.value & 0xFF;           // Lowest byte
    bytes[pos++] = (local_header.value >> 8) & 0xFF;    // Low byte
    bytes[pos++] = (local_header.value >> 16) & 0xFF;   // High byte
    bytes[pos++] = (local_header.value >> 24) & 0xFF;   // Highest byte
    
    // Address2 (20 bytes) with length prefix
    bytes[pos++] = 0x14;  // Length prefix (20)
    for (int i = 0; i < 20; i++) {
        bytes[pos++] = local_header.address2[i];
    }
    
    // Flag byte - should be 0 or 1
    bytes[pos++] = local_header.flag;
    
    // Timestamp (4 bytes in little-endian)
    bytes[pos++] = local_header.timestamp & 0xFF;
    bytes[pos++] = (local_header.timestamp >> 8) & 0xFF;
    bytes[pos++] = (local_header.timestamp >> 16) & 0xFF;
    bytes[pos++] = (local_header.timestamp >> 24) & 0xFF;
    
    // Nonce (4 bytes in little-endian)
    bytes[pos++] = local_header.nonce & 0xFF;
    bytes[pos++] = (local_header.nonce >> 8) & 0xFF;
    bytes[pos++] = (local_header.nonce >> 16) & 0xFF;
    bytes[pos++] = (local_header.nonce >> 24) & 0xFF;
    
    /*if (tid == 0) {
        printf("\nBlock data before padding:\n");
        printf("Block length: %u bytes\n", pos);
        printf("Raw bytes in hex:\n");
        for (int i = 0; i < pos; i++) {
            printf("%02x", bytes[i]);
            if ((i + 1) % 32 == 0) printf("\n");
        }
        printf("\n\n");
        
        printf("Nonce position (bytes %d-%d): ", pos-4, pos-1);
        for (int i = pos-4; i < pos; i++) {
            printf("%02x", bytes[i]);
        }
        printf(" (nonce value: %08x)\n", local_header.nonce);
    }*/
    
    // Add padding
    uint32_t original_pos = pos;
    bytes[pos++] = 0x80;  // Padding bit
    
    // Fill with zeros until we have room for the length
    while ((pos % 64) != 56) {  // 56 = 64 - 8 (for length)
        bytes[pos++] = 0;
    }
    
    // Add message length in bits as big-endian
    uint64_t total_bits = (uint64_t)original_pos * 8;
    bytes[pos++] = (total_bits >> 56) & 0xFF;
    bytes[pos++] = (total_bits >> 48) & 0xFF;
    bytes[pos++] = (total_bits >> 40) & 0xFF;
    bytes[pos++] = (total_bits >> 32) & 0xFF;
    bytes[pos++] = (total_bits >> 24) & 0xFF;
    bytes[pos++] = (total_bits >> 16) & 0xFF;
    bytes[pos++] = (total_bits >> 8) & 0xFF;
    bytes[pos++] = total_bits & 0xFF;
    
    /*if (tid == 0) {
        printf("Block data after padding (hex):\n");
        for (int i = 0; i < pos; i++) {
            printf("%02x", bytes[i]);
            if ((i + 1) % 64 == 0) printf("\n");
        }
        printf("\n");
        printf("Total length: %d bytes (%d bits)\n", pos, pos * 8);
    }*/
    
    // Process each 64-byte (512-bit) chunk
    for (uint32_t chunk = 0; chunk < pos; chunk += 64) {
        uint32_t w[64] = {0};
        
        // Copy chunk into first 16 words
        for (int i = 0; i < 16; i++) {
            w[i] = (bytes[chunk + i*4] << 24) |
                   (bytes[chunk + i*4 + 1] << 16) |
                   (bytes[chunk + i*4 + 2] << 8) |
                   bytes[chunk + i*4 + 3];
        }
        
        /*if (tid == 0 && chunk == 0) {
            printf("First 16 words of first chunk:\n");
            for (int i = 0; i < 16; i++) {
                printf("%08x ", w[i]);
                if ((i + 1) % 8 == 0) printf("\n");
            }
            printf("\n");
        }*/
        
        sha256_transform(state, w);
    }
    
    /*if (tid == 0) {
        printf("First SHA-256 state:\n");
        for (int i = 0; i < 8; i++) {
            printf("%08x ", state[i]);
        }
        printf("\n");
    }*/
    
    // Second SHA-256 hash
    uint32_t final_state[8];
    memcpy(final_state, sha256_init_state, sizeof(final_state));
    
    // Prepare block for second hash
    uint8_t final_bytes[64] = {0};  // Changed to byte array
    
    // Copy first hash result
    for (int i = 0; i < 8; i++) {
        final_bytes[i*4] = (state[i] >> 24) & 0xFF;
        final_bytes[i*4 + 1] = (state[i] >> 16) & 0xFF;
        final_bytes[i*4 + 2] = (state[i] >> 8) & 0xFF;
        final_bytes[i*4 + 3] = state[i] & 0xFF;
    }
    
    // Add padding for second hash
    final_bytes[32] = 0x80;  // Padding bit after 32 bytes
    // Length is 256 bits = 0x100
    final_bytes[62] = 0x01;  // Upper byte of 0x100
    final_bytes[63] = 0x00;  // Lower byte of 0x100
    
    // Convert to words for transform
    uint32_t final_w[16] = {0};
    for (int i = 0; i < 16; i++) {
        final_w[i] = (final_bytes[i*4] << 24) |
                     (final_bytes[i*4 + 1] << 16) |
                     (final_bytes[i*4 + 2] << 8) |
                     final_bytes[i*4 + 3];
    }
    
    sha256_transform(final_state, final_w);
    
    /*if (tid == 0) {
        printf("Final SHA-256 state (before endian swap):\n");
        for (int i = 0; i < 8; i++) {
            printf("%08x ", final_state[i]);
        }
        printf("\n");
    }*/
    
    // Compare hash with target (both in big-endian)
    bool valid = true;
    /*if (tid == 0) {
        printf("Comparing hash with target:\n");
        printf("Hash:   ");
        for (int i = 7; i >= 0; i--) {
            printf("%08x", swap32(final_state[i]));  // Keep word in little-endian
        }
        printf("\nTarget: ");
        for (int i = 0; i < 8; i++) {
            printf("%08x", target.words[i]);
        }
        printf("\n");
    }*/

    // Compare in Bitcoin's byte order (reversed words, each word in little-endian)
    for (int i = 7; i >= 0 && valid; i--) {
        uint32_t hash_word = swap32(final_state[i]);  // Convert word to little-endian
        uint32_t target_word = target.words[7-i];
        if (hash_word > target_word) {
            valid = false;
        }
        else if (hash_word < target_word) {
            break;
        }
    }

    if (valid) {
        *found = tid;
        // Copy final hash to output in Bitcoin's byte order (reversed words, each word in little-endian)
        for (int i = 0; i < 8; i++) {
            ((uint32_t*)output)[i] = swap32(final_state[7-i]);  // Convert word to little-endian
        }
    }
}

// Hex string to bytes conversion utility
__host__ bool hex_to_bytes(const char* hex_str, uint8_t* bytes, size_t len) {
    if (!hex_str || !bytes || strlen(hex_str) < len * 2) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        char hex[3] = {hex_str[i*2], hex_str[i*2+1], 0};
        char* endptr;
        long val = strtol(hex, &endptr, 16);
        if (*endptr != '\0' || val < 0 || val > 255) {
            return false;
        }
        bytes[i] = (uint8_t)val;
    }
    return true;
}

// Save current mining state to a file
bool save_mining_state(const char* filename, const MiningHeader* header, const Target* target) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        printf("Error: Could not open file %s for writing\n", filename);
        printf("Error code: %d\n", errno);
        printf("Error message: %s\n", strerror(errno));
        return false;
    }

    // Write magic number and version
    const uint32_t MAGIC = 0x4D494E45;  // "MINE"
    const uint32_t VERSION = 1;
    size_t written;
    
    written = fwrite(&MAGIC, sizeof(MAGIC), 1, f);
    if (written != 1) {
        printf("Error writing magic number\n");
        fclose(f);
        return false;
    }
    
    written = fwrite(&VERSION, sizeof(VERSION), 1, f);
    if (written != 1) {
        printf("Error writing version\n");
        fclose(f);
        return false;
    }

    // Write header
    written = fwrite(header, sizeof(MiningHeader), 1, f);
    if (written != 1) {
        printf("Error writing header\n");
        fclose(f);
        return false;
    }

    // Write target
    written = fwrite(target, sizeof(Target), 1, f);
    if (written != 1) {
        printf("Error writing target\n");
        fclose(f);
        return false;
    }

    if (fclose(f) != 0) {
        printf("Error closing file\n");
        return false;
    }
    return true;
}

// Load mining state from a file
bool load_mining_state(const char* filename, MiningHeader* header, Target* target) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("Error: Could not open file %s for reading\n", filename);
        return false;
    }

    // Read and verify magic number and version
    uint32_t magic, version;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != 0x4D494E45) {
        printf("Error: Invalid state file format\n");
        fclose(f);
        return false;
    }
    if (fread(&version, sizeof(version), 1, f) != 1 || version != 1) {
        printf("Error: Unsupported state file version\n");
        fclose(f);
        return false;
    }

    // Read header
    if (fread(header, sizeof(MiningHeader), 1, f) != 1) {
        printf("Error: Failed to read header from state file\n");
        fclose(f);
        return false;
    }

    // Read target
    if (fread(target, sizeof(Target), 1, f) != 1) {
        printf("Error: Failed to read target from state file\n");
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

bool mine_block(MiningHeader* header, Target target, float time_limit) {
    MiningHeader* d_header;
    uint8_t* d_output;
    uint32_t* d_found;
    cudaError_t cuda_status;
    bool success = false;
    
    // Allocate device memory
    if ((cuda_status = cudaMalloc(&d_header, sizeof(MiningHeader))) != cudaSuccess) {
        printf("Error: Failed to allocate device memory for header: %s\n", cudaGetErrorString(cuda_status));
        return false;
    }
    
    if ((cuda_status = cudaMalloc(&d_output, 32)) != cudaSuccess) {
        printf("Error: Failed to allocate device memory for output: %s\n", cudaGetErrorString(cuda_status));
        cudaFree(d_header);
        return false;
    }
    
    if ((cuda_status = cudaMalloc(&d_found, sizeof(uint32_t))) != cudaSuccess) {
        printf("Error: Failed to allocate device memory for found flag: %s\n", cudaGetErrorString(cuda_status));
        cudaFree(d_header);
        cudaFree(d_output);
        return false;
    }
    
    // Create CUDA events for timing
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    
    // Calculate grid dimensions
    int blocks = 8192;  // Adjust based on your GPU
    
    // Print mining parameters
    printf("Starting mining with parameters:\n");
    printf("Threads per block: %d\n", 256);
    printf("Blocks per grid: %d\n", blocks);
    printf("Hashes per launch: %d\n", 256 * blocks);
    
    float elapsed_time = 0;
    uint64_t total_hashes = 0;
    bool interrupted = false;
    
    while (elapsed_time < time_limit && !interrupted) {
        // Check for keyboard input (Windows)
        if (_kbhit()) {
            char c = _getch();
            printf("\nKey pressed: %d\n", (int)c);  // Debug output
            
            // Save state on Ctrl+C (3) or 'q'
            if (c == 3 || c == 'q' || c == 'Q') {
                printf("\n\nMining interrupted. Saving state...\n");
                const char* state_path = "mining_state.bin";  // Try simple path first
                printf("Current working directory: ");
                fflush(stdout);  // Ensure output is shown
                system("cd");    // Print current directory
                
                printf("Attempting to save state to: %s\n", state_path);
                fflush(stdout);  // Ensure output is shown
                
                if (save_mining_state(state_path, header, &target)) {
                    printf("Mining state saved to %s\n", state_path);
                } else {
                    printf("Failed to save mining state to %s\n", state_path);
                    
                    // Try alternate location
                    state_path = "C:/Users/Omer/Documents/Work/Mine/miner/mining_state.bin";
                    printf("Trying alternate path: %s\n", state_path);
                    if (save_mining_state(state_path, header, &target)) {
                        printf("Mining state saved to alternate location: %s\n", state_path);
                    } else {
                        printf("Failed to save to alternate location\n");
                    }
                }
                interrupted = true;
                break;
            }
        }
        
        printf("\rHashes: %llu (%.2f MH/s)", total_hashes, total_hashes / (elapsed_time * 1000000));
        fflush(stdout);
        
        // Reset found flag
        if ((cuda_status = cudaMemset(d_found, 0, sizeof(uint32_t))) != cudaSuccess) {
            printf("Error: Failed to reset found flag: %s\n", cudaGetErrorString(cuda_status));
            break;
        }
        
        // Copy header to device
        if ((cuda_status = cudaMemcpy(d_header, header, sizeof(MiningHeader), cudaMemcpyHostToDevice)) != cudaSuccess) {
            printf("Error: Failed to copy header to device: %s\n", cudaGetErrorString(cuda_status));
            break;
        }
        
        // Launch kernel
        sha256_gpu<<<blocks, 256>>>(d_header, d_output, target, d_found);
        
        if ((cuda_status = cudaGetLastError()) != cudaSuccess) {
            printf("Error: Failed to launch kernel: %s\n", cudaGetErrorString(cuda_status));
            break;
        }
        
        // Check if a valid nonce was found
        uint32_t winning_thread;
        if ((cuda_status = cudaMemcpy(&winning_thread, d_found, sizeof(uint32_t), cudaMemcpyDeviceToHost)) != cudaSuccess) {
            printf("Error: Failed to copy found flag: %s\n", cudaGetErrorString(cuda_status));
            break;
        }
        
        if (winning_thread != 0) {
            uint32_t winning_nonce = header->nonce + winning_thread;
            uint32_t output_hash[8];
            if ((cuda_status = cudaMemcpy(output_hash, d_output, 32, cudaMemcpyDeviceToHost)) != cudaSuccess) {
                printf("Error: Failed to copy output hash: %s\n", cudaGetErrorString(cuda_status));
                break;
            }
            
            printf("\n\n=== Valid Nonce Found! ===\n");
            printf("Nonce (hex): %08x\n", winning_nonce);
            printf("Nonce (decimal): %u\n", winning_nonce);
            printf("Final Hash: ");
            for (int i = 0; i < 8; i++) {
                printf("%08x", output_hash[i]);
            }
            printf("\nTotal hashes tried: %llu\n", total_hashes);
            printf("Time elapsed: %.2f seconds\n", elapsed_time);
            printf("Hash rate: %.2f MH/s\n", total_hashes / (elapsed_time * 1000000));
            printf("========================\n\n");
            
            header->nonce = winning_nonce;
            success = true;
            break;
        }
        
        // Update progress
        total_hashes += 256 * blocks;
        header->nonce += 256 * blocks;
        
        // Update elapsed time
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&elapsed_time, start, stop);
        elapsed_time /= 1000.0f;  // Convert to seconds
    }
    
    // Cleanup
    cudaFree(d_header);
    cudaFree(d_output);
    cudaFree(d_found);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    
    return success;
}
