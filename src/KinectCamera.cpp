#include "KinectCamera.h"
#include <aixlog/aixlog.hpp>
#include "ImageUtils.h"
#include <unistd.h>

#define OPEN_TIMEOUT_MS (1000)
#define CAPTURE_TIMEOUT_MS (5000)

bool KinectCamera::OpenCamera() {
    if(this->is_open.load()) return false;
    this->is_open = true;

    auto open_res = k4a_device_open(K4A_DEVICE_DEFAULT, &this->current_device);
    if(open_res != K4A_RESULT_SUCCEEDED) {
        this->CloseCamera();
        LOG(ERROR) << "Failed to open k4a device." << std::endl;
        return false;
    }

    k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    config.color_resolution = K4A_COLOR_RESOLUTION_2160P;
    config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    config.camera_fps = K4A_FRAMES_PER_SECOND_5;

    auto start_res = k4a_device_start_cameras(this->current_device, &config);
    if(start_res != K4A_RESULT_SUCCEEDED) {
        LOG(ERROR) << "Failed to start k4a device." << std::endl;
        return false;
    }

    return true;
}

bool KinectCamera::CloseCamera() {
    if(!this->is_open.load()) return false;
    if(this->current_device != NULL) {
        k4a_device_stop_cameras(this->current_device);
        k4a_device_close(this->current_device);
        this->current_device = NULL;
        usleep(2000 * 1000);
    }

    this->is_open = false;
    return true;
}

std::pair<EncodedImage, EncodedImage> KinectCamera::GetFrame() {
    if(!this->is_open.load()) return std::make_pair(EncodedImage{}, EncodedImage{});
    k4a_capture_t cap;
    k4a_wait_result_t wait_res = k4a_device_get_capture(this->current_device, &cap, CAPTURE_TIMEOUT_MS);
    if(wait_res != K4A_WAIT_RESULT_SUCCEEDED) {
        LOG(ERROR) << "Could not get capture from camera, check aliveness." << std::endl;
        return std::make_pair(EncodedImage{}, EncodedImage{});
    }

    k4a_image_t color_img = k4a_capture_get_color_image(cap);
    if(color_img == NULL) {
        k4a_capture_release(cap);
        LOG(WARNING) << "Colour image could not be gotten." << std::endl;
        return std::make_pair(EncodedImage{}, EncodedImage{});
    }
    k4a_image_t depth_img = k4a_capture_get_depth_image(cap);
    if(depth_img == NULL) {
        k4a_image_release(color_img);
        k4a_capture_release(cap);
        LOG(WARNING) << "Depth image could not be gotten." << std::endl;
        return std::make_pair(EncodedImage{}, EncodedImage{});
    }

    EncodedImage rgb_jpg = ImageUtils::K4AToJpeg(&color_img);
    EncodedImage depth_png = ImageUtils::K4AToPng(&depth_img);

    k4a_image_release(color_img);
    k4a_image_release(depth_img);
    k4a_capture_release(cap);

    return std::make_pair(rgb_jpg, depth_png);
}

bool KinectCamera::IsOpen() {
    return this->is_open.load();
}