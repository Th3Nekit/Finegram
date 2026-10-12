/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.webrtc.MediaStreamTrack;
import org.webrtc.RtpParameters;

public class RtpTransceiver {

  public enum RtpTransceiverDirection {
    SEND_RECV(0),
    SEND_ONLY(1),
    RECV_ONLY(2),
    INACTIVE(3),
    STOPPED(4);

    private final int nativeIndex;

    private RtpTransceiverDirection(int nativeIndex) {
      this.nativeIndex = nativeIndex;
    }

    @CalledByNative("RtpTransceiverDirection")
    int getNativeIndex() {
      return nativeIndex;
    }

    @CalledByNative("RtpTransceiverDirection")
    static RtpTransceiverDirection fromNativeIndex(int nativeIndex) {
      for (RtpTransceiverDirection type : RtpTransceiverDirection.values()) {
        if (type.getNativeIndex() == nativeIndex) {
          return type;
        }
      }
      throw new IllegalArgumentException(
          "Uknown native RtpTransceiverDirection type" + nativeIndex);
    }
  }

  public static final class RtpTransceiverInit {
    private final RtpTransceiverDirection direction;
    private final List<String> streamIds;
    private final List<RtpParameters.Encoding> sendEncodings;

    public RtpTransceiverInit() {
      this(RtpTransceiverDirection.SEND_RECV);
    }

    public RtpTransceiverInit(RtpTransceiverDirection direction) {
      this(direction, Collections.emptyList(), Collections.emptyList());
    }

    public RtpTransceiverInit(RtpTransceiverDirection direction, List<String> streamIds) {
      this(direction, streamIds, Collections.emptyList());
    }

    public RtpTransceiverInit(RtpTransceiverDirection direction, List<String> streamIds,
        List<RtpParameters.Encoding> sendEncodings) {
      this.direction = direction;
      this.streamIds = new ArrayList<String>(streamIds);
      this.sendEncodings = new ArrayList<RtpParameters.Encoding>(sendEncodings);
    }

    @CalledByNative("RtpTransceiverInit")
    int getDirectionNativeIndex() {
      return direction.getNativeIndex();
    }

    @CalledByNative("RtpTransceiverInit")
    List<String> getStreamIds() {
      return new ArrayList<String>(this.streamIds);
    }

    @CalledByNative("RtpTransceiverInit")
    List<RtpParameters.Encoding> getSendEncodings() {
      return new ArrayList<RtpParameters.Encoding>(this.sendEncodings);
    }
  }

  private long nativeRtpTransceiver;
  private RtpSender cachedSender;
  private RtpReceiver cachedReceiver;

  @CalledByNative
  protected RtpTransceiver(long nativeRtpTransceiver) {
    this.nativeRtpTransceiver = nativeRtpTransceiver;
    cachedSender = nativeGetSender(nativeRtpTransceiver);
    cachedReceiver = nativeGetReceiver(nativeRtpTransceiver);
  }

  public MediaStreamTrack.MediaType getMediaType() {
    checkRtpTransceiverExists();
    return nativeGetMediaType(nativeRtpTransceiver);
  }

  public String getMid() {
    checkRtpTransceiverExists();
    return nativeGetMid(nativeRtpTransceiver);
  }

  public RtpSender getSender() {
    return cachedSender;
  }

  public RtpReceiver getReceiver() {
    return cachedReceiver;
  }

  public boolean isStopped() {
    checkRtpTransceiverExists();
    return nativeStopped(nativeRtpTransceiver);
  }

  public RtpTransceiverDirection getDirection() {
    checkRtpTransceiverExists();
    return nativeDirection(nativeRtpTransceiver);
  }

  public RtpTransceiverDirection getCurrentDirection() {
    checkRtpTransceiverExists();
    return nativeCurrentDirection(nativeRtpTransceiver);
  }

  public boolean setDirection(RtpTransceiverDirection rtpTransceiverDirection) {
    checkRtpTransceiverExists();
    return nativeSetDirection(nativeRtpTransceiver, rtpTransceiverDirection);
  }

  public void stop() {
    checkRtpTransceiverExists();
    nativeStopInternal(nativeRtpTransceiver);
  }

  public void setCodecPreferences(List<RtpCapabilities.CodecCapability> codecs) {
    checkRtpTransceiverExists();
    nativeSetCodecPreferences(nativeRtpTransceiver, codecs);
  }

  public void stopInternal() {
    checkRtpTransceiverExists();
    nativeStopInternal(nativeRtpTransceiver);
  }

  public void stopStandard() {
    checkRtpTransceiverExists();
    nativeStopStandard(nativeRtpTransceiver);
  }

  @CalledByNative
  public void dispose() {
    checkRtpTransceiverExists();
    cachedSender.dispose();
    cachedReceiver.dispose();
    JniCommon.nativeReleaseRef(nativeRtpTransceiver);
    nativeRtpTransceiver = 0;
  }

  private void checkRtpTransceiverExists() {
    if (nativeRtpTransceiver == 0) {
      throw new IllegalStateException("RtpTransceiver has been disposed.");
    }
  }

  private static native MediaStreamTrack.MediaType nativeGetMediaType(long rtpTransceiver);
  private static native String nativeGetMid(long rtpTransceiver);
  private static native RtpSender nativeGetSender(long rtpTransceiver);
  private static native RtpReceiver nativeGetReceiver(long rtpTransceiver);
  private static native boolean nativeStopped(long rtpTransceiver);
  private static native RtpTransceiverDirection nativeDirection(long rtpTransceiver);
  private static native RtpTransceiverDirection nativeCurrentDirection(long rtpTransceiver);
  private static native void nativeStopInternal(long rtpTransceiver);
  private static native void nativeStopStandard(long rtpTransceiver);
  private static native boolean nativeSetDirection(
      long rtpTransceiver, RtpTransceiverDirection rtpTransceiverDirection);
  private static native void nativeSetCodecPreferences(
      long rtpTransceiver, List<RtpCapabilities.CodecCapability> codecs);
}
