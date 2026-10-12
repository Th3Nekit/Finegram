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

import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;
import androidx.annotation.Nullable;
import java.util.List;
import java.util.UUID;

import org.telegram.messenger.SharedConfig;
import org.webrtc.Logging;

public class WebRtcAudioEffects {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioEffects";

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

    return isAcousticEchoCancelerEffectAvailable();
  }

  public static boolean isNoiseSuppressorSupported() {

    return isNoiseSuppressorEffectAvailable();
  }

  public static boolean isAcousticEchoCancelerBlacklisted() {
    List<String> blackListedModels = WebRtcAudioUtils.getBlackListedModelsForAecUsage();
    boolean isBlacklisted = blackListedModels.contains(Build.MODEL);
    if (isBlacklisted) {
      Logging.w(TAG, Build.MODEL + " is blacklisted for HW AEC usage!");
    }
    return isBlacklisted;
  }

  public static boolean isNoiseSuppressorBlacklisted() {
    List<String> blackListedModels = WebRtcAudioUtils.getBlackListedModelsForNsUsage();
    boolean isBlacklisted = blackListedModels.contains(Build.MODEL);
    if (isBlacklisted) {
      Logging.w(TAG, Build.MODEL + " is blacklisted for HW NS usage!");
    }
    return isBlacklisted;
  }

  private static boolean isAcousticEchoCancelerExcludedByUUID() {
    if (Build.VERSION.SDK_INT < 18)
      return false;
    for (Descriptor d : getAvailableEffects()) {
      if (d.type.equals(AudioEffect.EFFECT_TYPE_AEC)
          && d.uuid.equals(AOSP_ACOUSTIC_ECHO_CANCELER)) {
        return true;
      }
    }
    return false;
  }

  private static boolean isNoiseSuppressorExcludedByUUID() {
    if (Build.VERSION.SDK_INT < 18)
      return false;
    for (Descriptor d : getAvailableEffects()) {
      if (d.type.equals(AudioEffect.EFFECT_TYPE_NS) && d.uuid.equals(AOSP_NOISE_SUPPRESSOR)) {
        return true;
      }
    }
    return false;
  }

  private static boolean isAcousticEchoCancelerEffectAvailable() {
    if (Build.VERSION.SDK_INT < 18)
      return false;
    return isEffectTypeAvailable(AudioEffect.EFFECT_TYPE_AEC);
  }

  private static boolean isNoiseSuppressorEffectAvailable() {
    if (Build.VERSION.SDK_INT < 18)
      return false;
    return isEffectTypeAvailable(AudioEffect.EFFECT_TYPE_NS);
  }

  public static boolean canUseAcousticEchoCanceler() {
    boolean canUseAcousticEchoCanceler = isAcousticEchoCancelerSupported()
        && !WebRtcAudioUtils.useWebRtcBasedAcousticEchoCanceler()
        && !isAcousticEchoCancelerBlacklisted() && !isAcousticEchoCancelerExcludedByUUID();
    Logging.d(TAG, "canUseAcousticEchoCanceler: " + canUseAcousticEchoCanceler);
    return canUseAcousticEchoCanceler;
  }

  public static boolean canUseNoiseSuppressor() {
    boolean canUseNoiseSuppressor = isNoiseSuppressorSupported()
        && !WebRtcAudioUtils.useWebRtcBasedNoiseSuppressor() && !isNoiseSuppressorBlacklisted()
        && !isNoiseSuppressorExcludedByUUID();
    Logging.d(TAG, "canUseNoiseSuppressor: " + canUseNoiseSuppressor);
    return canUseNoiseSuppressor;
  }

  public static WebRtcAudioEffects create() {
    return new WebRtcAudioEffects();
  }

  private WebRtcAudioEffects() {
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
  }

  public boolean setAEC(boolean enable) {
    Logging.d(TAG, "setAEC(" + enable + ")");
    if (!canUseAcousticEchoCanceler()) {
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
    if (!canUseNoiseSuppressor()) {
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

  public void enable(int audioSession) {
    Logging.d(TAG, "enable(audioSession=" + audioSession + ")");
    assertTrue(aec == null);
    assertTrue(ns == null);

    if (DEBUG) {

      for (Descriptor d : AudioEffect.queryEffects()) {
        if (effectTypeIsVoIP(d.type)) {
          Logging.d(TAG, "name: " + d.name + ", "
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
        boolean enable = shouldEnableAec && canUseAcousticEchoCanceler() && !SharedConfig.disableVoiceAudioEffects;
        if (aec.setEnabled(enable) != AudioEffect.SUCCESS) {
          Logging.e(TAG, "Failed to set the AcousticEchoCanceler state");
        }
        Logging.d(TAG, "AcousticEchoCanceler: was " + (enabled ? "enabled" : "disabled")
                + ", enable: " + enable + ", is now: "
                + (aec.getEnabled() ? "enabled" : "disabled"));
      } else {
        Logging.e(TAG, "Failed to create the AcousticEchoCanceler instance");
      }
    }

    if (isNoiseSuppressorSupported()) {

      ns = NoiseSuppressor.create(audioSession);
      if (ns != null) {
        boolean enabled = ns.getEnabled();
        boolean enable = shouldEnableNs && canUseNoiseSuppressor() && !SharedConfig.disableVoiceAudioEffects;
        if (ns.setEnabled(enable) != AudioEffect.SUCCESS) {
          Logging.e(TAG, "Failed to set the NoiseSuppressor state");
        }
        Logging.d(TAG, "NoiseSuppressor: was " + (enabled ? "enabled" : "disabled") + ", enable: "
                + enable + ", is now: " + (ns.getEnabled() ? "enabled" : "disabled"));
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
    if (Build.VERSION.SDK_INT < 18)
      return false;

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

  private static boolean isEffectTypeAvailable(UUID effectType) {
    Descriptor[] effects = getAvailableEffects();
    if (effects == null) {
      return false;
    }
    for (Descriptor d : effects) {
      if (d.type.equals(effectType)) {
        return true;
      }
    }
    return false;
  }
}
