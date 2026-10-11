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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.webrtc.CandidatePairChangeEvent;
import org.webrtc.DataChannel;
import org.webrtc.MediaStreamTrack;
import org.webrtc.RtpTransceiver;

public class PeerConnection {

  public enum IceGatheringState {
    NEW,
    GATHERING,
    COMPLETE;

    @CalledByNative("IceGatheringState")
    static IceGatheringState fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  public enum IceConnectionState {
    NEW,
    CHECKING,
    CONNECTED,
    COMPLETED,
    FAILED,
    DISCONNECTED,
    CLOSED;

    @CalledByNative("IceConnectionState")
    static IceConnectionState fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  public enum PeerConnectionState {
    NEW,
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    FAILED,
    CLOSED;

    @CalledByNative("PeerConnectionState")
    static PeerConnectionState fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  public enum TlsCertPolicy {
    TLS_CERT_POLICY_SECURE,
    TLS_CERT_POLICY_INSECURE_NO_CHECK,
  }

  public enum SignalingState {
    STABLE,
    HAVE_LOCAL_OFFER,
    HAVE_LOCAL_PRANSWER,
    HAVE_REMOTE_OFFER,
    HAVE_REMOTE_PRANSWER,
    CLOSED;

    @CalledByNative("SignalingState")
    static SignalingState fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  public static interface Observer {

    @CalledByNative("Observer") void onSignalingChange(SignalingState newState);

    @CalledByNative("Observer") void onIceConnectionChange(IceConnectionState newState);

    @CalledByNative("Observer")
    default void onStandardizedIceConnectionChange(IceConnectionState newState) {}

    @CalledByNative("Observer")
    default void onConnectionChange(PeerConnectionState newState) {}

    @CalledByNative("Observer") void onIceConnectionReceivingChange(boolean receiving);

    @CalledByNative("Observer") void onIceGatheringChange(IceGatheringState newState);

    @CalledByNative("Observer") void onIceCandidate(IceCandidate candidate);

    default @CalledByNative("Observer") void onIceCandidateError(IceCandidateErrorEvent event) {}

    @CalledByNative("Observer") void onIceCandidatesRemoved(IceCandidate[] candidates);

    @CalledByNative("Observer")
    default void onSelectedCandidatePairChanged(CandidatePairChangeEvent event) {}

    @CalledByNative("Observer") void onAddStream(MediaStream stream);

    @CalledByNative("Observer") void onRemoveStream(MediaStream stream);

    @CalledByNative("Observer") void onDataChannel(DataChannel dataChannel);

    @CalledByNative("Observer") void onRenegotiationNeeded();

    @CalledByNative("Observer")
    default void onAddTrack(RtpReceiver receiver, MediaStream[] mediaStreams){};

    @CalledByNative("Observer") default void onRemoveTrack(RtpReceiver receiver){};

    @CalledByNative("Observer") default void onTrack(RtpTransceiver transceiver){};
  }

  public static class IceServer {

    @Deprecated public final String uri;
    public final List<String> urls;
    public final String username;
    public final String password;
    public final TlsCertPolicy tlsCertPolicy;

    public final String hostname;

    public final List<String> tlsAlpnProtocols;

    public final List<String> tlsEllipticCurves;

    @Deprecated
    public IceServer(String uri) {
      this(uri, "", "");
    }

    @Deprecated
    public IceServer(String uri, String username, String password) {
      this(uri, username, password, TlsCertPolicy.TLS_CERT_POLICY_SECURE);
    }

    @Deprecated
    public IceServer(String uri, String username, String password, TlsCertPolicy tlsCertPolicy) {
      this(uri, username, password, tlsCertPolicy, "");
    }

    @Deprecated
    public IceServer(String uri, String username, String password, TlsCertPolicy tlsCertPolicy,
        String hostname) {
      this(uri, Collections.singletonList(uri), username, password, tlsCertPolicy, hostname, null,
          null);
    }

    private IceServer(String uri, List<String> urls, String username, String password,
        TlsCertPolicy tlsCertPolicy, String hostname, List<String> tlsAlpnProtocols,
        List<String> tlsEllipticCurves) {
      if (uri == null || urls == null || urls.isEmpty()) {
        throw new IllegalArgumentException("uri == null || urls == null || urls.isEmpty()");
      }
      for (String it : urls) {
        if (it == null) {
          throw new IllegalArgumentException("urls element is null: " + urls);
        }
      }
      if (username == null) {
        throw new IllegalArgumentException("username == null");
      }
      if (password == null) {
        throw new IllegalArgumentException("password == null");
      }
      if (hostname == null) {
        throw new IllegalArgumentException("hostname == null");
      }
      this.uri = uri;
      this.urls = urls;
      this.username = username;
      this.password = password;
      this.tlsCertPolicy = tlsCertPolicy;
      this.hostname = hostname;
      this.tlsAlpnProtocols = tlsAlpnProtocols;
      this.tlsEllipticCurves = tlsEllipticCurves;
    }

    @Override
    public String toString() {
      return urls + " [" + username + ":" + password + "] [" + tlsCertPolicy + "] [" + hostname
          + "] [" + tlsAlpnProtocols + "] [" + tlsEllipticCurves + "]";
    }

    @Override
    public boolean equals(@Nullable Object obj) {
      if (obj == null) {
        return false;
      }
      if (obj == this) {
        return true;
      }
      if (!(obj instanceof IceServer)) {
        return false;
      }
      IceServer other = (IceServer) obj;
      return (uri.equals(other.uri) && urls.equals(other.urls) && username.equals(other.username)
          && password.equals(other.password) && tlsCertPolicy.equals(other.tlsCertPolicy)
          && hostname.equals(other.hostname) && tlsAlpnProtocols.equals(other.tlsAlpnProtocols)
          && tlsEllipticCurves.equals(other.tlsEllipticCurves));
    }

    @Override
    public int hashCode() {
      Object[] values = {uri, urls, username, password, tlsCertPolicy, hostname, tlsAlpnProtocols,
          tlsEllipticCurves};
      return Arrays.hashCode(values);
    }

    public static Builder builder(String uri) {
      return new Builder(Collections.singletonList(uri));
    }

    public static Builder builder(List<String> urls) {
      return new Builder(urls);
    }

    public static class Builder {
      @Nullable private final List<String> urls;
      private String username = "";
      private String password = "";
      private TlsCertPolicy tlsCertPolicy = TlsCertPolicy.TLS_CERT_POLICY_SECURE;
      private String hostname = "";
      private List<String> tlsAlpnProtocols;
      private List<String> tlsEllipticCurves;

      private Builder(List<String> urls) {
        if (urls == null || urls.isEmpty()) {
          throw new IllegalArgumentException("urls == null || urls.isEmpty(): " + urls);
        }
        this.urls = urls;
      }

      public Builder setUsername(String username) {
        this.username = username;
        return this;
      }

      public Builder setPassword(String password) {
        this.password = password;
        return this;
      }

      public Builder setTlsCertPolicy(TlsCertPolicy tlsCertPolicy) {
        this.tlsCertPolicy = tlsCertPolicy;
        return this;
      }

      public Builder setHostname(String hostname) {
        this.hostname = hostname;
        return this;
      }

      public Builder setTlsAlpnProtocols(List<String> tlsAlpnProtocols) {
        this.tlsAlpnProtocols = tlsAlpnProtocols;
        return this;
      }

      public Builder setTlsEllipticCurves(List<String> tlsEllipticCurves) {
        this.tlsEllipticCurves = tlsEllipticCurves;
        return this;
      }

      public IceServer createIceServer() {
        return new IceServer(urls.get(0), urls, username, password, tlsCertPolicy, hostname,
            tlsAlpnProtocols, tlsEllipticCurves);
      }
    }

    @Nullable
    @CalledByNative("IceServer")
    List<String> getUrls() {
      return urls;
    }

    @Nullable
    @CalledByNative("IceServer")
    String getUsername() {
      return username;
    }

    @Nullable
    @CalledByNative("IceServer")
    String getPassword() {
      return password;
    }

    @CalledByNative("IceServer")
    TlsCertPolicy getTlsCertPolicy() {
      return tlsCertPolicy;
    }

    @Nullable
    @CalledByNative("IceServer")
    String getHostname() {
      return hostname;
    }

    @CalledByNative("IceServer")
    List<String> getTlsAlpnProtocols() {
      return tlsAlpnProtocols;
    }

    @CalledByNative("IceServer")
    List<String> getTlsEllipticCurves() {
      return tlsEllipticCurves;
    }
  }

  public enum IceTransportsType { NONE, RELAY, NOHOST, ALL }

  public enum BundlePolicy { BALANCED, MAXBUNDLE, MAXCOMPAT }

  public enum RtcpMuxPolicy { NEGOTIATE, REQUIRE }

  public enum TcpCandidatePolicy { ENABLED, DISABLED }

  public enum CandidateNetworkPolicy { ALL, LOW_COST }

  public enum AdapterType {
    UNKNOWN(0),
    ETHERNET(1 << 0),
    WIFI(1 << 1),
    CELLULAR(1 << 2),
    VPN(1 << 3),
    LOOPBACK(1 << 4),
    ADAPTER_TYPE_ANY(1 << 5),
    CELLULAR_2G(1 << 6),
    CELLULAR_3G(1 << 7),
    CELLULAR_4G(1 << 8),
    CELLULAR_5G(1 << 9);

    public final Integer bitMask;
    private AdapterType(Integer bitMask) {
      this.bitMask = bitMask;
    }
    private static final Map<Integer, AdapterType> BY_BITMASK = new HashMap<>();
    static {
      for (AdapterType t : values()) {
        BY_BITMASK.put(t.bitMask, t);
      }
    }

    @Nullable
    @CalledByNative("AdapterType")
    static AdapterType fromNativeIndex(int nativeIndex) {
      return BY_BITMASK.get(nativeIndex);
    }
  }

  public enum KeyType { RSA, ECDSA }

  public enum ContinualGatheringPolicy { GATHER_ONCE, GATHER_CONTINUALLY }

  public enum PortPrunePolicy {
    NO_PRUNE,
    PRUNE_BASED_ON_PRIORITY,
    KEEP_FIRST_READY
  }

  public enum SdpSemantics {

    @Deprecated PLAN_B,
    UNIFIED_PLAN
  }

  public static class RTCConfiguration {
    public IceTransportsType iceTransportsType;
    public List<IceServer> iceServers;
    public BundlePolicy bundlePolicy;
    @Nullable public RtcCertificatePem certificate;
    public RtcpMuxPolicy rtcpMuxPolicy;
    public TcpCandidatePolicy tcpCandidatePolicy;
    public CandidateNetworkPolicy candidateNetworkPolicy;
    public int audioJitterBufferMaxPackets;
    public boolean audioJitterBufferFastAccelerate;
    public int iceConnectionReceivingTimeout;
    public int iceBackupCandidatePairPingInterval;
    public KeyType keyType;
    public ContinualGatheringPolicy continualGatheringPolicy;
    public int iceCandidatePoolSize;
    @Deprecated
    public boolean pruneTurnPorts;
    public PortPrunePolicy turnPortPrunePolicy;
    public boolean presumeWritableWhenFullyRelayed;
    public boolean surfaceIceCandidatesOnIceTransportTypeChanged;

    @Nullable public Integer iceCheckIntervalStrongConnectivityMs;
    @Nullable public Integer iceCheckIntervalWeakConnectivityMs;
    @Nullable public Integer iceCheckMinInterval;

    @Nullable public Integer iceUnwritableTimeMs;

    @Nullable public Integer iceUnwritableMinChecks;

    @Nullable public Integer stunCandidateKeepaliveIntervalMs;

    @Nullable public Integer stableWritableConnectionPingIntervalMs;
    public boolean disableIPv6OnWifi;

    public int maxIPv6Networks;

    public boolean enableDscp;
    public boolean enableCpuOveruseDetection;
    public boolean suspendBelowMinBitrate;
    @Nullable public Integer screencastMinBitrate;

    public AdapterType networkPreference;
    public SdpSemantics sdpSemantics;

    @Nullable public TurnCustomizer turnCustomizer;

    public boolean activeResetSrtpParams;

    @Nullable public CryptoOptions cryptoOptions;

    @Nullable public String turnLoggingId;

    public boolean enableImplicitRollback;

    public boolean offerExtmapAllowMixed;

    public RTCConfiguration(List<IceServer> iceServers) {
      iceTransportsType = IceTransportsType.ALL;
      bundlePolicy = BundlePolicy.BALANCED;
      rtcpMuxPolicy = RtcpMuxPolicy.REQUIRE;
      tcpCandidatePolicy = TcpCandidatePolicy.ENABLED;
      candidateNetworkPolicy = CandidateNetworkPolicy.ALL;
      this.iceServers = iceServers;
      audioJitterBufferMaxPackets = 50;
      audioJitterBufferFastAccelerate = false;
      iceConnectionReceivingTimeout = -1;
      iceBackupCandidatePairPingInterval = -1;
      keyType = KeyType.ECDSA;
      continualGatheringPolicy = ContinualGatheringPolicy.GATHER_ONCE;
      iceCandidatePoolSize = 0;
      pruneTurnPorts = false;
      turnPortPrunePolicy = PortPrunePolicy.NO_PRUNE;
      presumeWritableWhenFullyRelayed = false;
      surfaceIceCandidatesOnIceTransportTypeChanged = false;
      iceCheckIntervalStrongConnectivityMs = null;
      iceCheckIntervalWeakConnectivityMs = null;
      iceCheckMinInterval = null;
      iceUnwritableTimeMs = null;
      iceUnwritableMinChecks = null;
      stunCandidateKeepaliveIntervalMs = null;
      stableWritableConnectionPingIntervalMs = null;
      disableIPv6OnWifi = false;
      maxIPv6Networks = 5;
      enableDscp = false;
      enableCpuOveruseDetection = true;
      suspendBelowMinBitrate = false;
      screencastMinBitrate = null;
      networkPreference = AdapterType.UNKNOWN;
      sdpSemantics = SdpSemantics.UNIFIED_PLAN;
      activeResetSrtpParams = false;
      cryptoOptions = null;
      turnLoggingId = null;
      enableImplicitRollback = false;
      offerExtmapAllowMixed = true;
    }

    @CalledByNative("RTCConfiguration")
    IceTransportsType getIceTransportsType() {
      return iceTransportsType;
    }

    @CalledByNative("RTCConfiguration")
    List<IceServer> getIceServers() {
      return iceServers;
    }

    @CalledByNative("RTCConfiguration")
    BundlePolicy getBundlePolicy() {
      return bundlePolicy;
    }

    @CalledByNative("RTCConfiguration")
    PortPrunePolicy getTurnPortPrunePolicy() {
      return turnPortPrunePolicy;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    RtcCertificatePem getCertificate() {
      return certificate;
    }

    @CalledByNative("RTCConfiguration")
    RtcpMuxPolicy getRtcpMuxPolicy() {
      return rtcpMuxPolicy;
    }

    @CalledByNative("RTCConfiguration")
    TcpCandidatePolicy getTcpCandidatePolicy() {
      return tcpCandidatePolicy;
    }

    @CalledByNative("RTCConfiguration")
    CandidateNetworkPolicy getCandidateNetworkPolicy() {
      return candidateNetworkPolicy;
    }

    @CalledByNative("RTCConfiguration")
    int getAudioJitterBufferMaxPackets() {
      return audioJitterBufferMaxPackets;
    }

    @CalledByNative("RTCConfiguration")
    boolean getAudioJitterBufferFastAccelerate() {
      return audioJitterBufferFastAccelerate;
    }

    @CalledByNative("RTCConfiguration")
    int getIceConnectionReceivingTimeout() {
      return iceConnectionReceivingTimeout;
    }

    @CalledByNative("RTCConfiguration")
    int getIceBackupCandidatePairPingInterval() {
      return iceBackupCandidatePairPingInterval;
    }

    @CalledByNative("RTCConfiguration")
    KeyType getKeyType() {
      return keyType;
    }

    @CalledByNative("RTCConfiguration")
    ContinualGatheringPolicy getContinualGatheringPolicy() {
      return continualGatheringPolicy;
    }

    @CalledByNative("RTCConfiguration")
    int getIceCandidatePoolSize() {
      return iceCandidatePoolSize;
    }

    @CalledByNative("RTCConfiguration")
    boolean getPruneTurnPorts() {
      return pruneTurnPorts;
    }

    @CalledByNative("RTCConfiguration")
    boolean getPresumeWritableWhenFullyRelayed() {
      return presumeWritableWhenFullyRelayed;
    }

    @CalledByNative("RTCConfiguration")
    boolean getSurfaceIceCandidatesOnIceTransportTypeChanged() {
      return surfaceIceCandidatesOnIceTransportTypeChanged;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getIceCheckIntervalStrongConnectivity() {
      return iceCheckIntervalStrongConnectivityMs;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getIceCheckIntervalWeakConnectivity() {
      return iceCheckIntervalWeakConnectivityMs;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getIceCheckMinInterval() {
      return iceCheckMinInterval;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getIceUnwritableTimeout() {
      return iceUnwritableTimeMs;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getIceUnwritableMinChecks() {
      return iceUnwritableMinChecks;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getStunCandidateKeepaliveInterval() {
      return stunCandidateKeepaliveIntervalMs;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getStableWritableConnectionPingIntervalMs() {
      return stableWritableConnectionPingIntervalMs;
    }

    @CalledByNative("RTCConfiguration")
    boolean getDisableIPv6OnWifi() {
      return disableIPv6OnWifi;
    }

    @CalledByNative("RTCConfiguration")
    int getMaxIPv6Networks() {
      return maxIPv6Networks;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    TurnCustomizer getTurnCustomizer() {
      return turnCustomizer;
    }

    @CalledByNative("RTCConfiguration")
    boolean getEnableDscp() {
      return enableDscp;
    }

    @CalledByNative("RTCConfiguration")
    boolean getEnableCpuOveruseDetection() {
      return enableCpuOveruseDetection;
    }

    @CalledByNative("RTCConfiguration")
    boolean getSuspendBelowMinBitrate() {
      return suspendBelowMinBitrate;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    Integer getScreencastMinBitrate() {
      return screencastMinBitrate;
    }

    @CalledByNative("RTCConfiguration")
    AdapterType getNetworkPreference() {
      return networkPreference;
    }

    @CalledByNative("RTCConfiguration")
    SdpSemantics getSdpSemantics() {
      return sdpSemantics;
    }

    @CalledByNative("RTCConfiguration")
    boolean getActiveResetSrtpParams() {
      return activeResetSrtpParams;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    CryptoOptions getCryptoOptions() {
      return cryptoOptions;
    }

    @Nullable
    @CalledByNative("RTCConfiguration")
    String getTurnLoggingId() {
      return turnLoggingId;
    }

    @CalledByNative("RTCConfiguration")
    boolean getEnableImplicitRollback() {
      return enableImplicitRollback;
    }

    @CalledByNative("RTCConfiguration")
    boolean getOfferExtmapAllowMixed() {
      return offerExtmapAllowMixed;
    }
  };

  private final List<MediaStream> localStreams = new ArrayList<>();
  private final long nativePeerConnection;
  private List<RtpSender> senders = new ArrayList<>();
  private List<RtpReceiver> receivers = new ArrayList<>();
  private List<RtpTransceiver> transceivers = new ArrayList<>();

  public PeerConnection(NativePeerConnectionFactory factory) {
    this(factory.createNativePeerConnection());
  }

  PeerConnection(long nativePeerConnection) {
    this.nativePeerConnection = nativePeerConnection;
  }

  public SessionDescription getLocalDescription() {
    return nativeGetLocalDescription();
  }

  public SessionDescription getRemoteDescription() {
    return nativeGetRemoteDescription();
  }

  public RtcCertificatePem getCertificate() {
    return nativeGetCertificate();
  }

  public DataChannel createDataChannel(String label, DataChannel.Init init) {
    return nativeCreateDataChannel(label, init);
  }

  public void createOffer(SdpObserver observer, MediaConstraints constraints) {
    nativeCreateOffer(observer, constraints);
  }

  public void createAnswer(SdpObserver observer, MediaConstraints constraints) {
    nativeCreateAnswer(observer, constraints);
  }

  public void setLocalDescription(SdpObserver observer) {
    nativeSetLocalDescriptionAutomatically(observer);
  }

  public void setLocalDescription(SdpObserver observer, SessionDescription sdp) {
    nativeSetLocalDescription(observer, sdp);
  }

  public void setRemoteDescription(SdpObserver observer, SessionDescription sdp) {
    nativeSetRemoteDescription(observer, sdp);
  }

  public void restartIce() {
    nativeRestartIce();
  }

  public void setAudioPlayout(boolean playout) {
    nativeSetAudioPlayout(playout);
  }

  public void setAudioRecording(boolean recording) {
    nativeSetAudioRecording(recording);
  }

  public boolean setConfiguration(RTCConfiguration config) {
    return nativeSetConfiguration(config);
  }

  public boolean addIceCandidate(IceCandidate candidate) {
    return nativeAddIceCandidate(candidate.sdpMid, candidate.sdpMLineIndex, candidate.sdp);
  }

  public void addIceCandidate(IceCandidate candidate, AddIceObserver observer) {
    nativeAddIceCandidateWithObserver(
        candidate.sdpMid, candidate.sdpMLineIndex, candidate.sdp, observer);
  }

  public boolean removeIceCandidates(final IceCandidate[] candidates) {
    return nativeRemoveIceCandidates(candidates);
  }

  public boolean addStream(MediaStream stream) {
    boolean ret = nativeAddLocalStream(stream.getNativeMediaStream());
    if (!ret) {
      return false;
    }
    localStreams.add(stream);
    return true;
  }

  public void removeStream(MediaStream stream) {
    nativeRemoveLocalStream(stream.getNativeMediaStream());
    localStreams.remove(stream);
  }

  public RtpSender createSender(String kind, String stream_id) {
    RtpSender newSender = nativeCreateSender(kind, stream_id);
    if (newSender != null) {
      senders.add(newSender);
    }
    return newSender;
  }

  public List<RtpSender> getSenders() {
    for (RtpSender sender : senders) {
      sender.dispose();
    }
    senders = nativeGetSenders();
    return Collections.unmodifiableList(senders);
  }

  public List<RtpReceiver> getReceivers() {
    for (RtpReceiver receiver : receivers) {
      receiver.dispose();
    }
    receivers = nativeGetReceivers();
    return Collections.unmodifiableList(receivers);
  }

  public List<RtpTransceiver> getTransceivers() {
    for (RtpTransceiver transceiver : transceivers) {
      transceiver.dispose();
    }
    transceivers = nativeGetTransceivers();
    return Collections.unmodifiableList(transceivers);
  }

  public RtpSender addTrack(MediaStreamTrack track) {
    return addTrack(track, Collections.emptyList());
  }

  public RtpSender addTrack(MediaStreamTrack track, List<String> streamIds) {
    if (track == null || streamIds == null) {
      throw new NullPointerException("No MediaStreamTrack specified in addTrack.");
    }
    RtpSender newSender = nativeAddTrack(track.getNativeMediaStreamTrack(), streamIds);
    if (newSender == null) {
      throw new IllegalStateException("C++ addTrack failed.");
    }
    senders.add(newSender);
    return newSender;
  }

  public boolean removeTrack(RtpSender sender) {
    if (sender == null) {
      throw new NullPointerException("No RtpSender specified for removeTrack.");
    }
    return nativeRemoveTrack(sender.getNativeRtpSender());
  }

  public RtpTransceiver addTransceiver(MediaStreamTrack track) {
    return addTransceiver(track, new RtpTransceiver.RtpTransceiverInit());
  }

  public RtpTransceiver addTransceiver(
      MediaStreamTrack track, @Nullable RtpTransceiver.RtpTransceiverInit init) {
    if (track == null) {
      throw new NullPointerException("No MediaStreamTrack specified for addTransceiver.");
    }
    if (init == null) {
      init = new RtpTransceiver.RtpTransceiverInit();
    }
    RtpTransceiver newTransceiver =
        nativeAddTransceiverWithTrack(track.getNativeMediaStreamTrack(), init);
    if (newTransceiver == null) {
      throw new IllegalStateException("C++ addTransceiver failed.");
    }
    transceivers.add(newTransceiver);
    return newTransceiver;
  }

  public RtpTransceiver addTransceiver(MediaStreamTrack.MediaType mediaType) {
    return addTransceiver(mediaType, new RtpTransceiver.RtpTransceiverInit());
  }

  public RtpTransceiver addTransceiver(
      MediaStreamTrack.MediaType mediaType, @Nullable RtpTransceiver.RtpTransceiverInit init) {
    if (mediaType == null) {
      throw new NullPointerException("No MediaType specified for addTransceiver.");
    }
    if (init == null) {
      init = new RtpTransceiver.RtpTransceiverInit();
    }
    RtpTransceiver newTransceiver = nativeAddTransceiverOfType(mediaType, init);
    if (newTransceiver == null) {
      throw new IllegalStateException("C++ addTransceiver failed.");
    }
    transceivers.add(newTransceiver);
    return newTransceiver;
  }

  @Deprecated
  public boolean getStats(StatsObserver observer, @Nullable MediaStreamTrack track) {
    return nativeOldGetStats(observer, (track == null) ? 0 : track.getNativeMediaStreamTrack());
  }

  public void getStats(RTCStatsCollectorCallback callback) {
    nativeNewGetStats(callback);
  }

  public void getStats(RtpSender sender, RTCStatsCollectorCallback callback) {
    nativeNewGetStatsSender(sender.getNativeRtpSender(), callback);
  }

  public void getStats(RtpReceiver receiver, RTCStatsCollectorCallback callback) {
    nativeNewGetStatsReceiver(receiver.getNativeRtpReceiver(), callback);
  }

  public boolean setBitrate(Integer min, Integer current, Integer max) {
    return nativeSetBitrate(min, current, max);
  }

  public boolean startRtcEventLog(int file_descriptor, int max_size_bytes) {
    return nativeStartRtcEventLog(file_descriptor, max_size_bytes);
  }

  public void stopRtcEventLog() {
    nativeStopRtcEventLog();
  }

  public SignalingState signalingState() {
    return nativeSignalingState();
  }

  public IceConnectionState iceConnectionState() {
    return nativeIceConnectionState();
  }

  public PeerConnectionState connectionState() {
    return nativeConnectionState();
  }

  public IceGatheringState iceGatheringState() {
    return nativeIceGatheringState();
  }

  public void close() {
    nativeClose();
  }

  public void dispose() {
    close();
    for (MediaStream stream : localStreams) {
      nativeRemoveLocalStream(stream.getNativeMediaStream());
      stream.dispose();
    }
    localStreams.clear();
    for (RtpSender sender : senders) {
      sender.dispose();
    }
    senders.clear();
    for (RtpReceiver receiver : receivers) {
      receiver.dispose();
    }
    for (RtpTransceiver transceiver : transceivers) {
      transceiver.dispose();
    }
    transceivers.clear();
    receivers.clear();
    nativeFreeOwnedPeerConnection(nativePeerConnection);
  }

  public long getNativePeerConnection() {
    return nativeGetNativePeerConnection();
  }

  @CalledByNative
  long getNativeOwnedPeerConnection() {
    return nativePeerConnection;
  }

  public static long createNativePeerConnectionObserver(Observer observer) {
    return nativeCreatePeerConnectionObserver(observer);
  }

  private native long nativeGetNativePeerConnection();
  private native SessionDescription nativeGetLocalDescription();
  private native SessionDescription nativeGetRemoteDescription();
  private native RtcCertificatePem nativeGetCertificate();
  private native DataChannel nativeCreateDataChannel(String label, DataChannel.Init init);
  private native void nativeCreateOffer(SdpObserver observer, MediaConstraints constraints);
  private native void nativeCreateAnswer(SdpObserver observer, MediaConstraints constraints);
  private native void nativeSetLocalDescriptionAutomatically(SdpObserver observer);
  private native void nativeSetLocalDescription(SdpObserver observer, SessionDescription sdp);
  private native void nativeSetRemoteDescription(SdpObserver observer, SessionDescription sdp);
  private native void nativeRestartIce();
  private native void nativeSetAudioPlayout(boolean playout);
  private native void nativeSetAudioRecording(boolean recording);
  private native boolean nativeSetBitrate(Integer min, Integer current, Integer max);
  private native SignalingState nativeSignalingState();
  private native IceConnectionState nativeIceConnectionState();
  private native PeerConnectionState nativeConnectionState();
  private native IceGatheringState nativeIceGatheringState();
  private native void nativeClose();
  private static native long nativeCreatePeerConnectionObserver(Observer observer);
  private static native void nativeFreeOwnedPeerConnection(long ownedPeerConnection);
  private native boolean nativeSetConfiguration(RTCConfiguration config);
  private native boolean nativeAddIceCandidate(
      String sdpMid, int sdpMLineIndex, String iceCandidateSdp);
  private native void nativeAddIceCandidateWithObserver(
      String sdpMid, int sdpMLineIndex, String iceCandidateSdp, AddIceObserver observer);
  private native boolean nativeRemoveIceCandidates(final IceCandidate[] candidates);
  private native boolean nativeAddLocalStream(long stream);
  private native void nativeRemoveLocalStream(long stream);
  private native boolean nativeOldGetStats(StatsObserver observer, long nativeTrack);
  private native void nativeNewGetStats(RTCStatsCollectorCallback callback);
  private native void nativeNewGetStatsSender(long sender, RTCStatsCollectorCallback callback);
  private native void nativeNewGetStatsReceiver(long receiver, RTCStatsCollectorCallback callback);
  private native RtpSender nativeCreateSender(String kind, String stream_id);
  private native List<RtpSender> nativeGetSenders();
  private native List<RtpReceiver> nativeGetReceivers();
  private native List<RtpTransceiver> nativeGetTransceivers();
  private native RtpSender nativeAddTrack(long track, List<String> streamIds);
  private native boolean nativeRemoveTrack(long sender);
  private native RtpTransceiver nativeAddTransceiverWithTrack(
      long track, RtpTransceiver.RtpTransceiverInit init);
  private native RtpTransceiver nativeAddTransceiverOfType(
      MediaStreamTrack.MediaType mediaType, RtpTransceiver.RtpTransceiverInit init);
  private native boolean nativeStartRtcEventLog(int file_descriptor, int max_size_bytes);
  private native void nativeStopRtcEventLog();
}
