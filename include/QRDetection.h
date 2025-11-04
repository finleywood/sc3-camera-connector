#include <opencv2/opencv.hpp>

struct QRDetection {
    std::string identifier;
    uint8_t* rgb_image_buf;
    uint8_t* depth_image_buf;
};