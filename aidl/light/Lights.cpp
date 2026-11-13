/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Lights.h>

#define LOG_TAG "Lights"

#include <Utils.h>

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <chrono>
#include <sys/system_properties.h>

namespace aidl {
namespace android {
namespace hardware {
namespace light {

#define AutoHwLight(light) {.id = static_cast<int32_t>(light), .ordinal = 0, .type = light}

Lights::Lights() {
    reloadPreferences();

    // Without a property change callback in libbase the only way to react to the
    // settings switches is to poll. Three property reads a second is nothing.
    mPrefsPoller = std::thread([this] { pollPreferences(); });

    if (mDevices.hasNotificationDevices()) {
        mLights.push_back(AutoHwLight(LightType::BATTERY));
        mLights.push_back(AutoHwLight(LightType::NOTIFICATIONS));
        mLights.push_back(AutoHwLight(LightType::ATTENTION));
        mLights.push_back(AutoHwLight(LightType::MICROPHONE));
        mLights.push_back(AutoHwLight(LightType::CAMERA));
    }
}

ndk::ScopedAStatus Lights::setLightState(int32_t id, const HwLightState& hwLightState) {
    State state = fromAidl(hwLightState);

    LightType type = static_cast<LightType>(id);
    switch (type) {
        case LightType::BATTERY:
            if (mDevices.hasNotificationDevices()) {
                mLastBatteryState = state;
                updateNotificationColor();
            } else {
                return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            }
            break;
        case LightType::NOTIFICATIONS:
            if (mDevices.hasNotificationDevices()) {
                mLastNotificationsState = state;
                updateNotificationColor();
            } else {
                return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            }
            break;
        case LightType::ATTENTION:
            if (mDevices.hasNotificationDevices()) {
                mLastAttentionState = state;
                updateNotificationColor();
            } else {
                return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            }
            break;
        case LightType::MICROPHONE:
            if (mDevices.hasNotificationDevices()) {
                mLastMicrophoneState = state;
                updateNotificationColor();
            } else {
                return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            }
            break;
        case LightType::CAMERA:
            if (mDevices.hasNotificationDevices()) {
                mLastCameraState = state;
                updateNotificationColor();
            } else {
                return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            }
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            break;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight>* _aidl_return) {
    for (const auto& light : mLights) {
        _aidl_return->push_back(light);
    }

    return ndk::ScopedAStatus::ok();
}

binder_status_t Lights::dump(int fd, const char** /*args*/, uint32_t /*numArgs*/) {
    dprintf(fd, "Lights AIDL:\n");
    dprintf(fd, "\n");

    dprintf(fd, "Lights:\n");
    for (const auto& light : mLights) {
        dprintf(fd, "- %d: LightType::%s\n", light.id, toString(light.type).c_str());
    }
    dprintf(fd, "\n");

    dprintf(fd, "Devices:\n");
    mDevices.dump(fd);
    dprintf(fd, "\n");

    return STATUS_OK;
}

Lights::~Lights() {
    mStopPolling = true;
    if (mPrefsPoller.joinable()) mPrefsPoller.join();
}

bool Lights::reloadPreferences() {
    const bool enabled =
            ::android::base::GetBoolProperty("persist.sys.rear_light.enable", true);
    const bool battery =
            ::android::base::GetBoolProperty("persist.sys.rear_light.battery", true);
    const bool notification =
            ::android::base::GetBoolProperty("persist.sys.rear_light.notify", true);

    const bool changed = (enabled != mRearLightEnabled.load()) ||
                         (battery != mBatteryLightEnabled.load()) ||
                         (notification != mNotificationLightEnabled.load());

    mRearLightEnabled = enabled;
    mBatteryLightEnabled = battery;
    mNotificationLightEnabled = notification;

    return changed;
}

void Lights::pollPreferences() {
    const prop_info* pi = nullptr;
    uint32_t serial = 0;

    while (!mStopPolling) {
        if (pi == nullptr) {
            pi = __system_property_find(kGenerationProp);
            if (pi != nullptr) serial = __system_property_serial(pi);
        }

        if (pi == nullptr) {
            // The switch has never been touched, so the property does not exist
            // yet. Nothing to wait on; look again in a minute.
            std::this_thread::sleep_for(std::chrono::seconds(60));
        } else {
            // Blocked in the kernel until the generation counter moves, woken by
            // a futex when the settings page writes it. The timeout only exists
            // so a recreated property is noticed.
            timespec timeout{60, 0};
            uint32_t newSerial = serial;
            __system_property_wait(pi, serial, &newSerial, &timeout);
            serial = newSerial;
        }

        if (mStopPolling) break;
        if (reloadPreferences()) updateNotificationColor();
    }
}

void Lights::updateNotificationColor() {
    std::lock_guard<std::mutex> lock(mLedMutex);

    // The camera and microphone indicators are privacy LEDs the user should
    // always get; everything else follows the settings switches.
    State state;
    if (mRearLightEnabled) {
        if (mLastCameraState.isLit()) {
            state = mLastCameraState;
        } else if (mLastMicrophoneState.isLit()) {
            state = mLastMicrophoneState;
        } else if (mNotificationLightEnabled && mLastNotificationsState.isLit()) {
            state = mLastNotificationsState;
        } else if (mNotificationLightEnabled && mLastAttentionState.isLit()) {
            state = mLastAttentionState;
        } else if (mBatteryLightEnabled && mLastBatteryState.isLit()) {
            state = mLastBatteryState;
        }
    }

    mDevices.setNotificationState(state);

    return;
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
