#pragma once
// Motor de correccion de afinacion en tiempo real. C++ puro.
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include "PitchDetector.h"
#include "PitchShifter.h"

struct TuneParams
{
    int   keyRoot      = 0;       // 0 = C ... 11 = B
    int   scaleIndex   = 0;       // ver AutoTuneEngine::scaleNames
    float retuneMs     = 20.f;    // 0 = instantaneo (efecto "robot")
    float amount       = 1.f;     // 0..1
    float detuneCents  = 0.f;
    float refA         = 440.f;
    float mix          = 1.f;     // 0..1
    float outGainDb    = 0.f;
    bool  bypass       = false;
};

class AutoTuneEngine
{
public:
    static constexpr int kNumScales = 9;
    static const char* scaleName (int i)
    {
        static const char* n[kNumScales] = { "Chromatic", "Major", "Minor", "Harmonic Minor", "Pentatonic Major",
                                             "Pentatonic Minor", "Blues", "Dorian", "Mixolydian" };
        return n[std::clamp (i, 0, kNumScales - 1)];
    }
    // Mascara de 12 bits relativa a la tonica (bit 0 = tonica)
    static uint16_t scaleMask (int i)
    {
        static const int iv[kNumScales][8] = {
            { 0,1,2,3,4,5,6,7 },  // placeholder, cromatica se trata aparte
            { 0,2,4,5,7,9,11,-1 }, { 0,2,3,5,7,8,10,-1 }, { 0,2,3,5,7,8,11,-1 },
            { 0,2,4,7,9,-1,-1,-1 }, { 0,3,5,7,10,-1,-1,-1 }, { 0,3,5,6,7,10,-1,-1 },
            { 0,2,3,5,7,9,10,-1 }, { 0,2,4,5,7,9,10,-1 } };
        i = std::clamp (i, 0, kNumScales - 1);
        if (i == 0) return 0x0FFF;
        uint16_t m = 0;
        for (int k = 0; k < 8; ++k) if (iv[i][k] >= 0) m |= (uint16_t) (1u << iv[i][k]);
        return m;
    }

    // ---- Telemetria para la interfaz (lectura desde el hilo de GUI) ----
    std::atomic<float> meterDetectedMidi { -1.f };   // -1 = sin tono
    std::atomic<float> meterTargetMidi   { -1.f };
    std::atomic<float> meterCorrection   { 0.f };    // cents aplicados actualmente

    void prepare (double sampleRate, int channels)
    {
        sr = sampleRate; nch = std::clamp (channels, 1, 2);
        detector.prepare (sr);
        shifter.prepare (sr, nch);
        latency = (int) std::lround (0.020 * sr);
        for (auto& d : dry) d.assign (kDrySize, 0.f);
        hop = std::max (128, (int) std::lround (sr * 0.0107));
        reset();
    }

    void reset()
    {
        shifter.reset();
        for (auto& d : dry) std::fill (d.begin(), d.end(), 0.f);
        dryPos = 0; hopCount = 0; unvoiced = 99; histN = 0; curNote = -1000;
        corrCents = targetCents = 0.f; Ltarget = (float) (0.032 * sr);
        smoothMix = params.mix; smoothGain = std::pow (10.f, params.outGainDb / 20.f);
        meterDetectedMidi = -1.f; meterTargetMidi = -1.f; meterCorrection = 0.f;
    }

    int latencySamples() const { return latency; }
    void setParams (const TuneParams& p) { params = p; }

    void process (float* const* io, int channels, int numSamples)
    {
        const int n = std::min (channels, nch);
        if (n <= 0) return;
        const float gTarget = std::pow (10.f, params.outGainDb / 20.f);
        const float mixTarget = params.bypass ? 0.f : params.mix;
        const float sc = 1.f - std::exp (-1.f / (float) (0.02 * sr));
        const float releaseCoef = 1.f - std::exp (-1.f / (float) (0.04 * sr));

        float in[2] = { 0, 0 }, wet[2] = { 0, 0 };
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.f;
            for (int c = 0; c < n; ++c) { in[c] = io[c][i]; mono += in[c]; }
            detector.push (mono / (float) n);

            if (++hopCount >= hop) { hopCount = 0; analyse(); }

            // suavizado de la correccion (velocidad de retune)
            const float coef = voicedNow ? retuneCoef : releaseCoef;
            corrCents += coef * (targetCents - corrCents);
            const float ratio = std::exp2 (corrCents / 1200.f);

            shifter.tick (in, wet, n, ratio, Ltarget);

            smoothMix  += sc * (mixTarget - smoothMix);
            smoothGain += sc * (gTarget - smoothGain);

            const int rp = (dryPos - latency) & (kDrySize - 1);
            for (int c = 0; c < n; ++c)
            {
                dry[(size_t) c][(size_t) dryPos] = in[c];
                const float d = dry[(size_t) c][(size_t) rp];
                io[c][i] = (d + smoothMix * (wet[c] - d)) * smoothGain;
            }
            dryPos = (dryPos + 1) & (kDrySize - 1);
        }
        meterCorrection.store (corrCents, std::memory_order_relaxed);
    }

private:
    static constexpr int kDrySize = 1 << 14;

    void analyse()
    {
        const auto r = detector.detect();
        const bool ok = r.rms > 0.003f && r.confidence > 0.80f && r.freq >= 65.f && r.freq <= 1100.f;

        if (!ok)
        {
            voicedNow = false;
            if (++unvoiced > 3) { targetCents = 0.f; histN = 0; curNote = -1000;
                                  meterDetectedMidi.store (-1.f); meterTargetMidi.store (-1.f); }
            return;
        }
        unvoiced = 0; voicedNow = true;

        const float midiRaw = 69.f + 12.f * std::log2 (r.freq / params.refA);
        hist[2] = hist[1]; hist[1] = hist[0]; hist[0] = midiRaw;
        if (histN < 3) ++histN;
        const float midi = histN >= 3 ? std::max (std::min (hist[0], hist[1]),
                                                  std::min (std::max (hist[0], hist[1]), hist[2])) : midiRaw;

        // ventana del cambiador: multiplo par del periodo
        const float period = (float) (sr / r.freq);
        const float minL = (float) (0.016 * sr);
        const int nn = std::max (2, (int) std::ceil (minL / (2.f * period)));
        Ltarget = 2.f * (float) nn * period;

        // nota objetivo con histeresis
        const uint16_t mask = scaleMask (params.scaleIndex);
        auto allowed = [&] (int note) { return (mask >> (((note - params.keyRoot) % 12 + 12) % 12)) & 1; };
        int best = -1000; float bestD = 1e9f;
        const int base = (int) std::lround (midi);
        for (int nt = base - 6; nt <= base + 6; ++nt)
            if (allowed (nt)) { const float d = std::abs (midi - (float) nt); if (d < bestD) { bestD = d; best = nt; } }
        if (best == -1000) best = base;

        if (curNote == -1000 || !allowed (curNote) || std::abs (midi - (float) curNote) > 1.6f) curNote = best;
        else if (best != curNote && bestD + 0.10f < std::abs (midi - (float) curNote)) curNote = best;

        const float target = (float) curNote + params.detuneCents / 100.f;
        targetCents = (target - midi) * 100.f * params.amount;

        const float tau = params.retuneMs * 0.001f;
        retuneCoef = tau <= 1e-5f ? 1.f : 1.f - std::exp (-1.f / (tau * (float) sr));

        meterDetectedMidi.store (midi, std::memory_order_relaxed);
        meterTargetMidi.store (target, std::memory_order_relaxed);
    }

    TuneParams params;
    PitchDetector detector;
    PitchShifter shifter;
    std::vector<float> dry[2];
    double sr = 44100.0;
    int nch = 2, latency = 882, hop = 470, hopCount = 0, dryPos = 0, unvoiced = 99, histN = 0, curNote = -1000;
    bool voicedNow = false;
    float hist[3] = { 0, 0, 0 };
    float corrCents = 0.f, targetCents = 0.f, retuneCoef = 0.01f, Ltarget = 1400.f;
    float smoothMix = 1.f, smoothGain = 1.f;
};
