#include "core/Equalizer.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Doce puntos repartidos casi en octavas. Frente al reparto anterior de ocho
// se ganan tres nodos por debajo de 200 Hz (32, 100 y 170 Hz), que es donde la
// curva se quedaba corta: con una sola banda a 60 Hz no habia forma de dibujar
// nada en el grave sin arrastrar todo el resto.
const std::array<float, Equalizer::kBands> kDefaultFreqs = {
       32.0f,   60.0f,  100.0f,   170.0f,
      310.0f,  600.0f, 1000.0f,  1800.0f,
     3000.0f, 6000.0f,10000.0f, 16000.0f
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
    prepare(m_sampleRate);
}

void Equalizer::prepare(double sampleRate)
{
    if (sampleRate > 0.0)
        m_sampleRate = sampleRate;
    for (auto& chan : m_state)
        chan.fill(State{});

    // Constante de tiempo de unos 8 ms para perseguir los mandos: lo bastante
    // rapido para que el arrastre se sienta inmediato y lo bastante lento para
    // que no se oiga el salto.
    const double blocksPerSecond = m_sampleRate / double(kControlBlock);
    m_chase = 1.0 - std::exp(-1.0 / (0.008 * blocksPerSecond));

    m_coeffsValid = false;
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

void Equalizer::recomputeFromSmoothed()
{
    for (int b = 0; b < kBands; ++b)
        m_coeffs[b] = peaking(m_smoothFreq[b], m_sampleRate, m_smoothQ[b], m_smoothGain[b]);
    m_coeffsValid = true;
}

void Equalizer::updateSmoothed()
{
    // Primera pasada: se colocan los valores donde estan, sin perseguir nada.
    if (!m_coeffsValid) {
        for (int b = 0; b < kBands; ++b) {
            m_smoothGain[b] = m_gains[b].load(std::memory_order_relaxed);
            m_smoothFreq[b] = m_freqs[b].load(std::memory_order_relaxed);
            m_smoothQ[b]    = m_qs[b].load(std::memory_order_relaxed);
        }
        m_smoothPreamp = dbToLinear(m_preamp.load(std::memory_order_relaxed));
        recomputeFromSmoothed();
        return;
    }

    bool moved = false;
    const double chase = m_chase;

    const auto follow = [&](double& current, double target) {
        const double next = current + (target - current) * chase;
        if (std::abs(next - current) > 1e-9) {
            current = next;
            moved = true;
        } else if (current != target) {
            current = target;      // remate: evita perseguir eternamente
            moved = true;
        }
    };

    for (int b = 0; b < kBands; ++b) {
        follow(m_smoothGain[b], m_gains[b].load(std::memory_order_relaxed));
        follow(m_smoothFreq[b], m_freqs[b].load(std::memory_order_relaxed));
        follow(m_smoothQ[b],    m_qs[b].load(std::memory_order_relaxed));
    }
    follow(m_smoothPreamp, dbToLinear(m_preamp.load(std::memory_order_relaxed)));

    if (moved)
        recomputeFromSmoothed();
}

void Equalizer::process(float* interleaved, unsigned frameCount, int channels)
{
    if (!interleaved || channels <= 0 || frameCount == 0)
        return;

    m_dirty.store(false, std::memory_order_relaxed);

    if (!m_enabled.load(std::memory_order_relaxed)) {
        // Apagado no se toca una sola muestra, pero el estado se deja limpio
        // para que al volver a encenderlo no salte la cola del filtro.
        for (auto& chan : m_state)
            chan.fill(State{});
        m_coeffsValid = false;
        return;
    }

    const int ch = std::min(channels, kMaxChans);

    for (unsigned done = 0; done < frameCount; ) {
        const unsigned block = std::min<unsigned>(kControlBlock, frameCount - done);
        updateSmoothed();

        const double pre = m_smoothPreamp;

        for (unsigned f = 0; f < block; ++f) {
            float* frame = interleaved + size_t(done + f) * size_t(channels);

            for (int c = 0; c < ch; ++c) {
                double x = double(frame[c]) * pre;

                for (int b = 0; b < kBands; ++b) {
                    const Coeffs& k = m_coeffs[b];
                    State& s = m_state[c][b];

                    const double y = k.b0 * x + s.z1;
                    s.z1 = k.b1 * x - k.a1 * y + s.z2;
                    s.z2 = k.b2 * x - k.a2 * y;
                    x = y;
                }

                // Sin recortador: el que habia actuaba en cada pico por encima
                // de 0,9 aunque la curva estuviera plana, asi que ensuciaba
                // cualquier tema bien masterizado. De que no se pase de 0 dBFS
                // se encarga el limitador del final de la cadena.
                frame[c] = float(x);
            }
        }

        done += block;
    }
}

void Equalizer::responseDb(const float* freqHz, float* outDb, int count) const
{
    if (!freqHz || !outDb || count <= 0)
        return;

    if (!m_enabled.load(std::memory_order_relaxed)) {
        std::fill(outDb, outDb + count, 0.0f);
        return;
    }

    // La curva se dibuja con los valores que el usuario ha puesto, no con los
    // perseguidos: si no, al arrastrar un nodo la linea iria por detras del
    // raton.
    std::array<Coeffs, kBands> coeffs;
    for (int b = 0; b < kBands; ++b) {
        coeffs[b] = peaking(m_freqs[b].load(std::memory_order_relaxed),
                            m_sampleRate,
                            m_qs[b].load(std::memory_order_relaxed),
                            m_gains[b].load(std::memory_order_relaxed));
    }
    const double preamp = m_preamp.load(std::memory_order_relaxed);

    for (int i = 0; i < count; ++i) {
        const double w = 2.0 * kPi * freqHz[i] / m_sampleRate;
        const std::complex<double> z1 = std::polar(1.0, -w);
        const std::complex<double> z2 = z1 * z1;

        double magnitude = 1.0;
        for (const Coeffs& k : coeffs) {
            const std::complex<double> num = k.b0 + k.b1 * z1 + k.b2 * z2;
            const std::complex<double> den = 1.0  + k.a1 * z1 + k.a2 * z2;
            if (std::abs(den) > 1e-12)
                magnitude *= std::abs(num / den);
        }

        outDb[i] = float(20.0 * std::log10(std::max(magnitude, 1e-9)) + preamp);
    }
}

float Equalizer::responseDb(float freqHz) const
{
    float db = 0.0f;
    responseDb(&freqHz, &db, 1);
    return db;
}
