/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioSession.h"

NS_ASSUME_NONNULL_BEGIN

namespace webrtc {
class AudioSessionObserver;
}

@interface RTCNativeAudioSessionDelegateAdapter : NSObject <RTC_OBJC_TYPE (RTCAudioSessionDelegate)>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithObserver:(webrtc::AudioSessionObserver *)observer NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
