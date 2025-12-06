#pragma once

#include "BaseMessage.h"
#include "QRDetection.h"
#include <string>

class EventMessage : public BaseMessage {
public:
    EventMessage() : BaseMessage() {}

    std::string Uid;

    QRDetection QrDetection;
};