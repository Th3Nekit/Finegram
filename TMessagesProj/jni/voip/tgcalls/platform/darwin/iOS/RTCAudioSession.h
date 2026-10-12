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

extern NSString *const kRTCAudioSessionErrorDomain;

extern NSInteger const kRTCAudioSessionErrorLockRequired;

extern NSInteger const kRTCAudioSessionErrorConfiguration;

@class RTC_OBJC_TYPE(RTCAudioSession);
@class RTC_OBJC_TYPE(RTCAudioSessionConfiguration);

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCAudioSessionDelegate)<NSObject>

    @optional

- (void)audioSessionDidBeginInterruption:(RTC_OBJC_TYPE(RTCAudioSession) *)session;

- (void)audioSessionDidEndInterruption:(RTC_OBJC_TYPE(RTCAudioSession) *)session
                   shouldResumeSession:(BOOL)shouldResumeSession;

- (void)audioSessionDidChangeRoute:(RTC_OBJC_TYPE(RTCAudioSession) *)session
                            reason:(AVAudioSessionRouteChangeReason)reason
                     previousRoute:(AVAudioSessionRouteDescription *)previousRoute;

- (void)audioSessionMediaServerTerminated:(RTC_OBJC_TYPE(RTCAudioSession) *)session;

- (void)audioSessionMediaServerReset:(RTC_OBJC_TYPE(RTCAudioSession) *)session;

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)session
    didChangeCanPlayOrRecord:(BOOL)canPlayOrRecord;

- (void)audioSessionDidStartPlayOrRecord:(RTC_OBJC_TYPE(RTCAudioSession) *)session;

- (void)audioSessionDidStopPlayOrRecord:(RTC_OBJC_TYPE(RTCAudioSession) *)session;

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)audioSession
    didChangeOutputVolume:(float)outputVolume;

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)audioSession
    didDetectPlayoutGlitch:(int64_t)totalNumberOfGlitches;

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)audioSession willSetActive:(BOOL)active;

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)audioSession didSetActive:(BOOL)active;

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)audioSession
    failedToSetActive:(BOOL)active
                error:(NSError *)error;

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)audioSession
    audioUnitStartFailedWithError:(NSError *)error;

@end

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCAudioSessionActivationDelegate)<NSObject>

    - (void)audioSessionDidActivate : (AVAudioSession *)session;

- (void)audioSessionDidDeactivate:(AVAudioSession *)session;

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioSession) : NSObject <RTC_OBJC_TYPE(RTCAudioSessionActivationDelegate)>

@property(nonatomic, readonly) AVAudioSession *session;

@property(nonatomic, readonly) BOOL isActive;

@property(nonatomic, assign) BOOL useManualAudio;

@property(nonatomic, assign) BOOL isAudioEnabled;

@property(readonly) NSString *category;
@property(readonly) AVAudioSessionCategoryOptions categoryOptions;
@property(readonly) NSString *mode;
@property(readonly) BOOL secondaryAudioShouldBeSilencedHint;
@property(readonly) AVAudioSessionRouteDescription *currentRoute;
@property(readonly) NSInteger maximumInputNumberOfChannels;
@property(readonly) NSInteger maximumOutputNumberOfChannels;
@property(readonly) float inputGain;
@property(readonly) BOOL inputGainSettable;
@property(readonly) BOOL inputAvailable;
@property(readonly, nullable) NSArray<AVAudioSessionDataSourceDescription *> *inputDataSources;
@property(readonly, nullable) AVAudioSessionDataSourceDescription *inputDataSource;
@property(readonly, nullable) NSArray<AVAudioSessionDataSourceDescription *> *outputDataSources;
@property(readonly, nullable) AVAudioSessionDataSourceDescription *outputDataSource;
@property(readonly) double sampleRate;
@property(readonly) double preferredSampleRate;
@property(readonly) NSInteger inputNumberOfChannels;
@property(readonly) NSInteger outputNumberOfChannels;
@property(readonly) float outputVolume;
@property(readonly) NSTimeInterval inputLatency;
@property(readonly) NSTimeInterval outputLatency;
@property(readonly) NSTimeInterval IOBufferDuration;
@property(readonly) NSTimeInterval preferredIOBufferDuration;

@property(nonatomic) BOOL ignoresPreferredAttributeConfigurationErrors;

+ (instancetype)sharedInstance;
- (instancetype)init NS_UNAVAILABLE;

- (void)addDelegate:(id<RTC_OBJC_TYPE(RTCAudioSessionDelegate)>)delegate;

- (void)removeDelegate:(id<RTC_OBJC_TYPE(RTCAudioSessionDelegate)>)delegate;

- (void)lockForConfiguration;

- (void)unlockForConfiguration;

- (BOOL)setActive:(BOOL)active error:(NSError **)outError;

- (BOOL)setCategory:(NSString *)category
        withOptions:(AVAudioSessionCategoryOptions)options
              error:(NSError **)outError;
- (BOOL)setMode:(NSString *)mode error:(NSError **)outError;
- (BOOL)setInputGain:(float)gain error:(NSError **)outError;
- (BOOL)setPreferredSampleRate:(double)sampleRate error:(NSError **)outError;
- (BOOL)setPreferredIOBufferDuration:(NSTimeInterval)duration error:(NSError **)outError;
- (BOOL)setPreferredInputNumberOfChannels:(NSInteger)count error:(NSError **)outError;
- (BOOL)setPreferredOutputNumberOfChannels:(NSInteger)count error:(NSError **)outError;
- (BOOL)overrideOutputAudioPort:(AVAudioSessionPortOverride)portOverride error:(NSError **)outError;
- (BOOL)setPreferredInput:(AVAudioSessionPortDescription *)inPort error:(NSError **)outError;
- (BOOL)setInputDataSource:(AVAudioSessionDataSourceDescription *)dataSource
                     error:(NSError **)outError;
- (BOOL)setOutputDataSource:(AVAudioSessionDataSourceDescription *)dataSource
                      error:(NSError **)outError;
@end

@interface RTC_OBJC_TYPE (RTCAudioSession)
(Configuration)

    - (BOOL)setConfiguration : (RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *)configuration error
    : (NSError **)outError;

- (BOOL)setConfiguration:(RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *)configuration
                  active:(BOOL)active
                   error:(NSError **)outError
        disableRecording:(BOOL)disableRecording;

@end

NS_ASSUME_NONNULL_END
