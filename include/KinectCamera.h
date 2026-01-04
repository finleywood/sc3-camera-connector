#pragma once

#include "ICamera.h"
#include <atomic>
#include <k4a/k4a.h>

class KinectCamera : public ICamera {
public:
KinectCamera() : ICamera() {}

// Interface methods
bool OpenCamera() override;
std::pair<EncodedImage, EncodedImage> GetFrame() override;
bool CloseCamera() override;
bool IsOpen() override;

private:

std::atomic<bool> is_open = { false };
k4a_device_t current_device = NULL;

};