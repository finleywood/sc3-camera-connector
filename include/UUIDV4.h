#pragma once

#pragma once
#include <array>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

class UuidV4 {
public:
    UuidV4()
        : rng_(std::random_device{}()),
          dist_(0, 255)
    {}

    std::string generate() {
        std::array<unsigned char, 16> bytes;

        // Generate random bytes
        for (auto &b : bytes) {
            b = static_cast<unsigned char>(dist_(rng_));
        }

        // Set the version (UUIDv4)
        bytes[6] = (bytes[6] & 0x0F) | 0x40;

        // Set the variant (RFC 4122)
        bytes[8] = (bytes[8] & 0x3F) | 0x80;

        // Convert to string
        return toString(bytes);
    }

private:
    std::string toString(const std::array<unsigned char, 16> &bytes) const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        for (size_t i = 0; i < bytes.size(); ++i) {
            oss << std::setw(2) << static_cast<int>(bytes[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9)
                oss << "-";
        }

        return oss.str();
    }

    std::mt19937 rng_;
    std::uniform_int_distribution<int> dist_;
};
