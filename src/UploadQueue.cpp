#include "UploadQueue.h"
#include "aixlog/aixlog.hpp"

#include <stdexcept>

bool UploadQueue::AllowMessage(MessageType msg_type)
{
    switch(msg_type) {
        case MessageType::DETECTION_CONFIRM:
            return true;
            break;
        default:
            return false;
            break;
    }
}

void UploadQueue::ProcessMessage(MessageType msg_type, std::string msg_content)
{
    switch(msg_type) {
        case MessageType::DETECTION_CONFIRM:
            ProcessDetectionConfirm(msg_content);
            break;
        default:
            break;
    }
}

void UploadQueue::ProcessDetectionConfirm(std::string uid)
{
    UploadItem* item_found = this->FindInQueueById(uid);
    if(!item_found) {
        LOG(WARNING) << "Detection item with ID: " << uid.c_str() << " not found in queue." << std::endl;
        return;
    }

    item_found->approved = true;
}

UploadItem* UploadQueue::FindInQueueById(std::string uid) {
    this->q_lock.lock();
    std::queue<UploadItem> temp = *this->upload_q_ptr.get();  // make a copy
    while (!temp.empty()) {
        UploadItem& obj = temp.front();
        if (obj.qr_det.uid == uid) {
            this->q_lock.unlock();
            return &temp.front();
        }
        temp.pop();
    }
    this->q_lock.unlock();
    return nullptr;
}

void UploadQueue::Enqueue(UploadItem item_to_upload)
{
    // lock for critsec
    this->q_lock.lock();
    if(this->upload_q_ptr->size() == this->max_size) {
        throw std::out_of_range("Cannot push anymore to the queue, it is full.");
        return;
    }

    this->upload_q_ptr->emplace(item_to_upload);

    // don't forget to unlock
    this->q_lock.unlock();

    // Tell UI if possible
    CommandManager& cm = CommandManager::GetCM();
    cm.SubmitMessage(MessageType::NEW_DETECTION, item_to_upload.qr_det.uid);
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
        // lock while dequeuing
        this->q_lock.lock();
        try {
            item = this->Dequeue();
        } catch(std::exception& e) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        if(!item.approved) {
            try {
                this->Enqueue(item);
            } catch(std::exception& e) {
                LOG(WARNING) << "Enqueue after dequeue failed. Due to " << e.what() << std::endl;
            }
            this->q_lock.unlock();
            continue;
        }

        while (true) {
            item.retry_num = item.retry_num + 1;
            LOG(INFO) << "Starting upload attempt " << static_cast<int>(item.retry_num) << " out of " << static_cast<int>(this->max_retry) << "." << std::endl;
            bool upload_success = this->transport_ptr->UploadQueueItem(item);
            if(upload_success) {
                LOG(INFO) << "Upload success." << std::endl;
                break;
            }
            if(item.retry_num == this->max_retry) {
                LOG(WARNING) << "Max retries hit, falling back." << std::endl;
                break;
            }
            LOG(INFO) << "Upload failed. Will Retry in 5s." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }

        LOG(INFO) << "Falling back to USB." << std::endl;
        // TODO
        // if(this->transport_ptr->USBFallback(item)) {
        //     LOG(INFO) << "USB Fallback success." << std::endl;
        // } else {
        //     LOG(INFO) << "USB Fallback failed, cannot store event." << std::endl;
        // }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

}
