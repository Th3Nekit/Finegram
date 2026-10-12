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

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Build;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;
import java.util.Timer;
import java.util.TimerTask;
import org.webrtc.ContextUtils;
import org.webrtc.Logging;

@Keep
public class WebRtcAudioManager {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioManager";

  private static final boolean blacklistDeviceForAAudioUsage = true;

  private static boolean useStereoOutput;
  private static boolean useStereoInput;

  private static boolean blacklistDeviceForOpenSLESUsage;
  private static boolean blacklistDeviceForOpenSLESUsageIsOverridden;

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized void setBlacklistDeviceForOpenSLESUsage(boolean enable) {
    blacklistDeviceForOpenSLESUsageIsOverridden = true;
    blacklistDeviceForOpenSLESUsage = enable;
  }

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized void setStereoOutput(boolean enable) {
    Logging.w(TAG, "Overriding default output behavior: setStereoOutput(" + enable + ')');
    useStereoOutput = enable;
  }

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized void setStereoInput(boolean enable) {
    Logging.w(TAG, "Overriding default input behavior: setStereoInput(" + enable + ')');
    useStereoInput = enable;
  }

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized boolean getStereoOutput() {
    return useStereoOutput;
  }

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized boolean getStereoInput() {
    return useStereoInput;
  }

  private static final int BITS_PER_SAMPLE = 16;

  private static final int DEFAULT_FRAME_PER_BUFFER = 256;

  private static class VolumeLogger {
    private static final String THREAD_NAME = "WebRtcVolumeLevelLoggerThread";
    private static final int TIMER_PERIOD_IN_SECONDS = 30;

    private final AudioManager audioManager;
    private @Nullable Timer timer;

    public VolumeLogger(AudioManager audioManager) {
      this.audioManager = audioManager;
    }

    public void start() {
      timer = new Timer(THREAD_NAME);
      timer.schedule(new LogVolumeTask(audioManager.getStreamMaxVolume(AudioManager.STREAM_RING),
                         audioManager.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL)),
          0, TIMER_PERIOD_IN_SECONDS * 1000);
    }

    private class LogVolumeTask extends TimerTask {
      private final int maxRingVolume;
      private final int maxVoiceCallVolume;

      LogVolumeTask(int maxRingVolume, int maxVoiceCallVolume) {
        this.maxRingVolume = maxRingVolume;
        this.maxVoiceCallVolume = maxVoiceCallVolume;
      }

      @Override
      public void run() {
        final int mode = audioManager.getMode();
        if (mode == AudioManager.MODE_RINGTONE) {
          Logging.d(TAG, "STREAM_RING stream volume: "
                  + audioManager.getStreamVolume(AudioManager.STREAM_RING) + " (max="
                  + maxRingVolume + ")");
        } else if (mode == AudioManager.MODE_IN_COMMUNICATION) {
          Logging.d(TAG, "VOICE_CALL stream volume: "
                  + audioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL) + " (max="
                  + maxVoiceCallVolume + ")");
        }
      }
    }

    private void stop() {
      if (timer != null) {
        timer.cancel();
        timer = null;
      }
    }
  }

  private final long nativeAudioManager;
  private final AudioManager audioManager;

  private boolean initialized;
  private int nativeSampleRate;
  private int nativeChannels;

  private boolean hardwareAEC;
  private boolean hardwareAGC;
  private boolean hardwareNS;
  private boolean lowLatencyOutput;
  private boolean lowLatencyInput;
  private boolean proAudio;
  private boolean aAudio;
  private int sampleRate;
  private int outputChannels;
  private int inputChannels;
  private int outputBufferSize;
  private int inputBufferSize;

  private final VolumeLogger volumeLogger;

  WebRtcAudioManager(long nativeAudioManager) {
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
    this.nativeAudioManager = nativeAudioManager;
    audioManager =
        (AudioManager) ContextUtils.getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
    if (DEBUG) {
      WebRtcAudioUtils.logDeviceInfo(TAG);
    }
    volumeLogger = new VolumeLogger(audioManager);
    storeAudioParameters();
    nativeCacheAudioParameters(sampleRate, outputChannels, inputChannels, hardwareAEC, hardwareAGC,
        hardwareNS, lowLatencyOutput, lowLatencyInput, proAudio, aAudio, outputBufferSize,
        inputBufferSize, nativeAudioManager);
    WebRtcAudioUtils.logAudioState(TAG);
  }

  private boolean init() {
    Logging.d(TAG, "init" + WebRtcAudioUtils.getThreadInfo());
    if (initialized) {
      return true;
    }
    Logging.d(TAG, "audio mode is: "
        + WebRtcAudioUtils.modeToString(audioManager.getMode()));
    initialized = true;
    volumeLogger.start();
    return true;
  }

  private void dispose() {
    Logging.d(TAG, "dispose" + WebRtcAudioUtils.getThreadInfo());
    if (!initialized) {
      return;
    }
    volumeLogger.stop();
  }

  private boolean isCommunicationModeEnabled() {
    return (audioManager.getMode() == AudioManager.MODE_IN_COMMUNICATION);
  }

  private boolean isDeviceBlacklistedForOpenSLESUsage() {
    boolean blacklisted = blacklistDeviceForOpenSLESUsageIsOverridden
        ? blacklistDeviceForOpenSLESUsage
        : WebRtcAudioUtils.deviceIsBlacklistedForOpenSLESUsage();
    if (blacklisted) {
      Logging.d(TAG, Build.MODEL + " is blacklisted for OpenSL ES usage!");
    }
    return true;
  }

  private void storeAudioParameters() {
    outputChannels = getStereoOutput() ? 2 : 1;
    inputChannels = getStereoInput() ? 2 : 1;
    sampleRate = getNativeOutputSampleRate();
    hardwareAEC = isAcousticEchoCancelerSupported();

    hardwareAGC = false;
    hardwareNS = isNoiseSuppressorSupported();
    lowLatencyOutput = isLowLatencyOutputSupported();
    lowLatencyInput = isLowLatencyInputSupported();
    proAudio = isProAudioSupported();
    aAudio = isAAudioSupported();
    outputBufferSize = lowLatencyOutput ? getLowLatencyOutputFramesPerBuffer()
                                        : getMinOutputFrameSize(sampleRate, outputChannels);
    inputBufferSize = lowLatencyInput ? getLowLatencyInputFramesPerBuffer()
                                      : getMinInputFrameSize(sampleRate, inputChannels);
  }

  private boolean hasEarpiece() {
    return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
        PackageManager.FEATURE_TELEPHONY);
  }

  private boolean isLowLatencyOutputSupported() {
    return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
        PackageManager.FEATURE_AUDIO_LOW_LATENCY);
  }

  public boolean isLowLatencyInputSupported() {

    return Build.VERSION.SDK_INT >= 21 && isLowLatencyOutputSupported();
  }

  private boolean isProAudioSupported() {
    return Build.VERSION.SDK_INT >= 23
        && ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
               PackageManager.FEATURE_AUDIO_PRO);
  }

  private boolean isAAudioSupported() {
    if (blacklistDeviceForAAudioUsage) {
      Logging.w(TAG, "AAudio support is currently disabled on all devices!");
    }
    return !blacklistDeviceForAAudioUsage && Build.VERSION.SDK_INT >= 27;
  }

  private int getNativeOutputSampleRate() {

    if (WebRtcAudioUtils.runningOnEmulator()) {
      Logging.d(TAG, "Running emulator, overriding sample rate to 8 kHz.");
      return 8000;
    }

    if (WebRtcAudioUtils.isDefaultSampleRateOverridden()) {
      Logging.d(TAG, "Default sample rate is overriden to "
              + WebRtcAudioUtils.getDefaultSampleRateHz() + " Hz");
      return WebRtcAudioUtils.getDefaultSampleRateHz();
    }

    final int sampleRateHz = getSampleRateForApiLevel();
    Logging.d(TAG, "Sample rate is set to " + sampleRateHz + " Hz");
    return sampleRateHz;
  }

  private int getSampleRateForApiLevel() {
    if (Build.VERSION.SDK_INT < 17) {
      return WebRtcAudioUtils.getDefaultSampleRateHz();
    }
    String sampleRateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
    return (sampleRateString == null) ? WebRtcAudioUtils.getDefaultSampleRateHz()
                                      : Integer.parseInt(sampleRateString);
  }

  private int getLowLatencyOutputFramesPerBuffer() {
    assertTrue(isLowLatencyOutputSupported());
    if (Build.VERSION.SDK_INT < 17) {
      return DEFAULT_FRAME_PER_BUFFER;
    }
    String framesPerBuffer =
        audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
    return framesPerBuffer == null ? DEFAULT_FRAME_PER_BUFFER : Integer.parseInt(framesPerBuffer);
  }

  private static boolean isAcousticEchoCancelerSupported() {
    return WebRtcAudioEffects.canUseAcousticEchoCanceler();
  }
  private static boolean isNoiseSuppressorSupported() {
    return WebRtcAudioEffects.canUseNoiseSuppressor();
  }

  private static int getMinOutputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    final int channelConfig =
        (numChannels == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO);
    return AudioTrack.getMinBufferSize(
               sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
        / bytesPerFrame;
  }

  private int getLowLatencyInputFramesPerBuffer() {
    assertTrue(isLowLatencyInputSupported());
    return getLowLatencyOutputFramesPerBuffer();
  }

  private static int getMinInputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    final int channelConfig =
        (numChannels == 1 ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO);
    return AudioRecord.getMinBufferSize(
               sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
        / bytesPerFrame;
  }

  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private native void nativeCacheAudioParameters(int sampleRate, int outputChannels,
      int inputChannels, boolean hardwareAEC, boolean hardwareAGC, boolean hardwareNS,
      boolean lowLatencyOutput, boolean lowLatencyInput, boolean proAudio, boolean aAudio,
      int outputBufferSize, int inputBufferSize, long nativeAudioManager);
}
