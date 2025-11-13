#include "CameraManager.h"

#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include "ImageUtils.h"

// Define static constexpr members
constexpr int CameraManager::MAX_TIMEOUTS;
constexpr int CameraManager::MAX_RECONNECT_ATTEMPTS;
constexpr int CameraManager::MAX_ENUM_ATTEMPTS;
constexpr int CameraManager::RECONNECT_COOLDOWN_SEC;

std::string generate_uuid_v4() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(0, 0xffffffff);

    uint32_t data[4];
    for (auto &d : data) {
        d = dis(gen);
    }

    // Set version (4) and variant bits per RFC 4122
    data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000; // Version 4
    data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000; // Variant 10xxxxxx

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << data[0] << '-'
        << std::setw(4) << ((data[1] >> 16) & 0xFFFF) << '-'
        << std::setw(4) << (data[1] & 0xFFFF) << '-'
        << std::setw(4) << ((data[2] >> 16) & 0xFFFF) << '-'
        << std::setw(4) << (data[2] & 0xFFFF)
        << std::setw(8) << data[3];

    return oss.str();
}

void CameraManager::RegisterDetectionCallback(det_callback_t callback_func) {
    this->detection_callback = callback_func;
}

void CameraManager::RegisterStateCallback(state_callback_t callback_func) {
    this->state_callback = callback_func;
}

bool CameraManager::Start(bool is_blocking) {
    if (current_state != CameraState::UNINITIALIZED && 
        current_state != CameraState::STOPPED) {
        std::cerr << "Cannot start: already running" << std::endl;
        return false;
    }
    
    close_flag = false;
    
    if (is_blocking) {
        // Run state machine in current thread
        RunStateMachine();
    } else {
        // Run state machine in separate thread
        state_machine_thread = std::thread(&CameraManager::RunStateMachine, this);
    }
    
    return true;
}

void CameraManager::Stop() {
    close_flag = true;
    if (state_machine_thread.joinable()) {
        state_machine_thread.join();
    }
}

void CameraManager::TransitionTo(CameraState new_state) {
    CameraState old_state = current_state.load();
    if (old_state == new_state) return;
    
    current_state = new_state;
    LOG(DEBUG) << "State transition: " << static_cast<int>(old_state) 
              << " -> " << static_cast<int>(new_state) << std::endl;
    
    if (state_callback) {
        state_callback(old_state, new_state);
    }
}

void CameraManager::RunStateMachine() {
    TransitionTo(CameraState::INITIALIZING);
    
    while (!close_flag) {
        CameraState state = current_state.load();
        
        switch (state) {
            case CameraState::UNINITIALIZED:
                HandleUninitialized();
                break;
            case CameraState::INITIALIZING:
                HandleInitializing();
                break;
            case CameraState::DEVICE_OPEN:
                HandleDeviceOpen();
                break;
            case CameraState::STARTING_CAMERAS:
                HandleStartingCameras();
                break;
            case CameraState::STREAMING:
                HandleStreaming();
                break;
            case CameraState::CAPTURE_TIMEOUT:
                HandleCaptureTimeout();
                break;
            case CameraState::HEALTH_CHECK:
                HandleHealthCheck();
                break;
            case CameraState::DISCONNECTED:
                HandleDisconnected();
                break;
            case CameraState::RECONNECTING:
                HandleReconnecting();
                break;
            case CameraState::WAITING_FOR_DEVICE:
                HandleWaitingForDevice();
                break;
            case CameraState::RECONNECT_FAILED:
                HandleReconnectFailed();
                break;
            case CameraState::ERROR:
                HandleError();
                break;
            case CameraState::STOPPING:
                HandleStopping();
                break;
            case CameraState::STOPPED:
                HandleStopped();
                return; // Exit state machine
        }
        
        // Small sleep to prevent busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Clean shutdown requested
    if (current_state != CameraState::STOPPED) {
        TransitionTo(CameraState::STOPPING);
        HandleStopping();
    }
}

void CameraManager::HandleUninitialized() {
    // Waiting for Start() to be called
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void CameraManager::HandleInitializing() {
    LOG(INFO) << "Initializing device..." << std::endl;
    
    if (OpenDevice()) {
        TransitionTo(CameraState::DEVICE_OPEN);
    } else {
        LOG(ERROR) << "Failed to open device" << std::endl;
        TransitionTo(CameraState::ERROR);
    }
}

void CameraManager::HandleDeviceOpen() {
    LOG(INFO) << "Device opened, configuring cameras..." << std::endl;
    TransitionTo(CameraState::STARTING_CAMERAS);
}

void CameraManager::HandleStartingCameras() {
    if (StartCameras()) {
        consecutive_timeouts = 0;
        TransitionTo(CameraState::STREAMING);
    } else {
        LOG(ERROR) << "Failed to start cameras" << std::endl;
        TransitionTo(CameraState::ERROR);
    }
}

void CameraManager::HandleStreaming() {
    k4a_capture_t capture = nullptr;
    k4a_wait_result_t result = k4a_device_get_capture(
        current_device, &capture, 1000);
    
    if (result == K4A_WAIT_RESULT_SUCCEEDED) {
        consecutive_timeouts = 0;
        HandleCapture(&capture);
        if (capture != nullptr) {
            k4a_capture_release(capture);
        }
    } else if (result == K4A_WAIT_RESULT_TIMEOUT) {
        consecutive_timeouts++;
        if (consecutive_timeouts >= MAX_TIMEOUTS) {
            LOG(INFO) << "Too many timeouts, checking health..." << std::endl;
            TransitionTo(CameraState::HEALTH_CHECK);
        }
    } else {
        LOG(WARNING) << "Capture failed" << std::endl;
        TransitionTo(CameraState::DISCONNECTED);
    }
}

void CameraManager::HandleCaptureTimeout() {
    // This is handled in STREAMING state
    TransitionTo(CameraState::HEALTH_CHECK);
}

void CameraManager::HandleHealthCheck() {
    LOG(INFO) << "Performing health check..." << std::endl;
    
    if (IsDeviceConnected()) {
        LOG(INFO) << "Device OK, resuming streaming" << std::endl;
        consecutive_timeouts = 0;
        TransitionTo(CameraState::STREAMING);
    } else {
        LOG(INFO) << "Device not responding" << std::endl;
        TransitionTo(CameraState::DISCONNECTED);
    }
}

void CameraManager::HandleDisconnected() {
    LOG(INFO) << "Device disconnected!" << std::endl;
    CloseDevice();
    reconnect_attempts = 0;
    device_enum_attempts = 0;
    TransitionTo(CameraState::RECONNECTING);
}

void CameraManager::HandleReconnecting() {
    LOG(INFO) << "Starting reconnection process..." << std::endl;
    LOG(INFO) << "Wating for device." << std::endl;
    CleanupResources();
    TransitionTo(CameraState::WAITING_FOR_DEVICE);
}

void CameraManager::HandleWaitingForDevice() {
    device_enum_attempts++;
    
    LOG(DEBUG) << "Waiting for device (attempt " << device_enum_attempts 
              << "/" << MAX_ENUM_ATTEMPTS << ")..." << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    uint32_t device_count = k4a_device_get_installed_count();
    
    if (device_count > camera_idx) {
        LOG(INFO) << "Device detected, attempting to open..." << std::endl;
        device_enum_attempts = 0;
        
        if (OpenDevice()) {
            TransitionTo(CameraState::DEVICE_OPEN);
            reconnect_attempts = 0;
        } else {
            reconnect_attempts++;
            if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
                TransitionTo(CameraState::RECONNECT_FAILED);
            }
        }
    } else if (device_enum_attempts >= MAX_ENUM_ATTEMPTS) {
        device_enum_attempts = 0;
        reconnect_attempts++;
        
        if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
            TransitionTo(CameraState::RECONNECT_FAILED);
        }
    }
}

void CameraManager::HandleReconnectFailed() {
    LOG(WARNING) << "Reconnection failed after " << reconnect_attempts 
              << " attempts" << std::endl;
    
    last_reconnect_fail_time = std::chrono::steady_clock::now();
    
    // Wait cooldown period
    std::this_thread::sleep_for(std::chrono::seconds(RECONNECT_COOLDOWN_SEC));
    
    // Reset and try again
    reconnect_attempts = 0;
    device_enum_attempts = 0;
    LOG(INFO) << "Retrying after cooldown..." << std::endl;
    TransitionTo(CameraState::WAITING_FOR_DEVICE);
}

void CameraManager::HandleError() {
    LOG(ERROR) << "Error state - cleaning up" << std::endl;
    CleanupResources();
    TransitionTo(CameraState::STOPPED);
}

void CameraManager::HandleStopping() {
    LOG(INFO) << "Stopping camera manager..." << std::endl;
    CleanupResources();
    TransitionTo(CameraState::STOPPED);
}

void CameraManager::HandleStopped() {
    LOG(INFO) << "Camera manager stopped" << std::endl;
    // Terminal state - do nothing
}

bool CameraManager::OpenDevice() {
    uint32_t device_count = k4a_device_get_installed_count();
    
    if (device_count == 0) {
        LOG(WARNING) << "No devices found" << std::endl;
        return false;
    }
    
    if (camera_idx >= device_count) {
        LOG(ERROR) << "Camera index " << camera_idx 
                  << " out of range (only " << device_count 
                  << " devices available)" << std::endl;
        return false;
    }
    
    // Disable k4a internal logger as it bloats the log
    k4a_set_debug_message_handler(NULL, NULL, K4A_LOG_LEVEL_OFF);
    if (K4A_FAILED(k4a_device_open(camera_idx, &current_device))) {
        LOG(ERROR) << "Failed to open device " << camera_idx << std::endl;
        return false;
    }
    
    LOG(INFO) << "Device " << camera_idx << " opened successfully" << std::endl;
    return true;
}

bool CameraManager::StartCameras() {
    device_config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    device_config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    device_config.color_resolution = K4A_COLOR_RESOLUTION_2160P;
    device_config.depth_mode = enable_depth ? 
        K4A_DEPTH_MODE_NFOV_UNBINNED : K4A_DEPTH_MODE_OFF;
    device_config.camera_fps = K4A_FRAMES_PER_SECOND_5;
    
    if (K4A_FAILED(k4a_device_start_cameras(current_device, &device_config))) {
        LOG(ERROR) << "Failed to start cameras" << std::endl;
        return false;
    }
    
    LOG(INFO) << "Cameras started successfully" << std::endl;
    return true;
}

bool CameraManager::IsDeviceConnected() {
    if (current_device == nullptr) {
        return false;
    }
    
    // Try to get serial number as health check
    size_t serial_size = 0;
    k4a_buffer_result_t result = k4a_device_get_serialnum(
        current_device, nullptr, &serial_size);
    
    return result != K4A_BUFFER_RESULT_FAILED;
}

void CameraManager::CloseDevice() {
    if (current_device == nullptr) {
        return;
    }
    
    LOG(INFO) << "Closing device..." << std::endl;
    k4a_device_stop_cameras(current_device);
    k4a_device_close(current_device);
    current_device = nullptr;
}

void CameraManager::CleanupResources() {
    CloseDevice();
    prev_detection = QRDetection{};
    cooldown_active = false;
    num_frames_since_last = 0;
}

QRDetection CameraManager::LookForQR(k4a_image_t* img) {
    cv::Mat cv_mat_img = ImageUtils::K4ARGBToCV(img);
    
    cv::QRCodeDetector cv_qr;
    std::vector<cv::Point> qr_detections;
    std::string found_data;
    try {
        found_data = cv_qr.detectAndDecode(cv_mat_img, qr_detections);
    } catch(std::exception& e) {
        LOG(WARNING) << "QR Code detection failed. Due to " << e.what() << std::endl;
    }
    
    if (!found_data.empty()) {
        QRDetection found_qr;
        found_qr.identifier = found_data;
        found_qr.uid = generate_uuid_v4();
    }
    return {};
}

void CameraManager::HandleCapture(k4a_capture_t* cap) {
    if (cap == nullptr || *cap == nullptr) return;
    
    k4a_image_t color_image = k4a_capture_get_color_image(*cap);
    k4a_image_t depth_image = nullptr;
    
    if (enable_depth) {
        depth_image = k4a_capture_get_depth_image(*cap);
    }
    
    if (color_image != nullptr) {
        QRDetection found_qr;
        
        if (!cooldown_active) {
            found_qr = this->LookForQR(&color_image);
        }
        
        if (!found_qr.identifier.empty() && 
            found_qr.identifier != prev_detection.identifier) {
            
            EncodedImage rgb_jpeg = ImageUtils::K4AToJpeg(&color_image);
            if (!rgb_jpeg.valid()) {
                LOG(ERROR) << "Failed to encode RGB image" << std::endl;
                if (color_image != nullptr) k4a_image_release(color_image);
                if (depth_image != nullptr) k4a_image_release(depth_image);
                return;
            }
            
            found_qr.rgb_img = std::move(rgb_jpeg);
            
            if (enable_depth && depth_image != nullptr) {
                EncodedImage depth_png = ImageUtils::K4AToPng(&depth_image);
                if (depth_png.valid()) {
                    found_qr.depth_image = std::move(depth_png);
                }
            }
            
            auto now = std::chrono::system_clock::now();
            found_qr.event_time = std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();
            
            if (detection_callback) {
                detection_callback(found_qr);
            }
        }
        
        // Cooldown management
        if ((prev_detection.identifier != found_qr.identifier) && !cooldown_active) {
            cooldown_active = true;
            num_frames_since_last = 0;
            LOG(DEBUG) << "QR lost. Cooldown active for " 
                      << max_frames_since_last << " frames" << std::endl;
            prev_detection = QRDetection{};
        }
        
        if (num_frames_since_last >= max_frames_since_last && cooldown_active) {
            cooldown_active = false;
            num_frames_since_last = 0;
            LOG(DEBUG) << "Cooldown over" << std::endl;
        }
        
        if (cooldown_active) {
            num_frames_since_last++;
        }
        
        prev_detection = found_qr;
    }
    
    if (color_image != nullptr) k4a_image_release(color_image);
    if (depth_image != nullptr) k4a_image_release(depth_image);
}