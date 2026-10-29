#pragma once
// Cambiador de tono en el dominio del tiempo (dos cabezales de lectura con
// ventana Hann, estilo "tape head"). La longitud de ventana L se ajusta a un
// multiplo par del periodo detectado, de modo que los dos cabezales siempre
// estan separados un numero entero de periodos -> suman en fase y el sonido
// no tiene el "flanger" tipico de este metodo. Estereo: ambos canales
// comparten la misma posicion de los cabezales.
#include <vector>
#include <cmath>
#include <algorithm>

class PitchShifter
{
public:
    static constexpr int kSize = 1 << 15;
    static constexpr int kMask = kSize - 1;
    static constexpr int kMaxCh = 2;

    void prepare (double sampleRate, int channels)
    {
        sr = sampleRate; nch = std::min (channels, kMaxCh);
        for (auto& b : buf) b.assign (kSize, 0.f);
        wpos = 0;
        L = (float) (0.032 * sr);
        d1 = 0.25 * L; d2 = 0.75 * L;
        lCoef = 1.f - std::exp (-1.f / (float) (0.12 * sr));
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.f);
        d1 = 0.25 * L; d2 = 0.75 * L;
    }

    // ratio > 1 sube el tono. Ltarget = longitud de ventana deseada en muestras.
    inline void tick (const float* in, float* out, int channels, float ratio, float Ltarget)
    {
        const int n = std::min (channels, nch);
        for (int c = 0; c < n; ++c) buf[(size_t) c][(size_t) wpos] = in[c];

        L += lCoef * (std::clamp (Ltarget, 64.f, (float) (kSize / 2 - 16)) - L);

        const float w1 = 0.5f - 0.5f * std::cos (6.2831853f * (float) (d1 / L));
        const float w2 = 0.5f - 0.5f * std::cos (6.2831853f * (float) (d2 / L));
        const float norm = 1.f / std::max (w1 + w2, 0.2f);

        for (int c = 0; c < n; ++c)
        {
            const float s1 = read (buf[(size_t) c], d1 + 2.0);
            const float s2 = read (buf[(size_t) c], d2 + 2.0);
            out[c] = (w1 * s1 + w2 * s2) * norm;
        }

        const double step = 1.0 - (double) ratio;
        d1 = wrap (d1 + step);
        d2 = wrap (d2 + step);
        wpos = (wpos + 1) & kMask;
    }

private:
    inline double wrap (double d) const
    {
        while (d >= L) d -= L;
        while (d < 0.0) d += L;
        return d;
    }

    inline float read (const std::vector<float>& b, double delay) const
    {
        const double pos = (double) wpos - delay;
        const double fl = std::floor (pos);
        const int i = (int) fl;
        const float f = (float) (pos - fl);
        const float y0 = b[(size_t) ((i - 1) & kMask)], y1 = b[(size_t) (i & kMask)],
                    y2 = b[(size_t) ((i + 1) & kMask)], y3 = b[(size_t) ((i + 2) & kMask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    std::vector<float> buf[kMaxCh];
    double sr = 44100.0, d1 = 0.0, d2 = 0.0;
    float L = 1400.f, lCoef = 0.001f;
    int wpos = 0, nch = 2;
};
