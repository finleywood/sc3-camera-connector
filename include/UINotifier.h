#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <map>
#include "UploadQueue.h"

enum class SocketStatus {
    UINITIALIZED,
    INITIALIZING,
    SOCKET_CREATED,
    SOCKET_CREATION_FAILURE,
    WAITING_FOR_RETRY,
    WAITING_FOR_CONNECTION,
    WAITING_FOR_MESSAGE,
    CLOSING,
};

enum class MessageType {
    NEW_DETECTION,
    DETECTION_CONFIRM,
    DETECTION_UPLOAD_STATUS,
    LIST_USB,
    SET_USB,
    UNKNOWN
};

static std::map<MessageType, const char*> MessageTypeMap = {
    {MessageType::NEW_DETECTION, "NEW_DETECTION"},
    {MessageType::DETECTION_CONFIRM, "DETECTION_CONFIRM"},
    {MessageType::DETECTION_UPLOAD_STATUS, "DETECTION_UPLOAD_STATUS"},
    {MessageType::LIST_USB, "LIST_USB"},
    {MessageType::SET_USB, "SET_USB"}
};

static std::string format_ui_message(MessageType type, std::string msg);
static MessageType message_type_str_to_enum(std::string msg_type);

class UINotifier {
public:
    UINotifier(std::string socket_path, UploadQueue* queue)
        : socket_path(socket_path),
        uploader_queue(queue),
        current_status(SocketStatus::UINITIALIZED)
    {}

    void SendMessage(std::string);
    bool Start(bool is_blocking = false);
    void Stop();

private:
    UploadQueue* uploader_queue;
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