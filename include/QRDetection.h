#pragma once

#include <string>

struct QRDetection {
    std::string identifier;
    uint8_t* rgb_image_buf;
    uint8_t* depth_image_buf;
};