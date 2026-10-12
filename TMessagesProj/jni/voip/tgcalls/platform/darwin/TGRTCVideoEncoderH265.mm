/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "TGRTCVideoEncoderH265.h"

#import <VideoToolbox/VideoToolbox.h>
#include <vector>

#import "RTCCodecSpecificInfoH265.h"
#import "api/peerconnection/RTCVideoCodecInfo+Private.h"
#import "base/RTCI420Buffer.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif

#include "api/video_codecs/h264_profile_level_id.h"
#include "common_video/h265/h265_bitstream_parser.h"
#include "common_video/include/bitrate_adjuster.h"
#include "libyuv/convert_from.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/Framework/Classes/VideoToolbox/nalu_rewriter.h"
#include "system_wrappers/include/clock.h"
#include "h265_nalu_rewriter.h"

@interface RTC_OBJC_TYPE (RTCVideoEncoderH265)
()

    - (void)frameWasEncoded : (OSStatus)status flags : (VTEncodeInfoFlags)infoFlags sampleBuffer
    : (CMSampleBufferRef)sampleBuffer codecSpecificInfo
    : (id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)codecSpecificInfo width : (int32_t)width height
    : (int32_t)height renderTimeMs : (int64_t)renderTimeMs timestamp : (uint32_t)timestamp rotation
    : (RTCVideoRotation)rotation;

@end

namespace {

const float kLimitToAverageBitRateFactor = 1.5f;

const int kLowh265QpThreshold = 28;
const int kHighh265QpThreshold = 39;

struct API_AVAILABLE(ios(11.0)) RTCFrameEncodeParams {
  RTCFrameEncodeParams(RTC_OBJC_TYPE(RTCVideoEncoderH265) * e,
                       RTC_OBJC_TYPE(RTCCodecSpecificInfoH265) * csi,
                       int32_t w,
                       int32_t h,
                       int64_t rtms,
                       uint32_t ts,
                       RTCVideoRotation r)
      : encoder(e),
        width(w),
        height(h),
        render_time_ms(rtms),
        timestamp(ts),
        rotation(r) {
    if (csi) {
      codecSpecificInfo = csi;
    } else {
      codecSpecificInfo = [[RTC_OBJC_TYPE(RTCCodecSpecificInfoH265) alloc] init];
    }
  }

  RTC_OBJC_TYPE(RTCVideoEncoderH265) * encoder;
  RTC_OBJC_TYPE(RTCCodecSpecificInfoH265) * codecSpecificInfo;
  int32_t width;
  int32_t height;
  int64_t render_time_ms;
  uint32_t timestamp;
  RTCVideoRotation rotation;
};

bool CopyVideoFrameToPixelBuffer(id<RTCI420Buffer> frameBuffer,
                                 CVPixelBufferRef pixelBuffer) {
  RTC_DCHECK(pixelBuffer);
  RTC_DCHECK_EQ(CVPixelBufferGetPixelFormatType(pixelBuffer),
                kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
  RTC_DCHECK_EQ(CVPixelBufferGetHeightOfPlane(pixelBuffer, 0),
                frameBuffer.height);
  RTC_DCHECK_EQ(CVPixelBufferGetWidthOfPlane(pixelBuffer, 0),
                frameBuffer.width);

  CVReturn cvRet = CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  if (cvRet != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock base address: " << cvRet;
    return false;
  }

  uint8_t* dstY = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
  int dstStrideY = (int)CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  uint8_t* dstUV = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
  int dstStrideUV = (int)CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

  int ret = libyuv::I420ToNV12(
      frameBuffer.dataY, frameBuffer.strideY, frameBuffer.dataU,
      frameBuffer.strideU, frameBuffer.dataV, frameBuffer.strideV, dstY,
      dstStrideY, dstUV, dstStrideUV, frameBuffer.width, frameBuffer.height);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  if (ret) {
    RTC_LOG(LS_ERROR) << "Error converting I420 VideoFrame to NV12 :" << ret;
    return false;
  }
  return true;
}

CVPixelBufferRef CreatePixelBuffer(CVPixelBufferPoolRef pixel_buffer_pool) {
  if (!pixel_buffer_pool) {
    RTC_LOG(LS_ERROR) << "Failed to get pixel buffer pool.";
    return nullptr;
  }
  CVPixelBufferRef pixel_buffer;
  CVReturn ret = CVPixelBufferPoolCreatePixelBuffer(nullptr, pixel_buffer_pool,
                                                    &pixel_buffer);
  if (ret != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to create pixel buffer: " << ret;

    return nullptr;
  }
  return pixel_buffer;
}

void compressionOutputCallback(void* encoder,
                               void* params,
                               OSStatus status,
                               VTEncodeInfoFlags infoFlags,
                               CMSampleBufferRef sampleBuffer)
    API_AVAILABLE(ios(11.0)) {
  if (!params) {

    return;
  }
  std::unique_ptr<RTCFrameEncodeParams> encodeParams(
      reinterpret_cast<RTCFrameEncodeParams*>(params));
  RTC_CHECK(encodeParams->encoder);
  [encodeParams->encoder frameWasEncoded:status
                                   flags:infoFlags
                            sampleBuffer:sampleBuffer
                       codecSpecificInfo:encodeParams->codecSpecificInfo
                                   width:encodeParams->width
                                  height:encodeParams->height
                            renderTimeMs:encodeParams->render_time_ms
                               timestamp:encodeParams->timestamp
                                rotation:encodeParams->rotation];
}
}

@implementation RTCVideoEncoderH265 {
  RTCVideoCodecInfo* _codecInfo;
  std::unique_ptr<webrtc::BitrateAdjuster> _bitrateAdjuster;
  uint32_t _targetBitrateBps;
  uint32_t _encoderBitrateBps;
  CFStringRef _profile;
  RTCVideoEncoderCallback _callback;
  int32_t _width;
  int32_t _height;
  VTCompressionSessionRef _compressionSession;
  RTCVideoCodecMode _mode;
  int framesLeft;

  webrtc::H265BitstreamParser _h265BitstreamParser;
  std::vector<uint8_t> _nv12ScaleBuffer;
}

- (instancetype)initWithCodecInfo:(RTCVideoCodecInfo*)codecInfo {
  if (self = [super init]) {
    _codecInfo = codecInfo;
    _bitrateAdjuster.reset(new webrtc::BitrateAdjuster(.5, .95));
    RTC_CHECK([codecInfo.name isEqualToString:@"H265"]);
  }
  return self;
}

- (void)dealloc {
  [self destroyCompressionSession];
}

- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings*)settings
                       numberOfCores:(int)numberOfCores {
  RTC_DCHECK(settings);
  RTC_DCHECK([settings.name isEqualToString:@"H265"]);

  _width = settings.width;
  _height = settings.height;
  _mode = settings.mode;

  _targetBitrateBps = settings.startBitrate;
  _bitrateAdjuster->SetTargetBitrateBps(_targetBitrateBps);

  return [self resetCompressionSession];
}

- (NSInteger)encode:(RTCVideoFrame*)frame
    codecSpecificInfo:(id<RTCCodecSpecificInfo>)codecSpecificInfo
           frameTypes:(NSArray<NSNumber*>*)frameTypes {
  RTC_DCHECK_EQ(frame.width, _width);
  RTC_DCHECK_EQ(frame.height, _height);
  if (!_callback || !_compressionSession) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  BOOL isKeyframeRequired = NO;

  CVPixelBufferPoolRef pixelBufferPool =
      VTCompressionSessionGetPixelBufferPool(_compressionSession);

#if defined(WEBRTC_IOS)
  if (!pixelBufferPool) {

    [self resetCompressionSession];
    pixelBufferPool =
        VTCompressionSessionGetPixelBufferPool(_compressionSession);
    isKeyframeRequired = YES;
    RTC_LOG(LS_INFO) << "Resetting compression session due to invalid pool.";
  }
#endif

  CVPixelBufferRef pixelBuffer = nullptr;
  if ([frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]) {

    RTCCVPixelBuffer* rtcPixelBuffer = (RTCCVPixelBuffer*)frame.buffer;
    if (![rtcPixelBuffer requiresCropping]) {

      pixelBuffer = rtcPixelBuffer.pixelBuffer;
      CVBufferRetain(pixelBuffer);
    } else {

      pixelBuffer = CreatePixelBuffer(pixelBufferPool);
      if (!pixelBuffer) {
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      int dstWidth = (int)CVPixelBufferGetWidth(pixelBuffer);
      int dstHeight = (int)CVPixelBufferGetHeight(pixelBuffer);
      if ([rtcPixelBuffer requiresScalingToWidth:dstWidth height:dstHeight]) {
        int size =
            [rtcPixelBuffer bufferSizeForCroppingAndScalingToWidth:dstWidth
                                                            height:dstHeight];
        _nv12ScaleBuffer.resize(size);
      } else {
        _nv12ScaleBuffer.clear();
      }
      _nv12ScaleBuffer.shrink_to_fit();
      if (![rtcPixelBuffer cropAndScaleTo:pixelBuffer
                           withTempBuffer:_nv12ScaleBuffer.data()]) {
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
    }
  }

  if (!pixelBuffer) {

    pixelBuffer = CreatePixelBuffer(pixelBufferPool);
    if (!pixelBuffer) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    RTC_DCHECK(pixelBuffer);
    if (!CopyVideoFrameToPixelBuffer([frame.buffer toI420], pixelBuffer)) {
      RTC_LOG(LS_ERROR) << "Failed to copy frame data.";
      CVBufferRelease(pixelBuffer);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  if (!isKeyframeRequired && frameTypes) {
    for (NSNumber* frameType in frameTypes) {
      if ((RTCFrameType)frameType.intValue == RTCFrameTypeVideoFrameKey) {
        isKeyframeRequired = YES;
        break;
      }
    }
  }

  CMTime presentationTimeStamp =
      CMTimeMake(frame.timeStampNs / rtc::kNumNanosecsPerMillisec, 1000);
  CFDictionaryRef frameProperties = nullptr;
  if (isKeyframeRequired) {
    CFTypeRef keys[] = {kVTEncodeFrameOptionKey_ForceKeyFrame};
    CFTypeRef values[] = {kCFBooleanTrue};
    frameProperties = CreateCFTypeDictionary(keys, values, 1);
  }

  std::unique_ptr<RTCFrameEncodeParams> encodeParams;
  encodeParams.reset(new RTCFrameEncodeParams(
      self, codecSpecificInfo,_width, _height,
      frame.timeStampNs / rtc::kNumNanosecsPerMillisec, frame.timeStamp,
      frame.rotation));

  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps()];

  OSStatus status = VTCompressionSessionEncodeFrame(
      _compressionSession, pixelBuffer, presentationTimeStamp, kCMTimeInvalid,
      frameProperties, encodeParams.release(), nullptr);
  if (frameProperties) {
    CFRelease(frameProperties);
  }
  if (pixelBuffer) {
    CVBufferRelease(pixelBuffer);
  }
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to encode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoEncoderCallback)callback {
  _callback = callback;
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
  _targetBitrateBps = 1000 * bitrateKbit;
  _bitrateAdjuster->SetTargetBitrateBps(_targetBitrateBps);
  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps()];
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (NSInteger)releaseEncoder {

  [self destroyCompressionSession];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

- (int)resetCompressionSession {
  [self destroyCompressionSession];

  const size_t attributesSize = 3;
  CFTypeRef keys[attributesSize] = {
#if defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
    kCVPixelBufferOpenGLCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef ioSurfaceValue = CreateCFTypeDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixelFormat =
      CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue,
                                      pixelFormat};
  CFDictionaryRef sourceAttributes =
      CreateCFTypeDictionary(keys, values, attributesSize);
  if (ioSurfaceValue) {
    CFRelease(ioSurfaceValue);
    ioSurfaceValue = nullptr;
  }
  if (pixelFormat) {
    CFRelease(pixelFormat);
    pixelFormat = nullptr;
  }
  CFMutableDictionaryRef encoder_specs = nullptr;
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

  encoder_specs =
      CFDictionaryCreateMutable(nullptr, 1, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(
      encoder_specs,
      kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
      kCFBooleanTrue);
#endif
  OSStatus status = VTCompressionSessionCreate(
      nullptr,
      _width, _height, kCMVideoCodecType_HEVC,
      encoder_specs,
      sourceAttributes,
      nullptr,
      compressionOutputCallback, nullptr, &_compressionSession);
  if (sourceAttributes) {
    CFRelease(sourceAttributes);
    sourceAttributes = nullptr;
  }
  if (encoder_specs) {
    CFRelease(encoder_specs);
    encoder_specs = nullptr;
  }
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create compression session: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  CFBooleanRef hwaccl_enabled = nullptr;
  status = VTSessionCopyProperty(
      _compressionSession,
      kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, nullptr,
      &hwaccl_enabled);
  if (status == noErr && (CFBooleanGetValue(hwaccl_enabled))) {
    RTC_LOG(LS_INFO) << "Compression session created with hw accl enabled";
  } else {
    RTC_LOG(LS_INFO) << "Compression session created with hw accl disabled";
  }
#endif
  [self configureCompressionSession];
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureCompressionSession {
  RTC_DCHECK(_compressionSession);
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_RealTime,
                       false);

  SetVTSessionProperty(_compressionSession,
                       kVTCompressionPropertyKey_AllowFrameReordering, false);
  [self setEncoderBitrateBps:_targetBitrateBps];

  SetVTSessionProperty(_compressionSession,
                       kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200);
  SetVTSessionProperty(_compressionSession,
                       kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration,
                       240);
  OSStatus status =
      VTCompressionSessionPrepareToEncodeFrames(_compressionSession);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Compression session failed to prepare encode frames.";
  }
}

- (void)destroyCompressionSession {
  if (_compressionSession) {
    VTCompressionSessionInvalidate(_compressionSession);
    CFRelease(_compressionSession);
    _compressionSession = nullptr;
  }
}

- (NSString*)implementationName {
  return @"VideoToolbox";
}

- (void)setBitrateBps:(uint32_t)bitrateBps {
  if (_encoderBitrateBps != bitrateBps) {
    [self setEncoderBitrateBps:bitrateBps];
  }
}

- (void)setEncoderBitrateBps:(uint32_t)bitrateBps {
  if (_compressionSession) {
    SetVTSessionProperty(_compressionSession,
                         kVTCompressionPropertyKey_AverageBitRate, bitrateBps);

    int64_t dataLimitBytesPerSecondValue =
        static_cast<int64_t>(bitrateBps * kLimitToAverageBitRateFactor / 8);
    CFNumberRef bytesPerSecond =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type,
                       &dataLimitBytesPerSecondValue);
    int64_t oneSecondValue = 1;
    CFNumberRef oneSecond = CFNumberCreate(
        kCFAllocatorDefault, kCFNumberSInt64Type, &oneSecondValue);
    const void* nums[2] = {bytesPerSecond, oneSecond};
    CFArrayRef dataRateLimits =
        CFArrayCreate(nullptr, nums, 2, &kCFTypeArrayCallBacks);
    OSStatus status = VTSessionSetProperty(
        _compressionSession, kVTCompressionPropertyKey_DataRateLimits,
        dataRateLimits);
    if (bytesPerSecond) {
      CFRelease(bytesPerSecond);
    }
    if (oneSecond) {
      CFRelease(oneSecond);
    }
    if (dataRateLimits) {
      CFRelease(dataRateLimits);
    }
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to set data rate limit";
    }

    _encoderBitrateBps = bitrateBps;
  }
}

- (void)frameWasEncoded:(OSStatus)status
                  flags:(VTEncodeInfoFlags)infoFlags
           sampleBuffer:(CMSampleBufferRef)sampleBuffer
      codecSpecificInfo:(id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)codecSpecificInfo
                  width:(int32_t)width
                 height:(int32_t)height
           renderTimeMs:(int64_t)renderTimeMs
              timestamp:(uint32_t)timestamp
               rotation:(RTCVideoRotation)rotation {
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "h265 encode failed.";
    return;
  }
  if (infoFlags & kVTEncodeInfo_FrameDropped) {
    RTC_LOG(LS_INFO) << "h265 encoder dropped a frame.";
    return;
  }

  BOOL isKeyframe = NO;
  CFArrayRef attachments =
      CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, 0);
  if (attachments != nullptr && CFArrayGetCount(attachments)) {
    CFDictionaryRef attachment =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
    isKeyframe =
        !CFDictionaryContainsKey(attachment, kCMSampleAttachmentKey_NotSync);
  }

  if (isKeyframe) {
    RTC_LOG(LS_INFO) << "Generated keyframe";
  }

  __block std::unique_ptr<rtc::Buffer> buffer = std::make_unique<rtc::Buffer>();
  if (!webrtc::H265CMSampleBufferToAnnexBBuffer(sampleBuffer, isKeyframe, buffer.get())) {
    return;
  }

  RTCEncodedImage* frame = [[RTCEncodedImage alloc] init];
  frame.buffer = [NSData dataWithBytesNoCopy:buffer->data()
                                      length:buffer->size()
                                freeWhenDone:NO];
  frame.encodedWidth = width;
  frame.encodedHeight = height;
  frame.frameType =
      isKeyframe ? RTCFrameTypeVideoFrameKey : RTCFrameTypeVideoFrameDelta;
  frame.captureTimeMs = renderTimeMs;
  frame.timeStamp = timestamp;
  frame.rotation = rotation;
  frame.contentType = (_mode == RTCVideoCodecModeScreensharing)
                          ? RTCVideoContentTypeScreenshare
                          : RTCVideoContentTypeUnspecified;
  frame.flags = webrtc::VideoSendTiming::kInvalid;

  _h265BitstreamParser.ParseBitstream(rtc::ArrayView<const uint8_t>(buffer->data(), buffer->size()));
  absl::optional<int> qp = _h265BitstreamParser.GetLastSliceQp();
  frame.qp = @(qp.value_or(1));

  BOOL res = _callback(frame, codecSpecificInfo);
  if (!res) {
    RTC_LOG(LS_ERROR) << "Encode callback failed.";
    return;
  }
  _bitrateAdjuster->Update(frame.buffer.length);
}

- (RTCVideoEncoderQpThresholds*)scalingSettings {
  return [[RTCVideoEncoderQpThresholds alloc]
      initWithThresholdsLow:kLowh265QpThreshold
                       high:kHighh265QpThreshold];
}

- (NSInteger)resolutionAlignment {
  return 1;
}

- (BOOL)applyAlignmentToAllSimulcastLayers {
  return NO;
}

- (BOOL)supportsNativeHandle {
  return YES;
}

@end
