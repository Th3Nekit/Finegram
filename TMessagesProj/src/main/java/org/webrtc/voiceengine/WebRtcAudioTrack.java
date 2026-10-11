/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Process;
import android.util.Log;

import androidx.annotation.Nullable;

import org.telegram.messenger.FileLog;
import org.webrtc.ContextUtils;
import org.webrtc.Logging;
import org.webrtc.ThreadUtils;

import java.nio.ByteBuffer;

public class WebRtcAudioTrack {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioTrack";

  private static final int BITS_PER_SAMPLE = 16;

  private static final int CALLBACK_BUFFER_SIZE_MS = 10;

  private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

  private static final long AUDIO_TRACK_THREAD_JOIN_TIMEOUT_MS = 2000;

  private static final int DEFAULT_USAGE = getDefaultUsageAttribute();
  private static int usageAttribute = DEFAULT_USAGE;
  private static int streamType = AudioManager.STREAM_VOICE_CALL;

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized void setAudioTrackUsageAttribute(int usage) {
    Logging.w(TAG, "Default usage attribute is changed from: "
        + DEFAULT_USAGE + " to " + usage);
    usageAttribute = usage;
  }

  public static synchronized void setAudioStreamType(int type) {
    streamType = type;
  }

  private static int getDefaultUsageAttribute() {
    if (Build.VERSION.SDK_INT >= 21) {
      return AudioAttributes.USAGE_VOICE_COMMUNICATION;
    } else {

      return 0;
    }
  }

  private final long nativeAudioTrack;
  private final AudioManager audioManager;
  private final ThreadUtils.ThreadChecker threadChecker = new ThreadUtils.ThreadChecker();

  private ByteBuffer byteBuffer;

  private @Nullable AudioTrack audioTrack;
  private @Nullable AudioTrackThread audioThread;

  private static volatile boolean speakerMute;
  private byte[] emptyBytes;

  public enum AudioTrackStartErrorCode {
    AUDIO_TRACK_START_EXCEPTION,
    AUDIO_TRACK_START_STATE_MISMATCH,
  }

  @Deprecated
  public static interface WebRtcAudioTrackErrorCallback {
    void onWebRtcAudioTrackInitError(String errorMessage);
    void onWebRtcAudioTrackStartError(String errorMessage);
    void onWebRtcAudioTrackError(String errorMessage);
  }

  public static interface ErrorCallback {
    void onWebRtcAudioTrackInitError(String errorMessage);
    void onWebRtcAudioTrackStartError(AudioTrackStartErrorCode errorCode, String errorMessage);
    void onWebRtcAudioTrackError(String errorMessage);
  }

  private static @Nullable WebRtcAudioTrackErrorCallback errorCallbackOld;
  private static @Nullable ErrorCallback errorCallback;

  @Deprecated
  public static void setErrorCallback(WebRtcAudioTrackErrorCallback errorCallback) {
    Logging.d(TAG, "Set error callback (deprecated");
    WebRtcAudioTrack.errorCallbackOld = errorCallback;
  }

  public static void setErrorCallback(ErrorCallback errorCallback) {
    Logging.d(TAG, "Set extended error callback");
    WebRtcAudioTrack.errorCallback = errorCallback;
  }

  private class AudioTrackThread extends Thread {
    private volatile boolean keepAlive = true;

    private long writtenFrames = 0;
    private long lastPlaybackHeadPosition = 0;
    private long lastTimestamp = System.nanoTime();
    private long targetTimeNs;

    public AudioTrackThread(String name) {
      super(name);
    }

    @Override
    public void run() {
      Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
      Logging.d(TAG, "AudioTrackThread" + WebRtcAudioUtils.getThreadInfo());
      assertTrue(audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING);

      final int sizeInBytes = byteBuffer.capacity();
      final int bytesPerFrame = audioTrack.getChannelCount() * (BITS_PER_SAMPLE / 8);
      final int sampleRate = audioTrack.getSampleRate();

      targetTimeNs = System.nanoTime();
      boolean blocking = false;

      while (keepAlive && audioTrack != null) {

        try {
          nativeGetPlayoutData(sizeInBytes, nativeAudioTrack);
        } catch (Throwable e) {
          keepAlive = false;
          continue;
        }

        assertTrue(sizeInBytes <= byteBuffer.remaining());
        if (speakerMute) {
          byteBuffer.clear();
          byteBuffer.put(emptyBytes);
          byteBuffer.position(0);
        }
        int bytesWritten = writeBytes(audioTrack, byteBuffer, sizeInBytes, blocking);
        if (bytesWritten != sizeInBytes) {
          Logging.e(TAG, "AudioTrack.write played invalid number of bytes: " + bytesWritten);

          if (bytesWritten < 0) {
            keepAlive = false;
            reportWebRtcAudioTrackError("AudioTrack.write failed: " + bytesWritten);
          }
        }

        byteBuffer.rewind();
        blocking = !blocking;

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

      if (audioTrack != null) {
        Logging.d(TAG, "Calling AudioTrack.stop...");
        try {
          audioTrack.stop();
          Logging.d(TAG, "AudioTrack.stop is done.");
        } catch (Exception e) {
          Logging.e(TAG, "AudioTrack.stop failed: " + e.getMessage());
        }
      }
    }

    private int writeBytes(AudioTrack audioTrack, ByteBuffer byteBuffer, int sizeInBytes, boolean blocking) {
      if (audioTrack == null) return 0;
      if (Build.VERSION.SDK_INT >= 21) {
        return audioTrack.write(byteBuffer, sizeInBytes, blocking ? AudioTrack.WRITE_BLOCKING : AudioTrack.WRITE_NON_BLOCKING);
      } else {
        return audioTrack.write(byteBuffer.array(), byteBuffer.arrayOffset(), sizeInBytes);
      }
    }

    public void stopThread() {
      Logging.d(TAG, "stopThread");
      keepAlive = false;
    }
  }

  WebRtcAudioTrack(long nativeAudioTrack) {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
    this.nativeAudioTrack = nativeAudioTrack;
    audioManager =
        (AudioManager) ContextUtils.getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
    if (DEBUG) {
      WebRtcAudioUtils.logDeviceInfo(TAG);
    }
  }

  private int initPlayout(int sampleRate, int channels, double bufferSizeFactor) {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG,
        "initPlayout(sampleRate=" + sampleRate + ", channels=" + channels
            + ", bufferSizeFactor=" + bufferSizeFactor + ")");
    final int bytesPerFrame = channels * (BITS_PER_SAMPLE / 8);
    byteBuffer = ByteBuffer.allocateDirect(bytesPerFrame * (sampleRate / BUFFERS_PER_SECOND));
    Logging.d(TAG, "byteBuffer.capacity: " + byteBuffer.capacity());
    emptyBytes = new byte[byteBuffer.capacity()];

    nativeCacheDirectBufferAddress(byteBuffer, nativeAudioTrack);

    final int channelConfig = channelCountToConfiguration(channels);
    final int minBufferSizeInBytes = (int) (AudioTrack.getMinBufferSize(sampleRate, channelConfig,
                                                AudioFormat.ENCODING_PCM_16BIT)
        * bufferSizeFactor);
    Logging.d(TAG, "minBufferSizeInBytes: " + minBufferSizeInBytes);

    if (minBufferSizeInBytes < byteBuffer.capacity()) {
      reportWebRtcAudioTrackInitError("AudioTrack.getMinBufferSize returns an invalid value.");
      return -1;
    }

    if (audioTrack != null) {
      reportWebRtcAudioTrackInitError("Conflict with existing AudioTrack.");
      return -1;
    }
    try {

      if (Build.VERSION.SDK_INT >= 21) {

        audioTrack = createAudioTrackOnLollipopOrHigher(
            sampleRate, channelConfig, minBufferSizeInBytes);
      } else {

        audioTrack =
            createAudioTrackOnLowerThanLollipop(sampleRate, channelConfig, minBufferSizeInBytes);
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
    logMainParameters();
    logMainParametersExtended();
    return minBufferSizeInBytes;
  }

  private boolean startPlayout() {
    threadChecker.checkIsOnValidThread();
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
      reportWebRtcAudioTrackStartError(
          AudioTrackStartErrorCode.AUDIO_TRACK_START_STATE_MISMATCH,
          "AudioTrack.play failed - incorrect state :"
          + audioTrack.getPlayState());
      releaseAudioResources();
      return false;
    }

    audioThread = new AudioTrackThread("AudioTrackJavaThread");
    audioThread.start();
    return true;
  }

  private boolean stopPlayout() {
    try {
      threadChecker.checkIsOnValidThread();
      Logging.d(TAG, "stopPlayout");
      assertTrue(audioThread != null);
      logUnderrunCount();
      audioThread.stopThread();

      Logging.d(TAG, "Stopping the AudioTrackThread...");
      audioThread.interrupt();
      if (!ThreadUtils.joinUninterruptibly(audioThread, AUDIO_TRACK_THREAD_JOIN_TIMEOUT_MS)) {
        Logging.e(TAG, "Join of AudioTrackThread timed out.");
        WebRtcAudioUtils.logAudioState(TAG);
      }
      Logging.d(TAG, "AudioTrackThread has now been stopped.");
    } catch (Throwable e) {
      FileLog.e(e);
    } finally {
      audioThread = null;
    }
    try {
      releaseAudioResources();
    } catch (Throwable e) {
      FileLog.e(e);
    }
    return true;
  }

  private int getStreamMaxVolume() {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG, "getStreamMaxVolume");
    assertTrue(audioManager != null);
    return audioManager.getStreamMaxVolume(streamType);
  }

  private boolean setStreamVolume(int volume) {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG, "setStreamVolume(" + volume + ")");
    assertTrue(audioManager != null);
    if (isVolumeFixed()) {
      Logging.e(TAG, "The device implements a fixed volume policy.");
      return false;
    }
    audioManager.setStreamVolume(streamType, volume, 0);
    return true;
  }

  private boolean isVolumeFixed() {
    if (Build.VERSION.SDK_INT < 21)
      return false;
    return audioManager.isVolumeFixed();
  }

  private int getStreamVolume() {
    threadChecker.checkIsOnValidThread();
    Logging.d(TAG, "getStreamVolume");
    assertTrue(audioManager != null);
    return audioManager.getStreamVolume(streamType);
  }

  private void logMainParameters() {
    Logging.d(TAG, "AudioTrack: "
            + "session ID: " + audioTrack.getAudioSessionId() + ", "
            + "channels: " + audioTrack.getChannelCount() + ", "
            + "sample rate: " + audioTrack.getSampleRate() + ", "

            + "max gain: " + AudioTrack.getMaxVolume());
  }

  @TargetApi(21)
  private static AudioTrack createAudioTrackOnLollipopOrHigher(
      int sampleRateInHz, int channelConfig, int bufferSizeInBytes) {
    Logging.d(TAG, "createAudioTrackOnLollipopOrHigher");

    final int nativeOutputSampleRate =
        AudioTrack.getNativeOutputSampleRate(streamType);
    Logging.d(TAG, "nativeOutputSampleRate: " + nativeOutputSampleRate);
    if (sampleRateInHz != nativeOutputSampleRate) {
      Logging.w(TAG, "Unable to use fast mode since requested sample rate is not native");
    }
    if (usageAttribute != DEFAULT_USAGE) {
      Logging.w(TAG, "A non default usage attribute is used: " + usageAttribute);
    }

    return new AudioTrack(
        new AudioAttributes.Builder()
            .setUsage(usageAttribute)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
        .build(),
        new AudioFormat.Builder()
          .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
          .setSampleRate(sampleRateInHz)
          .setChannelMask(channelConfig)
          .build(),
        bufferSizeInBytes,
        AudioTrack.MODE_STREAM,
        AudioManager.AUDIO_SESSION_ID_GENERATE);
  }

  @SuppressWarnings("deprecation")
  private static AudioTrack createAudioTrackOnLowerThanLollipop(
      int sampleRateInHz, int channelConfig, int bufferSizeInBytes) {
    return new AudioTrack(streamType, sampleRateInHz, channelConfig,
        AudioFormat.ENCODING_PCM_16BIT, bufferSizeInBytes, AudioTrack.MODE_STREAM);
  }

  private void logBufferSizeInFrames() {
    if (Build.VERSION.SDK_INT >= 23) {
      Logging.d(TAG, "AudioTrack: "

              + "buffer size in frames: " + audioTrack.getBufferSizeInFrames());
    }
  }

  private int getBufferSizeInFrames() {
    if (Build.VERSION.SDK_INT >= 23) {
      return audioTrack.getBufferSizeInFrames();
    }
    return -1;
  }

  private void logBufferCapacityInFrames() {
    if (Build.VERSION.SDK_INT >= 24) {
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
    if (Build.VERSION.SDK_INT >= 24) {
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

  private native void nativeCacheDirectBufferAddress(ByteBuffer byteBuffer, long nativeAudioRecord);

  private native void nativeGetPlayoutData(int bytes, long nativeAudioRecord);

  public static void setSpeakerMute(boolean mute) {
    Logging.w(TAG, "setSpeakerMute(" + mute + ")");
    speakerMute = mute;
  }

  public static boolean isSpeakerMuted() {
    return speakerMute;
  }

  private void releaseAudioResources() {
    Logging.e(TAG, "releaseAudioResources", new Exception());
    if (audioTrack != null) {
      AudioTrack track = audioTrack;
      audioTrack = null;
      try {
        track.release();
      } catch (Throwable e) {
        FileLog.e(e);
      }
    }
  }

  private void reportWebRtcAudioTrackInitError(String errorMessage) {
    Logging.e(TAG, "Init playout error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG);
    if (errorCallbackOld != null) {
      errorCallbackOld.onWebRtcAudioTrackInitError(errorMessage);
    }
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackInitError(errorMessage);
    }
  }

  private void reportWebRtcAudioTrackStartError(
      AudioTrackStartErrorCode errorCode, String errorMessage) {
    Logging.e(TAG, "Start playout error: "  + errorCode + ". " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG);
    if (errorCallbackOld != null) {
      errorCallbackOld.onWebRtcAudioTrackStartError(errorMessage);
    }
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackStartError(errorCode, errorMessage);
    }
  }

  private void reportWebRtcAudioTrackError(String errorMessage) {
    Logging.e(TAG, "Run-time playback error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG);
    if (errorCallbackOld != null) {
      errorCallbackOld.onWebRtcAudioTrackError(errorMessage);
    }
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackError(errorMessage);
    }
  }
}
