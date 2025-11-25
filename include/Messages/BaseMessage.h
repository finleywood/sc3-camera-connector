#pragma once

#include "MessageTypes.h"

class BaseMessage {
public:
    BaseMessage() : MsgType(MessageType::UNKNOWN) {}
    virtual ~BaseMessage() = 0;

    MessageType MsgType;

    // Allow copy
    BaseMessage(const BaseMessage&) = default;
    BaseMessage& operator=(const BaseMessage&) = default;

    // Disable move
    BaseMessage(BaseMessage&&) = delete;
    BaseMessage& operator=(BaseMessage&&) = delete;
};