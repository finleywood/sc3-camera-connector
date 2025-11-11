#pragma once

#include "UpstreamTransport.h"

class HttpTransport : public UpstreamTransport {
public:
    HttpTransport(std::string server_endpoint) :
        UpstreamTransport(),
        server_url(server_endpoint)
     {}

    virtual bool UploadQueueItem(UploadItem) override;

private:
    std::string server_url;

    std::string InitMetadata(std::string, uint64_t);
    bool UploadRGB(std::string, uint8_t*, size_t);
    bool UploadDepth(std::string, uint8_t*, size_t);
    bool PutBin(std::string, bool, uint8_t*, size_t);
};