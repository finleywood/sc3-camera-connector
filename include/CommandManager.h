#pragma once

#include "MessageTypes.h"
#include "CommandReceiver.h"
#include <vector>
#include <map>

struct RegisteredCR {
    CommandReceiver* cr;
    std::string referrer_id;
    bool valid() { return cr != nullptr; };
};

class CommandManager {
public:


    static CommandManager& GetCM();
    void SubmitMessage(MessageType, std::string);

    void Register(std::string, CommandReceiver*);


    // no copy & move
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;
private:
    CommandManager() {}
    std::vector<RegisteredCR> Receivers;
};