#pragma once

#include "QRDetection.h"

struct UploadItem {
    QRDetection qr_det;
    uint8_t retry_num = 0;
};