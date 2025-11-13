#pragma once

#include <k4a/k4a.h>
#include <opencv2/opencv.hpp>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include "EncodedImage.h"
#include "QRDetection.h"

enum class CameraState {
    UNINITIALIZED,
    INITIALIZING,
    DEVICE_OPEN,
    STARTING_CAMERAS,
    STREAMING,
    CAPTURE_TIMEOUT,
    HEALTH_CHECK,
    DISCONNECTED,
    RECONNECTING,
    WAITING_FOR_DEVICE,
    RECONNECT_FAILED,
    ERROR,
    STOPPING,
    STOPPED
};

using det_callback_t = std::function<void(const QRDetection&)>;
using state_callback_t = std::function<void(CameraState, CameraState)>;

class CameraManager {
public:
    CameraManager(uint32_t camera_idx = 0, bool enable_depth = false)
        : camera_idx(camera_idx)
        , enable_depth(enable_depth)
        , current_state(CameraState::UNINITIALIZED)
    {}

    ~CameraManager() {
        Stop();
    }

    void RegisterDetectionCallback(det_callback_t callback_func);
    void RegisterStateCallback(state_callback_t callback_func);
    
    bool Start(bool is_blocking = false);
    void Stop();
    
    CameraState GetCurrentState() const { return current_state.load(); }
    bool IsStreaming() const { return current_state.load() == CameraState::STREAMING; }

private:
    // State machine
    std::atomic<CameraState> current_state;
    void TransitionTo(CameraState new_state);
    void RunStateMachine();
    
    // State handlers
    void HandleUninitialized();
    void HandleInitializing();
    void HandleDeviceOpen();
    void HandleStartingCameras();
    void HandleStreaming();
    void HandleCaptureTimeout();
    void HandleHealthCheck();
    void HandleDisconnected();
    void HandleReconnecting();
    void HandleWaitingForDevice();
    void HandleReconnectFailed();
    void HandleError();
    void HandleStopping();
    void HandleStopped();
    
    // Device management
    bool OpenDevice();
    bool StartCameras();
    bool IsDeviceConnected();
    void CloseDevice();
    void CleanupResources();
    
    // Capture handling
    void HandleCapture(k4a_capture_t* cap);
    QRDetection LookForQR(k4a_image_t* img);
    
    // Configuration
    uint32_t camera_idx;
    bool enable_depth;
    k4a_device_t current_device = nullptr;
    k4a_device_configuration_t device_config;
    
    // State tracking
    int consecutive_timeouts = 0;
    int reconnect_attempts = 0;
    int device_enum_attempts = 0;
    std::chrono::steady_clock::time_point last_reconnect_fail_time;
    
    // Limits
    static constexpr int MAX_TIMEOUTS = 5;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int MAX_ENUM_ATTEMPTS = 20;
    static constexpr int RECONNECT_COOLDOWN_SEC = 5;
    
    // QR detection
    det_callback_t detection_callback;
    state_callback_t state_callback;
    QRDetection prev_detection;
    bool cooldown_active = false;
    int num_frames_since_last = 0;
    int max_frames_since_last = 15;
    
    // Control
    std::atomic<bool> close_flag{false};
    std::thread state_machine_thread;
};