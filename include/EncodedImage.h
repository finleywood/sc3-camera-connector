#pragma once

#include <cstdint>
#include <memory>

struct EncodedImage {
    std::shared_ptr<uint8_t[]> data;
    size_t size = 0;
    int width = 0;
    int height = 0;
    bool valid() const { return data != nullptr && size > 0; }
};