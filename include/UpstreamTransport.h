#pragma once

#include "UploadItem.h"

class UpstreamTransport {
public:
    UpstreamTransport() {}

    virtual bool UploadQueueItem(UploadItem);

    bool USBFallback(UploadItem);
    
};