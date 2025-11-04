#include <stdio.h>
#include <aixlog/aixlog.hpp>
#include "CameraManager.h"
#include "QRDetection.h"

void detection_callback(QRDetection*)
{

}

int main()
{
    // Init logger
    AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace);

    auto cam_manager_1 = new CameraManager(0, true);
    cam_manager_1->RegsiterDetectionCallback(detection_callback);
    bool status = cam_manager_1->Start();
    return status ? 0 : -1;
}