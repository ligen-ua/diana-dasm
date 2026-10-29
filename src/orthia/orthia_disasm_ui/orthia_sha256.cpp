#include "orthia_sha256.h"
#include <algorithm>
#include <cstring>

namespace orthia
{
    namespace
    {
        const uint32_t c_k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
        };

        inline uint32_t Rotr(uint32_t value, int bits)
        {
            return (value >> bits) | (value << (32 - bits));
        }
    }

    CSha256::CSha256()
    {
        const uint32_t initial[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
        };
        memcpy(m_state, initial, sizeof(m_state));
    }

    void CSha256::Transform(const unsigned char* block)
    {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
        {
            w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
        }
        for (int i = 16; i < 64; ++i)
        {
            const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
        for (int i = 0; i < 64; ++i)
        {
            const uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = h + s1 + ch + c_k[i] + w[i];
            const uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

    void CSha256::Update(const void* data, size_t size)
    {
        const unsigned char* bytes = (const unsigned char*)data;
        m_totalSize += size;
        while (size)
        {
            const size_t part = std::min(size, sizeof(m_block) - m_blockSize);
            memcpy(m_block + m_blockSize, bytes, part);
            m_blockSize += part;
            bytes += part;
            size -= part;
            if (m_blockSize == sizeof(m_block))
            {
                Transform(m_block);
                m_blockSize = 0;
            }
        }
    }

    std::string CSha256::FinishHex()
    {
        const uint64_t bits = m_totalSize * 8;
        const unsigned char one = 0x80;
        const unsigned char zero = 0;
        Update(&one, 1);
        while (m_blockSize != 56)
        {
            Update(&zero, 1);
        }
        unsigned char length[8];
        for (int i = 0; i < 8; ++i)
        {
            length[i] = (unsigned char)(bits >> (56 - i * 8));
        }
        Update(length, sizeof(length));

        static const char digits[] = "0123456789abcdef";
        std::string text;
        for (int i = 0; i < 8; ++i)
        {
            for (int shift = 28; shift >= 0; shift -= 4)
            {
                text += digits[(m_state[i] >> shift) & 0xF];
            }
        }
        return text;
    }

    std::string Sha256Hex(const void* data, size_t size)
    {
        CSha256 sha;
        sha.Update(data, size);
        return sha.FinishHex();
    }
}
