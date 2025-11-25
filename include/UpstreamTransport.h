#pragma once

#include "UploadItem.h"

class UpstreamTransport {
public:
    UpstreamTransport() {}
    virtual ~UpstreamTransport();

    virtual bool UploadQueueItem(UploadItem) = 0;

    bool USBFallback(UploadItem);
    
};