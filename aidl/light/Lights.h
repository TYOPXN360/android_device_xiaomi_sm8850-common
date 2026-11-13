/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Devices.h>
#include <models/State.h>

#include <aidl/android/hardware/light/BnLights.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace aidl {
namespace android {
namespace hardware {
namespace light {

class Lights : public BnLights {
  public:
    Lights();
    ~Lights();

    ndk::ScopedAStatus setLightState(int32_t id, const HwLightState& hwLightState) override;
    ndk::ScopedAStatus getLights(std::vector<HwLight>* _aidl_return) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

  private:
    std::vector<HwLight> mLights;

    Devices mDevices;

    State mLastBatteryState;
    State mLastNotificationsState;
    State mLastAttentionState;
    State mLastMicrophoneState;
    State mLastCameraState;
    std::mutex mLedMutex;

    // Rear light preferences, read from properties written by the settings page.
    // The HAL cannot read Settings.System, and system properties are the only
    // channel available to a vendor process.
    std::atomic<bool> mRearLightEnabled{true};
    std::atomic<bool> mBatteryLightEnabled{true};
    std::atomic<bool> mNotificationLightEnabled{true};

    std::atomic<bool> mStopPolling{false};
    std::thread mPrefsPoller;

    // Returns true when any of the three preferences changed.
    bool reloadPreferences();
    void pollPreferences();

    void updateNotificationColor();
};

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
