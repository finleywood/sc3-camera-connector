#include "ImageUtils.h"
#include <vector>

namespace ImageUtils {

cv::Mat BufToCvCpy(const EncodedImage& img)
{
    // Validate input
    if (!img.data || img.size == 0) {
        return cv::Mat();
    }
    
    // Expected size for BGRA format
    size_t expected_size = img.height * img.width * 4;
    if (img.size != expected_size) {
        LOG(WARNING) << "Size mismatch: expected " << expected_size 
                     << " but got " << img.size;
        return cv::Mat();
    }
    
    // Create Mat and copy data in one step
    cv::Mat bgra_mat(img.height, img.width, CV_8UC4);
    std::memcpy(bgra_mat.data, img.data.get(), img.size);
    
    // Convert to BGR (this creates a new Mat with its own memory)
    cv::Mat bgr_mat;
    cv::cvtColor(bgra_mat, bgr_mat, cv::COLOR_BGRA2BGR);
    
    return bgr_mat;
}

EncodedImage RawToJpg(EncodedImage& image, int quality)
{
    EncodedImage result;

    if (!image.valid())
        return result;

    cv::Mat mat;

    cv::Mat bgra(image.height, image.width, CV_8UC4, image.data.get());
    cv::cvtColor(bgra, mat, cv::COLOR_BGRA2BGR);

    std::vector<uint8_t> jpeg_buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, quality };

    if (!cv::imencode(".jpg", mat, jpeg_buf, params)) {
        LOG(ERROR) << "JPEG encoding failed" << std::endl;
        return result;
    }

    result.size = jpeg_buf.size();
    result.data.reset(new uint8_t[result.size]);
    std::copy(jpeg_buf.begin(), jpeg_buf.end(), result.data.get());
    result.width = image.width;
    result.height = image.height;

    return result;
}

EncodedImage k4a_to_enc_img(k4a_image_t* image) {
    EncodedImage result;

    if (!image)
        return result;

    uint8_t* img_buf = k4a_image_get_buffer(*image);
    result.width  = k4a_image_get_width_pixels(*image);
    result.height = k4a_image_get_height_pixels(*image);

    size_t sz = result.width * result.height * 4;
    result.size = sz;
    
    // Use shared_ptr so the data stays alive when cv::Mat wraps it
    result.data = std::shared_ptr<uint8_t[]>(new uint8_t[result.size]);
    std::memcpy(result.data.get(), img_buf, result.size);
    
    return result;
}

EncodedImage RawToPng(EncodedImage& image) {
    EncodedImage result;

    if (!image.valid())
        return result;

    cv::Mat mat;

    cv::Mat bgra(image.height, image.width, CV_8UC4, image.data.get());
    cv::cvtColor(bgra, mat, cv::COLOR_BGRA2BGR);
    
    if (mat.empty()) {
        return result;
    }
    
    std::vector<uint8_t> encodedBuffer;
    std::vector<int> params;
    params.push_back(cv::IMWRITE_PNG_COMPRESSION);
    params.push_back(3);
    
    if (!cv::imencode(".png", mat, encodedBuffer, params)) {
        return result;
    }
    
    result.size = encodedBuffer.size();
    result.data.reset(new uint8_t[result.size]);
    std::copy(encodedBuffer.begin(), encodedBuffer.end(), result.data.get());
    result.width = image.width;
    result.height = image.height;
    
    return result;
}

EncodedImage JpegToThumbnail(const EncodedImage& orig_img, int target_width, int out_quality)
{
    EncodedImage result;
    if(!orig_img.valid()) {
        LOG(WARNING) << "Tried to resize a null image." << std::endl;
        return {};
    }

    cv::Mat mat;

    try {
        std::vector<uint8_t> data(orig_img.data.get(), orig_img.data.get() + orig_img.size);
        mat = cv::imdecode(data, cv::IMREAD_COLOR);
    } catch(std::exception& e) {
        LOG(WARNING) << "Thumnail gen failed due to: " << e.what() << std::endl;
        return {};
    }

    if(mat.empty()) {
        LOG(WARNING) << "OpenCV failed to decode raw image buffer or jpeg image." << std::endl;
        return result;
    }

    int target_height = (target_width* 9) / 16;

    cv::Mat resized;
    cv::resize(mat, resized, cv::Size(target_width, target_height), 0, 0, cv::INTER_AREA);
    if(resized.empty()) {
        LOG(WARNING) << "Failed to resize image for thumnail." << std::endl;
        return result; 
    }

    std::vector<uint8_t> jpeg_buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, out_quality };

    if(!cv::imencode(".jpg", resized, jpeg_buf, params)) {
        LOG(WARNING) << "Failed to encode output image after resize." << std::endl;
        return result;
    }

    result.size = jpeg_buf.size();
    result.data.reset(new uint8_t[result.size]);
    std::copy(jpeg_buf.begin(), jpeg_buf.end(), result.data.get());
    result.width = target_width;
    result.height = target_height;

    return result;
}

}