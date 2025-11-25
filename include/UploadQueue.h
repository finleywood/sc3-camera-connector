#pragma once

#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>

#include "UploadItem.h"
#include "UpstreamTransport.h"
#include "UINotifier.h"
#include "CommandReceiver.h"
#include "CommandManager.h"
#include "Messages/BaseMessage.h"
#include "Messages/EventMessage.h"

typedef enum {
    STOPPED,
    RUNNING
} QueueStatus_t;

class UploadQueue : public CommandReceiver {
public:
    QueueStatus_t CurrentStatus = QueueStatus_t::STOPPED;

    UploadQueue(UpstreamTransport* transport, uint8_t max_retry, uint8_t max_size = 64) :
        CommandReceiver(),
        max_size(max_size),
        max_retry(max_retry),
        transport_ptr(std::unique_ptr<UpstreamTransport> { transport })
    {
        CommandManager& cm = CommandManager::GetCM();
        cm.Register("UploadQueue", this);
    }

    virtual bool AllowMessage(MessageType) override;
    virtual void ProcessMessage(const BaseMessage&) override;

    void AddUploadItem(const UploadItem&);
    void DeleteUnapprovedItem(std::string);

    bool Start();
    void Stop();

    void ProcessNewUpload(const EventMessage&);
private:
    std::thread q_thread;
    std::unique_ptr<UpstreamTransport> transport_ptr;
    std::queue<UploadItem> upload_q {};
    uint8_t max_size;
    uint8_t max_retry;
    std::mutex q_lock;
    std::atomic_bool should_stop;

    void Enqueue(UploadItem);
    UploadItem Dequeue();
    void MainLoop();
    bool Close();
};