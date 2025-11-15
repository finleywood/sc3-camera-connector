#pragma once
#include "MessageTypes.h"
#include <vector>

class CommandReceiver {
public:
    CommandReceiver() {}
    virtual ~CommandReceiver();

    virtual void ProcessMessage(MessageType, std::string);
    virtual bool AllowMessage(MessageType);
};