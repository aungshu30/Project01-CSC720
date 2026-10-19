// pattern.h - known data pattern for the test file + byte verification (Role A, task 1.5)
//
// PROPOSED pattern (agree with Siri before using): the file is split into 4 KiB blocks,
// and every 8-byte word in block k holds the number k. So any read can be checked
// from its offset alone, with no copy of the file needed.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pat {

constexpr size_t kBlock = 4096;

// Fill `len` bytes (a multiple of 8) that start at file offset `off`.
inline void fill(void* buf, uint64_t off, size_t len) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    for (size_t i = 0; i < len; i += 8) {
        uint64_t block = (off + i) / kBlock;
        std::memcpy(p + i, &block, 8);
    }
}

// Returns -1 if every byte matches the pattern, else the index of the first bad
// 8-byte word (in bytes, relative to buf).
inline long long verify(const void* buf, uint64_t off, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i + 8 <= len; i += 8) {
        uint64_t want = (off + i) / kBlock, got;
        std::memcpy(&got, p + i, 8);
        if (got != want) return static_cast<long long>(i);
    }
    return -1;
}

}  // namespace pat
