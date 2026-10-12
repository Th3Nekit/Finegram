package org.telegram.messenger.video.remix;

import androidx.annotation.NonNull;

import java.nio.ShortBuffer;

public class DownMixAudioRemixer implements AudioRemixer {

    private static final int SIGNED_SHORT_LIMIT = 32768;
    private static final int UNSIGNED_SHORT_MAX = 65535;

    @Override
    public void remix(@NonNull ShortBuffer inputBuffer, int inputChannelCount, @NonNull ShortBuffer outputBuffer, int outputChannelCount) {
        final int inRemaining = inputBuffer.remaining() / 2;
        final int outSpace = outputBuffer.remaining();

        final int samplesToBeProcessed = Math.min(inRemaining, outSpace);
        for (int i = 0; i < samplesToBeProcessed; ++i) {
            outputBuffer.put(mix(inputBuffer.get(), inputBuffer.get()));
        }
    }

    @Override
    public int getRemixedSize(int inputSize, int inputChannelCount, int outputChannelCount) {
        return inputSize / 2;
    }

    public static short mix(short input1, short input2) {

        final int a = input1 + SIGNED_SHORT_LIMIT;
        final int b = input2 + SIGNED_SHORT_LIMIT;
        int m;

        if ((a < SIGNED_SHORT_LIMIT) || (b < SIGNED_SHORT_LIMIT)) {

            m = a * b / SIGNED_SHORT_LIMIT;
        } else {

            m = 2 * (a + b) - (a * b) / SIGNED_SHORT_LIMIT - UNSIGNED_SHORT_MAX;
        }

        if (m == UNSIGNED_SHORT_MAX + 1) m = UNSIGNED_SHORT_MAX;
        return (short) (m - SIGNED_SHORT_LIMIT);
    }
}
