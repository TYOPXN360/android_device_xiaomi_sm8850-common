/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The helpers the AOSP effect base classes need from
 * hardware/interfaces/audio/aidl/common/include/Utils.h, for
 * android.media.audio.common.types-V4 (that header needs a newer version).
 */

#pragma once

#include <cstdint>
#include <limits>

#include <aidl/android/media/audio/common/AudioChannelLayout.h>
#include <aidl/android/media/audio/common/AudioFormatDescription.h>
#include <aidl/android/media/audio/common/PcmType.h>

namespace aidl::android::hardware::audio::common {

constexpr size_t getPcmSampleSizeInBytes(::aidl::android::media::audio::common::PcmType pcm) {
    using ::aidl::android::media::audio::common::PcmType;
    switch (pcm) {
        case PcmType::UINT_8_BIT:
            return 1;
        case PcmType::INT_16_BIT:
            return 2;
        case PcmType::INT_32_BIT:
        case PcmType::FIXED_Q_8_24:
        case PcmType::FLOAT_32_BIT:
            return 4;
        case PcmType::INT_24_BIT:
            return 3;
    }
    return 0;
}

constexpr size_t getChannelCount(
        const ::aidl::android::media::audio::common::AudioChannelLayout& layout,
        int32_t mask = std::numeric_limits<int32_t>::max()) {
    using Tag = ::aidl::android::media::audio::common::AudioChannelLayout::Tag;
    switch (layout.getTag()) {
        case Tag::none:
        case Tag::invalid:
            return 0;
        case Tag::indexMask:
            return __builtin_popcount(layout.get<Tag::indexMask>() & mask);
        case Tag::layoutMask:
            return __builtin_popcount(layout.get<Tag::layoutMask>() & mask);
        case Tag::voiceMask:
            return __builtin_popcount(layout.get<Tag::voiceMask>() & mask);
    }
    return 0;
}

constexpr size_t getFrameSizeInBytes(
        const ::aidl::android::media::audio::common::AudioFormatDescription& format,
        const ::aidl::android::media::audio::common::AudioChannelLayout& layout) {
    using ::aidl::android::media::audio::common::AudioFormatType;
    if (format == ::aidl::android::media::audio::common::AudioFormatDescription{}) {
        return 0;
    }
    if (format.type == AudioFormatType::PCM) {
        return getPcmSampleSizeInBytes(format.pcm) * getChannelCount(layout);
    }
    if (format.type == AudioFormatType::NON_PCM) {
        return getPcmSampleSizeInBytes(format.pcm);
    }
    return 0;
}

}  // namespace aidl::android::hardware::audio::common
