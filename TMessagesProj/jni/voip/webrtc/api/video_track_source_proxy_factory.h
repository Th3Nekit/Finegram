/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_TRACK_SOURCE_PROXY_FACTORY_H_
#define API_VIDEO_TRACK_SOURCE_PROXY_FACTORY_H_

#include "api/media_stream_interface.h"
#include "rtc_base/thread.h"

namespace webrtc {

rtc::scoped_refptr<VideoTrackSourceInterface> RTC_EXPORT
CreateVideoTrackSourceProxy(rtc::Thread* signaling_thread,
                            rtc::Thread* worker_thread,
                            VideoTrackSourceInterface* source);

}

#endif
