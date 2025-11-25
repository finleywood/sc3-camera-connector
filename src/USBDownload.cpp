#include "USBDownload.h"
#include "Messages/StringMessage.h"
#include <chrono>

bool USBDownload::AllowMessage(MessageType msg_type)
{
    switch(msg_type) {
        case MessageType::USB_DOWNLOAD:
        case MessageType::USB_LIST:
            return true;
            break;
        default:
            return false;
            break;
    }
}

void USBDownload::ProcessMessage(const BaseMessage& msg)
{
    switch(msg.MsgType) {
        case MessageType::USB_DOWNLOAD:
            this->HandleUSBDownload(static_cast<const EventMessage&>(msg));
            break;
        case MessageType::USB_LIST:
            this->HandleUSBList();
            break;
        default:
            break;
    }
}

void USBDownload::HandleUSBDownload(const EventMessage& msg)
{
    
}

void USBDownload::HandleUSBList()
{
    std::vector<USBDevice> usb_devices = this->udm.getAvailableDevices();
    for(auto& dev : usb_devices) {
        std::string msg_content = "name=" + dev.deviceName + ",path=" + dev.devicePath;
        StringMessage dev_msg;
        dev_msg.MsgType = MessageType::USB_DEV;
        dev_msg.content = msg_content;
        CommandManager::GetCM().SubmitMessage(dev_msg);
    }
}