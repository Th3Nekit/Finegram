/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Process;
import androidx.annotation.Nullable;
import java.nio.ByteBuffer;

import org.telegram.messenger.FileLog;
import org.webrtc.CalledByNative;
import org.webrtc.Logging;
import org.webrtc.ThreadUtils;
import org.webrtc.audio.JavaAudioDeviceModule.AudioTrackErrorCallback;
import org.webrtc.audio.JavaAudioDeviceModule.AudioTrackStartErrorCode;
import org.webrtc.audio.JavaAudioDeviceModule.AudioTrackStateCallback;
import org.webrtc.audio.LowLatencyAudioBufferManager;

class WebRtcAudioTrack {
  private static final String TAG = "WebRtcAudioTrackExternal";

  private static final int BITS_PER_SAMPLE = 16;

  private static final int CALLBACK_BUFFER_SIZE_MS = 10;

  private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

  private static final long AUDIO_TRACK_THREAD_JOIN_TIMEOUT_MS = 2000;

  private static final int DEFAULT_USAGE = getDefaultUsageAttribute();

  private static int getDefaultUsageAttribute() {
    if (Build.VERSION.SDK_INT >= 21) {
      return AudioAttributes.USAGE_VOICE_COMMUNICATION;
    } else {

      return 0;
    }
  }

  private static final int AUDIO_TRACK_START = 0;

  private static final int AUDIO_TRACK_STOP = 1;

  private long nativeAudioTrack;
  private final Context context;
  private final AudioManager audioManager;
  private final ThreadUtils.ThreadChecker threadChecker = new ThreadUtils.ThreadChecker();

  private ByteBuffer byteBuffer;

  private @Nullable final AudioAttributes audioAttributes;
  private @Nullable AudioTrack audioTrack;
  private @Nullable AudioTrackThread audioThread;
  private final VolumeLogger volumeLogger;

  private volatile boolean speakerMute;
  private byte[] emptyBytes;
  private boolean useLowLatency;
  private int initialBufferSizeInFrames;

  private final @Nullable AudioTrackErrorCallback errorCallback;
  private final @Nullable AudioTrackStateCallback stateCallback;

  private class AudioTrackThread extends Thread {
    private volatile boolean keepAlive = true;
    private LowLatencyAudioBufferManager bufferManager;

    private long writtenFrames = 0;
    private long lastPlaybackHeadPosition = 0;
    private long lastTimestamp = System.nanoTime();
    private long targetTimeNs;

    public AudioTrackThread(String name) {
      super(name);
      bufferManager = new LowLatencyAudioBufferManager();
    }

    @Override
    public void run() {
      Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
      Logging.d(TAG, "AudioTrackThread" + WebRtcAudioUtils.getThreadInfo());
      assertTrue(audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING);

      doAudioTrackStateCallback(AUDIO_TRACK_START);

      final int sizeInBytes = byteBuffer.capacity();
      final int bytesPerFrame = audioTrack.getChannelCount() * (BITS_PER_SAMPLE / 8);
      final int sampleRate = audioTrack.getSampleRate();

      targetTimeNs = System.nanoTime();

      while (keepAlive) {

        nativeGetPlayoutData(nativeAudioTrack, sizeInBytes);

        assertTrue(sizeInBytes <= byteBuffer.remaining());
        if (speakerMute) {
          byteBuffer.clear();
          byteBuffer.put(emptyBytes);
          byteBuffer.position(0);
        }
        int bytesWritten = writeBytes(audioTrack, byteBuffer, sizeInBytes);
        if (bytesWritten != sizeInBytes) {
          Logging.e(TAG, "AudioTrack.write played invalid number of bytes: " + bytesWritten);

          if (bytesWritten < 0) {
            keepAlive = false;
            reportWebRtcAudioTrackError("AudioTrack.write failed: " + bytesWritten);
          }
        }
        if (useLowLatency) {
          bufferManager.maybeAdjustBufferSize(audioTrack);
        }

        byteBuffer.rewind();

        writtenFrames += bytesWritten / bytesPerFrame;

        long playbackHeadPosition = audioTrack.getPlaybackHeadPosition();
        long delayInFrames = writtenFrames - playbackHeadPosition;
        long delayInMs = (delayInFrames * 1000) / sampleRate;

        byteBuffer.rewind();

        targetTimeNs += CALLBACK_BUFFER_SIZE_MS * 1_000_000L;
        long currentTimeNs = System.nanoTime();
        long sleepTimeNs = targetTimeNs - currentTimeNs;
        if (sleepTimeNs > 0) {
          try {
            Thread.sleep(sleepTimeNs / 1_000_000L, (int) (sleepTimeNs % 1_000_000L));
          } catch (InterruptedException e) {
            FileLog.e(e);
          }
        } else {

          targetTimeNs = System.nanoTime();
        }
      }
    }

    private int writeBytes(AudioTrack audioTrack, ByteBuffer byteBuffer, int sizeInBytes) {
      if (Build.VERSION.SDK_INT >= 21) {
        return audioTrack.write(byteBuffer, sizeInBytes, AudioTrack.WRITE_BLOCKING);
      } else {
        return audioTrack.write(byteBuffer.array(), byteBuffer.arrayOffset(), sizeInBytes);
      }
    }

    public void stopThread() {
      Logging.d(TAG, "stopThread");
      keepAlive = false;
    }
  }

  @CalledByNative
  WebRtcAudioTrack(Context context, AudioManager audioManager) {
    this(context, audioManager, null                      , null                    ,
        null                    , false                    , true                         );
  }

  WebRtcAudioTrack(Context context, AudioManager audioManager,
      @Nullable AudioAttributes audioAttributes, @Nullable AudioTrackErrorCallback errorCallback,
      @Nullable AudioTrackStateCallback stateCallback, boolean useLowLatency,
      boolean enableVolumeLogger) {
    threadChecker.detachThread();
    this.context = context;
    this.audioManager = audioManager;
    this.audioAttributes = audioAttributes;
    this.errorCallback = errorCallback;
    this.stateCallback = stateCallback;
    this.volumeLogger = enableVolumeLogger ? new VolumeLogger(audioManager) : null;
    this.useLowLatency = useLowLatency;
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
  }

  @CalledByNative
  public void setNativeAudioTrack(long nativeAudioTrack) {
    this.nativeAudioTrack = nativeAudioTrack;
  }

  @CalledByNative
  private int initPlayout(int sampleRate, int channels, double bufferSizeFactor) {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG,
        "initPlayout(sampleRate=" + sampleRate + ", channels=" + channels
            + ", bufferSizeFactor=" + bufferSizeFactor + ")");
    final int bytesPerFrame = channels * (BITS_PER_SAMPLE / 8);
    byteBuffer = ByteBuffer.allocateDirect(bytesPerFrame * (sampleRate / BUFFERS_PER_SECOND));
    Logging.d(TAG, "byteBuffer.capacity: " + byteBuffer.capacity());
    emptyBytes = new byte[byteBuffer.capacity()];

    nativeCacheDirectBufferAddress(nativeAudioTrack, byteBuffer);

    final int channelConfig = channelCountToConfiguration(channels);
    final int minBufferSizeInBytes = (int) (AudioTrack.getMinBufferSize(sampleRate, channelConfig,
                                                AudioFormat.ENCODING_PCM_16BIT)
        * bufferSizeFactor);
    Logging.d(TAG, "minBufferSizeInBytes: " + minBufferSizeInBytes);

    if (minBufferSizeInBytes < byteBuffer.capacity()) {
      reportWebRtcAudioTrackInitError("AudioTrack.getMinBufferSize returns an invalid value.");
      return -1;
    }

    if (bufferSizeFactor > 1.0) {
      useLowLatency = false;
    }

    if (audioTrack != null) {
      reportWebRtcAudioTrackInitError("Conflict with existing AudioTrack.");
      return -1;
    }
    try {

      if (useLowLatency && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {

        audioTrack = createAudioTrackOnOreoOrHigher(
            sampleRate, channelConfig, minBufferSizeInBytes, audioAttributes);
      } else {

        audioTrack = createAudioTrackBeforeOreo(
            sampleRate, channelConfig, minBufferSizeInBytes, audioAttributes);
      }
    } catch (IllegalArgumentException e) {
      reportWebRtcAudioTrackInitError(e.getMessage());
      releaseAudioResources();
      return -1;
    }

    if (audioTrack == null || audioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
      reportWebRtcAudioTrackInitError("Initialization of audio track failed.");
      releaseAudioResources();
      return -1;
    }
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      initialBufferSizeInFrames = audioTrack.getBufferSizeInFrames();
    } else {
      initialBufferSizeInFrames = -1;
    }
    logMainParameters();
    logMainParametersExtended();
    return minBufferSizeInBytes;
  }

  @CalledByNative
  private boolean startPlayout() {
    threadChecker.checkIsOnValidThread();
    if (volumeLogger != null) {
      volumeLogger.start();
    }
    Logging.d(TAG, "startPlayout");
    assertTrue(audioTrack != null);
    assertTrue(audioThread == null);

    try {
      audioTrack.play();
    } catch (IllegalStateException e) {
      reportWebRtcAudioTrackStartError(AudioTrackStartErrorCode.AUDIO_TRACK_START_EXCEPTION,
          "AudioTrack.play failed: " + e.getMessage());
      releaseAudioResources();
      return false;
    }
    if (audioTrack.getPlayState() != AudioTrack.PLAYSTATE_PLAYING) {
      reportWebRtcAudioTrackStartError(AudioTrackStartErrorCode.AUDIO_TRACK_START_STATE_MISMATCH,
          "AudioTrack.play failed - incorrect state :" + audioTrack.getPlayState());
      releaseAudioResources();
      return false;
    }

    audioThread = new AudioTrackThread("AudioTrackJavaThread");
    audioThread.start();
    return true;
  }

  @CalledByNative
  private boolean stopPlayout() {
    threadChecker.checkIsOnValidThread();
    if (volumeLogger != null) {
      volumeLogger.stop();
    }
    Logging.d(TAG, "stopPlayout");
    assertTrue(audioThread != null);
    logUnderrunCount();
    audioThread.stopThread();

    Logging.d(TAG, "Stopping the AudioTrackThread...");
    audioThread.interrupt();
    if (!ThreadUtils.joinUninterruptibly(audioThread, AUDIO_TRACK_THREAD_JOIN_TIMEOUT_MS)) {
      Logging.e(TAG, "Join of AudioTrackThread timed out.");
      WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    }
    Logging.d(TAG, "AudioTrackThread has now been stopped.");
    audioThread = null;
    if (audioTrack != null) {
      Logging.d(TAG, "Calling AudioTrack.stop...");
      try {
        audioTrack.stop();
        Logging.d(TAG, "AudioTrack.stop is done.");
        doAudioTrackStateCallback(AUDIO_TRACK_STOP);
      } catch (IllegalStateException e) {
        Logging.e(TAG, "AudioTrack.stop failed: " + e.getMessage());
      }
    }
    releaseAudioResources();
    return true;
  }

  @CalledByNative
  private int getStreamMaxVolume() {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG, "getStreamMaxVolume");
    return audioManager.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL);
  }

  @CalledByNative
  private boolean setStreamVolume(int volume) {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG, "setStreamVolume(" + volume + ")");
    if (audioManager.isVolumeFixed()) {
      Logging.e(TAG, "The device implements a fixed volume policy.");
      return false;
    }
    audioManager.setStreamVolume(AudioManager.STREAM_VOICE_CALL, volume, 0);
    return true;
  }

  @CalledByNative
  private int getStreamVolume() {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG, "getStreamVolume");
    return audioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
  }

  @CalledByNative
  private int GetPlayoutUnderrunCount() {
    if (Build.VERSION.SDK_INT >= 24) {
      if (audioTrack != null) {
        return audioTrack.getUnderrunCount();
      } else {
        return -1;
      }
    } else {
      return -2;
    }
  }

  private void logMainParameters() {
    Logging.d(TAG,
        "AudioTrack: "
            + "session ID: " + audioTrack.getAudioSessionId() + ", "
            + "channels: " + audioTrack.getChannelCount() + ", "
            + "sample rate: " + audioTrack.getSampleRate()
            + ", "

            + "max gain: " + AudioTrack.getMaxVolume());
  }

  private static void logNativeOutputSampleRate(int requestedSampleRateInHz) {
    final int nativeOutputSampleRate =
        AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_VOICE_CALL);
    Logging.d(TAG, "nativeOutputSampleRate: " + nativeOutputSampleRate);
    if (requestedSampleRateInHz != nativeOutputSampleRate) {
      Logging.w(TAG, "Unable to use fast mode since requested sample rate is not native");
    }
  }

  @TargetApi(Build.VERSION_CODES.LOLLIPOP)
  private static AudioAttributes getAudioAttributes(@Nullable AudioAttributes overrideAttributes) {
    AudioAttributes.Builder attributesBuilder =
        new AudioAttributes.Builder()
            .setUsage(DEFAULT_USAGE)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH);

    if (overrideAttributes != null) {
      if (overrideAttributes.getUsage() != AudioAttributes.USAGE_UNKNOWN) {
        attributesBuilder.setUsage(overrideAttributes.getUsage());
      }
      if (overrideAttributes.getContentType() != AudioAttributes.CONTENT_TYPE_UNKNOWN) {
        attributesBuilder.setContentType(overrideAttributes.getContentType());
      }

      attributesBuilder.setFlags(overrideAttributes.getFlags());

      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        attributesBuilder = applyAttributesOnQOrHigher(attributesBuilder, overrideAttributes);
      }
    }
    return attributesBuilder.build();
  }

  @TargetApi(Build.VERSION_CODES.LOLLIPOP)
  private static AudioTrack createAudioTrackBeforeOreo(int sampleRateInHz, int channelConfig,
      int bufferSizeInBytes, @Nullable AudioAttributes overrideAttributes) {
    Logging.d(TAG, "createAudioTrackBeforeOreo");
    logNativeOutputSampleRate(sampleRateInHz);

    return new AudioTrack(getAudioAttributes(overrideAttributes),
        new AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setSampleRate(sampleRateInHz)
            .setChannelMask(channelConfig)
            .build(),
        bufferSizeInBytes, AudioTrack.MODE_STREAM, AudioManager.AUDIO_SESSION_ID_GENERATE);
  }

  @TargetApi(Build.VERSION_CODES.O)
  private static AudioTrack createAudioTrackOnOreoOrHigher(int sampleRateInHz, int channelConfig,
      int bufferSizeInBytes, @Nullable AudioAttributes overrideAttributes) {
    Logging.d(TAG, "createAudioTrackOnOreoOrHigher");
    logNativeOutputSampleRate(sampleRateInHz);

    return new AudioTrack.Builder()
        .setAudioAttributes(getAudioAttributes(overrideAttributes))
        .setAudioFormat(new AudioFormat.Builder()
                            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                            .setSampleRate(sampleRateInHz)
                            .setChannelMask(channelConfig)
                            .build())
        .setBufferSizeInBytes(bufferSizeInBytes)
        .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
        .setTransferMode(AudioTrack.MODE_STREAM)
        .setSessionId(AudioManager.AUDIO_SESSION_ID_GENERATE)
        .build();
  }

  @TargetApi(Build.VERSION_CODES.Q)
  private static AudioAttributes.Builder applyAttributesOnQOrHigher(
      AudioAttributes.Builder builder, AudioAttributes overrideAttributes) {
    return builder.setAllowedCapturePolicy(overrideAttributes.getAllowedCapturePolicy());
  }

  private void logBufferSizeInFrames() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      Logging.d(TAG,
          "AudioTrack: "

              + "buffer size in frames: " + audioTrack.getBufferSizeInFrames());
    }
  }

  @CalledByNative
  private int getBufferSizeInFrames() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      return audioTrack.getBufferSizeInFrames();
    }
    return -1;
  }

  @CalledByNative
  private int getInitialBufferSizeInFrames() {
    return initialBufferSizeInFrames;
  }

  private void logBufferCapacityInFrames() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
      Logging.d(TAG,
          "AudioTrack: "

              + "buffer capacity in frames: " + audioTrack.getBufferCapacityInFrames());
    }
  }

  private void logMainParametersExtended() {
    logBufferSizeInFrames();
    logBufferCapacityInFrames();
  }

  private void logUnderrunCount() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
      Logging.d(TAG, "underrun count: " + audioTrack.getUnderrunCount());
    }
  }

  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private int channelCountToConfiguration(int channels) {
    return (channels == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO);
  }

  private static native void nativeCacheDirectBufferAddress(
      long nativeAudioTrackJni, ByteBuffer byteBuffer);
  private static native void nativeGetPlayoutData(long nativeAudioTrackJni, int bytes);

  public void setSpeakerMute(boolean mute) {
    Logging.w(TAG, "setSpeakerMute(" + mute + ")");
    speakerMute = mute;
  }

  private void releaseAudioResources() {
    Logging.d(TAG, "releaseAudioResources");
    if (audioTrack != null) {
      audioTrack.release();
      audioTrack = null;
    }
  }

  private void reportWebRtcAudioTrackInitError(String errorMessage) {
    Logging.e(TAG, "Init playout error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackInitError(errorMessage);
    }
  }

  private void reportWebRtcAudioTrackStartError(
      AudioTrackStartErrorCode errorCode, String errorMessage) {
    Logging.e(TAG, "Start playout error: " + errorCode + ". " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackStartError(errorCode, errorMessage);
    }
  }

  private void reportWebRtcAudioTrackError(String errorMessage) {
    Logging.e(TAG, "Run-time playback error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackError(errorMessage);
    }
  }

  private void doAudioTrackStateCallback(int audioState) {
    Logging.d(TAG, "doAudioTrackStateCallback: " + audioState);
    if (stateCallback != null) {
      if (audioState == WebRtcAudioTrack.AUDIO_TRACK_START) {
        stateCallback.onWebRtcAudioTrackStart();
      } else if (audioState == WebRtcAudioTrack.AUDIO_TRACK_STOP) {
        stateCallback.onWebRtcAudioTrackStop();
      } else {
        Logging.e(TAG, "Invalid audio state");
      }
    }
  }
}
