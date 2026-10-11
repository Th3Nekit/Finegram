/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioSessionConfiguration.h"
#import "RTCAudioSession.h"

#import "helpers/RTCDispatcher.h"
#import "helpers/UIDevice+RTCDevice.h"

const int kRTCAudioSessionPreferredNumberOfChannels = 1;

const double kRTCAudioSessionHighPerformanceSampleRate = 48000.0;

const double kRTCAudioSessionLowComplexitySampleRate = 16000.0;

const double kRTCAudioSessionHighPerformanceIOBufferDuration = 0.02;

const double kRTCAudioSessionLowComplexityIOBufferDuration = 0.06;

static RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *gWebRTCConfiguration = nil;

@implementation RTC_OBJC_TYPE (RTCAudioSessionConfiguration)

@synthesize category = _category;
@synthesize categoryOptions = _categoryOptions;
@synthesize mode = _mode;
@synthesize sampleRate = _sampleRate;
@synthesize ioBufferDuration = _ioBufferDuration;
@synthesize inputNumberOfChannels = _inputNumberOfChannels;
@synthesize outputNumberOfChannels = _outputNumberOfChannels;

- (instancetype)init {
  if (self = [super init]) {

    _category = AVAudioSessionCategoryPlayAndRecord;
#if defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000
    _categoryOptions = AVAudioSessionCategoryOptionAllowBluetoothHFP;
#else
      _categoryOptions = AVAudioSessionCategoryOptionAllowBluetooth;
#endif

    _mode = AVAudioSessionModeVoiceChat;

    NSUInteger processorCount = [NSProcessInfo processInfo].processorCount;

    if (processorCount > 1) {
      _sampleRate = kRTCAudioSessionHighPerformanceSampleRate;
      _ioBufferDuration = kRTCAudioSessionHighPerformanceIOBufferDuration;
    } else {
      _sampleRate = kRTCAudioSessionLowComplexitySampleRate;
      _ioBufferDuration = kRTCAudioSessionLowComplexityIOBufferDuration;
    }

    _inputNumberOfChannels = kRTCAudioSessionPreferredNumberOfChannels;
    _outputNumberOfChannels = kRTCAudioSessionPreferredNumberOfChannels;
  }
  return self;
}

+ (void)initialize {
  gWebRTCConfiguration = [[self alloc] init];
}

+ (instancetype)currentConfiguration {
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *config =
      [[RTC_OBJC_TYPE(RTCAudioSessionConfiguration) alloc] init];
  config.category = session.category;
  config.categoryOptions = session.categoryOptions;
  config.mode = session.mode;
  config.sampleRate = session.sampleRate;
  config.ioBufferDuration = session.IOBufferDuration;
  config.inputNumberOfChannels = session.inputNumberOfChannels;
  config.outputNumberOfChannels = session.outputNumberOfChannels;
  return config;
}

+ (instancetype)webRTCConfiguration {
  @synchronized(self) {
    return (RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *)gWebRTCConfiguration;
  }
}

+ (void)setWebRTCConfiguration:(RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *)configuration {
  @synchronized(self) {
    gWebRTCConfiguration = configuration;
  }
}

@end
