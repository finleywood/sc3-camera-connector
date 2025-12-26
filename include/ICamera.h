#pragma once

#include "EncodedImage.h"

class ICamera {
public:
ICamera() {}
virtual ~ICamera();

virtual bool OpenCamera();
virtual std::pair<EncodedImage, EncodedImage> GetFrame();
virtual bool CloseCamera();
virtual bool IsOpen();
};