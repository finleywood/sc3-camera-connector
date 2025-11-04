#include <aixlog/aixlog.hpp>
#include <k4a/k4a.h>
#include <opencv2/opencv.hpp>
#include <vector>

#include "QRDetection.h"

typedef std::function<void(QRDetection*)> det_callback_t;

typedef enum {
    UNINIT,
    OPEN,
    STREAMING,
    CLOSED
} camera_manager_status_t;

class CameraManager {
public:
    camera_manager_status_t CurrentStatus = camera_manager_status_t::UNINIT;
    bool CloseFlag = false;

    CameraManager(int k4a_idx, bool include_d = false) :
        camera_idx(k4a_idx),
        enable_depth(include_d)
    {}

    void RegsiterDetectionCallback(det_callback_t);
    bool Start();

private:
    bool enable_depth = false;
    det_callback_t detection_callback;
    uint8_t camera_idx;
    std::unique_ptr<k4a_device_t> current_device = std::unique_ptr<k4a_device_t>();
    uint8_t retry_max;
    uint8_t current_retry = 0;
    bool StartPipeline();
    void CloseDevice();
    void HandleCapture(k4a_capture_t*);
    QRDetection* LookForQR(k4a_image_t*);
    cv::Mat* K4ARGBToCV(k4a_image_t*);
};