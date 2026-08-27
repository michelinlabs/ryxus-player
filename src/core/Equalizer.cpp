#include "core/Equalizer.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace {

constexpr double kPi = 3.14159265358979323846;

const std::array<float, Equalizer::kBands> kDefaultFreqs = {
    60.0f, 170.0f, 310.0f, 600.0f, 1000.0f, 3000.0f, 6000.0f, 12000.0f
};

inline float dbToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }

} // namespace

const std::array<float, Equalizer::kBands>& Equalizer::defaultFrequencies()
{
    return kDefaultFreqs;
}

Equalizer::Equalizer()
{
    for (int b = 0; b < kBands; ++b) {
        m_gains[b].store(0.0f, std::memory_order_relaxed);
        m_freqs[b].store(kDefaultFreqs[b], std::memory_order_relaxed);
        m_qs[b].store(kDefaultQ, std::memory_order_relaxed);
    }
    recompute();
}

void Equalizer::prepare(double sampleRate)
{
    if (sampleRate > 0.0)
        m_sampleRate = sampleRate;
    for (auto& chan : m_state)
        chan.fill(State{});
    m_dirty.store(true, std::memory_order_release);
}

void Equalizer::setGain(int band, float dB)
{
    if (band < 0 || band >= kBands)
        return;
    m_gains[band].store(std::clamp(dB, -kRangeDb, kRangeDb), std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

float Equalizer::gain(int band) const
{
    if (band < 0 || band >= kBands)
        return 0.0f;
    return m_gains[band].load(std::memory_order_relaxed);
}

void Equalizer::setFrequency(int band, float hz)
{
    if (band < 0 || band >= kBands)
        return;
    m_freqs[band].store(std::clamp(hz, kMinHz, kMaxHz), std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

float Equalizer::frequency(int band) const
{
    if (band < 0 || band >= kBands)
        return 0.0f;
    return m_freqs[band].load(std::memory_order_relaxed);
}

void Equalizer::setQ(int band, float q)
{
    if (band < 0 || band >= kBands)
        return;
    m_qs[band].store(std::clamp(q, kMinQ, kMaxQ), std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

float Equalizer::q(int band) const
{
    if (band < 0 || band >= kBands)
        return kDefaultQ;
    return m_qs[band].load(std::memory_order_relaxed);
}

void Equalizer::setPreamp(float dB)
{
    m_preamp.store(std::clamp(dB, -kPreampRangeDb, kPreampRangeDb), std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

float Equalizer::preamp() const { return m_preamp.load(std::memory_order_relaxed); }

void Equalizer::resetBand(int band)
{
    if (band < 0 || band >= kBands)
        return;
    m_gains[band].store(0.0f, std::memory_order_relaxed);
    m_freqs[band].store(kDefaultFreqs[band], std::memory_order_relaxed);
    m_qs[band].store(kDefaultQ, std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

void Equalizer::resetBands()
{
    for (int b = 0; b < kBands; ++b) {
        m_gains[b].store(0.0f, std::memory_order_relaxed);
        m_freqs[b].store(kDefaultFreqs[b], std::memory_order_relaxed);
        m_qs[b].store(kDefaultQ, std::memory_order_relaxed);
    }
    m_preamp.store(0.0f, std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

Equalizer::Coeffs Equalizer::peaking(double freq, double sampleRate, double q, double gainDb)
{
    Coeffs c;

    // Por encima de Nyquist el filtro deja de tener sentido: pasa-todo.
    const double nyquist = sampleRate * 0.5;
    if (sampleRate <= 0.0 || freq >= nyquist * 0.95 || freq <= 0.0)
        return c;

    const double A     = std::pow(10.0, gainDb / 40.0);
    const double w0    = 2.0 * kPi * freq / sampleRate;
    const double cosw0 = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);

    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * cosw0;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha / A;

    c.b0 = static_cast<float>(b0 / a0);
    c.b1 = static_cast<float>(b1 / a0);
    c.b2 = static_cast<float>(b2 / a0);
    c.a1 = static_cast<float>(a1 / a0);
    c.a2 = static_cast<float>(a2 / a0);
    return c;
}

void Equalizer::recompute()
{
    for (int b = 0; b < kBands; ++b) {
        m_coeffs[b] = peaking(m_freqs[b].load(std::memory_order_relaxed),
                              m_sampleRate,
                              m_qs[b].load(std::memory_order_relaxed),
                              m_gains[b].load(std::memory_order_relaxed));
    }
    m_preampLinear = dbToLinear(m_preamp.load(std::memory_order_relaxed));
}

void Equalizer::process(float* interleaved, unsigned frameCount, int channels)
{
    if (!interleaved || channels <= 0)
        return;

    if (m_dirty.exchange(false, std::memory_order_acquire))
        recompute();

    if (!m_enabled.load(std::memory_order_relaxed))
        return;

    const int   ch  = std::min(channels, kMaxChans);
    const float pre = m_preampLinear;

    for (unsigned f = 0; f < frameCount; ++f) {
        float* frame = interleaved + static_cast<size_t>(f) * channels;
        for (int c = 0; c < ch; ++c) {
            float x = frame[c] * pre;

            for (int b = 0; b < kBands; ++b) {
                const Coeffs& k = m_coeffs[b];
                State& s = m_state[c][b];

                const float y = k.b0 * x + k.b1 * s.x1 + k.b2 * s.x2
                                         - k.a1 * s.y1 - k.a2 * s.y2;
                s.x2 = s.x1;
                s.x1 = x;
                s.y2 = s.y1;
                s.y1 = y;
                x = y;
            }

            // Saturacion suave por encima de 0.9: evita el recorte duro al
            // sumar realces de varias bandas. El tanh solo se evalua en los
            // picos, asi que no pesa en el caso normal.
            if (x > 0.9f)
                x = 0.9f + 0.1f * std::tanh((x - 0.9f) * 10.0f);
            else if (x < -0.9f)
                x = -0.9f + 0.1f * std::tanh((x + 0.9f) * 10.0f);

            frame[c] = x;
        }
    }
}

float Equalizer::responseDb(float freqHz) const
{
    if (!m_enabled.load(std::memory_order_relaxed))
        return 0.0f;

    const double w = 2.0 * kPi * freqHz / m_sampleRate;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = z1 * z1;

    double magnitude = 1.0;
    for (int b = 0; b < kBands; ++b) {
        const Coeffs k = peaking(m_freqs[b].load(std::memory_order_relaxed),
                                 m_sampleRate,
                                 m_qs[b].load(std::memory_order_relaxed),
                                 m_gains[b].load(std::memory_order_relaxed));
        const std::complex<double> num = double(k.b0) + double(k.b1) * z1 + double(k.b2) * z2;
        const std::complex<double> den = 1.0          + double(k.a1) * z1 + double(k.a2) * z2;
        if (std::abs(den) > 1e-12)
            magnitude *= std::abs(num / den);
    }

    const double db = 20.0 * std::log10(std::max(magnitude, 1e-9))
                    + m_preamp.load(std::memory_order_relaxed);
    return static_cast<float>(db);
}
