#include "USBDeviceManager.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <libudev.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

USBDeviceManager::USBDeviceManager() 
    : m_running(false), m_mountedDevice(nullptr) {
}

USBDeviceManager::~USBDeviceManager() {
    stopMonitoring();
    closeFile();
    if (m_mountedDevice) {
        unmountDevice();
        delete m_mountedDevice;
    }
}

void USBDeviceManager::startMonitoring(DeviceChangeCallback callback) {
    if (m_running) return;
    
    m_callback = callback;
    m_running = true;
    m_monitorThread = std::thread(&USBDeviceManager::monitoringThread, this);
}

void USBDeviceManager::stopMonitoring() {
    if (!m_running) return;
    
    m_running = false;
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
}

void USBDeviceManager::monitoringThread() {
    std::vector<USBDevice> previousDevices;
    
    while (m_running) {
        auto devices = scanDevices();
        
        // Check if devices changed
        bool changed = false;
        if (devices.size() != previousDevices.size()) {
            changed = true;
        } else {
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i].devicePath != previousDevices[i].devicePath) {
                    changed = true;
                    break;
                }
            }
        }
        
        if (changed) {
            std::lock_guard<std::mutex> lock(m_devicesMutex);
            m_currentDevices = devices;
            
            if (m_callback) {
                m_callback(devices);
            }
        }
        
        previousDevices = devices;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

std::vector<USBDevice> USBDeviceManager::getAvailableDevices() {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    return m_currentDevices;
}

std::vector<USBDevice> USBDeviceManager::scanDevices() {
    std::vector<USBDevice> devices;
    
    struct udev* udev = udev_new();
    if (!udev) return devices;
    
    struct udev_enumerate* enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "ID_BUS", "usb");
    udev_enumerate_scan_devices(enumerate);
    
    struct udev_list_entry* devices_list = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry;
    
    udev_list_entry_foreach(entry, devices_list) {
        const char* path = udev_list_entry_get_name(entry);
        struct udev_device* dev = udev_device_new_from_syspath(udev, path);
        
        if (dev) {
            const char* devnode = udev_device_get_devnode(dev);
            const char* devtype = udev_device_get_devtype(dev);

            // Check if device is mounted at system critical paths
            bool isSystemDisk = false;
            if (devnode) {
                std::ifstream mounts("/proc/mounts");
                std::string line;
                while (std::getline(mounts, line)) {
                    if (line.find(devnode) != std::string::npos) {
                        // Check if mounted at /, /home, /boot, /usr, /var, etc.
                        if (line.find(" / ") != std::string::npos ||
                            line.find(" /home ") != std::string::npos ||
                            line.find(" /boot ") != std::string::npos ||
                            line.find(" /usr ") != std::string::npos ||
                            line.find(" /var ") != std::string::npos ||
                            line.find(" /opt ") != std::string::npos ||
                            line.find(" /tmp ") != std::string::npos) {
                            isSystemDisk = true;
                            break;
                        }
                    }
                }
            }
            
            // Only add partition devices or disk devices
            if (devnode && (!devtype || strcmp(devtype, "partition") == 0) && !isSystemDisk) {
                USBDevice device;
                device.devicePath = devnode;
                device.deviceName = udev_device_get_sysattr_value(dev, "device/model") 
                                    ? udev_device_get_sysattr_value(dev, "device/model") 
                                    : "USB Device";
                device.serialNumber = udev_device_get_property_value(dev, "ID_SERIAL_SHORT") 
                                      ? udev_device_get_property_value(dev, "ID_SERIAL_SHORT") 
                                      : "";
                device.isMounted = false;
                device.capacity = 0;
                
                devices.push_back(device);
            }
            
            udev_device_unref(dev);
        }
    }
    
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    
    return devices;
}

bool USBDeviceManager::mountDevice(const std::string& devicePath) {
    std::lock_guard<std::mutex> lock(m_mountMutex);
    
    if (m_mountedDevice) {
        std::cerr << "Device already mounted. Unmount first." << std::endl;
        return false;
    }
    
    auto devices = getAvailableDevices();
    auto it = std::find_if(devices.begin(), devices.end(), 
                          [&devicePath](const USBDevice& d) { 
                              return d.devicePath == devicePath; 
                          });
    
    if (it == devices.end()) {
        std::cerr << "Device not found: " << devicePath << std::endl;
        return false;
    }

    // Create mount point
    std::string mountPoint = "/mnt/usb_" + it->serialNumber;
    mkdir(mountPoint.c_str(), 0755);
    
    // Mount the device
    if (mount(devicePath.c_str(), mountPoint.c_str(), "vfat", 0, nullptr) == 0) {
        m_mountedDevice = new USBDevice(*it);
        m_mountedDevice->mountPoint = mountPoint;
        m_mountedDevice->isMounted = true;
        return true;
    } else {
        std::cerr << "Failed to mount device: " << devicePath << std::endl;
        return false;
    }
}

bool USBDeviceManager::unmountDevice() {
    std::lock_guard<std::mutex> lock(m_mountMutex);
    
    if (!m_mountedDevice) return false;
    
    closeFile();
    
#ifdef __linux__
    if (m_mountedDevice->isMounted) {
        umount(m_mountedDevice->mountPoint.c_str());
    }
#endif
    
    delete m_mountedDevice;
    m_mountedDevice = nullptr;
    return true;
}

USBDevice* USBDeviceManager::getMountedDevice() {
    std::lock_guard<std::mutex> lock(m_mountMutex);
    return m_mountedDevice;
}

bool USBDeviceManager::openFileForWriting(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    
    if (!m_mountedDevice) {
        std::cerr << "No device mounted" << std::endl;
        return false;
    }
    
    std::string fullPath = m_mountedDevice->mountPoint + "/" + filename;
    m_outputFile.open(fullPath, std::ios::binary | std::ios::out);
    
    return m_outputFile.is_open();
}

void USBDeviceManager::closeFile() {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_outputFile.is_open()) {
        m_outputFile.close();
    }
}

bool USBDeviceManager::writeDataChunk(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    
    if (!m_outputFile.is_open()) {
        std::cerr << "No file open for writing" << std::endl;
        return false;
    }
    
    m_outputFile.write(reinterpret_cast<const char*>(data), size);
    return m_outputFile.good();
}

bool USBDeviceManager::writeData(const std::vector<uint8_t>& data, const std::string& filename) {
    if (!m_mountedDevice) {
        std::cerr << "No device mounted" << std::endl;
        return false;
    }
    
    std::string fullPath = m_mountedDevice->mountPoint + "/" + filename;
    std::ofstream file(fullPath, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fullPath << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    
    return true;
}