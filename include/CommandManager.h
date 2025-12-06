#pragma once

#include "MessageTypes.h"
#include "Messages/BaseMessage.h"
#include "CommandReceiver.h"
#include <vector>
#include <map>
#include <thread>
#include <memory>
#include <type_traits>
#include <utility>

struct RegisteredCR {
    CommandReceiver* cr;
    std::string referrer_id;
    bool valid() { return cr != nullptr; };
};

class CommandManager {
public:


    static CommandManager& GetCM();

    template<typename T>
    void SubmitMessage(T&& msg)
    {
        using U = typename std::decay<T>::type;

        static_assert(std::is_base_of<BaseMessage, U>::value,
                    "SubmitMessage requires a BaseMessage-derived type");

        auto stored = std::make_shared<U>(std::forward<T>(msg));

        for(auto& recv : this->Receivers) {
            if(!recv.valid()) continue;
            if(!recv.cr->AllowMessage(stored->MsgType)) continue;

            CommandReceiver* cr_ptr = recv.cr;
            std::thread([cr_ptr, stored]() {
                cr_ptr->ProcessMessage(static_cast<const BaseMessage&>(*stored));
            }).detach();
        }
    }

    void Register(std::string, CommandReceiver*);


    // no copy & move
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;
private:
    CommandManager() {}
    std::vector<RegisteredCR> Receivers;
};