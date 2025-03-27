#pragma once
#include <vector>
#include <openssl/sha.h>

class HashWriter {
public:
    HashWriter();
    void write(const void* data, size_t len);
    void finalize(unsigned char hash[SHA256_DIGEST_LENGTH]);
    
private:
    SHA256_CTX ctx;
    bool finalized;
};
