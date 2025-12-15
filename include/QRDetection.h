#pragma once

#include <string>
#include <cstdint>
#include "EncodedImage.h"

struct QRDetection {
    std::string identifier;
    EncodedImage rgb_img;
    EncodedImage depth_image;
    uint64_t event_time;
    bool valid() { return rgb_img.valid() && depth_image.valid() && !identifier.empty(); };
};
