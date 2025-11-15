#include "CommandReceiver.h"

CommandReceiver::~CommandReceiver() = default;

bool CommandReceiver::AllowMessage(MessageType) { return false; };

void CommandReceiver::ProcessMessage(MessageType, std::string) {};