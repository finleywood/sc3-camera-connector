#pragma once

#include <k4a/k4a.h>
#include <opencv2/opencv.hpp>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include "EncodedImage.h"
#include "QRDetection.h"
#include "CommandManager.h"
#include "CommandReceiver.h"
#include "Messages/BaseMessage.h"
#include "Messages/StringMessage.h"

enum class CameraState {
  UNINITIALIZED,
  WAITING_FOR_COLLECTION,
	WAITING_FOR_DECISION,
	COLLECTION_FINISHED,
	INITIALIZING,
	DEVICE_OPEN,
	STARTING_CAMERAS,
	STREAMING,
	CAPTURE_TIMEOUT,
	HEALTH_CHECK,
	DISCONNECTED,
	RECONNECTING,
	WAITING_FOR_DEVICE,
	RECONNECT_FAILED,
	ERROR,
	STOPPING,
	STOPPED
};

struct CollectionEventState {
	std::string uid;
	QRDetection detection_event;
	bool collection_period_done = false;
	bool valid() { return !uid.empty(); };
};

using det_callback_t = std::function<void(const QRDetection&)>;
using state_callback_t = std::function<void(CameraState, CameraState)>;

class CameraManager : public CommandReceiver {
public:
	CameraManager(uint32_t camera_idx = 0, bool enable_depth = false, bool enable_gui_preview = false)
		: CommandReceiver()
		, camera_idx(camera_idx)
		, enable_depth(enable_depth)
		, current_state(CameraState::UNINITIALIZED)
		, gui_enabled(enable_gui_preview)
	{
		CommandManager::GetCM().Register("CameraManager", this);
	}

	~CameraManager() {
		Stop();
	}

	// Command receiver methods
	virtual bool AllowMessage(MessageType) override;
	virtual void ProcessMessage(const BaseMessage&) override;

	void RegisterStateCallback(state_callback_t callback_func);
    
	bool Start(bool is_blocking = false);
	void Stop();
    
	CameraState GetCurrentState() const { return current_state.load(); }
	bool IsStreaming() const { return current_state.load() == CameraState::STREAMING; }

private:
	// State machine
	std::atomic<CameraState> current_state;
	void TransitionTo(CameraState new_state);
	void RunStateMachine();
    
	// State handlers
	void HandleUninitialized();
	void HandleWaitingForCollection();
	void HandleCollectionFinished();
	void HandleWaitingForDecision();
	void HandleInitializing();
	void HandleDeviceOpen();
	void HandleStartingCameras();
	void HandleStreaming();
	void HandleCaptureTimeout();
	void HandleHealthCheck();
	void HandleDisconnected();
	void HandleReconnecting();
	void HandleWaitingForDevice();
	void HandleReconnectFailed();
	void HandleError();
	void HandleStopping();
	void HandleStopped();
    
	// Device management
	bool OpenDevice();
	bool StartCameras();
	bool IsDeviceConnected();
	void CloseDevice();
	void CleanupResources();
	void ResetEventState();

	// Process methods for command receiver
	void ProcessCollectionStart(const StringMessage&);
  void ProcessCollectionReject();
	void ProcessStartUpload(const StringMessage&);
	
	// Capture handling
	void HandleCapture(k4a_capture_t* cap);
	QRDetection LookForQR(k4a_image_t* img);

	// Event state
	CollectionEventState event_state;
    
	// Configuration
	uint32_t camera_idx;
	bool enable_depth;
	k4a_device_t current_device = nullptr;
	k4a_device_configuration_t device_config;
    
	// State tracking
	int consecutive_timeouts = 0;
	int reconnect_attempts = 0;
	int device_enum_attempts = 0;
	std::chrono::steady_clock::time_point last_reconnect_fail_time;
    
	// Limits
	static constexpr int MAX_TIMEOUTS = 5;
	static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
	static constexpr int MAX_ENUM_ATTEMPTS = 20;
	static constexpr int RECONNECT_COOLDOWN_SEC = 5;
	static constexpr int MAX_FRAMES_FOR_COLLECTION = 15;
    
	// callbacks
	state_callback_t state_callback;
    
	// Control
	std::atomic<bool> close_flag{false};
	std::thread state_machine_thread;
	std::atomic<int> frames_since_event_started = { 0 };

	// GUI Stuff
	bool gui_enabled = false;
	std::mutex gui_mutex;
	std::string gui_window_name = "Azure Kinect Debug Preview";
};
