#include "SimulatedCamera.h"

#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>
#include <cstring>

/* ---------------------------------------------------------
   Utility: compute 16:9 height from width
   --------------------------------------------------------- */
inline int height16by9(int width)
{
    return static_cast<int>(width * 9 / 16);
}

/* ---------------------------------------------------------
   Encode RGB JPEG → EncodedImage
   --------------------------------------------------------- */
EncodedImage generateRGBJPEG(
    int width,
    int jpegQuality = 90
)
{
    const int height = height16by9(width);

    cv::Mat image(height, width, CV_8UC3);

    // Simple deterministic gradient
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            cv::Vec3b& p = image.at<cv::Vec3b>(y, x);
            p[0] = static_cast<uint8_t>((x + y) & 0xFF); // B
            p[1] = static_cast<uint8_t>(y & 0xFF);       // G
            p[2] = static_cast<uint8_t>(x & 0xFF);       // R
        }
    }

    std::vector<int> params = {
        cv::IMWRITE_JPEG_QUALITY, jpegQuality
    };

    std::vector<uint8_t> encoded;
    if (!cv::imencode(".jpg", image, encoded, params)) {
        return {};
    }

    EncodedImage ret_img;
    ret_img.width = width;
    ret_img.height = height;

    ret_img.size = encoded.size();
    ret_img.data.reset(new uint8_t[ret_img.size]);
    std::memcpy(ret_img.data.get(), encoded.data(), ret_img.size);

    return ret_img;
}

/* ---------------------------------------------------------
   Encode depth PNG (16-bit) → EncodedImage
   --------------------------------------------------------- */
EncodedImage generateDepthPNG(
    int width,
    uint16_t maxDepthMM = 5000,
    int pngCompression = 3
)
{
    const int height = height16by9(width);

    cv::Mat depth(height, width, CV_16UC1);

    // Horizontal depth ramp
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            depth.at<uint16_t>(y, x) =
                static_cast<uint16_t>(
                    (static_cast<float>(x) / (width - 1)) * maxDepthMM
                );
        }
    }

    std::vector<int> params = {
        cv::IMWRITE_PNG_COMPRESSION, pngCompression
    };

    std::vector<uint8_t> encoded;
    if (!cv::imencode(".png", depth, encoded, params)) {
        return {};
    }

    EncodedImage ret_img;
    ret_img.width = width;
    ret_img.height = height;

    ret_img.size = encoded.size();
    ret_img.data.reset(new uint8_t[ret_img.size]);
    std::memcpy(ret_img.data.get(), encoded.data(), ret_img.size);

    return ret_img;
}

bool SimulatedCamera::OpenCamera() {
    if(!this->is_open.load()) {
        this->is_open = true;
        return true;
    } else {
        return false;
    }
}

bool SimulatedCamera::CloseCamera() {
    if(!this->is_open.load()) return false;
    else {
        this->is_open = false;
        return true;
    }
}

std::pair<EncodedImage, EncodedImage> SimulatedCamera::GetFrame() {
    if(this->is_open) {
        EncodedImage rgb = generateRGBJPEG(2160);
        EncodedImage depth = generateDepthPNG(480);
        if(rgb.valid() && depth.valid()) {
            return std::make_pair(rgb, depth);
        } else {
            return std::make_pair(EncodedImage{}, EncodedImage{});
        }
    } else {
        return std::make_pair(EncodedImage{}, EncodedImage{});
    }
}

bool SimulatedCamera::IsOpen() {
    return this->is_open.load();
}