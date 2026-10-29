// Prueba offline del motor DSP (sin JUCE). Compilar: g++ -O2 -std=c++17 -I../Source/DSP dsp_test.cpp
#include <functional>
#include <cstdio>
#include <vector>
#include <cmath>
#include "AutoTuneEngine.h"

static std::vector<float> voice (double sr, double secs, std::function<double(double)> f0fn)
{
    std::vector<float> x ((size_t) (sr * secs));
    double ph = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        double t = i / sr, f = f0fn (t); ph += 2 * M_PI * f / sr;
        double s = 0;
        for (int h = 1; h <= 12; ++h) s += std::sin (h * ph) / h * (h < 6 ? 1.0 : 0.5);
        x[i] = (float) (0.25 * s);
    }
    return x;
}

static double measure (const std::vector<float>& y, double sr, size_t from, size_t to)
{
    PitchDetector d; d.prepare (sr);
    double sumCents = 0; int n = 0; double hz = 0;
    for (size_t i = 0; i < to; ++i) {
        d.push (y[i]);
        if (i >= from && (i % 512) == 0) { auto r = d.detect(); if (r.confidence > 0.8f) { hz += r.freq; ++n; } }
    }
    (void) sumCents;
    return n ? hz / n : 0.0;
}

static double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

int main()
{
    int fails = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        struct Case { double inHz, expectHz; const char* name; };
        Case cases[] = { { 220.0 * std::pow (2, 35 / 1200.0), 220.0, "A3 +35c" },
                         { 330.0 * std::pow (2, -40 / 1200.0), 329.63, "E4 -40c" },
                         { 110.0 * std::pow (2, 20 / 1200.0), 110.0, "A2 +20c" },
                         { 523.25 * std::pow (2, -30 / 1200.0), 523.25, "C5 -30c" } };
        for (auto& c : cases)
        {
            auto in = voice (sr, 2.0, [&] (double) { return c.inHz; });
            AutoTuneEngine e; e.prepare (sr, 1);
            TuneParams p; p.retuneMs = 0.f; e.setParams (p);
            std::vector<float> out = in;
            float* ch[1] = { out.data() };
            for (size_t i = 0; i < out.size(); i += 256) { float* b[1] = { ch[0] + i }; e.process (b, 1, (int) std::min<size_t> (256, out.size() - i)); }
            double hz = measure (out, sr, (size_t) (sr * 1.2), out.size());
            double err = cents (hz, c.expectHz);
            double peak = 0; for (size_t i = (size_t) (sr * 1.2); i < out.size(); ++i) peak = std::max (peak, (double) std::abs (out[i]));
            bool ok = std::abs (err) < 6.0 && peak > 0.3;
            std::printf ("sr=%6.0f %-8s in=%7.2f out=%7.2f Hz  error=%+6.2f c  peak=%.2f  %s\n", sr, c.name, c.inHz, hz, err, peak, ok ? "OK" : "FALLA");
            fails += !ok;
        }
    }
    // Retune lento: debe acercarse gradualmente
    {
        double sr = 48000; auto in = voice (sr, 2.0, [] (double) { return 220.0 * std::pow (2, 40 / 1200.0); });
        AutoTuneEngine e; e.prepare (sr, 1); TuneParams p; p.retuneMs = 200.f; e.setParams (p);
        auto out = in; for (size_t i = 0; i < out.size(); i += 256) { float* b[1] = { out.data() + i }; e.process (b, 1, (int) std::min<size_t> (256, out.size() - i)); }
        double early = cents (measure (out, sr, (size_t) (sr * 0.35), (size_t) (sr * 0.5)), 220.0);
        double late  = cents (measure (out, sr, (size_t) (sr * 1.6), out.size()), 220.0);
        bool ok = early > late && std::abs (late) < 8;
        std::printf ("retune 200ms: temprano=%+.1f c, tarde=%+.1f c  %s\n", early, late, ok ? "OK" : "FALLA"); fails += !ok;
    }
    // Escala Do mayor: 233 Hz (A#3) debe ir a A3 (220) o B3 (246.94), nunca quedarse en A#
    {
        double sr = 48000; auto in = voice (sr, 2.0, [] (double) { return 233.08; });
        AutoTuneEngine e; e.prepare (sr, 1); TuneParams p; p.retuneMs = 0; p.scaleIndex = 1; p.keyRoot = 0; e.setParams (p);
        auto out = in; for (size_t i = 0; i < out.size(); i += 256) { float* b[1] = { out.data() + i }; e.process (b, 1, (int) std::min<size_t> (256, out.size() - i)); }
        double hz = measure (out, sr, (size_t) (sr * 1.2), out.size());
        bool ok = std::abs (cents (hz, 220.0)) < 8 || std::abs (cents (hz, 246.94)) < 8;
        std::printf ("C mayor, A#3 -> %.2f Hz  %s\n", hz, ok ? "OK" : "FALLA"); fails += !ok;
    }
    // Bypass / silencio: sin NaN, sin ruido
    {
        double sr = 48000; std::vector<float> z (48000, 0.f);
        AutoTuneEngine e; e.prepare (sr, 2); e.setParams (TuneParams{});
        std::vector<float> r = z; float* b[2] = { z.data(), r.data() };
        for (size_t i = 0; i + 512 <= z.size(); i += 512) { float* bb[2] = { b[0] + i, b[1] + i }; e.process (bb, 2, 512); }
        bool ok = true; for (float v : z) ok = ok && std::isfinite (v) && std::abs (v) < 1e-6f;
        std::printf ("silencio estereo  %s\n", ok ? "OK" : "FALLA"); fails += !ok;
    }
    std::printf (fails ? "\n%d PRUEBAS FALLARON\n" : "\nTODAS LAS PRUEBAS OK\n", fails);
    return fails != 0;
}
