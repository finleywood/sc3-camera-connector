#include "UploadQueue.h"
#include "MessageTypes.h"
#include "USBDownload.h"
#include "aixlog/aixlog.hpp"

#include <stdexcept>

bool UploadQueue::AllowMessage(MessageType msg_type)
{
	switch(msg_type) {
	case MessageType::QUEUE_FOR_UPLOAD:
		return true;
	default:
		return false;
	}
}

void UploadQueue::ProcessMessage(const BaseMessage& msg)
{
	switch(msg.MsgType) {
	case MessageType::QUEUE_FOR_UPLOAD:
		this->ProcessNewUpload(static_cast<const EventMessage&>(msg));
		break;
	default:
		break;
	}
}


void UploadQueue::ProcessNewUpload(const EventMessage& msg)
{
	LOG(INFO) << "Upload queue received request to upload event with UID: " << msg.Uid.c_str() << "." << std::endl;
	try {
		UploadItem item_to_upload = {};
		item_to_upload.uid = msg.Uid;
		item_to_upload.qr_det = msg.QrDetection;
		this->Enqueue(item_to_upload);
	} catch(std::exception& e) {
		LOG(WARNING) << "Failed to queue for upload event with UID: " << msg.Uid.c_str() << ", because of: " << e.what() << std::endl;
	}
}

void UploadQueue::Enqueue(UploadItem item_to_upload)
{
	// lock for critsec
	this->q_lock.lock();
	if(this->upload_q.size() == this->max_size) {
		throw std::out_of_range("Cannot push anymore to the queue, it is full.");
		return;
	}

	this->upload_q.emplace(item_to_upload);
	LOG(INFO) << "Placed detection " << item_to_upload.uid.c_str() << " in queue." << std::endl;

	// don't forget to unlock
	this->q_lock.unlock();
}

UploadItem UploadQueue::Dequeue()
{
	if(this->upload_q.size() == 0) {
		throw std::out_of_range("Cannot dequeue, as queue is empty.");
		return {};
	}

	// lock
	this->q_lock.lock();

	UploadItem ret = this->upload_q.front();
	this->upload_q.pop();

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
		CommandManager& cm = CommandManager::GetCM();

		// Notify ui of upload started
		StringMessage upload_start_message;
		upload_start_message.MsgType = MessageType::UPLOAD_STARTED;
		upload_start_message.content = item.uid;
		cm.SubmitMessage(upload_start_message);

		bool upload_success = false;
		while (true) {
			if (!this->http_upload_enabled)
				break;
			item.retry_num = item.retry_num + 1;
			LOG(INFO) << "Starting upload attempt " << static_cast<int>(item.retry_num) << " out of " << static_cast<int>(this->max_retry) << "." << std::endl;
			if(this->transport_ptr->UploadQueueItem(item)) {
				LOG(INFO) << "Upload success." << std::endl;
				upload_success = true;
				break;
			}
			if(item.retry_num == this->max_retry) {
				LOG(WARNING) << "Max retries hit, falling back." << std::endl;
				break;
			}
			LOG(INFO) << "Upload failed. Will Retry in 5s." << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		}

		bool final_success = upload_success;
		// If upload wasn't successful, fallback on usb
		if(!upload_success) {
			LOG(INFO) << "Http upload unsuccessful, falling back to USB." << std::endl;
			USBDownload usb_d;
			final_success = usb_d.SaveDetection(item.qr_det);
			LOG(INFO) << "USB Download " <<  (final_success ? "was successful." : "failed.") << std::endl;
 		}

		StringMessage upload_status_msg;
		upload_status_msg.content = item.uid;
		upload_status_msg.MsgType = final_success ? MessageType::UPLOAD_SUCESS : MessageType::UPLOAD_FAILURE;
		cm.SubmitMessage(upload_status_msg);

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

}
