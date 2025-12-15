#include "USBDownload.h"
#include <aixlog/aixlog.hpp>
#include <json/json.hpp>

extern "C" {

#include <miniz/miniz.h>

}

using json = nlohmann::json;

std::string USBDownload::getCmdOutput(std::string cmd, bool remove_whitespace, int* rt_code) {
  FILE* proc = popen(cmd.c_str(), "r");
  if(!proc) return "";

  char buffer[256];
  std::string result;

  
  if(fgets(buffer, sizeof(buffer), proc)) {
    result = buffer;
    if(remove_whitespace) {
      result.erase(result.find_last_not_of(" \n\r\t") + 1);
    }
  }

  int status = pclose(proc);
  if(rt_code != nullptr) {
    if(status != -1) {
       if(WIFEXITED(status)) {
          *rt_code = WEXITSTATUS(status);
       } else {
          *rt_code = -1;
       }
    } else {
      *rt_code = -1;
    }
  }
  
  return result;
}

WriteablePart USBDownload::getUSBDevice() {
 WriteablePart wp_res;
 wp_res.devicePath = getCmdOutput("/sbin/blkid -o device -t TYPE=vfat /dev/sd*", true);
 return wp_res;
}

WriteablePart& USBDownload::getMountPoint(WriteablePart& wp) {
  std::string cmd = std::string("/bin/lsblk -o MOUNTPOINT ") + wp.devicePath; 
  std::string cur_mnt_pt = getCmdOutput(cmd, true);
  if(cur_mnt_pt.empty()) {
    // Doesn't exist, need to make one
    std::string proposed_mnt = generateMountPoint(wp);
    // Ensure folder exists
    cmd = std::string("mkdir -p ") + proposed_mnt;
    getCmdOutput(cmd, true);
    // Now ensure mounted
    wp.mountPoint = proposed_mnt;
    bool could_mount = ensureMounted(wp);
    if(!could_mount) wp.mountPoint = "";
  } else {
    wp.mountPoint = cur_mnt_pt;
  }
  return wp;
}

std::string USBDownload::generateMountPoint(const WriteablePart& wp) {
  if(wp.devicePath.empty()) return "";

  auto pos = wp.devicePath.find_last_of('/');
  std::string last_part = (pos == std::string::npos) ? wp.devicePath : wp.devicePath.substr(pos + 1);
  std::string mnt_path = std::string("/mnt/export_") + last_part;
  return mnt_path;
}

bool USBDownload::ensureMounted(const WriteablePart& wp) {
  if(wp.devicePath.empty() || wp.mountPoint.empty()) return false;

  std::string cmd = std::string("/bin/mount ") + wp.devicePath + std::string(" ") + wp.mountPoint;
  int ret_code = -1; 
  getCmdOutput(cmd, true, &ret_code); 
  return ret_code == 0;
}

bool USBDownload::SaveDetection(QRDetection& qr_det) {
  if(!qr_det.valid() || !this->wp.valid()) return false;
  
  // Construct output 
  json output_obj;
  output_obj["patient_id"] = qr_det.identifier;
  output_obj["datetime"] = qr_det.event_time;
  output_obj["weight"] = 0.0f;

  // Construct zip in memory

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  std::string out_name = generateOutputPath(this->wp);
  if(!mz_zip_writer_init_file(&zip, out_name.c_str(), 0); // Fill this out, needs output name 
  

  return true;
}

std::string USBDownload::generateOutputPath(const WriteablePart& wp) {
 if(wp.mountPoint.empty()) return "";

 // Do timestamp gen on unix timetamp and get date folder.
}
