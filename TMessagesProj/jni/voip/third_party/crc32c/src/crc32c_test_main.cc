// Copyright 2017 The CRC32C Authors. All rights reserved.

#include "voip/third_party/crc32c/src/include/crc32c/crc32c_config.h"

#include "gtest/gtest.h"

#if CRC32C_TESTS_BUILT_WITH_GLOG
#include "glog/logging.h"
#endif

int main(int argc, char** argv) {
#if CRC32C_TESTS_BUILT_WITH_GLOG
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
