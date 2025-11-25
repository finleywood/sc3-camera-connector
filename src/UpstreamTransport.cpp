#include "UpstreamTransport.h"

#include <aixlog/aixlog.hpp>

UpstreamTransport::~UpstreamTransport() = default;

bool UpstreamTransport::USBFallback(UploadItem item_to_download)
{
    LOG(INFO) << "Attempting USB fallback." << std::endl;

    return false;
}