#include "CameraManager.h"

void CameraManager::RegsiterDetectionCallback(det_callback_t callback_func)
{
    this->detection_callback = callback_func;
}

bool CameraManager::Start()
{
    return this->StartPipeline();
}

void CameraManager::CloseDevice()
{
    if(this->current_device == nullptr)
        return;
    
    k4a_device_stop_cameras(this->current_device);
    k4a_device_close(this->current_device);

    CurrentStatus = camera_manager_status_t::CLOSED;
}

cv::Mat CameraManager::K4ARGBToCV(k4a_image_t* img)
{
    int width  = k4a_image_get_width_pixels(*img);
    int height = k4a_image_get_height_pixels(*img);
    uint8_t* buffer = k4a_image_get_buffer(*img);

    auto bgra_mat = cv::Mat(height, width, CV_8UC4, buffer);

    //Convert to BGR (remove alpha channel)
    cv::Mat bgr_mat;
    cv::cvtColor(bgra_mat, bgr_mat, cv::COLOR_BGRA2BGR);
    return bgr_mat;
}

QRDetection CameraManager::LookForQR(k4a_image_t* img) 
{
    cv::Mat cv_mat_img = this->K4ARGBToCV(img);

    cv::QRCodeDetector cv_qr;
    std::vector<cv::Point> qr_detections;
    std::string found_data = cv_qr.detectAndDecode(cv_mat_img, qr_detections);

    if(!found_data.empty()) {
        auto detection_st = QRDetection{
            found_data
        };
        return detection_st;
    } else {
        return {};
    }
}

void CameraManager::HandleCapture(k4a_capture_t* cap) {
    if(cap == nullptr)
        return;

    k4a_image_t color_image = k4a_capture_get_color_image(*cap);
    k4a_image_t depth_image = nullptr;
    if(this->enable_depth)
        depth_image = k4a_capture_get_depth_image(*cap);

    // Look for and decode QR codes
    if(color_image != nullptr) {
        QRDetection found_qr = this->LookForQR(&color_image);
        if(!found_qr.identifier.empty() && found_qr.identifier != this->prev_detection.identifier) {
            found_qr.rgb_image_buf = k4a_image_get_buffer(color_image);
            if(this->enable_depth) {
                found_qr.depth_image_buf = k4a_image_get_buffer(depth_image);
            }
            this->prev_detection = found_qr;
            this->detection_callback(found_qr);
        }
    } else {
        LOG(INFO) << "Could not get color image" << std::endl;
    }

    if(color_image != nullptr)
        k4a_image_release(color_image);
    if(depth_image != nullptr)
        k4a_image_release(depth_image);
}

bool CameraManager::StartPipeline()
{
    // Init camera

    uint32_t device_count = k4a_device_get_installed_count();
    if(device_count == 0) {
        LOG(ERROR) << "Not enough connected devices to fulfill request\n";
        return false;
    }

    if(this->camera_idx + 1 > device_count) {
        LOG(ERROR) << "Only " << device_count << " devices available! Cannot fulfill request for index " << this->camera_idx << std::endl;
        return false;
    }

    if (K4A_FAILED(k4a_device_open(this->camera_idx, &this->current_device))) {
        LOG(ERROR) << "Failed to open device" << std::endl;
        return false;
    }
    LOG(INFO) << "Device open!" << std::endl;
    CurrentStatus = camera_manager_status_t::OPEN;

    k4a_device_configuration_t d_conf = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    d_conf.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    d_conf.color_resolution = K4A_COLOR_RESOLUTION_2160P;
    d_conf.depth_mode = this->enable_depth ? K4A_DEPTH_MODE_NFOV_UNBINNED : K4A_DEPTH_MODE_OFF;
    d_conf.camera_fps = K4A_FRAMES_PER_SECOND_5;

    if(K4A_FAILED(k4a_device_start_cameras(this->current_device, &d_conf))) {
        LOG(ERROR) << "Failed to start camera " << this->camera_idx << std::endl;
        this->CloseDevice();
        return false;
    }

    CurrentStatus = camera_manager_status_t::STREAMING;

    k4a_capture_t current_capture = nullptr;
    while(!CloseFlag) {
        k4a_wait_result_t wait_res = k4a_device_get_capture(this->current_device, &current_capture, 1000);
        if(wait_res == K4A_WAIT_RESULT_SUCCEEDED) {
            this->HandleCapture(&current_capture);
        } else {
            LOG(DEBUG) << "Capture get timed out." << std::endl;
        }
        if(current_capture != nullptr)
            k4a_capture_release(current_capture);
    }

    k4a_capture_release(current_capture);
    this->CloseDevice();

    return true;
}