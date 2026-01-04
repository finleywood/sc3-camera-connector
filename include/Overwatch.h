#pragma once

#include "CommandManager.h"
#include "CommandReceiver.h"
#include "CameraManager.h"
#include "UINotifier.h"
#include "UploadQueue.h"

#include <atomic>

class Overwatch : public CommandReceiver {
public:
    Overwatch(CameraManager* cam_mgr, UINotifier* ui_notif, UploadQueue* up_q) :
        CommandReceiver(),
        cam_mgr(cam_mgr),
        ui_notif(ui_notif),
        up_q(up_q)
    {
        CommandManager::GetCM().Register("Overwatch", this);
    }

    void Start();
    void Stop();

    virtual bool AllowMessage(MessageType) override;
	virtual void ProcessMessage(const BaseMessage&) override;

private:
    std::atomic<bool> should_close = { false };

    std::unique_ptr<CameraManager> cam_mgr;
    std::unique_ptr<UINotifier> ui_notif;
    std::unique_ptr<UploadQueue> up_q;

    void HandleShutdown();
    void HandleRestart();

    void ShutdownServices();
};