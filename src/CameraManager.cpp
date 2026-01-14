#include "CameraManager.h"

#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include "CommandManager.h"
#include "ImageUtils.h"
#include "MessageTypes.h"
#include "Messages/EventMessage.h"
#include "Base64.h"
#include "Messages/StringMessage.h"
#include "UUIDV4.h"
#include "SimulatedCamera.h"
#include "KinectCamera.h"
#include "WeightExporter.h"
#include <json/json.hpp>

using json = nlohmann::json;

// Define static constexpr members
constexpr int CameraManager::MAX_RECONNECT_ATTEMPTS;
constexpr int CameraManager::RECONNECT_COOLDOWN_SEC;

bool CameraManager::AllowMessage(MessageType msg_type)
{
	switch(msg_type) {
	case MessageType::START_COLLECTION:
  case MessageType::COLLECTION_REJECT:
	case MessageType::START_UPLOAD:
	case MessageType::COLLECTION_THUMBNAIL_REQUEST:
    return true;
		break;
	default:
		return false;
		break;
	}
}

void CameraManager::ProcessMessage(const BaseMessage& msg)
{
	switch(msg.MsgType) {
	case MessageType::START_COLLECTION:
		this->ProcessCollectionStart(static_cast<const StringMessage&>(msg));
		break;
  case MessageType::START_UPLOAD:
    this->ProcessStartUpload(static_cast<const StringMessage &>(msg));
		break;
	case MessageType::COLLECTION_THUMBNAIL_REQUEST:
		this->ProcessCollectionThumnailRequest(static_cast<const StringMessage &>(msg));
		break;
	case MessageType::COLLECTION_REJECT:
		this->ProcessCollectionReject();
		break;
	default:
		break;
	}
}

void CameraManager::ProcessCollectionThumnailRequest(const StringMessage& msg)
{
	if(!msg.content.empty())
		return;

	LOG(INFO) << "Received request for collection thumnail." << std::endl;
	if(this->event_state.valid()) {
		this->SendThumnail(this->event_state.detection_event.rgb_img);
	}
}

void CameraManager::ProcessCollectionStart(const StringMessage &msg)
{
  if (this->current_state == CameraState::WAITING_FOR_DECISION) {
    // Silently discard
    LOG(WARNING) << "New collection requested received while a collection is "
                    "waiting on a decision, discarding the old one."
                 << std::endl;
    this->ResetEventState();
		if(msg.content.empty()) {
			UuidV4 uuid_gen;
			this->event_state.uid = uuid_gen.generate();
		} else {
			this->event_state.uid = msg.content;
		}
    this->frames_since_event_started = 0;
		this->TransitionTo(CameraState::WAITING_FOR_COLLECTION);
		return;
  }
	
	if(this->event_state.valid()) {
		// Collection already active, cannot start new one
		StringMessage msg_out;
		msg_out.MsgType = MessageType::COLLECTION_FAILURE;
		msg_out.content = "Collection already in progress, cannot start a new one.";
		CommandManager::GetCM().SubmitMessage(msg_out);
	} else {
		if(msg.content.empty()) {
			UuidV4 uuid_gen;
			this->event_state.uid = uuid_gen.generate();
		} else {
			this->event_state.uid = msg.content;
		}
		this->frames_since_event_started = 0;
	}
}

void CameraManager::ProcessStartUpload(const StringMessage &msg)
{
  if (this->current_state != CameraState::WAITING_FOR_DECISION)
    return;

  if (!msg.content.empty()) {
	try {
    	int indentifier_as_num = std::stoi(msg.content);
		this->event_state.detection_event.identifier = indentifier_as_num;
	} catch(std::exception& e) {
		LOG(WARNING) << "Provided intentifier " << msg.content.c_str() << " not accepted, as not a valid integer." << std::endl;;
	}
  }

  EventMessage q_msg;
  q_msg.MsgType = MessageType::QUEUE_FOR_UPLOAD;
  q_msg.Uid = this->event_state.uid;
  q_msg.QrDetection = this->event_state.detection_event;

  CommandManager::GetCM().SubmitMessage(q_msg);

  this->TransitionTo(CameraState::COLLECTION_FINISHED);
}

void CameraManager::ProcessCollectionReject()
{
  if (this->current_state != CameraState::WAITING_FOR_DECISION)
    return;

  LOG(INFO) << "Rejecting current event." << std::endl;
	this->TransitionTo(CameraState::COLLECTION_FINISHED);
} 

void CameraManager::RegisterStateCallback(state_callback_t callback_func) {
	this->state_callback = callback_func;
}

bool CameraManager::Start(bool is_blocking) {
	if (current_state != CameraState::UNINITIALIZED && 
			current_state != CameraState::STOPPED) {
		std::cerr << "Cannot start: already running" << std::endl;
		return false;
	}
    
	close_flag = false;
    
	if (is_blocking) {
		// Run state machine in current thread
		RunStateMachine();
	} else {
		// Run state machine in separate thread
		state_machine_thread = std::thread(&CameraManager::RunStateMachine, this);
	}
    
	return true;
}

void CameraManager::Stop() {
	close_flag = true;
	if (state_machine_thread.joinable()) {
		state_machine_thread.join();
	}
}

void CameraManager::TransitionTo(CameraState new_state) {
	CameraState old_state = current_state.load();
	if (old_state == new_state) return;
    
	current_state = new_state;
	LOG(DEBUG) << "State transition: " << static_cast<int>(old_state) 
						 << " -> " << static_cast<int>(new_state) << std::endl;
    
	if (state_callback) {
		state_callback(old_state, new_state);
	}
}

void CameraManager::RunStateMachine() {
	TransitionTo(CameraState::WAITING_FOR_COLLECTION);
    
	while (!close_flag) {
		CameraState state = current_state.load();
        
		switch (state) {
		case CameraState::UNINITIALIZED:
			HandleUninitialized();
			break;
		case CameraState::WAITING_FOR_COLLECTION:
			HandleWaitingForCollection();
			break;
		case CameraState::WAITING_FOR_DECISION:
			HandleWaitingForDecision();
			break;
		case CameraState::COLLECTION_FINISHED:
			HandleCollectionFinished();
			break;
		case CameraState::INITIALIZING:
			HandleInitializing();
			break;
		case CameraState::STREAMING:
			HandleStreaming();
			break;
		case CameraState::DISCONNECTED:
			HandleDisconnected();
			break;
		case CameraState::RECONNECTING:
			HandleReconnecting();
			break;
		case CameraState::WAITING_FOR_DEVICE:
			HandleWaitingForDevice();
			break;
		case CameraState::RECONNECT_FAILED:
			HandleReconnectFailed();
			break;
		case CameraState::ERROR:
			HandleError();
			break;
		case CameraState::STOPPING:
			HandleStopping();
			break;
		case CameraState::STOPPED:
			HandleStopped();
			return; // Exit state machine
		}
        
		// Small sleep to prevent busy loop
		int sleep_time = this->event_state.valid() ? 10 : 1000;
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
            
	}
    
	// Clean shutdown requested
	if (current_state != CameraState::STOPPED) {
		TransitionTo(CameraState::STOPPING);
		HandleStopping();
	}
}

void CameraManager::HandleUninitialized() {
	// Waiting for Start() to be called
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void CameraManager::HandleWaitingForCollection()
{
	if(this->event_state.valid()) {
		LOG(INFO) << "Collection period started, will open device and start streaming." << std::endl;
		this->TransitionTo(CameraState::INITIALIZING);
	}
}

void CameraManager::HandleCollectionFinished()
{
	LOG(INFO) << "Collection period finished, closing device." << std::endl;
	this->TransitionTo(CameraState::WAITING_FOR_DECISION);
}

void CameraManager::HandleWaitingForDecision()
{
	this->CleanupResources();
	return;
}

void CameraManager::HandleInitializing() {
	LOG(INFO) << "Initializing device..." << std::endl;
    
	if (OpenDevice()) {
		TransitionTo(CameraState::STREAMING);
	} else {
		LOG(ERROR) << "Failed to open device" << std::endl;
		TransitionTo(CameraState::RECONNECTING);
	}
}

void CameraManager::HandleStreaming() {
	LOG(INFO) << "Handling streaming" << std::endl;

	if(this->current_camera.get() == nullptr) {
		this->TransitionTo(CameraState::INITIALIZING);
		return;
	}

	if(!this->current_camera->IsOpen()) {
		this->TransitionTo(CameraState::INITIALIZING);
		return;
	}

	if(!this->event_state.valid()) {
		return;
	}

	std::pair<EncodedImage, EncodedImage> cap = this->current_camera->GetFrame();
	if(!cap.first.valid() || !cap.second.valid()) {
		LOG(WARNING) << "Invalid capture received, skipping..." << std::endl;
		return;
	}
	HandleCapture(cap);
}

void CameraManager::HandleDisconnected() {
	LOG(INFO) << "Device disconnected!" << std::endl;
	CloseDevice();
	reconnect_attempts = 0;
	TransitionTo(CameraState::RECONNECTING);
}

void CameraManager::HandleReconnecting() {
	LOG(INFO) << "Starting reconnection process..." << std::endl;
	LOG(INFO) << "Wating for device." << std::endl;
	CleanupResources();
	TransitionTo(CameraState::WAITING_FOR_DEVICE);
}

void CameraManager::HandleWaitingForDevice() {
	LOG(DEBUG) << "Waiting for device (attempt " << reconnect_attempts 
						<< "/" << MAX_RECONNECT_ATTEMPTS << ")..." << std::endl;
        
	if(this->reconnect_attempts == MAX_RECONNECT_ATTEMPTS) {
		this->reconnect_attempts = 0;
		this->TransitionTo(CameraState::RECONNECT_FAILED);
	} else {
		this->reconnect_attempts++;
		this->TransitionTo(CameraState::INITIALIZING);
	}
}

void CameraManager::HandleReconnectFailed() {
	LOG(WARNING) << "Reconnection failed after " << reconnect_attempts 
							 << " attempts" << std::endl;
    
	// if collection started, end it
	if(this->event_state.valid()) {
		StringMessage msg;
		msg.MsgType = MessageType::COLLECTION_FAILURE;
		msg.content = "Failed to connect to camera, please try again.";
		CommandManager::GetCM().SubmitMessage(msg);
	}
	this->TransitionTo(CameraState::COLLECTION_FINISHED);
}

void CameraManager::HandleError() {
	LOG(ERROR) << "Error state - cleaning up" << std::endl;
	CleanupResources();
	TransitionTo(CameraState::STOPPED);
}

void CameraManager::HandleStopping() {
	LOG(INFO) << "Stopping camera manager..." << std::endl;
	CleanupResources();
	TransitionTo(CameraState::STOPPED);
}

void CameraManager::HandleStopped() {
	LOG(INFO) << "Camera manager stopped" << std::endl;
	// Terminal state - do nothing
}

bool CameraManager::OpenDevice() {

	// GUI Init
	if (gui_enabled) {
		std::lock_guard<std::mutex> lock(gui_mutex);
		cv::namedWindow(gui_window_name, cv::WINDOW_NORMAL);
		cv::resizeWindow(gui_window_name, 960, 540);
	}

	if(is_simulated) {
		LOG(WARNING) << "Will start a simulated camera." << std::endl;
		this->current_camera.reset(new SimulatedCamera());
	} else {
		this->current_camera.reset(new KinectCamera());
	}

	if(this->current_camera.get() == nullptr) {
		LOG(WARNING) << "Failed to open camera, will retry." << std::endl;
		return false;
	}
	bool open_success = this->current_camera->OpenCamera();
	if(open_success) {
		LOG(INFO) << "Camera opened successfully." << std::endl;
		return true;
	} else {
		LOG(WARNING) << "Failed to open camera, will retry." << std::endl;
		return false;
	}
}

void CameraManager::CloseDevice() {
	if (current_camera.get() == nullptr) return;

	if(!current_camera->IsOpen()) return;
    
	LOG(INFO) << "Closing device..." << std::endl;
	this->current_camera->CloseCamera();
}

void CameraManager::CleanupResources() {
	// GUI cleanup
	if (gui_enabled) {
		std::lock_guard<std::mutex> lock(gui_mutex);
		cv::destroyWindow(gui_window_name);
	}

	CloseDevice();
}

void CameraManager::ResetEventState()
{
	this->event_state = {};
	this->frames_since_event_started = 0;
}

QRDetection CameraManager::LookForQR(const EncodedImage& img) {
    if (!img.valid()) {
        return {};
    }
    
    // Wrap the buffer - data stays alive because EncodedImage uses shared_ptr
    cv::Mat mat(img.height, img.width, CV_8UC4, img.data.get());
    
    // QR detector works with BGRA, no need to convert!
    cv::QRCodeDetector cv_qr;
    std::vector<cv::Point> qr_detections;
    
    try {
        std::string found_data = cv_qr.detectAndDecode(mat, qr_detections);
        
        if (found_data.empty()) {
            return {};
        }
        
        int id_as_int = -1;
        try {
            id_as_int = std::stoi(found_data);
        } catch(const std::exception& e) {
            LOG(WARNING) << "Found identifier was not a valid int: " << found_data << std::endl;
            return {};
        }

        if (id_as_int >= 0) {
            LOG(INFO) << "Detector detected and decoded QR with ID: " << id_as_int << std::endl;
            QRDetection found_qr;
            found_qr.identifier = id_as_int;
            return found_qr;
        }
        
        return {};
    } catch(const std::exception& e) {
        LOG(WARNING) << "QR Code detection failed: " << e.what() << std::endl;
        return {};
    }
}

void CameraManager::SendThumnail(const EncodedImage& orig_img)
{
	if(!orig_img.valid())
		return;
	EncodedImage jpeg_thumnail = ImageUtils::JpegToThumbnail(orig_img, 640);
	if(!jpeg_thumnail.valid()) {
		LOG(WARNING) << "Unable to get image thumnail." << std::endl;
	} else {
		std::string b64_thumnail = base64_encode(jpeg_thumnail.data.get(), jpeg_thumnail.size);
		size_t encoded_size = b64_thumnail.size();
		LOG(INFO) << "Sending thumnail of size " << static_cast<int>(jpeg_thumnail.size) << ", encoded size: " << static_cast<int>(encoded_size) << std::endl;
		if(!b64_thumnail.empty()) {
			StringMessage thumnail_msg;
			thumnail_msg.MsgType = MessageType::COLLECTION_THUMBNAIL;
			thumnail_msg.content = b64_thumnail;
			CommandManager& cm = CommandManager::GetCM();
			cm.SubmitMessage(thumnail_msg);
		} else {
			LOG(WARNING) << "Cannot send thumnail, unable to b64 encode byte array." << std::endl;
		}
	}
}

void CameraManager::HandleCapture(std::pair<EncodedImage, EncodedImage> cap) {
	if (!cap.first.valid() || !cap.second.valid()) {
		LOG(DEBUG) << "Capture is null, cannot process." << std::endl;
		return;
	}

	LOG(INFO) << "Doing capture stuff." << std::endl;

	// Look for QR
	QRDetection found_qr = this->LookForQR(cap.first);

	LOG(INFO) << "Pass QR det.";
	auto now = std::chrono::system_clock::now();
	found_qr.event_time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

	found_qr.rgb_img = ImageUtils::RawToJpg(cap.first);
	found_qr.depth_image = ImageUtils::RawToPng(cap.second);

	if(found_qr.identifier >= 0) {         
		// Get weight
		this->event_state.detection_event.weight = WeightExporter::read_weight_grams();   
		CommandManager& cm = CommandManager::GetCM();

		this->event_state.detection_event = found_qr;
		// Then let the ui know
		StringMessage notify_msg;
		notify_msg.MsgType = MessageType::COLLECTION_SUCCESS;
		json content_obj;
		content_obj["id"] = this->event_state.detection_event.identifier;
		content_obj["weight"] = this->event_state.detection_event.weight;
		notify_msg.content = content_obj.dump();
		cm.SubmitMessage(notify_msg);

		this->TransitionTo(CameraState::WAITING_FOR_DECISION);

	} else {
		if(this->frames_since_event_started >= (MAX_FRAMES_FOR_COLLECTION - 1)) {
			// Store last image as capture
			this->event_state.detection_event = found_qr;
			// Get weight
			this->event_state.detection_event.weight = WeightExporter::read_weight_grams();
			StringMessage msg;
			json content_obj;
			content_obj["weight"] = this->event_state.detection_event.weight;
			msg.content = content_obj.dump();
			msg.MsgType = MessageType::DETECTION_FAILURE;
			CommandManager::GetCM().SubmitMessage(msg);

			this->TransitionTo(CameraState::WAITING_FOR_DECISION);
		} else {
			this->frames_since_event_started = this->frames_since_event_started + 1;
		}
	}
}
