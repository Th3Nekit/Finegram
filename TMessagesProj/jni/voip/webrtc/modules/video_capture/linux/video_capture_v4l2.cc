/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/linux/video_capture_v4l2.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <new>
#include <string>

#include "api/scoped_refptr.h"
#include "media/base/video_common.h"
#include "modules/video_capture/video_capture.h"
#include "rtc_base/logging.h"

#ifndef V4L2_PIX_FMT_ABGR32
#define ABGR32_OVERRIDE 1
#define V4L2_PIX_FMT_ABGR32 v4l2_fourcc('A', 'R', '2', '4')
#endif

#ifndef V4L2_PIX_FMT_ARGB32
#define ARGB32_OVERRIDE 1
#define V4L2_PIX_FMT_ARGB32 v4l2_fourcc('B', 'A', '2', '4')
#endif

#ifndef V4L2_PIX_FMT_RGBA32
#define RGBA32_OVERRIDE 1
#define V4L2_PIX_FMT_RGBA32 v4l2_fourcc('A', 'B', '2', '4')
#endif

namespace webrtc {
namespace videocapturemodule {
VideoCaptureModuleV4L2::VideoCaptureModuleV4L2()
    : VideoCaptureImpl(),
      _deviceId(-1),
      _deviceFd(-1),
      _buffersAllocatedByDevice(-1),
      _captureStarted(false),
      _pool(NULL) {}

int32_t VideoCaptureModuleV4L2::Init(const char* deviceUniqueIdUTF8) {
  RTC_DCHECK_RUN_ON(&api_checker_);

  int len = strlen((const char*)deviceUniqueIdUTF8);
  _deviceUniqueId = new (std::nothrow) char[len + 1];
  if (_deviceUniqueId) {
    memcpy(_deviceUniqueId, deviceUniqueIdUTF8, len + 1);
  }

  int fd;
  char device[32];
  bool found = false;

  int n;
  for (n = 0; n < 64; n++) {
    snprintf(device, sizeof(device), "/dev/video%d", n);
    if ((fd = open(device, O_RDONLY)) != -1) {

      struct v4l2_capability cap;
      if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        if (cap.bus_info[0] != 0) {
          if (strncmp((const char*)cap.bus_info,
                      (const char*)deviceUniqueIdUTF8,
                      strlen((const char*)deviceUniqueIdUTF8)) ==
              0) {
            close(fd);
            found = true;
            break;
          }
        }
      }
      close(fd);
    }
  }
  if (!found) {
    RTC_LOG(LS_INFO) << "no matching device found";
    return -1;
  }
  _deviceId = n;
  return 0;
}

VideoCaptureModuleV4L2::~VideoCaptureModuleV4L2() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);

  StopCapture();
  if (_deviceFd != -1)
    close(_deviceFd);
}

int32_t VideoCaptureModuleV4L2::StartCapture(
    const VideoCaptureCapability& capability) {
  RTC_DCHECK_RUN_ON(&api_checker_);

  if (_captureStarted) {
    if (capability == _requestedCapability) {
      return 0;
    } else {
      StopCapture();
    }
  }

  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);

  configured_capability_ = capability;

  MutexLock lock(&capture_lock_);

  char device[20];
  snprintf(device, sizeof(device), "/dev/video%d", _deviceId);

  if ((_deviceFd = open(device, O_RDWR | O_NONBLOCK, 0)) < 0) {
    RTC_LOG(LS_INFO) << "error in opening " << device << " errono = " << errno;
    return -1;
  }

  unsigned int hdFmts[] = {
      V4L2_PIX_FMT_MJPEG,  V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YVU420,
      V4L2_PIX_FMT_YUYV,   V4L2_PIX_FMT_UYVY,   V4L2_PIX_FMT_NV12,
      V4L2_PIX_FMT_ABGR32, V4L2_PIX_FMT_ARGB32, V4L2_PIX_FMT_RGBA32,
      V4L2_PIX_FMT_BGR32,  V4L2_PIX_FMT_RGB32,  V4L2_PIX_FMT_BGR24,
      V4L2_PIX_FMT_RGB24,  V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_JPEG,
  };
  unsigned int sdFmts[] = {
      V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YVU420, V4L2_PIX_FMT_YUYV,
      V4L2_PIX_FMT_UYVY,   V4L2_PIX_FMT_NV12,   V4L2_PIX_FMT_ABGR32,
      V4L2_PIX_FMT_ARGB32, V4L2_PIX_FMT_RGBA32, V4L2_PIX_FMT_BGR32,
      V4L2_PIX_FMT_RGB32,  V4L2_PIX_FMT_BGR24,  V4L2_PIX_FMT_RGB24,
      V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_MJPEG,  V4L2_PIX_FMT_JPEG,
  };
  const bool isHd = capability.width > 640 || capability.height > 480;
  unsigned int* fmts = isHd ? hdFmts : sdFmts;
  static_assert(sizeof(hdFmts) == sizeof(sdFmts));
  constexpr int nFormats = sizeof(hdFmts) / sizeof(unsigned int);

  struct v4l2_fmtdesc fmt;
  int fmtsIdx = nFormats;
  memset(&fmt, 0, sizeof(fmt));
  fmt.index = 0;
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  RTC_LOG(LS_INFO) << "Video Capture enumerats supported image formats:";
  while (ioctl(_deviceFd, VIDIOC_ENUM_FMT, &fmt) == 0) {
    RTC_LOG(LS_INFO) << "  { pixelformat = "
                     << cricket::GetFourccName(fmt.pixelformat)
                     << ", description = '" << fmt.description << "' }";

    for (int i = 0; i < nFormats; i++) {
      if (fmt.pixelformat == fmts[i] && i < fmtsIdx)
        fmtsIdx = i;
    }

    fmt.index++;
  }

  if (fmtsIdx == nFormats) {
    RTC_LOG(LS_INFO) << "no supporting video formats found";
    return -1;
  } else {
    RTC_LOG(LS_INFO) << "We prefer format "
                     << cricket::GetFourccName(fmts[fmtsIdx]);
  }

  struct v4l2_format video_fmt;
  memset(&video_fmt, 0, sizeof(struct v4l2_format));
  video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  video_fmt.fmt.pix.sizeimage = 0;
  video_fmt.fmt.pix.width = capability.width;
  video_fmt.fmt.pix.height = capability.height;
  video_fmt.fmt.pix.pixelformat = fmts[fmtsIdx];

  if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
    configured_capability_.videoType = VideoType::kYUY2;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)
    configured_capability_.videoType = VideoType::kI420;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420)
    configured_capability_.videoType = VideoType::kYV12;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_UYVY)
    configured_capability_.videoType = VideoType::kUYVY;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)
    configured_capability_.videoType = VideoType::kNV12;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_BGR24)
    configured_capability_.videoType = VideoType::kRGB24;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24)
    configured_capability_.videoType = VideoType::kBGR24;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565)
    configured_capability_.videoType = VideoType::kRGB565;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_ABGR32 ||
           video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_BGR32)
    configured_capability_.videoType = VideoType::kARGB;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_ARGB32 ||
           video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB32)
    configured_capability_.videoType = VideoType::kBGRA;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGBA32)
    configured_capability_.videoType = VideoType::kABGR;
  else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG ||
           video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG)
    configured_capability_.videoType = VideoType::kMJPEG;
  else
    RTC_DCHECK_NOTREACHED();

  if (ioctl(_deviceFd, VIDIOC_S_FMT, &video_fmt) < 0) {
    RTC_LOG(LS_INFO) << "error in VIDIOC_S_FMT, errno = " << errno;
    return -1;
  }

  configured_capability_.width = video_fmt.fmt.pix.width;
  configured_capability_.height = video_fmt.fmt.pix.height;

  bool driver_framerate_support = true;
  struct v4l2_streamparm streamparms;
  memset(&streamparms, 0, sizeof(streamparms));
  streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(_deviceFd, VIDIOC_G_PARM, &streamparms) < 0) {
    RTC_LOG(LS_INFO) << "error in VIDIOC_G_PARM errno = " << errno;
    driver_framerate_support = false;

  } else {

    if (streamparms.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {

      memset(&streamparms, 0, sizeof(streamparms));
      streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      streamparms.parm.capture.timeperframe.numerator = 1;
      streamparms.parm.capture.timeperframe.denominator = capability.maxFPS;
      if (ioctl(_deviceFd, VIDIOC_S_PARM, &streamparms) < 0) {
        RTC_LOG(LS_INFO) << "Failed to set the framerate. errno=" << errno;
        driver_framerate_support = false;
      }
    }
  }

  if (!driver_framerate_support) {
    if (configured_capability_.width >= 800 &&
        configured_capability_.videoType != VideoType::kMJPEG) {
      configured_capability_.maxFPS = 15;
    } else {
      configured_capability_.maxFPS = 30;
    }
  }

  if (!AllocateVideoBuffers()) {
    RTC_LOG(LS_INFO) << "failed to allocate video capture buffers";
    return -1;
  }

  enum v4l2_buf_type type;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(_deviceFd, VIDIOC_STREAMON, &type) == -1) {
    RTC_LOG(LS_INFO) << "Failed to turn on stream";
    return -1;
  }

  _requestedCapability = capability;
  _captureStarted = true;
  _streaming = true;

  if (_captureThread.empty()) {
    quit_ = false;
    _captureThread = rtc::PlatformThread::SpawnJoinable(
        [this] {
          while (CaptureProcess()) {
          }
        },
        "CaptureThread",
        rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
  }
  return 0;
}

int32_t VideoCaptureModuleV4L2::StopCapture() {
  RTC_DCHECK_RUN_ON(&api_checker_);

  if (!_captureThread.empty()) {
    {
      MutexLock lock(&capture_lock_);
      quit_ = true;
    }

    _captureThread.Finalize();
  }

  _captureStarted = false;

  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);
  MutexLock lock(&capture_lock_);
  if (_streaming) {
    _streaming = false;

    DeAllocateVideoBuffers();
    close(_deviceFd);
    _deviceFd = -1;

    _requestedCapability = configured_capability_ = VideoCaptureCapability();
  }

  return 0;
}

bool VideoCaptureModuleV4L2::AllocateVideoBuffers() {
  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);
  struct v4l2_requestbuffers rbuffer;
  memset(&rbuffer, 0, sizeof(v4l2_requestbuffers));

  rbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rbuffer.memory = V4L2_MEMORY_MMAP;
  rbuffer.count = kNoOfV4L2Bufffers;

  if (ioctl(_deviceFd, VIDIOC_REQBUFS, &rbuffer) < 0) {
    RTC_LOG(LS_INFO) << "Could not get buffers from device. errno = " << errno;
    return false;
  }

  if (rbuffer.count > kNoOfV4L2Bufffers)
    rbuffer.count = kNoOfV4L2Bufffers;

  _buffersAllocatedByDevice = rbuffer.count;

  _pool = new Buffer[rbuffer.count];

  for (unsigned int i = 0; i < rbuffer.count; i++) {
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(v4l2_buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;

    if (ioctl(_deviceFd, VIDIOC_QUERYBUF, &buffer) < 0) {
      return false;
    }

    _pool[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, _deviceFd, buffer.m.offset);

    if (MAP_FAILED == _pool[i].start) {
      for (unsigned int j = 0; j < i; j++)
        munmap(_pool[j].start, _pool[j].length);
      return false;
    }

    _pool[i].length = buffer.length;

    if (ioctl(_deviceFd, VIDIOC_QBUF, &buffer) < 0) {
      return false;
    }
  }
  return true;
}

bool VideoCaptureModuleV4L2::DeAllocateVideoBuffers() {
  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);

  for (int i = 0; i < _buffersAllocatedByDevice; i++)
    munmap(_pool[i].start, _pool[i].length);

  delete[] _pool;

  enum v4l2_buf_type type;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(_deviceFd, VIDIOC_STREAMOFF, &type) < 0) {
    RTC_LOG(LS_INFO) << "VIDIOC_STREAMOFF error. errno: " << errno;
  }

  return true;
}

bool VideoCaptureModuleV4L2::CaptureStarted() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  return _captureStarted;
}

bool VideoCaptureModuleV4L2::CaptureProcess() {
  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);

  int retVal = 0;
  fd_set rSet;
  struct timeval timeout;

  FD_ZERO(&rSet);
  FD_SET(_deviceFd, &rSet);
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  retVal = select(_deviceFd + 1, &rSet, NULL, NULL, &timeout);

  {
    MutexLock lock(&capture_lock_);

    if (quit_) {
      return false;
    }

    if (retVal < 0 && errno != EINTR) {

      return false;
    } else if (retVal == 0) {

      return true;
    } else if (!FD_ISSET(_deviceFd, &rSet)) {

      return true;
    }

    if (_streaming) {
      struct v4l2_buffer buf;
      memset(&buf, 0, sizeof(struct v4l2_buffer));
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;

      while (ioctl(_deviceFd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno != EINTR) {
          RTC_LOG(LS_INFO) << "could not sync on a buffer on device "
                           << strerror(errno);
          return true;
        }
      }

      IncomingFrame(reinterpret_cast<uint8_t*>(_pool[buf.index].start),
                    buf.bytesused, configured_capability_);

      if (ioctl(_deviceFd, VIDIOC_QBUF, &buf) == -1) {
        RTC_LOG(LS_INFO) << "Failed to enqueue capture buffer";
      }
    }
  }
  usleep(0);
  return true;
}

int32_t VideoCaptureModuleV4L2::CaptureSettings(
    VideoCaptureCapability& settings) {
  RTC_DCHECK_RUN_ON(&api_checker_);
  settings = _requestedCapability;

  return 0;
}
}
}

#ifdef ABGR32_OVERRIDE
#undef ABGR32_OVERRIDE
#undef V4L2_PIX_FMT_ABGR32
#endif

#ifdef ARGB32_OVERRIDE
#undef ARGB32_OVERRIDE
#undef V4L2_PIX_FMT_ARGB32
#endif

#ifdef RGBA32_OVERRIDE
#undef RGBA32_OVERRIDE
#undef V4L2_PIX_FMT_RGBA32
#endif
