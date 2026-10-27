#pragma once

// SHA-256 (FIPS 180-4): the hash of saved evidence, the one IOC lists and file reputation services use

#include <cstddef>
#include <cstdint>
#include <string>

namespace orthia
{
    class CSha256
    {
        uint32_t m_state[8];
        unsigned char m_block[64];
        size_t m_blockSize = 0;
        uint64_t m_totalSize = 0;

        void Transform(const unsigned char* block);
    public:
        CSha256();
        void Update(const void* data, size_t size);
        // lowercase hex, 64 characters; the object is done after it
        std::string FinishHex();
    };

    std::string Sha256Hex(const void* data, size_t size);
}
