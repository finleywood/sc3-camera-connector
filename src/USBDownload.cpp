#include "USBDownload.h"
#include <json/json.hpp>
#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <errno.h>

extern "C" {

#include <miniz/miniz.h>

}

using json = nlohmann::json;

std::string unixTimestampToDateString(uint64_t timestamp)
{
    time_t ts = static_cast<time_t>(timestamp);

    struct tm tm {};
    localtime_r(&ts, &tm);

    char buffer[11]; // "yyyy_mm_dd" + null
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%04d_%02d_%02d",
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday
    );

    return std::string(buffer);
}

WriteablePart USBDownload::getUSBDevice() {
 WriteablePart wp_res;
 std::string out = Util::getCmdOutput("/bin/lsblk -pnr -o NAME,FSTYPE /dev/sd*", true, nullptr, CMD_TIMEOUT);
 std::vector<std::string> lines;
 std::istringstream stream(out);
 std::string line;
 while(std::getline(stream, line)) {
  lines.push_back(line);
 }

 if(lines.size() == 0)
  return wp_res;

  for(std::string l : lines) {
    std::vector<std::string> words;
    std::istringstream iss(l);
    std::string word;
    while(iss >> word) {
      words.push_back(word);
    }

    if(words.size() != 2) continue;

    if(words.at(1) == "vfat") {
      wp_res.devicePath = words.at(0);
      break;
    }
  }

 return wp_res;
}

WriteablePart& USBDownload::getMountPoint(WriteablePart& wp) {
  if(!wp.valid()) return wp;
  std::string cmd = std::string("/bin/lsblk -n -o MOUNTPOINT ") + wp.devicePath; 
  std::string cur_mnt_pt = Util::getCmdOutput(cmd, true, nullptr, CMD_TIMEOUT);
  if(cur_mnt_pt.empty()) {
    // Doesn't exist, need to make one
    std::string proposed_mnt = generateMountPoint(wp);
    // Ensure folder exists
    int ret1 = mkdir(proposed_mnt.c_str(), 0775);
    if(ret1 == -1) {
      if(errno == EEXIST) {
        ret1 = 0;
      }
    }
    // Double Ensure permissions are correct
    int ret2 = chmod(proposed_mnt.c_str(), 0775);
    int ret_total = ret1 + ret2;
    if(ret_total != 0) {
      LOG(WARNING) << "Could not create mount path " << proposed_mnt.c_str() << std::endl;
    }
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
  std::string mnt_path = std::string("/mnt/sc3_mounts/export_") + last_part;
  return mnt_path;
}

bool USBDownload::ensureMounted(const WriteablePart& wp) {
  if(wp.devicePath.empty() || wp.mountPoint.empty()) return false;

  uid_t uid = geteuid();
  gid_t gid = getegid();
  std::string cmd = std::string("sudo /bin/mount -o ") + std::string("uid=") + std::to_string(uid) +
    std::string(",gid=") + std::to_string(gid) +
    std::string(",dmask=002,fmask=113 ") + wp.devicePath + std::string(" ") + wp.mountPoint;
  int ret_code = -1; 
  Util::getCmdOutput(cmd, true, &ret_code, CMD_TIMEOUT); 
  return ret_code == 0;
}

bool USBDownload::SaveDetection(QRDetection& qr_det) {
  if(!qr_det.valid() || !wp.valid()) return false;
  
  // Construct output 
  json output_obj;
  output_obj["patient_id"] = qr_det.identifier;
  output_obj["datetime"] = qr_det.event_time;
  output_obj["weight"] = qr_det.weight;

  // Construct zip in memory

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  std::string out_name = generateOutputPath(this->wp, qr_det);
  if(!mz_zip_writer_init_file(&zip, out_name.c_str(), 0)) {
    LOG(INFO) << "Was unable to create zip buffer." << std::endl;
    return false;
  }
  
  if(!mz_zip_writer_add_mem(&zip, "snapshot/rgb.jpeg", qr_det.rgb_img.data.get(), qr_det.rgb_img.size, MZ_BEST_COMPRESSION)) {
    LOG(WARNING) << "Could not add rgb image to zip file." << std::endl;
    return false;
  }

  if(!mz_zip_writer_add_mem(&zip, "snapshot/depth.png", qr_det.depth_image.data.get(), qr_det.depth_image.size, MZ_BEST_COMPRESSION)) {
    LOG(WARNING) << "Could not add depth to zip file." << std::endl;
    return false;
  }

  std::string manifest_str = output_obj.dump();
  if(!mz_zip_writer_add_mem(&zip, "snapshot/metadata.json", manifest_str.data(), manifest_str.size(), MZ_BEST_COMPRESSION)) {
    LOG(WARNING) << "Could not add manifest zip file." << std::endl;
    return false;
  }

  if (!mz_zip_writer_finalize_archive(&zip)) {
    LOG(WARNING) << "Failed to finalize zip archive.";
    return false;
  }

  mz_zip_writer_end(&zip);

  return true;
}

std::string USBDownload::generateOutputPath(const WriteablePart& wp, const QRDetection& qr) {
  if(wp.mountPoint.empty()) return "";

  std::string date_str = unixTimestampToDateString(qr.event_time);
  if(date_str.empty()) return "";

  std::string out_path_dir = wp.mountPoint + std::string("/") + date_str;
  // ensure dir exists

  std::string cmd = std::string("mkdir -p ") + out_path_dir;
  int ret = mkdir(out_path_dir.c_str(), 0775);
  if(ret == -1) {
    if(errno == EEXIST) {
      // Already exists, doesn't matter
      ret = 0;
    }
  }
  int ret2 = chmod(out_path_dir.c_str(), 0775);
  int ret_total = ret + ret2;
  if(ret_total != 0) {
    LOG(WARNING) << "Could not create path: " << out_path_dir.c_str() << std::endl;
    return "";
  }
  return out_path_dir + std::string("/") + std::to_string(qr.identifier) + std::string("_") + std::to_string(qr.event_time) + std::string(".zip");
}
