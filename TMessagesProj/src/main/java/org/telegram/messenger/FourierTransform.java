package org.telegram.messenger;

public abstract class FourierTransform {
    protected static final int LINAVG = 1;
    protected static final int LOGAVG = 2;
    protected static final int NOAVG = 3;

    protected static final float TWO_PI = (float) (2 * Math.PI);
    protected int timeSize;
    protected int sampleRate;
    protected float bandWidth;
    protected float[] real;
    protected float[] imag;
    protected float[] spectrum;
    protected float[] averages;
    protected int whichAverage;
    protected int octaves;
    protected int avgPerOctave;

    FourierTransform(int ts, float sr) {
        timeSize = ts;
        sampleRate = (int) sr;
        bandWidth = (2f / timeSize) * ((float) sampleRate / 2f);
        noAverages();
        allocateArrays();
    }

    protected abstract void allocateArrays();

    protected void setComplex(float[] r, float[] i) {
        if (real.length != r.length && imag.length != i.length) {
        } else {
            System.arraycopy(r, 0, real, 0, r.length);
            System.arraycopy(i, 0, imag, 0, i.length);
        }
    }

    protected void fillSpectrum() {
        for (int i = 0; i < spectrum.length; i++) {
            spectrum[i] = (float) Math.sqrt(real[i] * real[i] + imag[i] * imag[i]);
        }

        if (whichAverage == LINAVG) {
            int avgWidth = spectrum.length / averages.length;
            for (int i = 0; i < averages.length; i++) {
                float avg = 0;
                int j;
                for (j = 0; j < avgWidth; j++) {
                    int offset = j + i * avgWidth;
                    if (offset < spectrum.length) {
                        avg += spectrum[offset];
                    } else {
                        break;
                    }
                }
                avg /= j + 1;
                averages[i] = avg;
            }
        } else if (whichAverage == LOGAVG) {
            for (int i = 0; i < octaves; i++) {
                float lowFreq, hiFreq, freqStep;
                if (i == 0) {
                    lowFreq = 0;
                } else {
                    lowFreq = (sampleRate / 2) / (float) Math.pow(2, octaves - i);
                }
                hiFreq = (sampleRate / 2) / (float) Math.pow(2, octaves - i - 1);
                freqStep = (hiFreq - lowFreq) / avgPerOctave;
                float f = lowFreq;
                for (int j = 0; j < avgPerOctave; j++) {
                    int offset = j + i * avgPerOctave;
                    averages[offset] = calcAvg(f, f + freqStep);
                    f += freqStep;
                }
            }
        }
    }

    public void noAverages() {
        averages = new float[0];
        whichAverage = NOAVG;
    }

    public void linAverages(int numAvg) {
        if (numAvg > spectrum.length / 2) {
            return;
        } else {
            averages = new float[numAvg];
        }
        whichAverage = LINAVG;
    }

    public void logAverages(int minBandwidth, int bandsPerOctave) {
        float nyq = (float) sampleRate / 2f;
        octaves = 1;
        while ((nyq /= 2) > minBandwidth) {
            octaves++;
        }
        avgPerOctave = bandsPerOctave;
        averages = new float[octaves * bandsPerOctave];
        whichAverage = LOGAVG;
    }

    public int timeSize() {
        return timeSize;
    }

    public int specSize() {
        return spectrum.length;
    }

    public float getBand(int i) {
        if (i < 0) i = 0;
        if (i > spectrum.length - 1) i = spectrum.length - 1;
        return spectrum[i];
    }

    public float getBandWidth() {
        return bandWidth;
    }

    public abstract void setBand(int i, float a);

    public abstract void scaleBand(int i, float s);

    public int freqToIndex(float freq) {

        if (freq < getBandWidth() / 2) return 0;

        if (freq > sampleRate / 2 - getBandWidth() / 2) return spectrum.length - 1;

        float fraction = freq / (float) sampleRate;
        return Math.round(timeSize * fraction);
    }

    public float indexToFreq(int i) {
        float bw = getBandWidth();

        if (i == 0) return bw * 0.25f;

        if (i == spectrum.length - 1) {
            float lastBinBeginFreq = (sampleRate / 2) - (bw / 2);
            float binHalfWidth = bw * 0.25f;
            return lastBinBeginFreq + binHalfWidth;
        }

        return i * bw;
    }

    public float calcAvg(float lowFreq, float hiFreq) {
        int lowBound = freqToIndex(lowFreq);
        int hiBound = freqToIndex(hiFreq);
        float avg = 0;
        for (int i = lowBound; i <= hiBound; i++) {
            avg += spectrum[i];
        }
        avg /= (hiBound - lowBound + 1);
        return avg;
    }

    public float[] getSpectrumReal() {
        return real;
    }

    public float[] getSpectrumImaginary() {
        return imag;
    }

    public abstract void forward(float[] buffer);

    public void forward(float[] buffer, int startAt) {
        if (buffer.length - startAt < timeSize) {
            return;
        }

        float[] section = new float[timeSize];
        System.arraycopy(buffer, startAt, section, 0, section.length);
        forward(section);
    }

    public abstract void inverse(float[] buffer);

    public void inverse(float[] freqReal, float[] freqImag, float[] buffer) {
        setComplex(freqReal, freqImag);
        inverse(buffer);
    }

    public static class FFT extends FourierTransform {

        public FFT(int timeSize, float sampleRate) {
            super(timeSize, sampleRate);
            if ((timeSize & (timeSize - 1)) != 0)
                throw new IllegalArgumentException(
                        "FFT: timeSize must be a power of two.");
            buildReverseTable();
            buildTrigTables();
        }

        protected void allocateArrays() {
            spectrum = new float[timeSize / 2 + 1];
            real = new float[timeSize];
            imag = new float[timeSize];
        }

        public void scaleBand(int i, float s) {
            if (s < 0) {

                return;
            }

            real[i] *= s;
            imag[i] *= s;
            spectrum[i] *= s;

            if (i != 0 && i != timeSize / 2) {
                real[timeSize - i] = real[i];
                imag[timeSize - i] = -imag[i];
            }
        }

        public void setBand(int i, float a) {
            if (a < 0) {

                return;
            }
            if (real[i] == 0 && imag[i] == 0) {
                real[i] = a;
                spectrum[i] = a;
            } else {
                real[i] /= spectrum[i];
                imag[i] /= spectrum[i];
                spectrum[i] = a;
                real[i] *= spectrum[i];
                imag[i] *= spectrum[i];
            }
            if (i != 0 && i != timeSize / 2) {
                real[timeSize - i] = real[i];
                imag[timeSize - i] = -imag[i];
            }
        }

        private void fft() {
            for (int halfSize = 1; halfSize < real.length; halfSize *= 2) {

                float phaseShiftStepR = cos(halfSize);
                float phaseShiftStepI = sin(halfSize);

                float currentPhaseShiftR = 1.0f;
                float currentPhaseShiftI = 0.0f;
                for (int fftStep = 0; fftStep < halfSize; fftStep++) {
                    for (int i = fftStep; i < real.length; i += 2 * halfSize) {
                        int off = i + halfSize;
                        float tr = (currentPhaseShiftR * real[off]) - (currentPhaseShiftI * imag[off]);
                        float ti = (currentPhaseShiftR * imag[off]) + (currentPhaseShiftI * real[off]);
                        real[off] = real[i] - tr;
                        imag[off] = imag[i] - ti;
                        real[i] += tr;
                        imag[i] += ti;
                    }
                    float tmpR = currentPhaseShiftR;
                    currentPhaseShiftR = (tmpR * phaseShiftStepR) - (currentPhaseShiftI * phaseShiftStepI);
                    currentPhaseShiftI = (tmpR * phaseShiftStepI) + (currentPhaseShiftI * phaseShiftStepR);
                }
            }
        }

        public void forward(float[] buffer) {
            if (buffer.length != timeSize) {

                return;
            }

            bitReverseSamples(buffer, 0);

            fft();

            fillSpectrum();
        }

        @Override
        public void forward(float[] buffer, int startAt) {
            if (buffer.length - startAt < timeSize) {

                return;
            }

            bitReverseSamples(buffer, startAt);
            fft();
            fillSpectrum();
        }

        public void forward(float[] buffReal, float[] buffImag) {
            if (buffReal.length != timeSize || buffImag.length != timeSize) {

                return;
            }
            setComplex(buffReal, buffImag);
            bitReverseComplex();
            fft();
            fillSpectrum();
        }

        public void inverse(float[] buffer) {
            if (buffer.length > real.length) {

                return;
            }

            for (int i = 0; i < timeSize; i++) {
                imag[i] *= -1;
            }
            bitReverseComplex();
            fft();

            for (int i = 0; i < buffer.length; i++) {
                buffer[i] = real[i] / real.length;
            }
        }

        private int[] reverse;

        private void buildReverseTable() {
            int N = timeSize;
            reverse = new int[N];

            reverse[0] = 0;
            for (int limit = 1, bit = N / 2; limit < N; limit <<= 1, bit >>= 1)
                for (int i = 0; i < limit; i++)
                    reverse[i + limit] = reverse[i] + bit;
        }

        private void bitReverseSamples(float[] samples, int startAt) {
            for (int i = 0; i < timeSize; ++i) {
                real[i] = samples[startAt + reverse[i]];
                imag[i] = 0.0f;
            }
        }

        private void bitReverseComplex() {
            float[] revReal = new float[real.length];
            float[] revImag = new float[imag.length];
            for (int i = 0; i < real.length; i++) {
                revReal[i] = real[reverse[i]];
                revImag[i] = imag[reverse[i]];
            }
            real = revReal;
            imag = revImag;
        }

        private float[] sinlookup;
        private float[] coslookup;

        private float sin(int i) {
            return sinlookup[i];
        }

        private float cos(int i) {
            return coslookup[i];
        }

        private void buildTrigTables() {
            int N = timeSize;
            sinlookup = new float[N];
            coslookup = new float[N];
            for (int i = 0; i < N; i++) {
                sinlookup[i] = (float) Math.sin(-(float) Math.PI / i);
                coslookup[i] = (float) Math.cos(-(float) Math.PI / i);
            }
        }
    }
}
