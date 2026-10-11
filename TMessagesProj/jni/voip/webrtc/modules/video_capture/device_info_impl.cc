/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/device_info_impl.h"

#include <stdlib.h>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "rtc_base/logging.h"

#ifndef abs
#define abs(a) (a >= 0 ? a : -a)
#endif

namespace webrtc {
namespace videocapturemodule {

DeviceInfoImpl::DeviceInfoImpl()
    : _lastUsedDeviceName(NULL), _lastUsedDeviceNameLength(0) {}

DeviceInfoImpl::~DeviceInfoImpl(void) {
  MutexLock lock(&_apiLock);
  free(_lastUsedDeviceName);
}

int32_t DeviceInfoImpl::NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
  if (!deviceUniqueIdUTF8)
    return -1;

  MutexLock lock(&_apiLock);

  if (absl::EqualsIgnoreCase(
          deviceUniqueIdUTF8,
          absl::string_view(_lastUsedDeviceName, _lastUsedDeviceNameLength))) {
    return static_cast<int32_t>(_captureCapabilities.size());
  }

  int32_t ret = CreateCapabilityMap(deviceUniqueIdUTF8);
  return ret;
}

int32_t DeviceInfoImpl::GetCapability(const char* deviceUniqueIdUTF8,
                                      const uint32_t deviceCapabilityNumber,
                                      VideoCaptureCapability& capability) {
  RTC_DCHECK(deviceUniqueIdUTF8);

  MutexLock lock(&_apiLock);

  if (!absl::EqualsIgnoreCase(
          deviceUniqueIdUTF8,
          absl::string_view(_lastUsedDeviceName, _lastUsedDeviceNameLength))) {
    if (-1 == CreateCapabilityMap(deviceUniqueIdUTF8)) {
      return -1;
    }
  }

  if (deviceCapabilityNumber >= (unsigned int)_captureCapabilities.size()) {
    RTC_LOG(LS_ERROR) << "Invalid deviceCapabilityNumber "
                      << deviceCapabilityNumber << ">= number of capabilities ("
                      << _captureCapabilities.size() << ").";
    return -1;
  }

  capability = _captureCapabilities[deviceCapabilityNumber];
  return 0;
}

int32_t DeviceInfoImpl::GetBestMatchedCapability(
    const char* deviceUniqueIdUTF8,
    const VideoCaptureCapability& requested,
    VideoCaptureCapability& resulting) {
  if (!deviceUniqueIdUTF8)
    return -1;

  MutexLock lock(&_apiLock);
  if (!absl::EqualsIgnoreCase(
          deviceUniqueIdUTF8,
          absl::string_view(_lastUsedDeviceName, _lastUsedDeviceNameLength))) {
    if (-1 == CreateCapabilityMap(deviceUniqueIdUTF8)) {
      return -1;
    }
  }

  int32_t bestformatIndex = -1;
  int32_t bestWidth = 0;
  int32_t bestHeight = 0;
  int32_t bestFrameRate = 0;
  VideoType bestVideoType = VideoType::kUnknown;

  const int32_t numberOfCapabilies =
      static_cast<int32_t>(_captureCapabilities.size());

  for (int32_t tmp = 0; tmp < numberOfCapabilies;
       ++tmp)
  {
    VideoCaptureCapability& capability = _captureCapabilities[tmp];

    const int32_t diffWidth = capability.width - requested.width;
    const int32_t diffHeight = capability.height - requested.height;
    const int32_t diffFrameRate = capability.maxFPS - requested.maxFPS;

    const int32_t currentbestDiffWith = bestWidth - requested.width;
    const int32_t currentbestDiffHeight = bestHeight - requested.height;
    const int32_t currentbestDiffFrameRate = bestFrameRate - requested.maxFPS;

    if ((diffHeight >= 0 &&
         diffHeight <= abs(currentbestDiffHeight))

        || (currentbestDiffHeight < 0 && diffHeight >= currentbestDiffHeight)) {
      if (diffHeight ==
          currentbestDiffHeight)
      {
        if ((diffWidth >= 0 &&
             diffWidth <= abs(currentbestDiffWith))
            || (currentbestDiffWith < 0 && diffWidth >= currentbestDiffWith)) {
          if (diffWidth == currentbestDiffWith &&
              diffHeight == currentbestDiffHeight)
          {

            if (((diffFrameRate >= 0 &&
                  diffFrameRate <=
                      currentbestDiffFrameRate)

                 || (currentbestDiffFrameRate < 0 &&
                     diffFrameRate >=
                         currentbestDiffFrameRate))

            ) {
              if ((currentbestDiffFrameRate ==
                   diffFrameRate)

                  || (currentbestDiffFrameRate >= 0)) {
                if (bestVideoType != requested.videoType &&
                    requested.videoType != VideoType::kUnknown &&
                    (capability.videoType == requested.videoType ||
                     capability.videoType == VideoType::kI420 ||
                     capability.videoType == VideoType::kYUY2 ||
                     capability.videoType == VideoType::kYV12 ||
                     capability.videoType == VideoType::kNV12)) {
                  bestVideoType = capability.videoType;
                  bestformatIndex = tmp;
                }

                if (capability.height == requested.height &&
                    capability.width == requested.width &&
                    capability.maxFPS >= requested.maxFPS) {
                  bestformatIndex = tmp;
                }
              } else
              {
                bestWidth = capability.width;
                bestHeight = capability.height;
                bestFrameRate = capability.maxFPS;
                bestVideoType = capability.videoType;
                bestformatIndex = tmp;
              }
            }
          } else
          {
            bestWidth = capability.width;
            bestHeight = capability.height;
            bestFrameRate = capability.maxFPS;
            bestVideoType = capability.videoType;
            bestformatIndex = tmp;
          }
        }
      } else
      {
        bestWidth = capability.width;
        bestHeight = capability.height;
        bestFrameRate = capability.maxFPS;
        bestVideoType = capability.videoType;
        bestformatIndex = tmp;
      }
    }
  }

  RTC_LOG(LS_VERBOSE) << "Best camera format: " << bestWidth << "x"
                      << bestHeight << "@" << bestFrameRate
                      << "fps, color format: "
                      << static_cast<int>(bestVideoType);

  if (bestformatIndex < 0)
    return -1;
  resulting = _captureCapabilities[bestformatIndex];
  return bestformatIndex;
}

int32_t DeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                       VideoRotation& orientation) {
  orientation = kVideoRotation_0;
  return -1;
}
}
}
