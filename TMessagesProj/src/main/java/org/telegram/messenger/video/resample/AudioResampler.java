package org.telegram.messenger.video.resample;

import androidx.annotation.NonNull;

import java.nio.ShortBuffer;

public interface AudioResampler {

    void resample(@NonNull final ShortBuffer inputBuffer, int inputSampleRate, @NonNull final ShortBuffer outputBuffer, int outputSampleRate, int channels);

    AudioResampler DOWNSAMPLE = new DownsampleAudioResampler();

    AudioResampler UPSAMPLE = new UpsampleAudioResampler();

    AudioResampler PASSTHROUGH = new PassThroughAudioResampler();
}
