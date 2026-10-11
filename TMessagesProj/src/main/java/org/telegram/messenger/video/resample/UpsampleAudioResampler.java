package org.telegram.messenger.video.resample;

import androidx.annotation.NonNull;

import java.nio.ShortBuffer;

public class UpsampleAudioResampler implements AudioResampler {

    private static float ratio(int remaining, int all) {
        return (float) remaining / all;
    }

    @Override
    public void resample(@NonNull ShortBuffer inputBuffer, int inputSampleRate, @NonNull ShortBuffer outputBuffer, int outputSampleRate, int channels) {
        if (inputSampleRate > outputSampleRate) {
            throw new IllegalArgumentException("Illegal use of UpsampleAudioResampler");
        }
        if (channels != 1 && channels != 2) {
            throw new IllegalArgumentException("Illegal use of UpsampleAudioResampler. Channels:" + channels);
        }

        final int inputSamples = inputBuffer.remaining() / channels;
        final int outputSamples = (int) Math.ceil(inputSamples * ((double) outputSampleRate / inputSampleRate));

        final int fakeSamples = outputSamples - inputSamples;
        int remainingInputSamples = inputSamples;
        int remainingFakeSamples = fakeSamples;
        float remainingInputSamplesRatio = ratio(remainingInputSamples, inputSamples);
        float remainingFakeSamplesRatio = ratio(remainingFakeSamples, fakeSamples);
        while (remainingInputSamples > 0 && remainingFakeSamples > 0) {

            if (remainingInputSamplesRatio >= remainingFakeSamplesRatio) {
                outputBuffer.put(inputBuffer.get());
                if (channels == 2) outputBuffer.put(inputBuffer.get());
                remainingInputSamples--;
                remainingInputSamplesRatio = ratio(remainingInputSamples, inputSamples);
            } else {
                outputBuffer.put(fakeSample(outputBuffer, inputBuffer, 1, channels));
                if (channels == 2) outputBuffer.put(fakeSample(outputBuffer, inputBuffer, 2, channels));
                remainingFakeSamples--;
                remainingFakeSamplesRatio = ratio(remainingFakeSamples, fakeSamples);
            }
        }
    }

    @SuppressWarnings("unused")
    private static short fakeSample(@NonNull ShortBuffer output, @NonNull ShortBuffer input, int channel, int channels) {
        return output.get(output.position() - channels);
    }
}
