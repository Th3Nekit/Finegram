/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "TGRTCVideoDecoderH264.h"

#import <VideoToolbox/VideoToolbox.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"

#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"

struct RTCFrameDecodeParams {
  RTCFrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts) : callback(cb), timestamp(ts) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};

@interface RTC_OBJC_TYPE (TGRTCVideoDecoderH264)
() - (void)setError : (OSStatus)error;
@end

static void decompressionOutputCallback(void *decoderRef,
                                 void *params,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime timestamp,
                                 CMTime duration) {
  std::unique_ptr<RTCFrameDecodeParams> decodeParams(
      reinterpret_cast<RTCFrameDecodeParams *>(params));
  if (status != noErr) {
    RTC_OBJC_TYPE(TGRTCVideoDecoderH264) *decoder =
        (__bridge RTC_OBJC_TYPE(TGRTCVideoDecoderH264) *)decoderRef;
    [decoder setError:status];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }

  @autoreleasepool {
  RTC_OBJC_TYPE(RTCCVPixelBuffer) *frameBuffer =
      [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc] initWithPixelBuffer:imageBuffer];
  RTC_OBJC_TYPE(RTCVideoFrame) *decodedFrame = [[RTC_OBJC_TYPE(RTCVideoFrame) alloc]
      initWithBuffer:frameBuffer
            rotation:RTCVideoRotation_0
         timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = (int32_t)decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
  }
}

@implementation RTC_OBJC_TYPE (TGRTCVideoDecoderH264) {
  CMVideoFormatDescriptionRef _videoFormat;
  CMMemoryPoolRef _memoryPool;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _memoryPool = CMMemoryPoolCreate(nil);
  }
  return self;
}

- (void)dealloc {
  CMMemoryPoolInvalidate(_memoryPool);
  CFRelease(_memoryPool);
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)decode:(RTC_OBJC_TYPE(RTCEncodedImage) *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);

  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat =
      rtc::ScopedCF(webrtc::CreateVideoFormatDescription((uint8_t *)inputImage.buffer.bytes,
                                                         inputImage.buffer.length));
  if (inputFormat) {

    if (!CMFormatDescriptionEqual(inputFormat.get(), _videoFormat)) {
      [self setVideoFormat:inputFormat.get()];
      int resetDecompressionSessionError = [self resetDecompressionSession];
      if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
        return resetDecompressionSessionError;
      }
    }
  }
  if (!_videoFormat) {

    RTC_LOG(LS_WARNING) << "Missing video format. Frame with sps/pps required.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  CMSampleBufferRef sampleBuffer = nullptr;
  if (!webrtc::H264AnnexBBufferToCMSampleBuffer((uint8_t *)inputImage.buffer.bytes,
                                                inputImage.buffer.length,
                                                _videoFormat,
                                                &sampleBuffer,
                                                _memoryPool)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sampleBuffer);
  VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<RTCFrameDecodeParams> frameDecodeParams;
  frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, inputImage.timeStamp));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)

  if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_INFO) << "Failed to decode frame with code: " << status
                     << " retrying decode after decompression session reset";
    frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, inputImage.timeStamp));
    status = VTDecompressionSessionDecodeFrame(
        _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
  }
#endif
  CFRelease(sampleBuffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _callback = callback;
}

- (void)setError:(OSStatus)error {
  _error = error;
}

- (NSInteger)releaseDecoder {

  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
  [self destroyDecompressionSession];

  if (!_videoFormat) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

#if TARGET_OS_SIMULATOR
  static size_t const attributesSize = 2;
#else
  static size_t const attributesSize = 3;
#endif

  CFTypeRef keys[attributesSize] = {
#if defined(WEBRTC_IOS)
      kCVPixelBufferMetalCompatibilityKey,
#elif defined(WEBRTC_MAC)
      kCVPixelBufferOpenGLCompatibilityKey,
#endif
#if !(TARGET_OS_SIMULATOR)
      kCVPixelBufferIOSurfacePropertiesKey,
#endif
      kCVPixelBufferPixelFormatTypeKey};
  CFDictionaryRef ioSurfaceValue = CreateCFTypeDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
#if TARGET_OS_SIMULATOR
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, pixelFormat};
#else
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue, pixelFormat};
#endif

  CFDictionaryRef attributes = CreateCFTypeDictionary(keys, values, attributesSize);
  if (ioSurfaceValue) {
    CFRelease(ioSurfaceValue);
    ioSurfaceValue = nullptr;
  }
  if (pixelFormat) {
    CFRelease(pixelFormat);
    pixelFormat = nullptr;
  }
  VTDecompressionOutputCallbackRecord record = {
      decompressionOutputCallback, (__bridge void *)self,
  };
  OSStatus status = VTDecompressionSessionCreate(
      nullptr, _videoFormat, nullptr, attributes, &record, &_decompressionSession);
  CFRelease(attributes);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create decompression session: " << status;
    [self destroyDecompressionSession];
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  [self configureDecompressionSession];

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
  RTC_DCHECK(_decompressionSession);
#if defined(WEBRTC_IOS)
  VTSessionSetProperty(_decompressionSession, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
  if (_decompressionSession) {
#if defined(WEBRTC_IOS)
  VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
#endif
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
}

- (void)setVideoFormat:(CMVideoFormatDescriptionRef)videoFormat {
  if (_videoFormat == videoFormat) {
    return;
  }
  if (_videoFormat) {
    CFRelease(_videoFormat);
  }
  _videoFormat = videoFormat;
  if (_videoFormat) {
    CFRetain(_videoFormat);
  }
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

@end
