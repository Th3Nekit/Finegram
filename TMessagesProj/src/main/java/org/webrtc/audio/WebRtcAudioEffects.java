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

import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;
import androidx.annotation.Nullable;
import java.util.UUID;
import org.webrtc.Logging;

class WebRtcAudioEffects {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioEffectsExternal";

  private static final UUID AOSP_ACOUSTIC_ECHO_CANCELER =
      UUID.fromString("bb392ec0-8d4d-11e0-a896-0002a5d5c51b");
  private static final UUID AOSP_NOISE_SUPPRESSOR =
      UUID.fromString("c06c8400-8e06-11e0-9cb6-0002a5d5c51b");

  private static @Nullable Descriptor[] cachedEffects;

  private @Nullable AcousticEchoCanceler aec;
  private @Nullable NoiseSuppressor ns;

  private boolean shouldEnableAec;
  private boolean shouldEnableNs;

  public static boolean isAcousticEchoCancelerSupported() {
    return isEffectTypeAvailable(AudioEffect.EFFECT_TYPE_AEC, AOSP_ACOUSTIC_ECHO_CANCELER);
  }

  public static boolean isNoiseSuppressorSupported() {
    return isEffectTypeAvailable(AudioEffect.EFFECT_TYPE_NS, AOSP_NOISE_SUPPRESSOR);
  }

  public WebRtcAudioEffects() {
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
  }

  public boolean setAEC(boolean enable) {
    Logging.d(TAG, "setAEC(" + enable + ")");
    if (!isAcousticEchoCancelerSupported()) {
      Logging.w(TAG, "Platform AEC is not supported");
      shouldEnableAec = false;
      return false;
    }
    if (aec != null && (enable != shouldEnableAec)) {
      Logging.e(TAG, "Platform AEC state can't be modified while recording");
      return false;
    }
    shouldEnableAec = enable;
    return true;
  }

  public boolean setNS(boolean enable) {
    Logging.d(TAG, "setNS(" + enable + ")");
    if (!isNoiseSuppressorSupported()) {
      Logging.w(TAG, "Platform NS is not supported");
      shouldEnableNs = false;
      return false;
    }
    if (ns != null && (enable != shouldEnableNs)) {
      Logging.e(TAG, "Platform NS state can't be modified while recording");
      return false;
    }
    shouldEnableNs = enable;
    return true;
  }

  public boolean toggleNS(boolean enable) {
    if (ns == null) {
      Logging.e(TAG, "Attempting to enable or disable nonexistent NoiseSuppressor.");
      return false;
    }
    Logging.d(TAG, "toggleNS(" + enable + ")");
    boolean toggling_succeeded = ns.setEnabled(enable) == AudioEffect.SUCCESS;
    return toggling_succeeded;
  }

  public void enable(int audioSession) {
    Logging.d(TAG, "enable(audioSession=" + audioSession + ")");
    assertTrue(aec == null);
    assertTrue(ns == null);

    if (DEBUG) {

      for (Descriptor d : AudioEffect.queryEffects()) {
        if (effectTypeIsVoIP(d.type)) {
          Logging.d(TAG,
              "name: " + d.name + ", "
                  + "mode: " + d.connectMode + ", "
                  + "implementor: " + d.implementor + ", "
                  + "UUID: " + d.uuid);
        }
      }
    }

    if (isAcousticEchoCancelerSupported()) {

      aec = AcousticEchoCanceler.create(audioSession);
      if (aec != null) {
        boolean enabled = aec.getEnabled();
        boolean enable = shouldEnableAec && isAcousticEchoCancelerSupported();
        if (aec.setEnabled(enable) != AudioEffect.SUCCESS) {
          Logging.e(TAG, "Failed to set the AcousticEchoCanceler state");
        }
        Logging.d(TAG,
            "AcousticEchoCanceler: was " + (enabled ? "enabled" : "disabled") + ", enable: "
                + enable + ", is now: " + (aec.getEnabled() ? "enabled" : "disabled"));
      } else {
        Logging.e(TAG, "Failed to create the AcousticEchoCanceler instance");
      }
    }

    if (isNoiseSuppressorSupported()) {

      ns = NoiseSuppressor.create(audioSession);
      if (ns != null) {
        boolean enabled = ns.getEnabled();
        boolean enable = shouldEnableNs && isNoiseSuppressorSupported();
        if (ns.setEnabled(enable) != AudioEffect.SUCCESS) {
          Logging.e(TAG, "Failed to set the NoiseSuppressor state");
        }
        Logging.d(TAG,
            "NoiseSuppressor: was " + (enabled ? "enabled" : "disabled") + ", enable: " + enable
                + ", is now: " + (ns.getEnabled() ? "enabled" : "disabled"));
      } else {
        Logging.e(TAG, "Failed to create the NoiseSuppressor instance");
      }
    }
  }

  public void release() {
    Logging.d(TAG, "release");
    if (aec != null) {
      aec.release();
      aec = null;
    }
    if (ns != null) {
      ns.release();
      ns = null;
    }
  }

  private boolean effectTypeIsVoIP(UUID type) {
    return (AudioEffect.EFFECT_TYPE_AEC.equals(type) && isAcousticEchoCancelerSupported())
        || (AudioEffect.EFFECT_TYPE_NS.equals(type) && isNoiseSuppressorSupported());
  }

  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private static @Nullable Descriptor[] getAvailableEffects() {
    if (cachedEffects != null) {
      return cachedEffects;
    }

    cachedEffects = AudioEffect.queryEffects();
    return cachedEffects;
  }

  private static boolean isEffectTypeAvailable(UUID effectType, UUID blockListedUuid) {
    Descriptor[] effects = getAvailableEffects();
    if (effects == null) {
      return false;
    }
    for (Descriptor d : effects) {
      if (d.type.equals(effectType)) {
        return !d.uuid.equals(blockListedUuid);
      }
    }
    return false;
  }
}
