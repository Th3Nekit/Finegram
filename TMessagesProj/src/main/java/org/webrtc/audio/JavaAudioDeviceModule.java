/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import androidx.annotation.RequiresApi;
import java.util.concurrent.ScheduledExecutorService;
import org.webrtc.JniCommon;
import org.webrtc.Logging;

public class JavaAudioDeviceModule implements AudioDeviceModule {
  private static final String TAG = "JavaAudioDeviceModule";

  public static Builder builder(Context context) {
    return new Builder(context);
  }

  public static class Builder {
    private final Context context;
    private ScheduledExecutorService scheduler;
    private final AudioManager audioManager;
    private int inputSampleRate;
    private int outputSampleRate;
    private int audioSource = WebRtcAudioRecord.DEFAULT_AUDIO_SOURCE;
    private int audioFormat = WebRtcAudioRecord.DEFAULT_AUDIO_FORMAT;
    private AudioTrackErrorCallback audioTrackErrorCallback;
    private AudioRecordErrorCallback audioRecordErrorCallback;
    private SamplesReadyCallback samplesReadyCallback;
    private AudioTrackStateCallback audioTrackStateCallback;
    private AudioRecordStateCallback audioRecordStateCallback;
    private boolean useHardwareAcousticEchoCanceler = isBuiltInAcousticEchoCancelerSupported();
    private boolean useHardwareNoiseSuppressor = isBuiltInNoiseSuppressorSupported();
    private boolean useStereoInput;
    private boolean useStereoOutput;
    private AudioAttributes audioAttributes;
    private boolean useLowLatency;
    private boolean enableVolumeLogger;

    private Builder(Context context) {
      this.context = context;
      this.audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
      this.inputSampleRate = WebRtcAudioManager.getSampleRate(audioManager);
      this.outputSampleRate = WebRtcAudioManager.getSampleRate(audioManager);
      this.useLowLatency = false;
      this.enableVolumeLogger = true;
    }

    public Builder setScheduler(ScheduledExecutorService scheduler) {
      this.scheduler = scheduler;
      return this;
    }

    public Builder setSampleRate(int sampleRate) {
      Logging.d(TAG, "Input/Output sample rate overridden to: " + sampleRate);
      this.inputSampleRate = sampleRate;
      this.outputSampleRate = sampleRate;
      return this;
    }

    public Builder setInputSampleRate(int inputSampleRate) {
      Logging.d(TAG, "Input sample rate overridden to: " + inputSampleRate);
      this.inputSampleRate = inputSampleRate;
      return this;
    }

    public Builder setOutputSampleRate(int outputSampleRate) {
      Logging.d(TAG, "Output sample rate overridden to: " + outputSampleRate);
      this.outputSampleRate = outputSampleRate;
      return this;
    }

    public Builder setAudioSource(int audioSource) {
      this.audioSource = audioSource;
      return this;
    }

    public Builder setAudioFormat(int audioFormat) {
      this.audioFormat = audioFormat;
      return this;
    }

    public Builder setAudioTrackErrorCallback(AudioTrackErrorCallback audioTrackErrorCallback) {
      this.audioTrackErrorCallback = audioTrackErrorCallback;
      return this;
    }

    public Builder setAudioRecordErrorCallback(AudioRecordErrorCallback audioRecordErrorCallback) {
      this.audioRecordErrorCallback = audioRecordErrorCallback;
      return this;
    }

    public Builder setSamplesReadyCallback(SamplesReadyCallback samplesReadyCallback) {
      this.samplesReadyCallback = samplesReadyCallback;
      return this;
    }

    public Builder setAudioTrackStateCallback(AudioTrackStateCallback audioTrackStateCallback) {
      this.audioTrackStateCallback = audioTrackStateCallback;
      return this;
    }

    public Builder setAudioRecordStateCallback(AudioRecordStateCallback audioRecordStateCallback) {
      this.audioRecordStateCallback = audioRecordStateCallback;
      return this;
    }

    public Builder setUseHardwareNoiseSuppressor(boolean useHardwareNoiseSuppressor) {
      if (useHardwareNoiseSuppressor && !isBuiltInNoiseSuppressorSupported()) {
        Logging.e(TAG, "HW NS not supported");
        useHardwareNoiseSuppressor = false;
      }
      this.useHardwareNoiseSuppressor = useHardwareNoiseSuppressor;
      return this;
    }

    public Builder setUseHardwareAcousticEchoCanceler(boolean useHardwareAcousticEchoCanceler) {
      if (useHardwareAcousticEchoCanceler && !isBuiltInAcousticEchoCancelerSupported()) {
        Logging.e(TAG, "HW AEC not supported");
        useHardwareAcousticEchoCanceler = false;
      }
      this.useHardwareAcousticEchoCanceler = useHardwareAcousticEchoCanceler;
      return this;
    }

    public Builder setUseStereoInput(boolean useStereoInput) {
      this.useStereoInput = useStereoInput;
      return this;
    }

    public Builder setUseStereoOutput(boolean useStereoOutput) {
      this.useStereoOutput = useStereoOutput;
      return this;
    }

    public Builder setUseLowLatency(boolean useLowLatency) {
      this.useLowLatency = useLowLatency;
      return this;
    }

    public Builder setAudioAttributes(AudioAttributes audioAttributes) {
      this.audioAttributes = audioAttributes;
      return this;
    }

    public Builder setEnableVolumeLogger(boolean enableVolumeLogger) {
      this.enableVolumeLogger = enableVolumeLogger;
      return this;
    }

    public JavaAudioDeviceModule createAudioDeviceModule() {
      Logging.d(TAG, "createAudioDeviceModule");
      if (useHardwareNoiseSuppressor) {
        Logging.d(TAG, "HW NS will be used.");
      } else {
        if (isBuiltInNoiseSuppressorSupported()) {
          Logging.d(TAG, "Overriding default behavior; now using WebRTC NS!");
        }
        Logging.d(TAG, "HW NS will not be used.");
      }
      if (useHardwareAcousticEchoCanceler) {
        Logging.d(TAG, "HW AEC will be used.");
      } else {
        if (isBuiltInAcousticEchoCancelerSupported()) {
          Logging.d(TAG, "Overriding default behavior; now using WebRTC AEC!");
        }
        Logging.d(TAG, "HW AEC will not be used.");
      }

      final int MIN_LOW_LATENCY_SDK_VERSION = 26;
      if (useLowLatency && Build.VERSION.SDK_INT >= MIN_LOW_LATENCY_SDK_VERSION) {
        Logging.d(TAG, "Low latency mode will be used.");
      }
      ScheduledExecutorService executor = this.scheduler;
      if (executor == null) {
        executor = WebRtcAudioRecord.newDefaultScheduler();
      }
      final WebRtcAudioRecord audioInput = new WebRtcAudioRecord(context, executor, audioManager,
          audioSource, audioFormat, audioRecordErrorCallback, audioRecordStateCallback,
          samplesReadyCallback, useHardwareAcousticEchoCanceler, useHardwareNoiseSuppressor);
      final WebRtcAudioTrack audioOutput =
          new WebRtcAudioTrack(context, audioManager, audioAttributes, audioTrackErrorCallback,
              audioTrackStateCallback, useLowLatency, enableVolumeLogger);
      return new JavaAudioDeviceModule(context, audioManager, audioInput, audioOutput,
          inputSampleRate, outputSampleRate, useStereoInput, useStereoOutput);
    }
  }

  public enum AudioRecordStartErrorCode {
    AUDIO_RECORD_START_EXCEPTION,
    AUDIO_RECORD_START_STATE_MISMATCH,
  }

  public static interface AudioRecordErrorCallback {
    void onWebRtcAudioRecordInitError(String errorMessage);
    void onWebRtcAudioRecordStartError(AudioRecordStartErrorCode errorCode, String errorMessage);
    void onWebRtcAudioRecordError(String errorMessage);
  }

  public static interface AudioRecordStateCallback {
    void onWebRtcAudioRecordStart();
    void onWebRtcAudioRecordStop();
  }

  public static class AudioSamples {

    private final int audioFormat;

    private final int channelCount;

    private final int sampleRate;

    private final byte[] data;

    public AudioSamples(int audioFormat, int channelCount, int sampleRate, byte[] data) {
      this.audioFormat = audioFormat;
      this.channelCount = channelCount;
      this.sampleRate = sampleRate;
      this.data = data;
    }

    public int getAudioFormat() {
      return audioFormat;
    }

    public int getChannelCount() {
      return channelCount;
    }

    public int getSampleRate() {
      return sampleRate;
    }

    public byte[] getData() {
      return data;
    }
  }

  public static interface SamplesReadyCallback {
    void onWebRtcAudioRecordSamplesReady(AudioSamples samples);
  }

  public enum AudioTrackStartErrorCode {
    AUDIO_TRACK_START_EXCEPTION,
    AUDIO_TRACK_START_STATE_MISMATCH,
  }

  public static interface AudioTrackErrorCallback {
    void onWebRtcAudioTrackInitError(String errorMessage);
    void onWebRtcAudioTrackStartError(AudioTrackStartErrorCode errorCode, String errorMessage);
    void onWebRtcAudioTrackError(String errorMessage);
  }

  public static interface AudioTrackStateCallback {
    void onWebRtcAudioTrackStart();
    void onWebRtcAudioTrackStop();
  }

  public static boolean isBuiltInAcousticEchoCancelerSupported() {
    return WebRtcAudioEffects.isAcousticEchoCancelerSupported();
  }

  public static boolean isBuiltInNoiseSuppressorSupported() {
    return WebRtcAudioEffects.isNoiseSuppressorSupported();
  }

  private final Context context;
  private final AudioManager audioManager;
  private final WebRtcAudioRecord audioInput;
  private final WebRtcAudioTrack audioOutput;
  private final int inputSampleRate;
  private final int outputSampleRate;
  private final boolean useStereoInput;
  private final boolean useStereoOutput;

  private final Object nativeLock = new Object();
  private long nativeAudioDeviceModule;

  private JavaAudioDeviceModule(Context context, AudioManager audioManager,
      WebRtcAudioRecord audioInput, WebRtcAudioTrack audioOutput, int inputSampleRate,
      int outputSampleRate, boolean useStereoInput, boolean useStereoOutput) {
    this.context = context;
    this.audioManager = audioManager;
    this.audioInput = audioInput;
    this.audioOutput = audioOutput;
    this.inputSampleRate = inputSampleRate;
    this.outputSampleRate = outputSampleRate;
    this.useStereoInput = useStereoInput;
    this.useStereoOutput = useStereoOutput;
  }

  @Override
  public long getNativeAudioDeviceModulePointer() {
    synchronized (nativeLock) {
      if (nativeAudioDeviceModule == 0) {
        nativeAudioDeviceModule = nativeCreateAudioDeviceModule(context, audioManager, audioInput,
            audioOutput, inputSampleRate, outputSampleRate, useStereoInput, useStereoOutput);
      }
      return nativeAudioDeviceModule;
    }
  }

  @Override
  public void release() {
    synchronized (nativeLock) {
      if (nativeAudioDeviceModule != 0) {
        JniCommon.nativeReleaseRef(nativeAudioDeviceModule);
        nativeAudioDeviceModule = 0;
      }
    }
  }

  @Override
  public void setSpeakerMute(boolean mute) {
    Logging.d(TAG, "setSpeakerMute: " + mute);
    audioOutput.setSpeakerMute(mute);
  }

  @Override
  public void setMicrophoneMute(boolean mute) {
    Logging.d(TAG, "setMicrophoneMute: " + mute);
    audioInput.setMicrophoneMute(mute);
  }

  @Override
  public boolean setNoiseSuppressorEnabled(boolean enabled) {
    Logging.d(TAG, "setNoiseSuppressorEnabled: " + enabled);
    return audioInput.setNoiseSuppressorEnabled(enabled);
  }

  @RequiresApi(Build.VERSION_CODES.M)
  public void setPreferredInputDevice(AudioDeviceInfo preferredInputDevice) {
    Logging.d(TAG, "setPreferredInputDevice: " + preferredInputDevice);
    audioInput.setPreferredDevice(preferredInputDevice);
  }

  private static native long nativeCreateAudioDeviceModule(Context context,
      AudioManager audioManager, WebRtcAudioRecord audioInput, WebRtcAudioTrack audioOutput,
      int inputSampleRate, int outputSampleRate, boolean useStereoInput, boolean useStereoOutput);
}
