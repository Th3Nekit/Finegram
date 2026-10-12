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

import android.content.Context;
import android.os.Process;
import androidx.annotation.Nullable;
import java.util.List;
import org.webrtc.Logging.Severity;
import org.webrtc.MediaStreamTrack;
import org.webrtc.PeerConnection;
import org.webrtc.RtpCapabilities;
import org.webrtc.audio.AudioDeviceModule;
import org.webrtc.audio.JavaAudioDeviceModule;

public class PeerConnectionFactory {
  public static final String TRIAL_ENABLED = "Enabled";
  @Deprecated public static final String VIDEO_FRAME_EMIT_TRIAL = "VideoFrameEmit";

  private static final String TAG = "PeerConnectionFactory";
  private static final String VIDEO_CAPTURER_THREAD_NAME = "VideoCapturerThread";

  private static class ThreadInfo {
    final Thread thread;
    final int tid;

    public static ThreadInfo getCurrent() {
      return new ThreadInfo(Thread.currentThread(), Process.myTid());
    }

    private ThreadInfo(Thread thread, int tid) {
      this.thread = thread;
      this.tid = tid;
    }
  }

  private static volatile boolean internalTracerInitialized;

  @Nullable private static ThreadInfo staticNetworkThread;
  @Nullable private static ThreadInfo staticWorkerThread;
  @Nullable private static ThreadInfo staticSignalingThread;

  private long nativeFactory;
  @Nullable private volatile ThreadInfo networkThread;
  @Nullable private volatile ThreadInfo workerThread;
  @Nullable private volatile ThreadInfo signalingThread;

  public static class InitializationOptions {
    final Context applicationContext;
    final String fieldTrials;
    final boolean enableInternalTracer;
    final NativeLibraryLoader nativeLibraryLoader;
    final String nativeLibraryName;
    @Nullable Loggable loggable;
    @Nullable Severity loggableSeverity;

    private InitializationOptions(Context applicationContext, String fieldTrials,
        boolean enableInternalTracer, NativeLibraryLoader nativeLibraryLoader,
        String nativeLibraryName, @Nullable Loggable loggable,
        @Nullable Severity loggableSeverity) {
      this.applicationContext = applicationContext;
      this.fieldTrials = fieldTrials;
      this.enableInternalTracer = enableInternalTracer;
      this.nativeLibraryLoader = nativeLibraryLoader;
      this.nativeLibraryName = nativeLibraryName;
      this.loggable = loggable;
      this.loggableSeverity = loggableSeverity;
    }

    public static Builder builder(Context applicationContext) {
      return new Builder(applicationContext);
    }

    public static class Builder {
      private final Context applicationContext;
      private String fieldTrials = "";
      private boolean enableInternalTracer;
      private NativeLibraryLoader nativeLibraryLoader = new NativeLibrary.DefaultLoader();
      private String nativeLibraryName = "jingle_peerconnection_so";
      @Nullable private Loggable loggable;
      @Nullable private Severity loggableSeverity;

      Builder(Context applicationContext) {
        this.applicationContext = applicationContext;
      }

      public Builder setFieldTrials(String fieldTrials) {
        this.fieldTrials = fieldTrials;
        return this;
      }

      public Builder setEnableInternalTracer(boolean enableInternalTracer) {
        this.enableInternalTracer = enableInternalTracer;
        return this;
      }

      public Builder setNativeLibraryLoader(NativeLibraryLoader nativeLibraryLoader) {
        this.nativeLibraryLoader = nativeLibraryLoader;
        return this;
      }

      public Builder setNativeLibraryName(String nativeLibraryName) {
        this.nativeLibraryName = nativeLibraryName;
        return this;
      }

      public Builder setInjectableLogger(Loggable loggable, Severity severity) {
        this.loggable = loggable;
        this.loggableSeverity = severity;
        return this;
      }

      public PeerConnectionFactory.InitializationOptions createInitializationOptions() {
        return new PeerConnectionFactory.InitializationOptions(applicationContext, fieldTrials,
            enableInternalTracer, nativeLibraryLoader, nativeLibraryName, loggable,
            loggableSeverity);
      }
    }
  }

  public static class Options {

    public static final int ADAPTER_TYPE_UNKNOWN = 0;
    public static final int ADAPTER_TYPE_ETHERNET = 1 << 0;
    public static final int ADAPTER_TYPE_WIFI = 1 << 1;
    public static final int ADAPTER_TYPE_CELLULAR = 1 << 2;
    public static final int ADAPTER_TYPE_VPN = 1 << 3;
    public static final int ADAPTER_TYPE_LOOPBACK = 1 << 4;
    public static final int ADAPTER_TYPE_ANY = 1 << 5;

    public int networkIgnoreMask;
    public boolean disableEncryption;
    public boolean disableNetworkMonitor;

    @CalledByNative("Options")
    int getNetworkIgnoreMask() {
      return networkIgnoreMask;
    }

    @CalledByNative("Options")
    boolean getDisableEncryption() {
      return disableEncryption;
    }

    @CalledByNative("Options")
    boolean getDisableNetworkMonitor() {
      return disableNetworkMonitor;
    }
  }

  public static class Builder {
    @Nullable private Options options;
    @Nullable private AudioDeviceModule audioDeviceModule;
    private AudioEncoderFactoryFactory audioEncoderFactoryFactory =
        new BuiltinAudioEncoderFactoryFactory();
    private AudioDecoderFactoryFactory audioDecoderFactoryFactory =
        new BuiltinAudioDecoderFactoryFactory();
    @Nullable private VideoEncoderFactory videoEncoderFactory;
    @Nullable private VideoDecoderFactory videoDecoderFactory;
    @Nullable private AudioProcessingFactory audioProcessingFactory;
    @Nullable private FecControllerFactoryFactoryInterface fecControllerFactoryFactory;
    @Nullable private NetworkControllerFactoryFactory networkControllerFactoryFactory;
    @Nullable private NetworkStatePredictorFactoryFactory networkStatePredictorFactoryFactory;
    @Nullable private NetEqFactoryFactory neteqFactoryFactory;

    private Builder() {}

    public Builder setOptions(Options options) {
      this.options = options;
      return this;
    }

    public Builder setAudioDeviceModule(AudioDeviceModule audioDeviceModule) {
      this.audioDeviceModule = audioDeviceModule;
      return this;
    }

    public Builder setAudioEncoderFactoryFactory(
        AudioEncoderFactoryFactory audioEncoderFactoryFactory) {
      if (audioEncoderFactoryFactory == null) {
        throw new IllegalArgumentException(
            "PeerConnectionFactory.Builder does not accept a null AudioEncoderFactoryFactory.");
      }
      this.audioEncoderFactoryFactory = audioEncoderFactoryFactory;
      return this;
    }

    public Builder setAudioDecoderFactoryFactory(
        AudioDecoderFactoryFactory audioDecoderFactoryFactory) {
      if (audioDecoderFactoryFactory == null) {
        throw new IllegalArgumentException(
            "PeerConnectionFactory.Builder does not accept a null AudioDecoderFactoryFactory.");
      }
      this.audioDecoderFactoryFactory = audioDecoderFactoryFactory;
      return this;
    }

    public Builder setVideoEncoderFactory(VideoEncoderFactory videoEncoderFactory) {
      this.videoEncoderFactory = videoEncoderFactory;
      return this;
    }

    public Builder setVideoDecoderFactory(VideoDecoderFactory videoDecoderFactory) {
      this.videoDecoderFactory = videoDecoderFactory;
      return this;
    }

    public Builder setAudioProcessingFactory(AudioProcessingFactory audioProcessingFactory) {
      if (audioProcessingFactory == null) {
        throw new NullPointerException(
            "PeerConnectionFactory builder does not accept a null AudioProcessingFactory.");
      }
      this.audioProcessingFactory = audioProcessingFactory;
      return this;
    }

    public Builder setFecControllerFactoryFactoryInterface(
        FecControllerFactoryFactoryInterface fecControllerFactoryFactory) {
      this.fecControllerFactoryFactory = fecControllerFactoryFactory;
      return this;
    }

    public Builder setNetworkControllerFactoryFactory(
        NetworkControllerFactoryFactory networkControllerFactoryFactory) {
      this.networkControllerFactoryFactory = networkControllerFactoryFactory;
      return this;
    }

    public Builder setNetworkStatePredictorFactoryFactory(
        NetworkStatePredictorFactoryFactory networkStatePredictorFactoryFactory) {
      this.networkStatePredictorFactoryFactory = networkStatePredictorFactoryFactory;
      return this;
    }

    public Builder setNetEqFactoryFactory(NetEqFactoryFactory neteqFactoryFactory) {
      this.neteqFactoryFactory = neteqFactoryFactory;
      return this;
    }

    public PeerConnectionFactory createPeerConnectionFactory() {
      checkInitializeHasBeenCalled();
      if (audioDeviceModule == null) {
        audioDeviceModule = JavaAudioDeviceModule.builder(ContextUtils.getApplicationContext())
                                .createAudioDeviceModule();
      }
      return nativeCreatePeerConnectionFactory(ContextUtils.getApplicationContext(), options,
          audioDeviceModule.getNativeAudioDeviceModulePointer(),
          audioEncoderFactoryFactory.createNativeAudioEncoderFactory(),
          audioDecoderFactoryFactory.createNativeAudioDecoderFactory(), videoEncoderFactory,
          videoDecoderFactory,
          audioProcessingFactory == null ? 0 : audioProcessingFactory.createNative(),
          fecControllerFactoryFactory == null ? 0 : fecControllerFactoryFactory.createNative(),
          networkControllerFactoryFactory == null
              ? 0
              : networkControllerFactoryFactory.createNativeNetworkControllerFactory(),
          networkStatePredictorFactoryFactory == null
              ? 0
              : networkStatePredictorFactoryFactory.createNativeNetworkStatePredictorFactory(),
          neteqFactoryFactory == null ? 0 : neteqFactoryFactory.createNativeNetEqFactory());
    }
  }

  public static Builder builder() {
    return new Builder();
  }

  public static void initialize(InitializationOptions options) {
    ContextUtils.initialize(options.applicationContext);
    NativeLibrary.initialize(options.nativeLibraryLoader, options.nativeLibraryName);
    nativeInitializeAndroidGlobals();
    nativeInitializeFieldTrials(options.fieldTrials);
    if (options.enableInternalTracer && !internalTracerInitialized) {
      initializeInternalTracer();
    }
    if (options.loggable != null) {
      Logging.injectLoggable(options.loggable, options.loggableSeverity);
      nativeInjectLoggable(new JNILogging(options.loggable), options.loggableSeverity.ordinal());
    } else {
      Logging.d(TAG,
          "PeerConnectionFactory was initialized without an injected Loggable. "
              + "Any existing Loggable will be deleted.");
      Logging.deleteInjectedLoggable();
      nativeDeleteLoggable();
    }
  }

  private static void checkInitializeHasBeenCalled() {
    if (!NativeLibrary.isLoaded() || ContextUtils.getApplicationContext() == null) {
      throw new IllegalStateException(
          "PeerConnectionFactory.initialize was not called before creating a "
          + "PeerConnectionFactory.");
    }
  }

  private static void initializeInternalTracer() {
    internalTracerInitialized = true;
    nativeInitializeInternalTracer();
  }

  public static void shutdownInternalTracer() {
    internalTracerInitialized = false;
    nativeShutdownInternalTracer();
  }

  @Deprecated
  public static void initializeFieldTrials(String fieldTrialsInitString) {
    nativeInitializeFieldTrials(fieldTrialsInitString);
  }

  public static String fieldTrialsFindFullName(String name) {
    return NativeLibrary.isLoaded() ? nativeFindFieldTrialsFullName(name) : "";
  }

  public static boolean startInternalTracingCapture(String tracingFilename) {
    return nativeStartInternalTracingCapture(tracingFilename);
  }

  public static void stopInternalTracingCapture() {
    nativeStopInternalTracingCapture();
  }

  @CalledByNative
  PeerConnectionFactory(long nativeFactory) {
    checkInitializeHasBeenCalled();
    if (nativeFactory == 0) {
      throw new RuntimeException("Failed to initialize PeerConnectionFactory!");
    }
    this.nativeFactory = nativeFactory;
  }

  @Nullable
  PeerConnection createPeerConnectionInternal(PeerConnection.RTCConfiguration rtcConfig,
      MediaConstraints constraints, PeerConnection.Observer observer,
      SSLCertificateVerifier sslCertificateVerifier) {
    checkPeerConnectionFactoryExists();
    long nativeObserver = PeerConnection.createNativePeerConnectionObserver(observer);
    if (nativeObserver == 0) {
      return null;
    }
    long nativePeerConnection = nativeCreatePeerConnection(
        nativeFactory, rtcConfig, constraints, nativeObserver, sslCertificateVerifier);
    if (nativePeerConnection == 0) {
      return null;
    }
    return new PeerConnection(nativePeerConnection);
  }

  @Nullable
  @Deprecated
  public PeerConnection createPeerConnection(PeerConnection.RTCConfiguration rtcConfig,
      MediaConstraints constraints, PeerConnection.Observer observer) {
    return createPeerConnectionInternal(
        rtcConfig, constraints, observer,                               null);
  }

  @Nullable
  @Deprecated
  public PeerConnection createPeerConnection(List<PeerConnection.IceServer> iceServers,
      MediaConstraints constraints, PeerConnection.Observer observer) {
    PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);
    rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;
    return createPeerConnection(rtcConfig, constraints, observer);
  }

  @Nullable
  public PeerConnection createPeerConnection(
      List<PeerConnection.IceServer> iceServers, PeerConnection.Observer observer) {
    PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);
    rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;
    return createPeerConnection(rtcConfig, observer);
  }

  @Nullable
  public PeerConnection createPeerConnection(
      PeerConnection.RTCConfiguration rtcConfig, PeerConnection.Observer observer) {
    return createPeerConnection(rtcConfig, null                  , observer);
  }

  @Nullable
  public PeerConnection createPeerConnection(
      PeerConnection.RTCConfiguration rtcConfig, PeerConnectionDependencies dependencies) {
    return createPeerConnectionInternal(rtcConfig, null                  ,
        dependencies.getObserver(), dependencies.getSSLCertificateVerifier());
  }

  public MediaStream createLocalMediaStream(String label) {
    checkPeerConnectionFactoryExists();
    return new MediaStream(nativeCreateLocalMediaStream(nativeFactory, label));
  }

  public VideoSource createVideoSource(boolean isScreencast, boolean alignTimestamps) {
    checkPeerConnectionFactoryExists();
    return new VideoSource(nativeCreateVideoSource(nativeFactory, isScreencast, alignTimestamps));
  }

  public VideoSource createVideoSource(boolean isScreencast) {
    return createVideoSource(isScreencast,                        true);
  }

  public VideoTrack createVideoTrack(String id, VideoSource source) {
    checkPeerConnectionFactoryExists();
    return new VideoTrack(
        nativeCreateVideoTrack(nativeFactory, id, source.getNativeVideoTrackSource()));
  }

  public AudioSource createAudioSource(MediaConstraints constraints) {
    checkPeerConnectionFactoryExists();
    return new AudioSource(nativeCreateAudioSource(nativeFactory, constraints));
  }

  public AudioTrack createAudioTrack(String id, AudioSource source) {
    checkPeerConnectionFactoryExists();
    return new AudioTrack(nativeCreateAudioTrack(nativeFactory, id, source.getNativeAudioSource()));
  }

  public RtpCapabilities getRtpReceiverCapabilities(MediaStreamTrack.MediaType mediaType) {
    checkPeerConnectionFactoryExists();
    return nativeGetRtpReceiverCapabilities(nativeFactory, mediaType);
  }

  public RtpCapabilities getRtpSenderCapabilities(MediaStreamTrack.MediaType mediaType) {
    checkPeerConnectionFactoryExists();
    return nativeGetRtpSenderCapabilities(nativeFactory, mediaType);
  }

  public boolean startAecDump(int file_descriptor, int filesize_limit_bytes) {
    checkPeerConnectionFactoryExists();
    return nativeStartAecDump(nativeFactory, file_descriptor, filesize_limit_bytes);
  }

  public void stopAecDump() {
    checkPeerConnectionFactoryExists();
    nativeStopAecDump(nativeFactory);
  }

  public void dispose() {
    checkPeerConnectionFactoryExists();
    nativeFreeFactory(nativeFactory);
    networkThread = null;
    workerThread = null;
    signalingThread = null;
    nativeFactory = 0;
  }

  public long getNativePeerConnectionFactory() {
    checkPeerConnectionFactoryExists();
    return nativeGetNativePeerConnectionFactory(nativeFactory);
  }

  public long getNativeOwnedFactoryAndThreads() {
    checkPeerConnectionFactoryExists();
    return nativeFactory;
  }

  private void checkPeerConnectionFactoryExists() {
    if (nativeFactory == 0) {
      throw new IllegalStateException("PeerConnectionFactory has been disposed.");
    }
  }

  private static void printStackTrace(
      @Nullable ThreadInfo threadInfo, boolean printNativeStackTrace) {
    if (threadInfo == null) {

      return;
    }
    final String threadName = threadInfo.thread.getName();
    StackTraceElement[] stackTraces = threadInfo.thread.getStackTrace();
    if (stackTraces.length > 0) {
      Logging.w(TAG, threadName + " stacktrace:");
      for (StackTraceElement stackTrace : stackTraces) {
        Logging.w(TAG, stackTrace.toString());
      }
    }
    if (printNativeStackTrace) {

      Logging.w(TAG, "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***");
      Logging.w(TAG,
          "pid: " + Process.myPid() + ", tid: " + threadInfo.tid + ", name: " + threadName
              + "  >>> WebRTC <<<");
      nativePrintStackTrace(threadInfo.tid);
    }
  }

  @Deprecated
  public static void printStackTraces() {
    printStackTrace(staticNetworkThread,                              false);
    printStackTrace(staticWorkerThread,                              false);
    printStackTrace(staticSignalingThread,                              false);
  }

  public void printInternalStackTraces(boolean printNativeStackTraces) {
    printStackTrace(signalingThread, printNativeStackTraces);
    printStackTrace(workerThread, printNativeStackTraces);
    printStackTrace(networkThread, printNativeStackTraces);
  }

  @CalledByNative
  private void onNetworkThreadReady() {
    networkThread = ThreadInfo.getCurrent();
    staticNetworkThread = networkThread;
    Logging.d(TAG, "onNetworkThreadReady");
  }

  @CalledByNative
  private void onWorkerThreadReady() {
    workerThread = ThreadInfo.getCurrent();
    staticWorkerThread = workerThread;
    Logging.d(TAG, "onWorkerThreadReady");
  }

  @CalledByNative
  private void onSignalingThreadReady() {
    signalingThread = ThreadInfo.getCurrent();
    staticSignalingThread = signalingThread;
    Logging.d(TAG, "onSignalingThreadReady");
  }

  private static native void nativeInitializeAndroidGlobals();
  private static native void nativeInitializeFieldTrials(String fieldTrialsInitString);
  private static native String nativeFindFieldTrialsFullName(String name);
  private static native void nativeInitializeInternalTracer();

  private static native void nativeShutdownInternalTracer();
  private static native boolean nativeStartInternalTracingCapture(String tracingFilename);
  private static native void nativeStopInternalTracingCapture();

  private static native PeerConnectionFactory nativeCreatePeerConnectionFactory(Context context,
      Options options, long nativeAudioDeviceModule, long audioEncoderFactory,
      long audioDecoderFactory, VideoEncoderFactory encoderFactory,
      VideoDecoderFactory decoderFactory, long nativeAudioProcessor,
      long nativeFecControllerFactory, long nativeNetworkControllerFactory,
      long nativeNetworkStatePredictorFactory, long neteqFactory);

  private static native long nativeCreatePeerConnection(long factory,
      PeerConnection.RTCConfiguration rtcConfig, MediaConstraints constraints, long nativeObserver,
      SSLCertificateVerifier sslCertificateVerifier);
  private static native long nativeCreateLocalMediaStream(long factory, String label);
  private static native long nativeCreateVideoSource(
      long factory, boolean is_screencast, boolean alignTimestamps);
  private static native long nativeCreateVideoTrack(
      long factory, String id, long nativeVideoSource);
  private static native long nativeCreateAudioSource(long factory, MediaConstraints constraints);
  private static native long nativeCreateAudioTrack(long factory, String id, long nativeSource);
  private static native boolean nativeStartAecDump(
      long factory, int file_descriptor, int filesize_limit_bytes);
  private static native void nativeStopAecDump(long factory);
  private static native void nativeFreeFactory(long factory);
  private static native long nativeGetNativePeerConnectionFactory(long factory);
  private static native void nativeInjectLoggable(JNILogging jniLogging, int severity);
  private static native void nativeDeleteLoggable();
  private static native void nativePrintStackTrace(int tid);
  private static native RtpCapabilities nativeGetRtpSenderCapabilities(
      long factory, MediaStreamTrack.MediaType mediaType);
  private static native RtpCapabilities nativeGetRtpReceiverCapabilities(
      long factory, MediaStreamTrack.MediaType mediaType);
}
