#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <map>
#include "UploadQueue.h"
#include "MessageTypes.h"
#include "Messages/BaseMessage.h"
#include "Messages/StringMessage.h"
#include "CommandManager.h"
#include "CommandReceiver.h"

class UINotifier : public CommandReceiver {
public:
    UINotifier(std::string socket_path)
        : CommandReceiver(),
        socket_path(socket_path),
        current_status(SocketStatus::UINITIALIZED)
    {
        CommandManager::GetCM().Register("UINotifier", this);
    }

    bool SendMessage(std::string);
    bool Start(bool is_blocking = false);
    void Stop();

    // Command Receiver stuff
    virtual bool AllowMessage(MessageType) override;
    virtual void ProcessMessage(const BaseMessage& msg) override;

private:
    std::atomic<SocketStatus> current_status;
    std::string socket_path;
    std::thread state_machine_thread;
    bool state_machine_close_flag;
    int server_fd = -1;
    int client_fd = -1;

    void RunSocketStateMachine();
    void SocketStateTransitionTo(SocketStatus);

    void HandleUninitialized();
    void HandleSocketCreated();
    void HandleSocketCreationFailure();
    void HandleRetry();
    void HandleWaitingForConnection();
    void HandleWaitingForMessage();
    void HandleClosing();

    void ProcessClientMessage(std::string);

    // Socket handlers IN
    void ProcessStartCollection(std::string);
    void ProcessUSBList();

    // Command manager handlers OUT
    void ProcessSendStringMessage(const StringMessage&);

};