#pragma once

#include <aixlog/aixlog.hpp>
#include <vector>
#include <string>
#include <stdio.h>

#include "QRDetection.h"
#include "Util.h"

#define CMD_TIMEOUT (5000)

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

  ~USBDownload() {
    if(!wp.valid()) return;

    // Attempt to unmount and close
    std::string umount_cmd = std::string("sudo umount ") + wp.devicePath;
    Util::getCmdOutput(umount_cmd, false, nullptr, CMD_TIMEOUT);
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
