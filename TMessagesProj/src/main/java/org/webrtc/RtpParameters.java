/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;
import java.lang.Double;
import java.lang.String;
import java.util.List;
import java.util.Map;
import org.webrtc.MediaStreamTrack;

public class RtpParameters {
  public enum DegradationPreference {

    DISABLED,

    MAINTAIN_FRAMERATE,

    MAINTAIN_RESOLUTION,

    BALANCED;

    @CalledByNative("DegradationPreference")
    static DegradationPreference fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  public static class Encoding {

    @Nullable public String rid;

    public boolean active = true;

    public double bitratePriority = 1.0;

    public int networkPriority = 0;

    @Nullable public Integer maxBitrateBps;

    @Nullable public Integer minBitrateBps;

    @Nullable public Integer maxFramerate;

    @Nullable public Integer numTemporalLayers;

    @Nullable public Double scaleResolutionDownBy;

    public Long ssrc;

    public boolean adaptiveAudioPacketTime;

    public Encoding(String rid, boolean active, Double scaleResolutionDownBy) {
      this.rid = rid;
      this.active = active;
      this.scaleResolutionDownBy = scaleResolutionDownBy;
    }

    @CalledByNative("Encoding")
    Encoding(String rid, boolean active, double bitratePriority, int networkPriority,
        Integer maxBitrateBps, Integer minBitrateBps, Integer maxFramerate,
        Integer numTemporalLayers, Double scaleResolutionDownBy, Long ssrc,
        boolean adaptiveAudioPacketTime) {
      this.rid = rid;
      this.active = active;
      this.bitratePriority = bitratePriority;
      this.networkPriority = networkPriority;
      this.maxBitrateBps = maxBitrateBps;
      this.minBitrateBps = minBitrateBps;
      this.maxFramerate = maxFramerate;
      this.numTemporalLayers = numTemporalLayers;
      this.scaleResolutionDownBy = scaleResolutionDownBy;
      this.ssrc = ssrc;
      this.adaptiveAudioPacketTime = adaptiveAudioPacketTime;
    }

    @Nullable
    @CalledByNative("Encoding")
    String getRid() {
      return rid;
    }

    @CalledByNative("Encoding")
    boolean getActive() {
      return active;
    }

    @CalledByNative("Encoding")
    double getBitratePriority() {
      return bitratePriority;
    }

    @CalledByNative("Encoding")
    int getNetworkPriority() {
      return networkPriority;
    }

    @Nullable
    @CalledByNative("Encoding")
    Integer getMaxBitrateBps() {
      return maxBitrateBps;
    }

    @Nullable
    @CalledByNative("Encoding")
    Integer getMinBitrateBps() {
      return minBitrateBps;
    }

    @Nullable
    @CalledByNative("Encoding")
    Integer getMaxFramerate() {
      return maxFramerate;
    }

    @Nullable
    @CalledByNative("Encoding")
    Integer getNumTemporalLayers() {
      return numTemporalLayers;
    }

    @Nullable
    @CalledByNative("Encoding")
    Double getScaleResolutionDownBy() {
      return scaleResolutionDownBy;
    }

    @CalledByNative("Encoding")
    Long getSsrc() {
      return ssrc;
    }

    @CalledByNative("Encoding")
    boolean getAdaptivePTime() {
      return adaptiveAudioPacketTime;
    }
  }

  public static class Codec {

    public int payloadType;

    public String name;

    MediaStreamTrack.MediaType kind;

    public Integer clockRate;

    public Integer numChannels;

    public Map<String, String> parameters;

    @CalledByNative("Codec")
    Codec(int payloadType, String name, MediaStreamTrack.MediaType kind, Integer clockRate,
        Integer numChannels, Map<String, String> parameters) {
      this.payloadType = payloadType;
      this.name = name;
      this.kind = kind;
      this.clockRate = clockRate;
      this.numChannels = numChannels;
      this.parameters = parameters;
    }

    @CalledByNative("Codec")
    int getPayloadType() {
      return payloadType;
    }

    @CalledByNative("Codec")
    String getName() {
      return name;
    }

    @CalledByNative("Codec")
    MediaStreamTrack.MediaType getKind() {
      return kind;
    }

    @CalledByNative("Codec")
    Integer getClockRate() {
      return clockRate;
    }

    @CalledByNative("Codec")
    Integer getNumChannels() {
      return numChannels;
    }

    @CalledByNative("Codec")
    Map getParameters() {
      return parameters;
    }
  }

  public static class Rtcp {

    private final String cname;

    private final boolean reducedSize;

    @CalledByNative("Rtcp")
    Rtcp(String cname, boolean reducedSize) {
      this.cname = cname;
      this.reducedSize = reducedSize;
    }

    @CalledByNative("Rtcp")
    public String getCname() {
      return cname;
    }

    @CalledByNative("Rtcp")
    public boolean getReducedSize() {
      return reducedSize;
    }
  }

  public static class HeaderExtension {

    private final String uri;

    private final int id;

    private final boolean encrypted;

    @CalledByNative("HeaderExtension")
    HeaderExtension(String uri, int id, boolean encrypted) {
      this.uri = uri;
      this.id = id;
      this.encrypted = encrypted;
    }

    @CalledByNative("HeaderExtension")
    public String getUri() {
      return uri;
    }

    @CalledByNative("HeaderExtension")
    public int getId() {
      return id;
    }

    @CalledByNative("HeaderExtension")
    public boolean getEncrypted() {
      return encrypted;
    }
  }

  public final String transactionId;

  @Nullable public DegradationPreference degradationPreference;

  private final Rtcp rtcp;

  private final List<HeaderExtension> headerExtensions;

  public final List<Encoding> encodings;

  public final List<Codec> codecs;

  @CalledByNative
  RtpParameters(String transactionId, DegradationPreference degradationPreference, Rtcp rtcp,
      List<HeaderExtension> headerExtensions, List<Encoding> encodings, List<Codec> codecs) {
    this.transactionId = transactionId;
    this.degradationPreference = degradationPreference;
    this.rtcp = rtcp;
    this.headerExtensions = headerExtensions;
    this.encodings = encodings;
    this.codecs = codecs;
  }

  @CalledByNative
  String getTransactionId() {
    return transactionId;
  }

  @CalledByNative
  DegradationPreference getDegradationPreference() {
    return degradationPreference;
  }

  @CalledByNative
  public Rtcp getRtcp() {
    return rtcp;
  }

  @CalledByNative
  public List<HeaderExtension> getHeaderExtensions() {
    return headerExtensions;
  }

  @CalledByNative
  List<Encoding> getEncodings() {
    return encodings;
  }

  @CalledByNative
  List<Codec> getCodecs() {
    return codecs;
  }
}
