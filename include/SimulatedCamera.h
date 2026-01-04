#pragma once

#include <string>
#include <atomic>
#include "EncodedImage.h"
#include "ICamera.h"

class SimulatedCamera : public ICamera {
public:
SimulatedCamera() : ICamera() {}


// Interface methods
bool OpenCamera() override;
std::pair<EncodedImage, EncodedImage> GetFrame() override;
bool CloseCamera() override;
bool IsOpen() override;

private:
std::atomic<bool> is_open;

};
