#pragma once

#include "BaseMessage.h"
#include "MessageTypes.h"

#include <string>

class StringMessage : public BaseMessage {
public:
    StringMessage() : BaseMessage() {}

    std::string content;

};