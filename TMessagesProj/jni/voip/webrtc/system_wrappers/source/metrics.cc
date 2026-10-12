// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#include "system_wrappers/include/metrics.h"

#include <algorithm>

#include "absl/strings/string_view.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace metrics {
class Histogram;

namespace {

const int kMaxSampleMapSize = 300;

class RtcHistogram {
 public:
  RtcHistogram(absl::string_view name, int min, int max, int bucket_count)
      : min_(min), max_(max), info_(name, min, max, bucket_count) {
    RTC_DCHECK_GT(bucket_count, 0);
  }

  RtcHistogram(const RtcHistogram&) = delete;
  RtcHistogram& operator=(const RtcHistogram&) = delete;

  void Add(int sample) {
    sample = std::min(sample, max_);
    sample = std::max(sample, min_ - 1);

    MutexLock lock(&mutex_);
    if (info_.samples.size() == kMaxSampleMapSize &&
        info_.samples.find(sample) == info_.samples.end()) {
      return;
    }
    ++info_.samples[sample];
  }

  std::unique_ptr<SampleInfo> GetAndReset() {
    MutexLock lock(&mutex_);
    if (info_.samples.empty())
      return nullptr;

    SampleInfo* copy =
        new SampleInfo(info_.name, info_.min, info_.max, info_.bucket_count);

    std::swap(info_.samples, copy->samples);

    return std::unique_ptr<SampleInfo>(copy);
  }

  const std::string& name() const { return info_.name; }

  void Reset() {
    MutexLock lock(&mutex_);
    info_.samples.clear();
  }

  int NumEvents(int sample) const {
    MutexLock lock(&mutex_);
    const auto it = info_.samples.find(sample);
    return (it == info_.samples.end()) ? 0 : it->second;
  }

  int NumSamples() const {
    int num_samples = 0;
    MutexLock lock(&mutex_);
    for (const auto& sample : info_.samples) {
      num_samples += sample.second;
    }
    return num_samples;
  }

  int MinSample() const {
    MutexLock lock(&mutex_);
    return (info_.samples.empty()) ? -1 : info_.samples.begin()->first;
  }

  std::map<int, int> Samples() const {
    MutexLock lock(&mutex_);
    return info_.samples;
  }

 private:
  mutable Mutex mutex_;
  const int min_;
  const int max_;
  SampleInfo info_ RTC_GUARDED_BY(mutex_);
};

class RtcHistogramMap {
 public:
  RtcHistogramMap() {}
  ~RtcHistogramMap() {}

  RtcHistogramMap(const RtcHistogramMap&) = delete;
  RtcHistogramMap& operator=(const RtcHistogramMap&) = delete;

  Histogram* GetCountsHistogram(absl::string_view name,
                                int min,
                                int max,
                                int bucket_count) {
    MutexLock lock(&mutex_);
    const auto& it = map_.find(name);
    if (it != map_.end())
      return reinterpret_cast<Histogram*>(it->second.get());

    RtcHistogram* hist = new RtcHistogram(name, min, max, bucket_count);
    map_.emplace(name, hist);
    return reinterpret_cast<Histogram*>(hist);
  }

  Histogram* GetEnumerationHistogram(absl::string_view name, int boundary) {
    MutexLock lock(&mutex_);
    const auto& it = map_.find(name);
    if (it != map_.end())
      return reinterpret_cast<Histogram*>(it->second.get());

    RtcHistogram* hist = new RtcHistogram(name, 1, boundary, boundary + 1);
    map_.emplace(name, hist);
    return reinterpret_cast<Histogram*>(hist);
  }

  void GetAndReset(std::map<std::string,
                            std::unique_ptr<SampleInfo>,
                            rtc::AbslStringViewCmp>* histograms) {
    MutexLock lock(&mutex_);
    for (const auto& kv : map_) {
      std::unique_ptr<SampleInfo> info = kv.second->GetAndReset();
      if (info)
        histograms->insert(std::make_pair(kv.first, std::move(info)));
    }
  }

  void Reset() {
    MutexLock lock(&mutex_);
    for (const auto& kv : map_)
      kv.second->Reset();
  }

  int NumEvents(absl::string_view name, int sample) const {
    MutexLock lock(&mutex_);
    const auto& it = map_.find(name);
    return (it == map_.end()) ? 0 : it->second->NumEvents(sample);
  }

  int NumSamples(absl::string_view name) const {
    MutexLock lock(&mutex_);
    const auto& it = map_.find(name);
    return (it == map_.end()) ? 0 : it->second->NumSamples();
  }

  int MinSample(absl::string_view name) const {
    MutexLock lock(&mutex_);
    const auto& it = map_.find(name);
    return (it == map_.end()) ? -1 : it->second->MinSample();
  }

  std::map<int, int> Samples(absl::string_view name) const {
    MutexLock lock(&mutex_);
    const auto& it = map_.find(name);
    return (it == map_.end()) ? std::map<int, int>() : it->second->Samples();
  }

 private:
  mutable Mutex mutex_;
  std::map<std::string, std::unique_ptr<RtcHistogram>, rtc::AbslStringViewCmp>
      map_ RTC_GUARDED_BY(mutex_);
};

static std::atomic<RtcHistogramMap*> g_rtc_histogram_map(nullptr);

void CreateMap() {
  RtcHistogramMap* map = g_rtc_histogram_map.load(std::memory_order_acquire);
  if (map == nullptr) {
    RtcHistogramMap* new_map = new RtcHistogramMap();
    if (!g_rtc_histogram_map.compare_exchange_strong(map, new_map))
      delete new_map;
  }
}

#if RTC_DCHECK_IS_ON
static std::atomic<int> g_rtc_histogram_called(0);
#endif

RtcHistogramMap* GetMap() {
#if RTC_DCHECK_IS_ON
  g_rtc_histogram_called.store(1, std::memory_order_release);
#endif
  return g_rtc_histogram_map.load();
}
}

#ifndef WEBRTC_EXCLUDE_METRICS_DEFAULT

Histogram* HistogramFactoryGetCounts(absl::string_view name,
                                     int min,
                                     int max,
                                     int bucket_count) {

  return HistogramFactoryGetCountsLinear(name, min, max, bucket_count);
}

Histogram* HistogramFactoryGetCountsLinear(absl::string_view name,
                                           int min,
                                           int max,
                                           int bucket_count) {
  RtcHistogramMap* map = GetMap();
  if (!map)
    return nullptr;

  return map->GetCountsHistogram(name, min, max, bucket_count);
}

Histogram* HistogramFactoryGetEnumeration(absl::string_view name,
                                          int boundary) {
  RtcHistogramMap* map = GetMap();
  if (!map)
    return nullptr;

  return map->GetEnumerationHistogram(name, boundary);
}

Histogram* SparseHistogramFactoryGetEnumeration(absl::string_view name,
                                                int boundary) {
  return HistogramFactoryGetEnumeration(name, boundary);
}

void HistogramAdd(Histogram* histogram_pointer, int sample) {
  RtcHistogram* ptr = reinterpret_cast<RtcHistogram*>(histogram_pointer);
  ptr->Add(sample);
}

#endif

SampleInfo::SampleInfo(absl::string_view name,
                       int min,
                       int max,
                       size_t bucket_count)
    : name(name), min(min), max(max), bucket_count(bucket_count) {}

SampleInfo::~SampleInfo() {}

void Enable() {
  RTC_DCHECK(g_rtc_histogram_map.load() == nullptr);
#if RTC_DCHECK_IS_ON
  RTC_DCHECK_EQ(0, g_rtc_histogram_called.load(std::memory_order_acquire));
#endif
  CreateMap();
}

void GetAndReset(
    std::map<std::string, std::unique_ptr<SampleInfo>, rtc::AbslStringViewCmp>*
        histograms) {
  histograms->clear();
  RtcHistogramMap* map = GetMap();
  if (map)
    map->GetAndReset(histograms);
}

void Reset() {
  RtcHistogramMap* map = GetMap();
  if (map)
    map->Reset();
}

int NumEvents(absl::string_view name, int sample) {
  RtcHistogramMap* map = GetMap();
  return map ? map->NumEvents(name, sample) : 0;
}

int NumSamples(absl::string_view name) {
  RtcHistogramMap* map = GetMap();
  return map ? map->NumSamples(name) : 0;
}

int MinSample(absl::string_view name) {
  RtcHistogramMap* map = GetMap();
  return map ? map->MinSample(name) : -1;
}

std::map<int, int> Samples(absl::string_view name) {
  RtcHistogramMap* map = GetMap();
  return map ? map->Samples(name) : std::map<int, int>();
}

}
}
