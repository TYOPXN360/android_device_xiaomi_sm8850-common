/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Rear LED music light, a port of the stock HyperOS feature. Stock runs
 * libmis_audioled inside AudioFlinger on every buffer written to the HAL and
 * sends the result to the lights service every 40 ms. This effect sits on the
 * output mix and runs the same algorithm with the same tuning file:
 *
 *   level  = mean |mono sample| over a frame of 32 samples
 *   fast   = envelope follower (0.1 ms attack, 10 ms release)
 *   slow   = envelope follower (0.1 ms attack, 300 ms release)
 *   result = envelope follower (1.1 ms) of clamp(fast / slow, 0, 1)
 *   LED    = LUT[result * 256]
 *
 * The followers advance once per frame but use the sample rate, so the
 * effective times are 32x longer, as in stock. The brightness goes straight
 * to the AW21024 global current register; the app powers the LED up and down.
 */

#define LOG_TAG "AHAL_MusicLight"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <system/audio_effects/effect_uuid.h>

#include "effect-impl/EffectContext.h"
#include "effect-impl/EffectImpl.h"

using aidl::android::hardware::audio::effect::Descriptor;
using aidl::android::hardware::audio::effect::IEffect;
using aidl::android::hardware::audio::effect::stringToUuid;
using aidl::android::media::audio::common::AudioUuid;

namespace aidl::android::hardware::audio::effect {

namespace {

const AudioUuid kTypeUuid = stringToUuid("1a8feafc-486d-4fea-9e19-9a5f583f1c91");
const AudioUuid kImplUuid = stringToUuid("7e1bef4c-45d6-4672-8070-25a8a4d230a7");

constexpr char kConfigPath[] = "/odm/etc/audio/mis_audioled_configs.bin";
constexpr char kLedPath[] = "/sys/class/leds/aw21024_led/reg";
constexpr char kGlobalCurrentReg[] = "6e";

constexpr int kUpdateIntervalMs = 40;
constexpr int kMaxMixChannels = 3;
constexpr float kLevelGate = 4e-7f;
constexpr float kFastGate = 1e-7f;

/* Tuning, overwritten by the stock config file when it parses */
struct Config {
    int frameSize = 32;
    float slowAttack = 1e-4f, slowRelease = 0.3f;
    float fastAttack = 1e-4f, fastRelease = 0.01f;
    float smooth = 0.0011f;
    std::vector<uint8_t> lut;  // brightness per result step
};

template <typename T>
bool readAt(const std::string& data, size_t offset, T* value) {
    if (offset + sizeof(T) > data.size()) return false;
    memcpy(value, data.data() + offset, sizeof(T));
    return true;
}

bool tagAt(const std::string& data, size_t offset, const char* tag) {
    return offset + 4 <= data.size() && memcmp(data.data() + offset, tag, 4) == 0;
}

/* Layout as read by mis_audioled_proc_parse_bin */
Config loadConfig() {
    Config config;
    std::string data;
    int32_t lutSize = 0, frameSize = 0;
    if (::android::base::ReadFileToString(kConfigPath, &data) && tagAt(data, 0x00, "MLED") &&
        tagAt(data, 0x70, "PJBG") && tagAt(data, 0x1d4, "PJBE") && tagAt(data, 0x1d8, "LUTS") &&
        readAt(data, 0x74, &lutSize) && lutSize > 0 && lutSize <= 256 &&
        tagAt(data, 0x1dc + lutSize * 4, "LUTE") && readAt(data, 0x94, &config.smooth) &&
        readAt(data, 0xfc, &config.slowAttack) && readAt(data, 0x100, &config.slowRelease) &&
        readAt(data, 0x104, &config.fastAttack) && readAt(data, 0x108, &config.fastRelease) &&
        readAt(data, 0x10c, &frameSize) && frameSize > 0) {
        config.frameSize = std::min(frameSize, 256);
        for (int i = 0; i < lutSize; i++) {
            // 0xAABBGGRR; the LED is white, so the red byte is the level
            config.lut.push_back(static_cast<uint8_t>(data[0x1dc + i * 4]));
        }
        LOG(INFO) << "loaded " << kConfigPath << ", " << lutSize << " steps";
    } else {
        LOG(WARNING) << "no usable " << kConfigPath << ", using a linear ramp";
        for (int i = 0; i < 256; i++) {
            config.lut.push_back(static_cast<uint8_t>(i < 121 ? 0 : (i - 120) * 150 / 135));
        }
    }
    return config;
}

class EnvelopeFollower {
  public:
    void init(float sampleRate, float attack, float release) {
        mAttack = std::exp(-1.f / (sampleRate * attack));
        mRelease = std::exp(-1.f / (sampleRate * release));
        mState = 0;
    }
    float process(float x) {
        const float a = mState < x ? mAttack : mRelease;
        mState = a * mState + (1.f - a) * x;
        return mState;
    }

  private:
    float mAttack = 0, mRelease = 0, mState = 0;
};

/* Sysfs writes go through I2C, so keep them off the audio thread */
class LedWriter {
  public:
    static LedWriter& get() {
        static LedWriter* writer = new LedWriter();
        return *writer;
    }
    void set(int level) {
        if (mPending.exchange(level) != level) {
            mCv.notify_one();
        }
    }

  private:
    LedWriter() : mThread([this] { loop(); }) { mThread.detach(); }
    void loop() {
        int written = -1;
        std::unique_lock lock(mLock);
        while (true) {
            mCv.wait(lock, [&] { return mPending.load() != written; });
            int level = mPending.load();
            lock.unlock();
            char buf[16];
            int len = snprintf(buf, sizeof(buf), "%s %x", kGlobalCurrentReg, level);
            int fd = open(kLedPath, O_WRONLY | O_CLOEXEC);
            if (fd >= 0) {
                if (write(fd, buf, len) != len) PLOG(WARNING) << "write " << kLedPath;
                close(fd);
            }
            written = level;
            lock.lock();
        }
    }
    std::mutex mLock;
    std::condition_variable mCv;
    std::atomic<int> mPending{0};
    std::thread mThread;
};

class MusicLightContext final : public EffectContext {
  public:
    MusicLightContext(int statusDepth, const Parameter::Common& common)
        : EffectContext(statusDepth, common), mConfig(loadConfig()) {
        const float rate = common.input.base.sampleRate;
        mSlow.init(rate, mConfig.slowAttack, mConfig.slowRelease);
        mFast.init(rate, mConfig.fastAttack, mConfig.fastRelease);
        mSmooth.init(rate, mConfig.smooth, mConfig.smooth);
        mUpdateFrames = std::max(1, static_cast<int>(rate) * kUpdateIntervalMs / 1000);
    }

    RetCode disable() override {
        LedWriter::get().set(0);
        return EffectContext::disable();
    }

    IEffect::Status process(float* in, float* out, int samples) {
        if (in != out) memcpy(out, in, samples * sizeof(float));
        const int channels = std::max<int>(1, mInputChannelCount);
        const int mixChannels = std::min(channels, kMaxMixChannels);
        for (int i = 0; i + channels <= samples; i += channels) {
            float mono = 0;
            for (int c = 0; c < mixChannels; c++) mono += in[i + c];
            mono /= mixChannels;

            mSum += std::fabs(mono);
            if (++mSummed >= mConfig.frameSize) {
                analyzeFrame(mSum / mConfig.frameSize);
                mSum = 0;
                mSummed = 0;
            }
            if (++mSinceUpdate >= mUpdateFrames) {
                mSinceUpdate = 0;
                const size_t steps = mConfig.lut.size();
                size_t step = std::min(steps - 1, static_cast<size_t>(mResult * steps));
                LedWriter::get().set(mConfig.lut[step]);
            }
        }
        return {STATUS_OK, samples, samples};
    }

  private:
    void analyzeFrame(float level) {
        if (level <= kLevelGate) level = 0;
        const float fast = mFast.process(level);
        const float slow = mSlow.process(level);
        float ratio = 0;
        if (slow != 0) {
            ratio = std::clamp((fast > kFastGate ? fast : 0) / slow, 0.f, 1.f);
        }
        mResult = mSmooth.process(ratio);
    }

    const Config mConfig;
    EnvelopeFollower mSlow, mFast, mSmooth;
    float mSum = 0, mResult = 0;
    int mSummed = 0, mSinceUpdate = 0, mUpdateFrames = 0;
};

class MusicLightEffect final : public EffectImpl {
  public:
    static const Descriptor kDescriptor;
    ~MusicLightEffect() { cleanUp(); }

    ndk::ScopedAStatus getDescriptor(Descriptor* _aidl_return) override {
        *_aidl_return = kDescriptor;
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus setParameterSpecific(const Parameter::Specific&)
            REQUIRES(mImplMutex) override {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    ndk::ScopedAStatus getParameterSpecific(const Parameter::Id&, Parameter::Specific*)
            REQUIRES(mImplMutex) override {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::shared_ptr<EffectContext> createContext(const Parameter::Common& common)
            REQUIRES(mImplMutex) override {
        if (!mContext) mContext = std::make_shared<MusicLightContext>(1, common);
        return mContext;
    }
    RetCode releaseContext() REQUIRES(mImplMutex) override {
        mContext.reset();
        return RetCode::SUCCESS;
    }
    std::string getEffectName() override { return "MusicLight"; }
    IEffect::Status effectProcessImpl(float* in, float* out, int samples)
            REQUIRES(mImplMutex) override {
        if (!mContext) return {EX_NULL_POINTER, 0, 0};
        return mContext->process(in, out, samples);
    }

  private:
    std::shared_ptr<MusicLightContext> mContext GUARDED_BY(mImplMutex);
};

const Descriptor MusicLightEffect::kDescriptor = {
        .common = {.id = {.type = kTypeUuid, .uuid = kImplUuid, .proxy = std::nullopt},
                   .flags = {.type = Flags::Type::INSERT,
                             .insert = Flags::Insert::LAST,
                             .volume = Flags::Volume::NONE},
                   .name = "MusicLight",
                   .implementor = "The LineageOS Project"}};

}  // namespace

}  // namespace aidl::android::hardware::audio::effect

using aidl::android::hardware::audio::effect::MusicLightEffect;

extern "C" binder_exception_t createEffect(const AudioUuid* in_impl_uuid,
                                           std::shared_ptr<IEffect>* instanceSpp) {
    if (!in_impl_uuid || *in_impl_uuid != aidl::android::hardware::audio::effect::kImplUuid ||
        !instanceSpp) {
        return EX_ILLEGAL_ARGUMENT;
    }
    *instanceSpp = ndk::SharedRefBase::make<MusicLightEffect>();
    return EX_NONE;
}

extern "C" binder_exception_t queryEffect(const AudioUuid* in_impl_uuid, Descriptor* _aidl_return) {
    if (!in_impl_uuid || *in_impl_uuid != aidl::android::hardware::audio::effect::kImplUuid ||
        !_aidl_return) {
        return EX_ILLEGAL_ARGUMENT;
    }
    *_aidl_return = MusicLightEffect::kDescriptor;
    return EX_NONE;
}
