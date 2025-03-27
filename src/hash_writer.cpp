#include "hash_writer.hpp"
#include <stdexcept>

HashWriter::HashWriter() : finalized(false) {
    SHA256_Init(&ctx);
}

void HashWriter::write(const void* data, size_t len) {
    if (finalized) {
        throw std::runtime_error("HashWriter is already finalized");
    }
    SHA256_Update(&ctx, data, len);
}

void HashWriter::finalize(unsigned char hash[SHA256_DIGEST_LENGTH]) {
    if (!finalized) {
        SHA256_Final(hash, &ctx);
        finalized = true;
    }
}
