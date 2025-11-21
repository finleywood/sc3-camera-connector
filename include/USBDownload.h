#pragma once

#include <thread>
#include <atomic>
#include <aixlog/aixlog.hpp>

#include "USBDeviceManager.h"
#include "CommandManager.h"
#include "CommandReceiver.h"
#include "MessageTypes.h"
#include "Messages/BaseMessage.h"
#include "Messages/EventMessage.h"

class USBDownload : public CommandReceiver {
public:
    USBDownload()
        : CommandReceiver()
        {
            CommandManager::GetCM().Register("USBDownload", this);
            this->udm.startMonitoring([](const std::vector<USBDevice>& devices) {
                LOG(INFO) << "USB devices changed. Available devices:\n";
                for (const auto& device : devices) {
                    LOG(INFO) << "  - " << device.deviceName << " (" << device.devicePath << ")\n";
                }
            });
        }

    virtual bool AllowMessage(MessageType) override;
    virtual void ProcessMessage(const BaseMessage&) override;

private:
    USBDeviceManager udm;

    void HandleUSBDownload(const EventMessage&);
    void HandleUSBList();
};