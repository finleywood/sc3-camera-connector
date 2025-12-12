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

// Define static constexpr members
constexpr int CameraManager::MAX_TIMEOUTS;
constexpr int CameraManager::MAX_RECONNECT_ATTEMPTS;
constexpr int CameraManager::MAX_ENUM_ATTEMPTS;
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
    this->event_state.detection_event.identifier = msg.content;
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
		case CameraState::DEVICE_OPEN:
			HandleDeviceOpen();
			break;
		case CameraState::STARTING_CAMERAS:
			HandleStartingCameras();
			break;
		case CameraState::STREAMING:
			HandleStreaming();
			break;
		case CameraState::CAPTURE_TIMEOUT:
			HandleCaptureTimeout();
			break;
		case CameraState::HEALTH_CHECK:
			HandleHealthCheck();
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
		TransitionTo(CameraState::DEVICE_OPEN);
	} else {
		LOG(ERROR) << "Failed to open device" << std::endl;
		TransitionTo(CameraState::RECONNECTING);
	}
}

void CameraManager::HandleDeviceOpen() {
	LOG(INFO) << "Device opened, configuring cameras..." << std::endl;
	TransitionTo(CameraState::STARTING_CAMERAS);
}

void CameraManager::HandleStartingCameras() {
	if (StartCameras()) {
		consecutive_timeouts = 0;
		TransitionTo(CameraState::STREAMING);
	} else {
		LOG(ERROR) << "Failed to start cameras" << std::endl;
		TransitionTo(CameraState::RECONNECTING);
	}
}

void CameraManager::HandleStreaming() {
	LOG(INFO) << "Handling streaming" << std::endl;
	k4a_capture_t capture = nullptr;
	k4a_wait_result_t result = k4a_device_get_capture(current_device, &capture, 1000);
	LOG(INFO) << "Got result: " << static_cast<int>(result)  << std::endl;
	if(!this->event_state.valid()) {
		LOG(INFO) << "Event state not valid." << std::endl;
		if(capture)
			k4a_capture_release(capture);
		return;
	}

	if(result == K4A_WAIT_RESULT_TIMEOUT || result == K4A_WAIT_RESULT_FAILED) {
		consecutive_timeouts++;
		if (consecutive_timeouts >= MAX_TIMEOUTS) {
			LOG(INFO) << "Too many timeouts, checking health..." << std::endl;
			TransitionTo(CameraState::HEALTH_CHECK);
		}
		return;
	}
    
	consecutive_timeouts = 0;
	LOG(DEBUG) << "Starting streaming." << std::endl;
	HandleCapture(&capture);
	if (capture != nullptr) {
		k4a_capture_release(capture);
	}
}

void CameraManager::HandleCaptureTimeout() {
	// This is handled in STREAMING state
	TransitionTo(CameraState::HEALTH_CHECK);
}

void CameraManager::HandleHealthCheck() {
	LOG(INFO) << "Performing health check..." << std::endl;
    
	if (IsDeviceConnected()) {
		LOG(INFO) << "Device OK, resuming streaming" << std::endl;
		consecutive_timeouts = 0;
		TransitionTo(CameraState::STREAMING);
	} else {
		LOG(INFO) << "Device not responding" << std::endl;
		TransitionTo(CameraState::DISCONNECTED);
	}
}

void CameraManager::HandleDisconnected() {
	LOG(INFO) << "Device disconnected!" << std::endl;
	CloseDevice();
	reconnect_attempts = 0;
	device_enum_attempts = 0;
	TransitionTo(CameraState::RECONNECTING);
}

void CameraManager::HandleReconnecting() {
	LOG(INFO) << "Starting reconnection process..." << std::endl;
	LOG(INFO) << "Wating for device." << std::endl;
	CleanupResources();
	TransitionTo(CameraState::WAITING_FOR_DEVICE);
}

void CameraManager::HandleWaitingForDevice() {
	device_enum_attempts++;
    
	LOG(DEBUG) << "Waiting for device (attempt " << device_enum_attempts 
						 << "/" << MAX_ENUM_ATTEMPTS << ")..." << std::endl;
    
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
	uint32_t device_count = k4a_device_get_installed_count();
    
	if (device_count > camera_idx) {
		LOG(INFO) << "Device detected, attempting to open..." << std::endl;
		device_enum_attempts = 0;
        
		if (OpenDevice()) {
			TransitionTo(CameraState::DEVICE_OPEN);
			reconnect_attempts = 0;
		} else {
			reconnect_attempts++;
			if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
				reconnect_attempts = 0;
				TransitionTo(CameraState::RECONNECT_FAILED);
			}
		}
	} else if (device_enum_attempts >= MAX_ENUM_ATTEMPTS) {
		device_enum_attempts = 0;
		reconnect_attempts++;
        
		if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
			TransitionTo(CameraState::RECONNECT_FAILED);
		}
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
	uint32_t device_count = k4a_device_get_installed_count();
    
	if (device_count == 0) {
		LOG(WARNING) << "No devices found" << std::endl;
		return false;
	}
    
	if (camera_idx >= device_count) {
		LOG(ERROR) << "Camera index " << camera_idx 
							 << " out of range (only " << device_count 
							 << " devices available)" << std::endl;
		return false;
	}
    
	// Disable k4a internal logger as it bloats the log
	k4a_set_debug_message_handler(NULL, NULL, K4A_LOG_LEVEL_OFF);
	if (K4A_FAILED(k4a_device_open(camera_idx, &current_device))) {
		LOG(ERROR) << "Failed to open device " << camera_idx << std::endl;
		return false;
	}

	// GUI Init
	if (gui_enabled) {
		std::lock_guard<std::mutex> lock(gui_mutex);
		cv::namedWindow(gui_window_name, cv::WINDOW_NORMAL);
		cv::resizeWindow(gui_window_name, 960, 540);
	}
    
	LOG(INFO) << "Device " << camera_idx << " opened successfully" << std::endl;
	return true;
}

bool CameraManager::StartCameras() {
	device_config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	device_config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
	device_config.color_resolution = K4A_COLOR_RESOLUTION_2160P;
	device_config.depth_mode = enable_depth ? 
		K4A_DEPTH_MODE_NFOV_UNBINNED : K4A_DEPTH_MODE_OFF;
	device_config.camera_fps = K4A_FRAMES_PER_SECOND_5;
    
	if (K4A_FAILED(k4a_device_start_cameras(current_device, &device_config))) {
		LOG(ERROR) << "Failed to start cameras" << std::endl;
		return false;
	}
    
	LOG(INFO) << "Cameras started successfully" << std::endl;
	return true;
}

bool CameraManager::IsDeviceConnected() {
	if (current_device == nullptr) {
		return false;
	}
    
	// Try to get serial number as health check
	size_t serial_size = 0;
	k4a_buffer_result_t result = k4a_device_get_serialnum(
																												current_device, nullptr, &serial_size);
    
	return result != K4A_BUFFER_RESULT_FAILED;
}

void CameraManager::CloseDevice() {
	if (current_device == nullptr) {
		return;
	}
    
	LOG(INFO) << "Closing device..." << std::endl;
	try {
		k4a_device_stop_cameras(current_device);
		k4a_device_close(current_device);
	} catch(std::exception& e) {
		LOG(WARNING) << "Closing cameras failed due to: " << e.what() << std::endl;
	}
	current_device = nullptr;
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

QRDetection CameraManager::LookForQR(k4a_image_t* img) {
	cv::Mat cv_mat_img = ImageUtils::K4ARGBToCV(img);
    
	cv::QRCodeDetector cv_qr;
	std::vector<cv::Point> qr_detections;
	try {
		std::string found_data = cv_qr.detectAndDecode(cv_mat_img, qr_detections);
		if (!found_data.empty()) {
			LOG(DEBUG) << "Detector detected and decoded a QR." << std::endl;
			QRDetection found_qr;
			found_qr.identifier = found_data;
			return found_qr;
		}
		return {};
	} catch(std::exception& e) {
		LOG(WARNING) << "QR Code detection failed. Due to " << e.what() << std::endl;
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

void CameraManager::HandleCapture(k4a_capture_t* cap) {
	if (cap == nullptr || *cap == nullptr) {
		LOG(DEBUG) << "Capture is null, cannot process." << std::endl;
		return;
	}

	LOG(INFO) << "Doing capture stuff." << std::endl;
    
	k4a_image_t color_image = k4a_capture_get_color_image(*cap);
	k4a_image_t depth_image = nullptr;
    
	if (enable_depth) {
		depth_image = k4a_capture_get_depth_image(*cap);
	}
    
	if (color_image != nullptr) {
		// GUI Stuff
		if (gui_enabled) {
			cv::Mat cv_mat_img = ImageUtils::K4ARGBToCV(&color_image);
			std::lock_guard<std::mutex> lock(gui_mutex);
			cv::imshow(gui_window_name, cv_mat_img);
			cv::waitKey(1); // non-blocking
		}
		// Look for QR
		QRDetection found_qr = this->LookForQR(&color_image);
		auto now = std::chrono::system_clock::now();
		found_qr.event_time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

		EncodedImage rgb_jpeg = ImageUtils::K4AToJpeg(&color_image);
		if (!rgb_jpeg.valid()) {
			LOG(ERROR) << "Failed to encode RGB image" << std::endl;
			if (color_image != nullptr) k4a_image_release(color_image);
			if (depth_image != nullptr) k4a_image_release(depth_image);
			return;
		}
		found_qr.rgb_img = std::move(rgb_jpeg);

		if (enable_depth && depth_image != nullptr) {
			EncodedImage depth_png = ImageUtils::K4AToPng(&depth_image);
			if (depth_png.valid()) {
				found_qr.depth_image = std::move(depth_png);
			}
		}

		if(!found_qr.identifier.empty()) {            
			CommandManager& cm = CommandManager::GetCM();

			// Then let the ui know
			StringMessage notify_msg;
			notify_msg.MsgType = MessageType::COLLECTION_SUCCESS;
			notify_msg.content = this->event_state.detection_event.identifier;
			cm.SubmitMessage(notify_msg);

			this->event_state.detection_event = found_qr;

			this->TransitionTo(CameraState::WAITING_FOR_DECISION);

		} else {
			if(this->frames_since_event_started >= (MAX_FRAMES_FOR_COLLECTION - 1)) {
				StringMessage msg;
				msg.MsgType = MessageType::DETECTION_FAILURE;
				CommandManager::GetCM().SubmitMessage(msg);

				// Store last image as capture
				this->event_state.detection_event = found_qr;

				this->TransitionTo(CameraState::WAITING_FOR_DECISION);
			} else {
				this->frames_since_event_started = this->frames_since_event_started + 1;
			}
		}
	}
    
	if (color_image != nullptr) k4a_image_release(color_image);
	if (depth_image != nullptr) k4a_image_release(depth_image);
}
