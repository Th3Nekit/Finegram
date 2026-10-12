/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioSession.h"

NS_ASSUME_NONNULL_BEGIN

@class RTC_OBJC_TYPE(RTCAudioSessionConfiguration);

@interface RTC_OBJC_TYPE (RTCAudioSession)
()

    @property(nonatomic, readonly) int activationCount;

@property(nonatomic, readonly) int webRTCSessionCount;

@property(readonly) BOOL canPlayOrRecord;

@property(nonatomic, assign) BOOL isInterrupted;

- (void)pushDelegate:(id<RTC_OBJC_TYPE(RTCAudioSessionDelegate)>)delegate;

- (BOOL)beginWebRTCSession:(NSError **)outError;

- (BOOL)endWebRTCSession:(NSError **)outError;

- (BOOL)configureWebRTCSession:(NSError **)outError disableRecording:(BOOL)disableRecording;

- (BOOL)unconfigureWebRTCSession:(NSError **)outError;

- (NSError *)configurationErrorWithDescription:(NSString *)description;

- (void)notifyDidDetectPlayoutGlitch:(int64_t)totalNumberOfGlitches;

- (void)notifyAudioUnitStartFailedWithError:(OSStatus)error;

- (void)notifyDidBeginInterruption;
- (void)notifyDidEndInterruptionWithShouldResumeSession:(BOOL)shouldResumeSession;
- (void)notifyDidChangeRouteWithReason:(AVAudioSessionRouteChangeReason)reason
                         previousRoute:(AVAudioSessionRouteDescription *)previousRoute;
- (void)notifyMediaServicesWereLost;
- (void)notifyMediaServicesWereReset;
- (void)notifyDidChangeCanPlayOrRecord:(BOOL)canPlayOrRecord;
- (void)notifyDidStartPlayOrRecord;
- (void)notifyDidStopPlayOrRecord;

@end

NS_ASSUME_NONNULL_END
