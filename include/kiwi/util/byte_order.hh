#pragma once

#include <cstdint>

namespace kiwi {

class LittleEndian {
 public:
  static constexpr uint16_t Uint16(const uint8_t* b) {
    return static_cast<uint16_t>(b[0]) | static_cast<uint16_t>(b[1]) << 8;
  }

  static constexpr uint32_t Uint32(const uint8_t* b) {
    return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 |
           static_cast<uint32_t>(b[2]) << 16 |
           static_cast<uint32_t>(b[3]) << 24;
  }

  static constexpr uint64_t Uint64(const uint8_t* b) {
    return static_cast<uint64_t>(b[0]) | static_cast<uint64_t>(b[1]) << 8 |
           static_cast<uint64_t>(b[2]) << 16 |
           static_cast<uint64_t>(b[3]) << 24 |
           static_cast<uint64_t>(b[4]) << 32 |
           static_cast<uint64_t>(b[5]) << 40 |
           static_cast<uint64_t>(b[6]) << 48 |
           static_cast<uint64_t>(b[7]) << 56;
  }

  static constexpr void PutUint16(uint8_t* b, uint16_t v) {
    b[0] = static_cast<uint8_t>(v);
    b[1] = static_cast<uint8_t>(v >> 8);
  }

  static constexpr void PutUint32(uint8_t* b, uint32_t v) {
    b[0] = static_cast<uint8_t>(v);
    b[1] = static_cast<uint8_t>(v >> 8);
    b[2] = static_cast<uint8_t>(v >> 16);
    b[3] = static_cast<uint8_t>(v >> 24);
  }

  static constexpr void PutUint64(uint8_t* b, uint64_t v) {
    b[0] = static_cast<uint8_t>(v);
    b[1] = static_cast<uint8_t>(v >> 8);
    b[2] = static_cast<uint8_t>(v >> 16);
    b[3] = static_cast<uint8_t>(v >> 24);
    b[4] = static_cast<uint8_t>(v >> 32);
    b[5] = static_cast<uint8_t>(v >> 40);
    b[6] = static_cast<uint8_t>(v >> 48);
    b[7] = static_cast<uint8_t>(v >> 56);
  }

  static constexpr uint8_t* AppendUint16(uint8_t* b, uint16_t v) {
    *b++ = static_cast<uint8_t>(v);
    *b++ = static_cast<uint8_t>(v >> 8);

    return b;
  }

  static constexpr uint8_t* AppendUint32(uint8_t* b, uint32_t v) {
    *b++ = static_cast<uint8_t>(v);
    *b++ = static_cast<uint8_t>(v >> 8);
    *b++ = static_cast<uint8_t>(v >> 16);
    *b++ = static_cast<uint8_t>(v >> 24);

    return b;
  }

  static constexpr uint8_t* AppendUint64(uint8_t* b, uint64_t v) {
    *b++ = static_cast<uint8_t>(v);
    *b++ = static_cast<uint8_t>(v >> 8);
    *b++ = static_cast<uint8_t>(v >> 16);
    *b++ = static_cast<uint8_t>(v >> 24);
    *b++ = static_cast<uint8_t>(v >> 32);
    *b++ = static_cast<uint8_t>(v >> 40);
    *b++ = static_cast<uint8_t>(v >> 48);
    *b++ = static_cast<uint8_t>(v >> 56);

    return b;
  }
};

class BigEndian {
 public:
  static constexpr uint16_t Uint16(const uint8_t* b) {
    return static_cast<uint16_t>(b[1]) | static_cast<uint16_t>(b[0]) << 8;
  }

  static constexpr uint32_t Uint32(const uint8_t* b) {
    return static_cast<uint32_t>(b[3]) | static_cast<uint32_t>(b[2]) << 8 |
           static_cast<uint32_t>(b[1]) << 16 |
           static_cast<uint32_t>(b[0]) << 24;
  }

  static constexpr uint64_t Uint64(const uint8_t* b) {
    return static_cast<uint64_t>(b[7]) | static_cast<uint64_t>(b[6]) << 8 |
           static_cast<uint64_t>(b[5]) << 16 |
           static_cast<uint64_t>(b[4]) << 24 |
           static_cast<uint64_t>(b[3]) << 32 |
           static_cast<uint64_t>(b[2]) << 40 |
           static_cast<uint64_t>(b[1]) << 48 |
           static_cast<uint64_t>(b[0]) << 56;
  }

  static constexpr void PutUint16(uint8_t* b, uint16_t v) {
    b[0] = static_cast<uint8_t>(v >> 8);
    b[1] = static_cast<uint8_t>(v);
  }

  static constexpr void PutUint32(uint8_t* b, uint32_t v) {
    b[0] = static_cast<uint8_t>(v >> 24);
    b[1] = static_cast<uint8_t>(v >> 16);
    b[2] = static_cast<uint8_t>(v >> 8);
    b[3] = static_cast<uint8_t>(v);
  }

  static constexpr void PutUint64(uint8_t* b, uint64_t v) {
    b[0] = static_cast<uint8_t>(v >> 56);
    b[1] = static_cast<uint8_t>(v >> 48);
    b[2] = static_cast<uint8_t>(v >> 40);
    b[3] = static_cast<uint8_t>(v >> 32);
    b[4] = static_cast<uint8_t>(v >> 24);
    b[5] = static_cast<uint8_t>(v >> 16);
    b[6] = static_cast<uint8_t>(v >> 8);
    b[7] = static_cast<uint8_t>(v);
  }

  static constexpr uint8_t* AppendUint16(uint8_t* b, uint16_t v) {
    *b++ = static_cast<uint8_t>(v >> 8);
    *b++ = static_cast<uint8_t>(v);

    return b;
  }

  static constexpr uint8_t* AppendUint32(uint8_t* b, uint32_t v) {
    *b++ = static_cast<uint8_t>(v >> 24);
    *b++ = static_cast<uint8_t>(v >> 16);
    *b++ = static_cast<uint8_t>(v >> 8);
    *b++ = static_cast<uint8_t>(v);

    return b;
  }

  static constexpr uint8_t* AppendUint64(uint8_t* b, uint64_t v) {
    *b++ = static_cast<uint8_t>(v >> 56);
    *b++ = static_cast<uint8_t>(v >> 48);
    *b++ = static_cast<uint8_t>(v >> 40);
    *b++ = static_cast<uint8_t>(v >> 32);
    *b++ = static_cast<uint8_t>(v >> 24);
    *b++ = static_cast<uint8_t>(v >> 16);
    *b++ = static_cast<uint8_t>(v >> 8);
    *b++ = static_cast<uint8_t>(v);
    return b;
  }
};

}  // namespace kiwi