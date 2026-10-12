/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/buffer.h"

namespace webrtc {
namespace test {
namespace {

constexpr size_t kRtpDumpHeaderLength = 8;

rtc::Buffer ReadNextPacket(FILE* file) {

  rtc::Buffer buffer;
  buffer.SetData(kRtpDumpHeaderLength, [&](rtc::ArrayView<uint8_t> x) {
    return fread(x.data(), 1, x.size(), file);
  });
  if (buffer.size() != kRtpDumpHeaderLength) {
    return rtc::Buffer();
  }

  const uint16_t len = ByteReader<uint16_t>::ReadBigEndian(buffer.data());
  RTC_CHECK_GE(len, kRtpDumpHeaderLength);

  buffer.AppendData(len - kRtpDumpHeaderLength, [&](rtc::ArrayView<uint8_t> x) {
    return fread(x.data(), 1, x.size(), file);
  });
  if (buffer.size() != len) {
    buffer.Clear();
  }
  return buffer;
}

struct PacketAndTime {
  rtc::Buffer packet;
  int time;
};

void WritePacket(const PacketAndTime& packet, FILE* file) {

  const auto* payload_ptr = packet.packet.data();
  RTC_CHECK_EQ(fwrite(payload_ptr, 4, 1, file), 1);
  payload_ptr += 4;

  uint8_t time[sizeof(uint32_t)];
  ByteWriter<uint32_t, sizeof(uint32_t)>::WriteBigEndian(time, packet.time);
  RTC_CHECK_EQ(fwrite(time, sizeof(uint32_t), 1, file), 1);
  payload_ptr += 4;

  RTC_DCHECK_EQ(payload_ptr - packet.packet.data(), kRtpDumpHeaderLength);
  RTC_CHECK_EQ(
      fwrite(payload_ptr, packet.packet.size() - kRtpDumpHeaderLength, 1, file),
      1);
}

int RunRtpJitter(int argc, char* argv[]) {
  const std::string program_name = argv[0];
  const std::string usage =
      "Tool for alternating the arrival times in an RTP dump file.\n"
      "Example usage:\n" +
      program_name + " input.rtp arrival_times_ms.txt output.rtp\n\n";
  if (argc != 4) {
    printf("%s", usage.c_str());
    return 1;
  }

  printf("Input RTP file: %s\n", argv[1]);
  FILE* in_file = fopen(argv[1], "rb");
  RTC_CHECK(in_file) << "Could not open file " << argv[1] << " for reading";
  printf("Timing file: %s\n", argv[2]);
  std::ifstream timing_file(argv[2]);
  printf("Output file: %s\n", argv[3]);
  FILE* out_file = fopen(argv[3], "wb");
  RTC_CHECK(out_file) << "Could not open file " << argv[2] << " for writing";

  char header_string[30];
  RTC_CHECK(fgets(header_string, 30, in_file));
  fprintf(out_file, "%s", header_string);
  uint8_t file_header[16];
  RTC_CHECK_EQ(fread(file_header, sizeof(file_header), 1, in_file), 1);
  RTC_CHECK_EQ(fwrite(file_header, sizeof(file_header), 1, out_file), 1);

  std::vector<int> new_arrival_times;
  int new_time;
  while (timing_file >> new_time) {
    new_arrival_times.push_back(new_time);
  }

  auto time_it = new_arrival_times.begin();
  std::vector<PacketAndTime> packets;
  while (1) {
    auto packet = ReadNextPacket(in_file);
    if (packet.empty() || time_it == new_arrival_times.end()) {
      break;
    }
    packets.push_back({std::move(packet), *time_it});
    ++time_it;
  }

  std::sort(packets.begin(), packets.end(),
            [](const PacketAndTime& a, const PacketAndTime& b) {
              return a.time < b.time;
            });

  for (const auto& p : packets) {
    WritePacket(p, out_file);
  }

  fclose(in_file);
  fclose(out_file);
  return 0;
}

}
}
}

int main(int argc, char* argv[]) {
  return webrtc::test::RunRtpJitter(argc, argv);
}
