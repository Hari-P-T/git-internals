#include "sha1.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace mini_git {

namespace {

constexpr std::uint32_t left_rotate(std::uint32_t value, unsigned int bits) {
    return (value << bits) | (value >> (32U - bits));
}

}  // namespace

Sha1::Sha1()
    : state_{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U},
      buffer_{},
      bit_count_(0),
      buffer_size_(0),
      finalized_(false) {}

void Sha1::update(const void* data, std::size_t length) {
    if (finalized_) {
        throw std::runtime_error("Cannot update SHA1 after finalize");
    }

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    bit_count_ += static_cast<std::uint64_t>(length) * 8U;

    while (length > 0) {
        const std::size_t chunk = std::min(length, buffer_.size() - buffer_size_);
        std::copy(bytes, bytes + chunk, buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_));
        buffer_size_ += chunk;
        bytes += chunk;
        length -= chunk;

        if (buffer_size_ == buffer_.size()) {
            process_block(buffer_.data());
            buffer_size_ = 0;
        }
    }
}

std::array<std::uint8_t, 20> Sha1::finalize() {
    if (!finalized_) {
        buffer_[buffer_size_++] = 0x80U;

        if (buffer_size_ > 56U) {
            while (buffer_size_ < buffer_.size()) {
                buffer_[buffer_size_++] = 0U;
            }
            process_block(buffer_.data());
            buffer_size_ = 0;
        }

        while (buffer_size_ < 56U) {
            buffer_[buffer_size_++] = 0U;
        }

        for (int shift = 56; shift >= 0; shift -= 8) {
            buffer_[buffer_size_++] = static_cast<std::uint8_t>((bit_count_ >> shift) & 0xFFU);
        }

        process_block(buffer_.data());
        finalized_ = true;
    }

    std::array<std::uint8_t, 20> digest_bytes{};
    for (std::size_t i = 0; i < state_.size(); ++i) {
        digest_bytes[i * 4] = static_cast<std::uint8_t>((state_[i] >> 24) & 0xFFU);
        digest_bytes[i * 4 + 1] = static_cast<std::uint8_t>((state_[i] >> 16) & 0xFFU);
        digest_bytes[i * 4 + 2] = static_cast<std::uint8_t>((state_[i] >> 8) & 0xFFU);
        digest_bytes[i * 4 + 3] = static_cast<std::uint8_t>(state_[i] & 0xFFU);
    }
    return digest_bytes;
}

std::array<std::uint8_t, 20> Sha1::digest(const std::string& data) {
    Sha1 sha1;
    sha1.update(data.data(), data.size());
    return sha1.finalize();
}

std::string Sha1::hex_digest(const std::string& data) {
    const auto digest_bytes = digest(data);
    return to_hex(digest_bytes.data(), digest_bytes.size());
}

std::string Sha1::to_hex(const std::uint8_t* data, std::size_t length) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < length; ++i) {
        stream << std::setw(2) << static_cast<int>(data[i]);
    }
    return stream.str();
}

std::vector<std::uint8_t> Sha1::hex_to_bytes(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Invalid hex length");
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);

    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const std::string chunk = hex.substr(index, 2);
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(chunk, nullptr, 16)));
    }

    return bytes;
}

void Sha1::process_block(const std::uint8_t* block) {
    std::uint32_t words[80];
    for (int index = 0; index < 16; ++index) {
        const int offset = index * 4;
        words[index] =
            (static_cast<std::uint32_t>(block[offset]) << 24) |
            (static_cast<std::uint32_t>(block[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(block[offset + 2]) << 8) |
            static_cast<std::uint32_t>(block[offset + 3]);
    }

    for (int index = 16; index < 80; ++index) {
        words[index] = left_rotate(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];

    for (int index = 0; index < 80; ++index) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;
        if (index < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999U;
        } else if (index < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        } else if (index < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }

        const std::uint32_t temp = left_rotate(a, 5) + f + e + k + words[index];
        e = d;
        d = c;
        c = left_rotate(b, 30);
        b = a;
        a = temp;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
}

}  // namespace mini_git
