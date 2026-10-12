package org.telegram.messenger.video.remix;

import androidx.annotation.NonNull;

import java.nio.ShortBuffer;

public class SurroundAudioRemixer implements AudioRemixer {

    @Override
    public void remix(@NonNull ShortBuffer inputBuffer, int inputChannelCount, @NonNull ShortBuffer outputBuffer, int outputChannelCount) {
        if (outputChannelCount != 1 && outputChannelCount != 2) {
            throw new IllegalArgumentException("Output must be 2 or 1 channels");
        }

        final int inFramesRemaining = inputBuffer.remaining() / inputChannelCount;
        final int outFramesSpace = outputBuffer.remaining() / outputChannelCount;
        final int framesToProcess = Math.min(inFramesRemaining, outFramesSpace);

        for (int i = 0; i < framesToProcess; ++i) {

            short frontLeft = inputBuffer.get();
            short frontRight = inputBuffer.get();

            inputBuffer.position(inputBuffer.position() + 4);

            if (outputChannelCount == 2) {
                outputBuffer.put(frontLeft);
                outputBuffer.put(frontRight);
            } else if (outputChannelCount == 1) {
                outputBuffer.put(DownMixAudioRemixer.mix(frontLeft, frontRight));
            }
        }
    }

    @Override
    public int getRemixedSize(int inputSize, int inputChannelCount, int outputChannelCount) {
        return inputSize / inputChannelCount * outputChannelCount;
    }
}
