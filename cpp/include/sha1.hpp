#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mini_git {

class Sha1 {
  public:
    Sha1();

    void update(const void* data, std::size_t length);
    std::array<std::uint8_t, 20> finalize();

    static std::array<std::uint8_t, 20> digest(const std::string& data);
    static std::string hex_digest(const std::string& data);
    static std::string to_hex(const std::uint8_t* data, std::size_t length);
    static std::vector<std::uint8_t> hex_to_bytes(const std::string& hex);

  private:
    void process_block(const std::uint8_t* block);

    std::array<std::uint32_t, 5> state_;
    std::array<std::uint8_t, 64> buffer_;
    std::uint64_t bit_count_;
    std::size_t buffer_size_;
    bool finalized_;
};

}  // namespace mini_git
