/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtcp_mux_filter.h"

#include "test/gtest.h"

TEST(RtcpMuxFilterTest, IsActiveSender) {
  cricket::RtcpMuxFilter filter;

  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  filter.SetOffer(true, cricket::CS_LOCAL);
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  filter.SetAnswer(true, cricket::CS_REMOTE);
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

TEST(RtcpMuxFilterTest, ReceivePrAnswer) {
  cricket::RtcpMuxFilter filter;
  filter.SetOffer(true, cricket::CS_LOCAL);

  EXPECT_TRUE(filter.SetProvisionalAnswer(true, cricket::CS_REMOTE));

  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  EXPECT_TRUE(filter.SetProvisionalAnswer(false, cricket::CS_REMOTE));

  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

TEST(RtcpMuxFilterTest, IsActiveReceiver) {
  cricket::RtcpMuxFilter filter;

  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  filter.SetOffer(true, cricket::CS_REMOTE);
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  filter.SetAnswer(true, cricket::CS_LOCAL);
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

TEST(RtcpMuxFilterTest, SendPrAnswer) {
  cricket::RtcpMuxFilter filter;
  filter.SetOffer(true, cricket::CS_REMOTE);

  EXPECT_TRUE(filter.SetProvisionalAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  EXPECT_TRUE(filter.SetProvisionalAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_FALSE(filter.IsFullyActive());

  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsProvisionallyActive());
  EXPECT_TRUE(filter.IsFullyActive());
}

TEST(RtcpMuxFilterTest, EnableFilterDuringUpdate) {
  cricket::RtcpMuxFilter filter;
  EXPECT_FALSE(filter.IsActive());
  EXPECT_TRUE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_FALSE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}

TEST(RtcpMuxFilterTest, SetOfferTwice) {
  cricket::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  cricket::RtcpMuxFilter filter2;
  EXPECT_TRUE(filter2.SetOffer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter2.SetOffer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter2.SetAnswer(false, cricket::CS_REMOTE));
  EXPECT_FALSE(filter2.IsActive());
}

TEST(RtcpMuxFilterTest, EnableFilterTwiceDuringUpdate) {
  cricket::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}

TEST(RtcpMuxFilterTest, KeepFilterDisabledDuringUpdate) {
  cricket::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());
}

TEST(RtcpMuxFilterTest, SetActiveCantDeactivate) {
  cricket::RtcpMuxFilter filter;

  filter.SetActive();
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetOffer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetProvisionalAnswer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetAnswer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetProvisionalAnswer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}
