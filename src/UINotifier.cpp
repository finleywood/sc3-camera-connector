#include "UINotifier.h"
#include "CommandManager.h"
#include "MessageTypes.h"
#include "Messages/StringMessage.h"

#include <aixlog/aixlog.hpp>
#include <chrono>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>

#define MAX_MSG_LEN (2000000) // 2MB

static void SetNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) flags = 0;
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static uint32_t to_uint32_be(const unsigned char* buf) {
    return (uint32_t(buf[0]) << 24) |
           (uint32_t(buf[1]) << 16) |
           (uint32_t(buf[2]) << 8)  |
           (uint32_t(buf[3]));
}

bool UINotifier::AllowMessage(MessageType msg_type)
{
	switch(msg_type) {
	case MessageType::COLLECTION_SUCCESS:
	case MessageType::COLLECTION_FAILURE:
	case MessageType::UPLOAD_STARTED:
	case MessageType::UPLOAD_SUCESS:
	case MessageType::UPLOAD_FAILURE:
	case MessageType::USB_DEV:
	case MessageType::COLLECTION_THUMBNAIL:
	case MessageType::DETECTION_FAILURE:
		return true;
		break;
	default:
		return false;
		break;
	}
}

void UINotifier::ProcessMessage(const BaseMessage& msg)
{
	switch(msg.MsgType) {
	case MessageType::COLLECTION_SUCCESS:
	case MessageType::COLLECTION_FAILURE:
	case MessageType::UPLOAD_STARTED:
	case MessageType::UPLOAD_SUCESS:
	case MessageType::UPLOAD_FAILURE:
	case MessageType::USB_DEV:
	case MessageType::COLLECTION_THUMBNAIL:
	case MessageType::DETECTION_FAILURE:
		this->ProcessSendStringMessage(static_cast<const StringMessage&>(msg));
		return;
	default:
		return;
	}
}

void UINotifier::ProcessSendStringMessage(const StringMessage& msg)
{
	bool msg_sent = this->SendMessage(format_ui_message(msg.MsgType, msg.content));
	if(!msg_sent)
		LOG(WARNING) << "Unable to send message for " << msg.content.c_str() << "." << std::endl;
}

bool UINotifier::Start(bool is_blocking)
{
	if(this->current_status != SocketStatus::UINITIALIZED && this->current_status == SocketStatus::CLOSING) {
		LOG(ERROR) << "Cannot start UI Notifier. Currently running." << std::endl;
		return false;
	}

	if(is_blocking)
		this->RunSocketStateMachine();
	else
		this->state_machine_thread = std::thread(&UINotifier::RunSocketStateMachine, this);

	return true;
}

void UINotifier::Stop()
{
	LOG(INFO) << "Requesting closure." << std::endl;
	this->SocketStateTransitionTo(SocketStatus::CLOSING);
}

void UINotifier::SocketStateTransitionTo(SocketStatus new_state)
{
	SocketStatus old_state = this->current_status.load();
	if(old_state == new_state)
		return;

	LOG(DEBUG) << "Transitioning state from " << static_cast<int>(old_state) << " to " << static_cast<int>(new_state) << std::endl;
	this->current_status = new_state;
}

void UINotifier::RunSocketStateMachine()
{
	this->SocketStateTransitionTo(SocketStatus::UINITIALIZED);
	while(!this->state_machine_close_flag) {
		SocketStatus current_state = this->current_status.load();
		switch(current_state) {
		case SocketStatus::UINITIALIZED:
			HandleUninitialized();
			break;
		case SocketStatus::INITIALIZING:
			LOG(INFO) << "UINotifier socket initializing" << std::endl;
			break;
		case SocketStatus::SOCKET_CREATED:
			HandleSocketCreated();
			break;
		case SocketStatus::SOCKET_CREATION_FAILURE:
			HandleSocketCreationFailure();
			break;
		case SocketStatus::WAITING_FOR_RETRY:
			HandleRetry();
			break;
		case SocketStatus::WAITING_FOR_CONNECTION:
			HandleWaitingForConnection();
			break;
		case SocketStatus::WAITING_FOR_MESSAGE:
			HandleWaitingForMessage();
			break;
		case SocketStatus::CLOSING:
			HandleClosing();
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	if(this->current_status != SocketStatus::UINITIALIZED) {
		this->Stop();
	}
}

void UINotifier::HandleUninitialized()
{
	int sfd;
	struct sockaddr_un addr{};

	LOG(INFO) << "Initializing UI Notifier socket." << std::endl;

	this->SocketStateTransitionTo(SocketStatus::INITIALIZING);

	sfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sfd < 0) {
		LOG(ERROR) << "Unable to call socket(). Call returned invalid fd: " << sfd << std::endl;
		this->SocketStateTransitionTo(SocketStatus::SOCKET_CREATION_FAILURE);
		return;
	}

	// Remove old socket if exists
	unlink(this->socket_path.c_str());

	if(this->socket_path.size() > sizeof(addr.sun_path) - 1) {
		LOG(ERROR) << "Socket path given is larger than supported." << std::endl;
		this->SocketStateTransitionTo(SocketStatus::SOCKET_CREATION_FAILURE);
		return;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, this->socket_path.c_str(), sizeof(addr.sun_path) - 1);

	// Bind the socket to the path
	int ret = bind(sfd, (struct sockaddr*)&addr, sizeof(addr));
	if(ret < 0) {
		LOG(ERROR) << "bind() call failed." << std::endl;
		close(sfd);
		this->SocketStateTransitionTo(SocketStatus::SOCKET_CREATION_FAILURE);
		return;
	}
	SetNonBlocking(sfd);

	// Otherwise all good
	this->server_fd = sfd;
	this->SocketStateTransitionTo(SocketStatus::SOCKET_CREATED);
}

void UINotifier::HandleSocketCreated()
{
	LOG(INFO) << "Socket creation succeeded. Now will wait for connections. And allow outgoing." << std::endl;
	this->SocketStateTransitionTo(SocketStatus::WAITING_FOR_CONNECTION);
}

void UINotifier::HandleSocketCreationFailure()
{
	LOG(INFO) << "Socket creation failed or died for some reason. Will retry." << std::endl;
	this->SocketStateTransitionTo(SocketStatus::WAITING_FOR_RETRY);
}

void UINotifier::HandleRetry()
{
	this->SocketStateTransitionTo(SocketStatus::UINITIALIZED);
}

void UINotifier::HandleWaitingForConnection()
{
	if (server_fd < 0) {
		LOG(ERROR) << "Server fd not init. Retrying connection." << std::endl;
		this->SocketStateTransitionTo(SocketStatus::SOCKET_CREATION_FAILURE);
		return;
	}

	// Ensure socket is listening only once
	static bool is_listening = false;
	if (!is_listening) {
		if (listen(this->server_fd, 1) < 0) {
			LOG(ERROR) << "listen() call failed: " << strerror(errno) << std::endl;
			close(this->server_fd);
			this->server_fd = -1;
			this->SocketStateTransitionTo(SocketStatus::SOCKET_CREATION_FAILURE);
			return;
		}
		is_listening = true;
	}
	LOG(TRACE) << "listen() success" << std::endl;

	// Non-blocking accept
	int cfd = accept(this->server_fd, nullptr, nullptr);
	if (cfd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// No connection yet, just keep waiting
			LOG(TRACE) << "accept() no connection yet." << std::endl;
			return;
		}
		// Real error
		LOG(ERROR) << "accept() failed: " << strerror(errno) << std::endl;
		this->SocketStateTransitionTo(SocketStatus::SOCKET_CREATION_FAILURE);
		return;
	}
	LOG(DEBUG) << "accept() success" << std::endl;

	SetNonBlocking(cfd);
	this->client_fd = cfd;
	LOG(INFO) << "Client connection received (non-blocking)." << std::endl;
	this->SocketStateTransitionTo(SocketStatus::WAITING_FOR_MESSAGE);
}

void UINotifier::HandleWaitingForMessage()
{
	if (client_fd < 0) {
		this->SocketStateTransitionTo(SocketStatus::WAITING_FOR_CONNECTION);
		return;
	}

	uint32_t buf_len = this->start_msg_recv ? this->p2_len + 1 : 4;
	unsigned char buf[buf_len];
	ssize_t msg_bytes = read(this->client_fd, buf, this->start_msg_recv ? buf_len - 1 : buf_len);

	if (msg_bytes > 0) {
		if (!this->start_msg_recv) {
			LOG(INFO) << "New message receive started, attempting to "
				"find length."
								<< std::endl;
			for(int i = 0; i < buf_len; i++) {
				LOG(INFO) << "Message byte " << i << ": " << buf[i] << std::endl;
			}
			if(msg_bytes != 4) {
				// Not 32-bit int, so not following messaging protocol, reject.
				LOG(INFO) << "Message received was not the length header, rejecting." << std::endl;
				return;
			} else {
				uint32_t be_len = to_uint32_be(&buf[0]);
				if(be_len <= 0) {
					LOG(INFO) << "Message type either an invalid length, or too long." << std::endl;
					return;
				} else if(be_len > MAX_MSG_LEN) {
					LOG(INFO) << "Cannot process expected message, as message length of " << be_len << " is too long." << std::endl;
					return;
				}
				else {
					this->start_msg_recv = true;
					this->p2_len = be_len;
					LOG(INFO) << "Expecting message of length " << this->p2_len << std::endl;
					return;
				}
			}						
		} else {
			if(msg_bytes != this->p2_len) {
				LOG(INFO) << "Received unexpected size of message, discarding." << std::endl;
				this->start_msg_recv = false;
				this->p2_len = 0;
				return;
			}
			buf[msg_bytes] = '\0';
			this->ProcessClientMessage(std::string(reinterpret_cast<const char*>(buf)));
			this->start_msg_recv = false;
			this->p2_len = 0;
			return;
		}
	}

	if (msg_bytes == 0) {
		// Graceful disconnect
		LOG(WARNING) << "Client disconnected gracefully." << std::endl;
		close(this->client_fd);
		this->client_fd = -1;
		this->SocketStateTransitionTo(SocketStatus::WAITING_FOR_CONNECTION);
		return;
	}

	// msg_bytes < 0
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
		// No data yet
		LOG(TRACE) << "read() no data yet" << std::endl;
		return;
	}

	// Real error or forced disconnect
	LOG(WARNING) << "Socket read error (" << strerror(errno)
							 << "). Closing client connection." << std::endl;
	close(this->client_fd);
	this->client_fd = -1;
	this->SocketStateTransitionTo(SocketStatus::WAITING_FOR_CONNECTION);
}

void UINotifier::HandleClosing()
{
	this->state_machine_close_flag = true;
	if(this->server_fd >= 0) {
		close(this->server_fd);
		this->server_fd = -1;
	}
	if(this->client_fd >= 0) {
		close(this->client_fd);
		this->client_fd = -1;
	}
	unlink(this->socket_path.c_str());
	if(this->state_machine_thread.joinable()) {
		this->state_machine_thread.join();
	}
}

bool UINotifier::SendMessage(std::string msg)
{
	LOG(INFO) << "Current Socket status = " << static_cast<int>(this->current_status.load()) << " Client fd = " << this->client_fd << std::endl;
	if((this->current_status != SocketStatus::WAITING_FOR_CONNECTION && this->current_status != SocketStatus::WAITING_FOR_MESSAGE) || this->client_fd < 0) {
		LOG(DEBUG) << "Cannot send message " << msg.c_str() << " as socket is not initialized." << std::endl;
		return false;
	}

	// Write 4 byte length followed by actual message
	uint32_t length = msg.size();
	LOG(INFO) << "Sending message to UI of size" << static_cast<int>(length) << std::endl;
	// Ensure big-endian
	uint32_t big_end_len = htonl(length);
	write(this->client_fd, &big_end_len, sizeof(big_end_len));
	write(this->client_fd, msg.c_str(), msg.size());
	return true;
}

void UINotifier::SendStringMsg(MessageType msg_type, std::string msg)
{
  if (msg_type >= MessageType::UNKNOWN)
    return;

  StringMessage s_msg;
  s_msg.MsgType = msg_type;
  s_msg.content = msg;
	CommandManager::GetCM().SubmitMessage(s_msg);
}

void UINotifier::ProcessClientMessage(std::string msg)
{
	std::istringstream iss(msg);
	std::vector<std::string> parts;
	std::string word;
	while (iss >> word) {
		parts.push_back(word);
	}

	if(parts.size() > 2 || parts.size() == 0) {
		LOG(WARNING) << "Invalid formatting on message received: " << msg.c_str() << std::endl;
		return;
	}

	std::string msg_type = parts[0];
	std::string msg_body;
	if(parts.size() == 2)
		msg_body = parts[1];
	MessageType msg_type_enum = message_type_str_to_enum(msg_type);
	if(msg_type_enum == MessageType::UNKNOWN) {
		LOG(WARNING) << "Unknown message type received: " << msg_type.c_str() << std::endl;
		return;
	}

	switch(msg_type_enum) {
	case MessageType::START_COLLECTION:
	case MessageType::USB_LIST:
	case MessageType::COLLECTION_REJECT:
  	case MessageType::START_UPLOAD:
	case MessageType::COLLECTION_THUMBNAIL_REQUEST:
	case MessageType::CTRL_SHUTDOWN:
	case MessageType::CTRL_RESTART:
		this->SendStringMsg(msg_type_enum, msg_body);
		break;
	default:
		break;
	}
}
