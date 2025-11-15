#pragma once

#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>

#include "UploadItem.h"
#include "UpstreamTransport.h"
#include "UINotifier.h"
#include "CommandReceiver.h"
#include "CommandManager.h"

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
        upload_q_ptr(std::unique_ptr<std::queue<UploadItem>> { new std::queue<UploadItem>() }),
        transport_ptr(std::unique_ptr<UpstreamTransport> { transport })
    {
        CommandManager& cm = CommandManager::GetCM();
        cm.Register("UploadQueue", this);
    }

    virtual bool AllowMessage(MessageType) override;
    virtual void ProcessMessage(MessageType, std::string) override;

    void Enqueue(UploadItem);
    bool Start();
    void Stop();

    UploadItem* FindInQueueById(std::string);
    void ProcessDetectionConfirm(std::string uid);
private:
    std::thread q_thread;
    std::unique_ptr<UpstreamTransport> transport_ptr;
    std::unique_ptr<std::queue<UploadItem>> upload_q_ptr;
    uint8_t max_size;
    uint8_t max_retry;
    std::mutex q_lock;
    std::atomic_bool should_stop;

    UploadItem Dequeue();
    void MainLoop();
    bool Close();
};