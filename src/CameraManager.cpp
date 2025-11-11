#include "CameraManager.h"

#include <chrono>

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

EncodedImage CameraManager::EncodeToJpeg(k4a_image_t* image, int quality)
{
    EncodedImage result;

    if (!image)
        return result;

    uint8_t* img_buf = k4a_image_get_buffer(*image);
    int width  = k4a_image_get_width_pixels(*image);
    int height = k4a_image_get_height_pixels(*image);
    k4a_image_format_t format = k4a_image_get_format(*image);

    cv::Mat mat;

    if (format == K4A_IMAGE_FORMAT_COLOR_BGRA32) {
        cv::Mat bgra(height, width, CV_8UC4, img_buf);
        cv::cvtColor(bgra, mat, cv::COLOR_BGRA2BGR);
    } 
    else if (format == K4A_IMAGE_FORMAT_COLOR_MJPG) {
        // Already JPEG-encoded, just copy
        result.size = k4a_image_get_size(*image);
        result.data.reset(new uint8_t[result.size]);
        memcpy(result.data.get(), img_buf, result.size);
        return result;
    } 
    else {
        LOG(ERROR) << "Unsupported image format for JPEG encoding" << std::endl;
        return result;
    }

    std::vector<uint8_t> jpeg_buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, quality };

    if (!cv::imencode(".jpg", mat, jpeg_buf, params)) {
        LOG(ERROR) << "JPEG encoding failed" << std::endl;
        return result;
    }

    result.size = jpeg_buf.size();
    result.data.reset(new uint8_t[result.size]);
    std::copy(jpeg_buf.begin(), jpeg_buf.end(), result.data.get());

    return result;
}

EncodedImage CameraManager::EncodeToPng(k4a_image_t* image) {
    EncodedImage result;
    
    if (image == nullptr) {
        return result;
    }
    
    int width = k4a_image_get_width_pixels(*image);
    int height = k4a_image_get_height_pixels(*image);
    k4a_image_format_t format = k4a_image_get_format(*image);
    uint8_t* buffer = k4a_image_get_buffer(*image);
    
    if (buffer == nullptr || width == 0 || height == 0) {
        return result;
    }
    
    cv::Mat mat;
    
    // Convert k4a format to OpenCV Mat
    switch (format) {
        case K4A_IMAGE_FORMAT_COLOR_BGRA32:
            mat = cv::Mat(height, width, CV_8UC4, buffer);
            break;
        case K4A_IMAGE_FORMAT_COLOR_MJPG:
            // MJPG needs decoding first
            {
                std::vector<uint8_t> jpgData(buffer, buffer + k4a_image_get_size(*image));
                mat = cv::imdecode(jpgData, cv::IMREAD_COLOR);
            }
            break;
        case K4A_IMAGE_FORMAT_DEPTH16:
        case K4A_IMAGE_FORMAT_IR16:
            mat = cv::Mat(height, width, CV_16UC1, buffer);
            break;
        case K4A_IMAGE_FORMAT_CUSTOM8:
            mat = cv::Mat(height, width, CV_8UC1, buffer);
            break;
        case K4A_IMAGE_FORMAT_CUSTOM16:
            mat = cv::Mat(height, width, CV_16UC1, buffer);
            break;
        default:
            return result;
    }
    
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
    
    return result;
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
        LOG(TRACE) << "Cooldown: " << (this->cooldown_active ? "Active" : "Not Active") << std::endl; 
        QRDetection found_qr;
        if(!cooldown_active)
            found_qr = this->LookForQR(&color_image);

        if(!found_qr.identifier.empty() && found_qr.identifier != this->prev_detection.identifier) {
            EncodedImage rgb_jpeg = this->EncodeToJpeg(&color_image);
            if(!rgb_jpeg.valid()) {
                LOG(ERROR) << "RGB JPEG INVALID, CANNOT CONTINUE!" << std::endl;
                if(color_image != nullptr)
                    k4a_image_release(color_image);
                if(depth_image != nullptr)
                    k4a_image_release(depth_image);
                return;
            }
            found_qr.rgb_img = rgb_jpeg;
            if(this->enable_depth) {
                EncodedImage depth_png = this->EncodeToPng(&depth_image);
                if(!depth_png.valid()) {
                    LOG(WARNING) << "DEPTH PNG INVALID, WILL CONTINUE WITHOUT." << std::endl;
                } else {
                    found_qr.depth_image = depth_png;
                }
            }
            auto clock = std::chrono::system_clock::now();
            uint64_t unix_time = std::chrono::duration_cast<std::chrono::seconds>(clock.time_since_epoch()).count();
            found_qr.event_time = unix_time;
            this->detection_callback(found_qr);
        } 

        // Spam guard
        if((this->prev_detection.identifier != found_qr.identifier) && !this->cooldown_active) {
            // Start cooldown
            this->cooldown_active = true;
            LOG(INFO) << "Lost QR. Cooldown active for next " << static_cast<int>(this->max_frames_since_last) << " frames." << std::endl;
            this->prev_detection = {};
        }

        if(this->num_frames_since_last >= this->max_frames_since_last && this->cooldown_active) {
            this->cooldown_active = false;
            LOG(INFO) << "Cooldown over. Starting detections again." << std::endl;
            this->num_frames_since_last = 0;
        }

        if(this->cooldown_active)
            this->num_frames_since_last = this->num_frames_since_last + 1;

        
        this->prev_detection = found_qr;
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