#pragma once

#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>

#include "UploadItem.h"
#include "UpstreamTransport.h"
#include "UINotifier.h"

typedef enum {
    STOPPED,
    RUNNING
} QueueStatus_t;

class UploadQueue {
public:
    QueueStatus_t CurrentStatus = QueueStatus_t::STOPPED;
    UINotifier* notifier;

    UploadQueue(UpstreamTransport* transport, uint8_t max_retry, uint8_t max_size = 64) :
        max_size(max_size),
        max_retry(max_retry)
    {
        this->upload_q_ptr = std::unique_ptr<std::queue<UploadItem>> { new std::queue<UploadItem>() };
        this->transport_ptr = std::unique_ptr<UpstreamTransport> { transport };
    }

    void Enqueue(UploadItem);
    bool Start();
    void Stop();

    UploadItem* FindInQueueById(std::string);

    void ApproveAction(std::string item_uid);

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