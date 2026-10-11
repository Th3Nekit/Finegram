//
// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#ifndef SYSTEM_WRAPPERS_INCLUDE_METRICS_H_
#define SYSTEM_WRAPPERS_INCLUDE_METRICS_H_

#include <stddef.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"

#if defined(RTC_DISABLE_METRICS)
#define RTC_METRICS_ENABLED 0
#else
#define RTC_METRICS_ENABLED 1
#endif

namespace webrtc {
namespace metrics_impl {
template <typename... Ts>
void NoOp(const Ts&...) {}
}
}

#if RTC_METRICS_ENABLED
#define EXPECT_METRIC_EQ(val1, val2) EXPECT_EQ(val1, val2)
#define EXPECT_METRIC_EQ_WAIT(val1, val2, timeout) \
  EXPECT_EQ_WAIT(val1, val2, timeout)
#define EXPECT_METRIC_GT(val1, val2) EXPECT_GT(val1, val2)
#define EXPECT_METRIC_LE(val1, val2) EXPECT_LE(val1, val2)
#define EXPECT_METRIC_TRUE(conditon) EXPECT_TRUE(conditon)
#define EXPECT_METRIC_FALSE(conditon) EXPECT_FALSE(conditon)
#define EXPECT_METRIC_THAT(value, matcher) EXPECT_THAT(value, matcher)
#else
#define EXPECT_METRIC_EQ(val1, val2) webrtc::metrics_impl::NoOp(val1, val2)
#define EXPECT_METRIC_EQ_WAIT(val1, val2, timeout) \
  webrtc::metrics_impl::NoOp(val1, val2, timeout)
#define EXPECT_METRIC_GT(val1, val2) webrtc::metrics_impl::NoOp(val1, val2)
#define EXPECT_METRIC_LE(val1, val2) webrtc::metrics_impl::NoOp(val1, val2)
#define EXPECT_METRIC_TRUE(condition) \
  webrtc::metrics_impl::NoOp(condition || true)
#define EXPECT_METRIC_FALSE(condition) \
  webrtc::metrics_impl::NoOp(condition && false)
#define EXPECT_METRIC_THAT(value, matcher) \
  webrtc::metrics_impl::NoOp(value, testing::_)
#endif

#if RTC_METRICS_ENABLED

#define RTC_HISTOGRAM_COUNTS_100(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 100, 50)

#define RTC_HISTOGRAM_COUNTS_200(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 200, 50)

#define RTC_HISTOGRAM_COUNTS_500(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 500, 50)

#define RTC_HISTOGRAM_COUNTS_1000(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 1000, 50)

#define RTC_HISTOGRAM_COUNTS_10000(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 10000, 50)

#define RTC_HISTOGRAM_COUNTS_100000(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 100000, 50)

#define RTC_HISTOGRAM_COUNTS(name, sample, min, max, bucket_count)       \
  RTC_HISTOGRAM_COMMON_BLOCK(name, sample,                               \
                             webrtc::metrics::HistogramFactoryGetCounts( \
                                 name, min, max, bucket_count))

#define RTC_HISTOGRAM_COUNTS_LINEAR(name, sample, min, max, bucket_count)      \
  RTC_HISTOGRAM_COMMON_BLOCK(name, sample,                                     \
                             webrtc::metrics::HistogramFactoryGetCountsLinear( \
                                 name, min, max, bucket_count))

#define RTC_HISTOGRAM_COUNTS_SPARSE_100(name, sample) \
  RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, 1, 100, 50)

#define RTC_HISTOGRAM_COUNTS_SPARSE_200(name, sample) \
  RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, 1, 200, 50)

#define RTC_HISTOGRAM_COUNTS_SPARSE_500(name, sample) \
  RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, 1, 500, 50)

#define RTC_HISTOGRAM_COUNTS_SPARSE_1000(name, sample) \
  RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, 1, 1000, 50)

#define RTC_HISTOGRAM_COUNTS_SPARSE_10000(name, sample) \
  RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, 1, 10000, 50)

#define RTC_HISTOGRAM_COUNTS_SPARSE_100000(name, sample) \
  RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, 1, 100000, 50)

#define RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, min, max, bucket_count)     \
  RTC_HISTOGRAM_COMMON_BLOCK_SLOW(name, sample,                               \
                                  webrtc::metrics::HistogramFactoryGetCounts( \
                                      name, min, max, bucket_count))

#define RTC_HISTOGRAM_PERCENTAGE_SPARSE(name, sample) \
  RTC_HISTOGRAM_ENUMERATION_SPARSE(name, sample, 101)

#define RTC_HISTOGRAM_BOOLEAN_SPARSE(name, sample) \
  RTC_HISTOGRAM_ENUMERATION_SPARSE(name, sample, 2)

#define RTC_HISTOGRAM_ENUMERATION_SPARSE(name, sample, boundary) \
  RTC_HISTOGRAM_COMMON_BLOCK_SLOW(                               \
      name, sample,                                              \
      webrtc::metrics::SparseHistogramFactoryGetEnumeration(name, boundary))

#define RTC_HISTOGRAM_PERCENTAGE(name, sample) \
  RTC_HISTOGRAM_ENUMERATION(name, sample, 101)

#define RTC_HISTOGRAM_BOOLEAN(name, sample) \
  RTC_HISTOGRAM_ENUMERATION(name, sample, 2)

#define RTC_HISTOGRAM_ENUMERATION(name, sample, boundary) \
  RTC_HISTOGRAM_COMMON_BLOCK_SLOW(                        \
      name, sample,                                       \
      webrtc::metrics::HistogramFactoryGetEnumeration(name, boundary))

#define RTC_HISTOGRAM_COMMON_BLOCK(constant_name, sample,                     \
                                   factory_get_invocation)                    \
  do {                                                                        \
    static std::atomic<webrtc::metrics::Histogram*> atomic_histogram_pointer( \
        nullptr);                                                             \
    webrtc::metrics::Histogram* histogram_pointer =                           \
        atomic_histogram_pointer.load(std::memory_order_acquire);             \
    if (!histogram_pointer) {                                                 \
      histogram_pointer = factory_get_invocation;                             \
      webrtc::metrics::Histogram* null_histogram = nullptr;                   \
      atomic_histogram_pointer.compare_exchange_strong(null_histogram,        \
                                                       histogram_pointer);    \
    }                                                                         \
    if (histogram_pointer) {                                                  \
      webrtc::metrics::HistogramAdd(histogram_pointer, sample);               \
    }                                                                         \
  } while (0)

#define RTC_HISTOGRAM_COMMON_BLOCK_SLOW(name, sample, factory_get_invocation) \
  do {                                                                        \
    webrtc::metrics::Histogram* histogram_pointer = factory_get_invocation;   \
    if (histogram_pointer) {                                                  \
      webrtc::metrics::HistogramAdd(histogram_pointer, sample);               \
    }                                                                         \
  } while (0)

#define RTC_HISTOGRAMS_COUNTS_100(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,           \
                        RTC_HISTOGRAM_COUNTS(name, sample, 1, 100, 50))

#define RTC_HISTOGRAMS_COUNTS_200(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,           \
                        RTC_HISTOGRAM_COUNTS(name, sample, 1, 200, 50))

#define RTC_HISTOGRAMS_COUNTS_500(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,           \
                        RTC_HISTOGRAM_COUNTS(name, sample, 1, 500, 50))

#define RTC_HISTOGRAMS_COUNTS_1000(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,            \
                        RTC_HISTOGRAM_COUNTS(name, sample, 1, 1000, 50))

#define RTC_HISTOGRAMS_COUNTS_10000(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,             \
                        RTC_HISTOGRAM_COUNTS(name, sample, 1, 10000, 50))

#define RTC_HISTOGRAMS_COUNTS_100000(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,              \
                        RTC_HISTOGRAM_COUNTS(name, sample, 1, 100000, 50))

#define RTC_HISTOGRAMS_ENUMERATION(index, name, sample, boundary) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,                      \
                        RTC_HISTOGRAM_ENUMERATION(name, sample, boundary))

#define RTC_HISTOGRAMS_PERCENTAGE(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample,           \
                        RTC_HISTOGRAM_PERCENTAGE(name, sample))

#define RTC_HISTOGRAMS_COMMON(index, name, sample, macro_invocation) \
  do {                                                               \
    switch (index) {                                                 \
      case 0:                                                        \
        macro_invocation;                                            \
        break;                                                       \
      case 1:                                                        \
        macro_invocation;                                            \
        break;                                                       \
      case 2:                                                        \
        macro_invocation;                                            \
        break;                                                       \
      default:                                                       \
        RTC_DCHECK_NOTREACHED();                                     \
    }                                                                \
  } while (0)

#else

#define RTC_HISTOGRAM_COUNTS_100(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_200(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_500(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_1000(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_10000(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_100000(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS(name, sample, min, max, bucket_count) \
  webrtc::metrics_impl::NoOp(name, sample, min, max, bucket_count)

#define RTC_HISTOGRAM_COUNTS_LINEAR(name, sample, min, max, bucket_count) \
  webrtc::metrics_impl::NoOp(name, sample, min, max, bucket_count)

#define RTC_HISTOGRAM_COUNTS_SPARSE_100(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_SPARSE_200(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_SPARSE_500(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_SPARSE_1000(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_SPARSE_10000(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_SPARSE_100000(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, min, max, bucket_count) \
  webrtc::metrics_impl::NoOp(name, sample, min, max, bucket_count)

#define RTC_HISTOGRAM_PERCENTAGE_SPARSE(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_BOOLEAN_SPARSE(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_ENUMERATION_SPARSE(name, sample, boundary) \
  webrtc::metrics_impl::NoOp(name, sample, boundary)

#define RTC_HISTOGRAM_PERCENTAGE(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_BOOLEAN(name, sample) \
  webrtc::metrics_impl::NoOp(name, sample)

#define RTC_HISTOGRAM_ENUMERATION(name, sample, boundary) \
  webrtc::metrics_impl::NoOp(name, sample, boundary)

#define RTC_HISTOGRAM_COMMON_BLOCK(constant_name, sample,  \
                                   factory_get_invocation) \
  webrtc::metrics_impl::NoOp(constant_name, sample, factory_get_invocation)

#define RTC_HISTOGRAM_COMMON_BLOCK_SLOW(name, sample, factory_get_invocation) \
  webrtc::metrics_impl::NoOp(name, sample, factory_get_invocation)

#define RTC_HISTOGRAMS_COUNTS_100(index, name, sample) \
  webrtc::metrics_impl::NoOp(index, name, sample)

#define RTC_HISTOGRAMS_COUNTS_200(index, name, sample) \
  webrtc::metrics_impl::NoOp(index, name, sample)

#define RTC_HISTOGRAMS_COUNTS_500(index, name, sample) \
  webrtc::metrics_impl::NoOp(index, name, sample)

#define RTC_HISTOGRAMS_COUNTS_1000(index, name, sample) \
  webrtc::metrics_impl::NoOp(index, name, sample)

#define RTC_HISTOGRAMS_COUNTS_10000(index, name, sample) \
  webrtc::metrics_impl::NoOp(index, name, sample)

#define RTC_HISTOGRAMS_COUNTS_100000(index, name, sample) \
  webrtc::metrics_impl::NoOp(index, name, sample)

#define RTC_HISTOGRAMS_ENUMERATION(index, name, sample, boundary) \
  webrtc::metrics_impl::NoOp(index, name, sample, boundary)

#define RTC_HISTOGRAMS_PERCENTAGE(index, name, sample) \
  webrtc::metrics_impl::NoOp(index, name, sample)

#define RTC_HISTOGRAMS_COMMON(index, name, sample, macro_invocation) \
  webrtc::metrics_impl::NoOp(index, name, sample, macro_invocation)

#endif

namespace webrtc {
namespace metrics {

constexpr int kMinRunTimeInSeconds = 10;

class Histogram;

Histogram* HistogramFactoryGetCounts(absl::string_view name,
                                     int min,
                                     int max,
                                     int bucket_count);

Histogram* HistogramFactoryGetCountsLinear(absl::string_view name,
                                           int min,
                                           int max,
                                           int bucket_count);

Histogram* HistogramFactoryGetEnumeration(absl::string_view name, int boundary);

Histogram* SparseHistogramFactoryGetEnumeration(absl::string_view name,
                                                int boundary);

void HistogramAdd(Histogram* histogram_pointer, int sample);

struct SampleInfo {
  SampleInfo(absl::string_view name, int min, int max, size_t bucket_count);
  ~SampleInfo();

  const std::string name;
  const int min;
  const int max;
  const size_t bucket_count;
  std::map<int, int> samples;
};

void Enable();

void GetAndReset(
    std::map<std::string, std::unique_ptr<SampleInfo>, rtc::AbslStringViewCmp>*
        histograms);

void Reset();

int NumEvents(absl::string_view name, int sample);

int NumSamples(absl::string_view name);

int MinSample(absl::string_view name);

std::map<int, int> Samples(absl::string_view name);

}
}

#endif
