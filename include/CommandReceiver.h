#pragma once
#include "MessageTypes.h"
#include "Messages/BaseMessage.h"
#include <vector>

class CommandReceiver {
public:
    CommandReceiver() {}
    virtual ~CommandReceiver();

    virtual void ProcessMessage(const BaseMessage&);
    virtual bool AllowMessage(MessageType);
};