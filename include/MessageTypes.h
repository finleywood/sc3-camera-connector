#pragma once
#include <string>
#include <map>

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

inline std::string format_ui_message(MessageType type, std::string msg)
{
    std::string msg_type_str = MessageTypeMap[type];
    std::string msg_to_send = msg_type_str + " " + msg;
    return msg_to_send;
}

inline MessageType message_type_str_to_enum(std::string msg_type)
{
    for (const auto& pair : MessageTypeMap) {
        if (pair.second == msg_type)
            return pair.first;
    }
    return MessageType::UNKNOWN;
}