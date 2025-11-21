#pragma once

#include "QRDetection.h"
#include <string>

struct UploadItem {
    QRDetection qr_det;
    std::string uid;
    uint8_t retry_num = 0;
};