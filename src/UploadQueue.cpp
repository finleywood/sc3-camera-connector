#include "UploadQueue.h"
#include "aixlog/aixlog.hpp"

#include <stdexcept>

void UploadQueue::Enqueue(UploadItem item_to_upload)
{
    if(this->upload_q_ptr->size() == this->max_size) {
        throw std::out_of_range("Cannot push anymore to the queue, it is full.");
        return;
    }

    // lock for critsec
    this->q_lock.lock();

    this->upload_q_ptr->emplace(item_to_upload);

    // don't forget to unlock
    this->q_lock.unlock();
}

UploadItem UploadQueue::Dequeue()
{
    if(this->upload_q_ptr->size() == 0) {
        throw std::out_of_range("Cannot dequeue, as queue is empty.");
        return {};
    }

    // lock
    this->q_lock.lock();

    UploadItem ret = this->upload_q_ptr->front();
    this->upload_q_ptr->pop();

    // unlock
    this->q_lock.unlock();

    // finally return it
    return ret;
}

bool UploadQueue::Start()
{
    // ensure not already initialised
    if(this->CurrentStatus == QueueStatus_t::RUNNING) {
        LOG(DEBUG) << "Queue already running, cannot start." << std::endl;
        return false;
    }

    this->should_stop = false;
    this->q_thread = std::thread(&UploadQueue::MainLoop, this);
    return true;
}

void UploadQueue::Stop()
{
    if(this->CurrentStatus == QueueStatus_t::RUNNING) {
        LOG(INFO) << "Stopping upload queue thread" << std::endl;
        this->should_stop = true;
        this->CurrentStatus = QueueStatus_t::STOPPED;
        this->q_thread.join();
    } else {
        LOG(WARNING) << "Cannot stop an upload queue which is not running." << std::endl;
    }
}

void UploadQueue::MainLoop()
{
    LOG(INFO) << "Upload queue started." << std::endl;
    this->CurrentStatus = QueueStatus_t::RUNNING;

    while(!this->should_stop) {
        UploadItem item;
        try {
            item = this->Dequeue();
        } catch(std::exception& e) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        while (item.retry_num <= this->max_retry) {
            item.retry_num++;
            LOG(INFO) << "Starting upload attempt " << item.retry_num << " out of " << this->max_retry << "." << std::endl;
            bool upload_success = this->transport_ptr->UploadQueueItem(item);
            if(upload_success) {
                LOG(INFO) << "Upload success." << std::endl;
                break;
            }
            LOG(INFO) << "Upload failed. Will Retry in 5s." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }

        LOG(INFO) << "Falling back to USB." << std::endl;
        if(this->transport_ptr->USBFallback(item)) {
            LOG(INFO) << "USB Fallback success." << std::endl;
        } else {
            LOG(INFO) << "USB Fallback failed, cannot store event." << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

}