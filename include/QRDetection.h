#pragma once

#include <string>
#include <cstdint>
#include "EncodedImage.h"

struct QRDetection {
    int identifier = -1;
    EncodedImage rgb_img;
    EncodedImage depth_image;
    uint64_t event_time;
    double weight = 0;
    bool valid() { return rgb_img.valid() && depth_image.valid() && identifier >= 0; };
};
