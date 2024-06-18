#include<cmath>

class BiQuadFilter {
public:
    BiQuadFilter(double sampleRate, double cutoffFreq) {
        this->sampleRate = sampleRate;
        this->cutoffFreq = cutoffFreq;
        double omega = 2.0 * M_PI * cutoffFreq / sampleRate;
        double sinOmega = sin(omega);
        double cosOmega = cos(omega);
        double alpha = sinOmega / (2.0 * 0.7071); // Q = 0.7071 (Butterworth)

        b0 = (1.0 - cosOmega) / 2.0;
        b1 = 1.0 - cosOmega;
        b2 = (1.0 - cosOmega) / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosOmega;
        a2 = 1.0 - alpha;

        // normalizare
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }

    double process(double input) {
        double output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        return output;
    }

    void switchCutoff(double newCutoff) {
        double omega = 2.0 * M_PI * newCutoff / sampleRate;
        double sinOmega = sin(omega);
        double cosOmega = cos(omega);
        double alpha = sinOmega / (2.0 * 0.7071); // Q = 0.7071 (Butterworth)

        b0 = (1.0 - cosOmega) / 2.0;
        b1 = 1.0 - cosOmega;
        b2 = (1.0 - cosOmega) / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosOmega;
        a2 = 1.0 - alpha;

        // normalizare
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }

private:
    double sampleRate;
    double b0, b1, b2, a0, a1, a2;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

public:
    double cutoffFreq;
};