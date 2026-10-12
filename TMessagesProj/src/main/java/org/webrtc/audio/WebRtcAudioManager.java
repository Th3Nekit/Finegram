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

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Build;
import org.webrtc.Logging;
import org.webrtc.CalledByNative;

class WebRtcAudioManager {
  private static final String TAG = "WebRtcAudioManagerExternal";

  private static final int DEFAULT_SAMPLE_RATE_HZ = 16000;

  private static final int BITS_PER_SAMPLE = 16;

  private static final int DEFAULT_FRAME_PER_BUFFER = 256;

  @CalledByNative
  static AudioManager getAudioManager(Context context) {
    return (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
  }

  @CalledByNative
  static int getOutputBufferSize(
      Context context, AudioManager audioManager, int sampleRate, int numberOfOutputChannels) {
    return isLowLatencyOutputSupported(context)
        ? getLowLatencyFramesPerBuffer(audioManager)
        : getMinOutputFrameSize(sampleRate, numberOfOutputChannels);
  }

  @CalledByNative
  static int getInputBufferSize(
      Context context, AudioManager audioManager, int sampleRate, int numberOfInputChannels) {
    return isLowLatencyInputSupported(context)
        ? getLowLatencyFramesPerBuffer(audioManager)
        : getMinInputFrameSize(sampleRate, numberOfInputChannels);
  }

  @CalledByNative
  static boolean isLowLatencyOutputSupported(Context context) {
    return context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY);
  }

  @CalledByNative
  static boolean isLowLatencyInputSupported(Context context) {

    return isLowLatencyOutputSupported(context);
  }

  @CalledByNative
  static int getSampleRate(AudioManager audioManager) {

    if (WebRtcAudioUtils.runningOnEmulator()) {
      Logging.d(TAG, "Running emulator, overriding sample rate to 8 kHz.");
      return 8000;
    }

    final int sampleRateHz = getSampleRateForApiLevel(audioManager);
    Logging.d(TAG, "Sample rate is set to " + sampleRateHz + " Hz");
    return sampleRateHz;
  }

  private static int getSampleRateForApiLevel(AudioManager audioManager) {
    String sampleRateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
    return (sampleRateString == null) ? DEFAULT_SAMPLE_RATE_HZ : Integer.parseInt(sampleRateString);
  }

  private static int getLowLatencyFramesPerBuffer(AudioManager audioManager) {
    String framesPerBuffer =
        audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
    return framesPerBuffer == null ? DEFAULT_FRAME_PER_BUFFER : Integer.parseInt(framesPerBuffer);
  }

  private static int getMinOutputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    final int channelConfig =
        (numChannels == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO);
    return AudioTrack.getMinBufferSize(
               sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
        / bytesPerFrame;
  }

  private static int getMinInputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    final int channelConfig =
        (numChannels == 1 ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO);
    return AudioRecord.getMinBufferSize(
               sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
        / bytesPerFrame;
  }
}
