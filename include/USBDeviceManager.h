#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>

struct USBDevice {
    std::string devicePath;
    std::string deviceName;
    std::string serialNumber;
    std::string mountPoint;
    bool isMounted;
    uint64_t capacity;
};

class USBDeviceManager {
public:
    using DeviceChangeCallback = std::function<void(const std::vector<USBDevice>&)>;

    USBDeviceManager();
    ~USBDeviceManager();

    // Start monitoring for USB device changes
    void startMonitoring(DeviceChangeCallback callback);
    
    // Stop monitoring
    void stopMonitoring();
    
    // Get current list of available USB devices
    std::vector<USBDevice> getAvailableDevices();
    
    // Mount/select a USB device by device path
    bool mountDevice(const std::string& devicePath);
    
    // Unmount the currently selected device
    bool unmountDevice();
    
    // Get currently mounted device
    USBDevice* getMountedDevice();
    
    // Write data to the mounted device
    bool writeData(const std::vector<uint8_t>& data, const std::string& filename);
    
    // Write data chunk (for streaming downloads)
    bool writeDataChunk(const uint8_t* data, size_t size);
    
    // Open file for writing on mounted device
    bool openFileForWriting(const std::string& filename);
    
    // Close currently open file
    void closeFile();

private:
    void monitoringThread();
    std::vector<USBDevice> scanDevices();

    std::thread m_monitorThread;
    std::atomic<bool> m_running;
    DeviceChangeCallback m_callback;
    
    std::vector<USBDevice> m_currentDevices;
    std::mutex m_devicesMutex;
    
    USBDevice* m_mountedDevice;
    std::mutex m_mountMutex;
    
    std::ofstream m_outputFile;
    std::mutex m_fileMutex;
};