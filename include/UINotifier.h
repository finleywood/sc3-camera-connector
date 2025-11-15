#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <map>
#include "UploadQueue.h"
#include "MessageTypes.h"

class UINotifier {
public:
    UINotifier(std::string socket_path)
        : socket_path(socket_path),
        current_status(SocketStatus::UINITIALIZED)
    {}

    void SendMessage(std::string);
    bool Start(bool is_blocking = false);
    void Stop();

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

    void ProcessMessage(std::string);

};