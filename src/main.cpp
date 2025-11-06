#include <stdio.h>
#include <aixlog/aixlog.hpp>
#include "CameraManager.h"
#include "UploadQueue.h"
#include "HttpTransport.h"
#include "UploadItem.h"

static UploadQueue* upload_queue;

void detection_callback(QRDetection detection)
{
    LOG(INFO) << "QR DETECTION - " << "Found ID: " << detection.identifier.c_str() << std::endl;
    if(upload_queue != nullptr) {
        LOG(INFO) << "Attempting to upload detection." << std::endl;
        try {
            UploadItem item_to_upload = {};
            item_to_upload.qr_det = detection;
            upload_queue->Enqueue(item_to_upload);
            LOG(INFO) << "Queuing success." << std::endl;
        } catch(std::exception& e) {
            LOG(WARNING) << "Unable to queue event for upload, because of: " << e.what() << std::endl;
        }
    }
}

int main()
{
    // Init logger
    AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace);

    // Start upload queue
    upload_queue = new UploadQueue(new HttpTransport(), 3);
    upload_queue->Start();

    auto cam_manager_1 = new CameraManager(0, true);
    cam_manager_1->RegsiterDetectionCallback(detection_callback);
    bool status = cam_manager_1->Start();

    // Stop upload queue if camera manager stops
    upload_queue->Stop();
    return status ? 0 : -1;
}