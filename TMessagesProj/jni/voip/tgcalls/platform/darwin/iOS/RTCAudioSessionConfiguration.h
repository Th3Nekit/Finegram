/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

RTC_EXTERN const double kRTCAudioSessionHighPerformanceSampleRate;
RTC_EXTERN const double kRTCAudioSessionLowComplexitySampleRate;
RTC_EXTERN const double kRTCAudioSessionHighPerformanceIOBufferDuration;
RTC_EXTERN const double kRTCAudioSessionLowComplexityIOBufferDuration;

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioSessionConfiguration) : NSObject

@property(nonatomic, strong) NSString *category;
@property(nonatomic, assign) AVAudioSessionCategoryOptions categoryOptions;
@property(nonatomic, strong) NSString *mode;
@property(nonatomic, assign) double sampleRate;
@property(nonatomic, assign) NSTimeInterval ioBufferDuration;
@property(nonatomic, assign) NSInteger inputNumberOfChannels;
@property(nonatomic, assign) NSInteger outputNumberOfChannels;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

+ (instancetype)currentConfiguration;

+ (instancetype)webRTCConfiguration;

+ (void)setWebRTCConfiguration:(RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *)configuration;

@end

NS_ASSUME_NONNULL_END
