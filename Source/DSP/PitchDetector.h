#pragma once
// Detector de tono monofonico basado en YIN (de Cheveigne & Kawahara, 2002).
// C++ puro, sin dependencias de JUCE. Sin asignaciones de memoria en tiempo real.
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

class PitchDetector
{
public:
    static constexpr int kRing   = 2048;   // buffer circular (a la tasa de analisis)
    static constexpr int kWindow = 1024;   // ventana de integracion YIN

    struct Result { float freq = 0.f; float confidence = 0.f; float rms = 0.f; };

    void prepare (double sampleRate)
    {
        sr     = sampleRate;
        decim  = sr >= 176400.0 ? 4 : (sr >= 88200.0 ? 2 : 1);
        ar     = sr / decim;
        tauMin = std::max (2, (int) (ar / 1100.0));
        tauMax = std::min (kRing - kWindow - 2, (int) (ar / 65.0));
        ring.assign (kRing, 0.f);
        lin.assign (kRing, 0.f);
        diff.assign ((size_t) tauMax + 2, 0.f);
        cmnd.assign ((size_t) tauMax + 2, 0.f);
        wpos = 0; acc = 0.f; accN = 0;
    }

    inline void push (float x)
    {
        acc += x;
        if (++accN == decim)
        {
            ring[(size_t) wpos] = acc / (float) decim;
            wpos = (wpos + 1) & (kRing - 1);
            acc = 0.f; accN = 0;
        }
    }

    Result detect (float threshold = 0.15f)
    {
        Result r;
        const int N = kWindow + tauMax + 1;
        for (int i = 0; i < N; ++i)
            lin[(size_t) i] = ring[(size_t) ((wpos - N + i) & (kRing - 1))];

        // Quitar DC de la ventana
        double mean = 0.0;
        for (int i = 0; i < N; ++i) mean += lin[(size_t) i];
        const float m = (float) (mean / N);
        for (int i = 0; i < N; ++i) lin[(size_t) i] -= m;

        // RMS del tramo mas reciente
        double e = 0.0;
        for (int i = N - kWindow; i < N; ++i) e += (double) lin[(size_t) i] * lin[(size_t) i];
        r.rms = (float) std::sqrt (e / kWindow);

        // Funcion de diferencia
        const float* x = lin.data();
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.f;
            for (int j = 0; j < kWindow; ++j)
            {
                const float d = x[j] - x[j + tau];
                s += d * d;
            }
            diff[(size_t) tau] = s;
        }

        // Diferencia media normalizada acumulada (CMND)
        cmnd[0] = 1.f;
        double run = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += diff[(size_t) tau];
            cmnd[(size_t) tau] = run > 1e-12 ? (float) (diff[(size_t) tau] * tau / run) : 1.f;
        }

        // Primer minimo por debajo del umbral absoluto
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0)
        {   // sin candidato claro: minimo global, confianza baja
            float mn = 1e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cmnd[(size_t) tau] < mn) { mn = cmnd[(size_t) tau]; best = tau; }
            if (best < 0) return r;
        }

        // Interpolacion parabolica
        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
            const float den = a - 2.f * b + c;
            if (std::abs (den) > 1e-9f) tauF += 0.5f * (a - c) / den;
        }
        r.freq       = (float) (ar / tauF);
        r.confidence = std::clamp (1.f - cmnd[(size_t) best], 0.f, 1.f);
        return r;
    }

private:
    double sr = 44100.0, ar = 44100.0;
    int decim = 1, tauMin = 2, tauMax = 600, wpos = 0, accN = 0;
    float acc = 0.f;
    std::vector<float> ring, lin, diff, cmnd;
};
