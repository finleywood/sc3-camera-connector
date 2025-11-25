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
    START_COLLECTION,
    COLLECTION_SUCCESS,
    COLLECTION_FAILURE,
    COLLECTION_MANUAL_INPUT,
    START_UPLOAD,
    UPLOAD_STARTED,
    UPLOAD_SUCESS,
    UPLOAD_FAILURE,
    USB_DOWNLOAD,
    USB_DOWNLOAD_SUCESS,
    USB_DOWNLOAD_FAILURE,
    USB_LIST,
    USB_DEV,
    USB_SET,
    UNKNOWN
};

static std::map<MessageType, const char*> MessageTypeMap = {
    {MessageType::START_COLLECTION, "START_COLLECTION"},
    {MessageType::COLLECTION_SUCCESS, "COLLECTION_SUCCESS"},
    {MessageType::COLLECTION_FAILURE, "COLLECTION_FAILURE"},
    {MessageType::COLLECTION_MANUAL_INPUT, "COLLECTION_MANUAL_INPUT"},
    {MessageType::START_UPLOAD, "START_UPLOAD"},
    {MessageType::UPLOAD_STARTED, "UPLOAD_STARTED"},
    {MessageType::UPLOAD_SUCESS, "UPLOAD_SUCESS"},
    {MessageType::UPLOAD_FAILURE, "UPLOAD_FAILURE"},
    {MessageType::USB_DOWNLOAD, "USB_DOWNLOAD"},
    {MessageType::USB_DOWNLOAD_SUCESS, "USB_DOWNLOAD_SUCESS"},
    {MessageType::USB_DOWNLOAD_FAILURE, "USB_DOWNLOAD_FAILURE"},
    {MessageType::USB_LIST, "USB_LIST"},
    {MessageType::USB_DEV, "USB_DEV"},
    {MessageType::USB_SET, "USB_SET"},
    {MessageType::UNKNOWN, "UNKNOWN"}
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

inline std::string message_type_enum_to_str(MessageType msg_type)
{
    if(MessageTypeMap.find(msg_type) == MessageTypeMap.end())
        return MessageTypeMap[MessageType::UNKNOWN];
    
    return MessageTypeMap[msg_type];
}