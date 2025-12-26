#pragma once

#include <aixlog/aixlog.hpp>
#include <vector>
#include <string>
#include <stdio.h>

#include "QRDetection.h"

struct WriteablePart {
  std::string partType = "vfat";
  std::string devicePath;
  std::string mountPoint;
  bool valid() { return !(devicePath.empty() && mountPoint.empty()); };
};

class USBDownload {
public:
  USBDownload() {
    this->wp = getUSBDevice();
    getMountPoint(this->wp);
    LOG(INFO) << "USB Store Started, registered device path: " << wp.devicePath.c_str() << " Mount point: " << wp.mountPoint.c_str() << std::endl;
  }

  bool SaveDetection(QRDetection&);
private:
  WriteablePart wp;

  std::string getCmdOutput(std::string, bool, int* = nullptr);
  WriteablePart getUSBDevice();
  WriteablePart& getMountPoint(WriteablePart&);
  bool ensureMounted(const WriteablePart&);
  std::string generateMountPoint(const WriteablePart&);
  std::string generateOutputPath(const WriteablePart&, const QRDetection&);
};
