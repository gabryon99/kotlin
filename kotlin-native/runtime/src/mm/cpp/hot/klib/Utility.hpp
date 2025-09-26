//
// Created by Gabriele.Pappalardo on 07/07/2025.
//

#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

//#define MAGIC_INDEX 0
#define MAGIC_INDEX 3

template<typename T>
using OptionalConstRef = std::optional<std::reference_wrapper<const T>>;

/// Read 4 bytes as 32-bytes big-endian int.
static uint32_t readBigEndian32(const uint8_t *buffer) {
    return (static_cast<uint32_t>(buffer[0]) << 24) |
           (static_cast<uint32_t>(buffer[1]) << 16) |
           (static_cast<uint32_t>(buffer[2]) << 8) |
           (static_cast<uint32_t>(buffer[3]));
}

/// Read the entire file into a byte array.
inline std::vector<uint8_t> readFileContent(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + filename);
    }

    return buffer;
}

namespace kotlin::ir {

inline uint64_t ushr(const uint64_t value, const int bitCount) {
    // Masking with 0x3F (binary 111111) ensures the shift is always between 0 and 63.
    return value >> (bitCount & 0x3F);
}

/// Converted from `BinaryLattice::decodeInt`
inline std::pair<uint32_t, uint32_t> decodeNameAndType(const uint64_t code) {
    auto decodeInt = [](uint64_t x) -> uint32_t {
        // First, filter out the bits we are not interested in for this integer.
        // This is the only step that was correct in the original.
        x = x & 0x5555555555555555ULL;
        x = (x | (x >> 1)) & 0x3333333333333333ULL;
        x = (x | (x >> 2)) & 0x0f0f0f0f0f0f0f0fULL;
        x = (x | (x >> 4)) & 0x00ff00ff00ff00ffULL;
        x = (x | (x >> 8)) & 0x0000ffff0000ffffULL;
        x = (x | (x >> 16)) & 0x00000000FFFFFFFFULL;
        return static_cast<uint32_t>(x);
    };
    return {decodeInt(code), decodeInt(ushr(code, 1))};
}

}

#endif //UTILITY_HPP
