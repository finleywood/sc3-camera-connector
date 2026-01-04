#include "ICamera.h"

ICamera::~ICamera() = default;

bool ICamera::OpenCamera() { return false; }
std::pair<EncodedImage, EncodedImage> ICamera::GetFrame() { return std::make_pair(EncodedImage{}, EncodedImage{}); }
bool ICamera::CloseCamera() { return false; }
bool ICamera::IsOpen() { return false; }