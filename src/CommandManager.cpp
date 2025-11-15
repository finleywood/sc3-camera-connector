#include "CommandManager.h"
#include <aixlog/aixlog.hpp>

CommandManager& CommandManager::GetCM()
{
    static CommandManager instance;
    return instance;
}

void CommandManager::Register(std::string referrer_id, CommandReceiver* cr)
{
    RegisteredCR registered_item;
    registered_item.cr = cr;
    registered_item.referrer_id = referrer_id;
    if(registered_item.valid()) {
        LOG(INFO) << "Registered new command receiver with the command manager: " << referrer_id.c_str() << "." << std::endl;
        this->Receivers.push_back(registered_item);
    }
}

void CommandManager::SubmitMessage(MessageType msg_type, std::string msg_content)
{
    for(auto& recv : this->Receivers) {
        if(!recv.valid())
            return;

        if(recv.cr->AllowMessage(msg_type))
            recv.cr->ProcessMessage(msg_type, msg_content);
    }
}