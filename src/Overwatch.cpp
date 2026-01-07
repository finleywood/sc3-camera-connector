#include "Overwatch.h"

#include <cstdio>
#include <chrono>
#include <thread>

#include <aixlog/aixlog.hpp>

#include "Util.h"

bool Overwatch::AllowMessage(MessageType msg_type) {
    switch(msg_type) {
        case MessageType::CTRL_SHUTDOWN:
        case MessageType::CTRL_RESTART:
            return true;
            break;
        default:
            return false;
            break;
    }
}

void Overwatch::ProcessMessage(const BaseMessage& msg) {
    switch(msg.MsgType) {
        case MessageType::CTRL_SHUTDOWN:
            HandleShutdown();
            break;
        case MessageType::CTRL_RESTART:
            HandleRestart();
            break;
    }
}

void Overwatch::ShutdownServices() {
    // Shutdown upload queue first
    if(this->up_q != nullptr) {
        this->up_q->Stop();
    }

    // Then Camera Manager
    if(this->cam_mgr != nullptr) {
        this->cam_mgr->Stop();
    }

    // Finally, UI Notifier, just because it's the connection to the UI, to have minimal frontend disconnection time
    if(this->ui_notif != nullptr) {
        this->ui_notif->Stop();
    }
}

void Overwatch::HandleShutdown() {
    this->ShutdownServices();
    Util::getCmdOutput("sudo /sbin/shutdown now", false);
}

void Overwatch::HandleRestart() {
    this->ShutdownServices();
    Util::getCmdOutput("sudo /sbin/reboot", false);
}

void Overwatch::Start() noexcept {
    // Start services
    this->up_q->Start();
    this->cam_mgr->Start();
    this->ui_notif->Start();

    // Infinte loop to keep application open
    while(!this->should_close.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void Overwatch::Stop() {
    this->ShutdownServices();
    this->should_close = true;
}