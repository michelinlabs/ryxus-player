#include "core/Limiter.h"

#include <algorithm>
#include <cmath>

namespace {

// Ventana de anticipacion, en milisegundos. Con 1,5 ms la ganancia ya ha
// bajado del todo cuando el pico llega a la salida, y el retardo es
// imperceptible incluso al sincronizar con video.
constexpr double kLookaheadMs = 1.5;

// La recuperacion tiene que ser lenta o se oye "bombear" el grave.
constexpr double kReleaseMs = 120.0;

} // namespace

void Limiter::prepare(double sampleRate, int channels)
{
    if (sampleRate > 0.0)
        m_sampleRate = sampleRate;
    m_channels = std::clamp(channels, 1, kMaxChans);

    m_lookahead = std::max(1, int(m_sampleRate * kLookaheadMs / 1000.0));
    m_delay.assign(size_t(m_lookahead) * size_t(m_channels), 0.0f);
    m_desired.assign(size_t(m_lookahead), 1.0);
    m_write = 0;

    m_release = 1.0 - std::exp(-1.0 / (kReleaseMs / 1000.0 * m_sampleRate));

    m_gain = 1.0;
    m_fall = 0.0;
}

void Limiter::reset()
{
    std::fill(m_delay.begin(), m_delay.end(), 0.0f);
    std::fill(m_desired.begin(), m_desired.end(), 1.0);
    m_write = 0;
    m_gain  = 1.0;
    m_fall  = 0.0;
}

void Limiter::setThresholdDb(double db)
{
    m_threshold = std::clamp(std::pow(10.0, db / 20.0), 0.05, 1.0);
}

double Limiter::reductionDb() const
{
    return m_gain >= 1.0 ? 0.0 : -20.0 * std::log10(std::max(m_gain, 1e-6));
}

void Limiter::process(float* interleaved, unsigned frameCount, int channels)
{
    if (!interleaved || frameCount == 0 || channels <= 0)
        return;

    // Si cambia el numero de canales a mitad de sesion, se rehace el buffer.
    if (channels != m_channels || m_delay.empty())
        prepare(m_sampleRate, channels);

    const int ch = m_channels;

    for (unsigned f = 0; f < frameCount; ++f) {
        float* frame = interleaved + size_t(f) * size_t(channels);

        // 1) ganancia que pediria esta muestra por si sola.
        double peak = 0.0;
        for (int c = 0; c < ch; ++c)
            peak = std::max(peak, std::abs(double(frame[c])));

        m_desired[size_t(m_write)] = (peak > m_threshold) ? (m_threshold / peak) : 1.0;

        // 2) la que hace falta para TODA la ventana de anticipacion. Mirar el
        //    minimo de la ventana, y no solo la muestra que entra, es lo que
        //    permite empezar a bajar antes de que llegue el pico.
        double target = 1.0;
        for (double g : m_desired)
            target = std::min(target, g);

        // 3) al bajar se usa una rampa que cubre la distancia en lo que dura la
        //    ventana: asi la ganancia esta puesta justo cuando el pico sale, y
        //    nunca se desborda. Al subir, despacio, para que no se oiga bombear.
        if (target < m_gain) {
            const double needed = (m_gain - target) / double(m_lookahead);
            m_fall = std::max(m_fall, needed);
            m_gain = std::max(target, m_gain - m_fall);
        } else {
            m_fall = 0.0;
            m_gain += (target - m_gain) * m_release;
        }

        // 4) sale la muestra retrasada, ya con la ganancia puesta.
        const size_t slot = size_t(m_write) * size_t(ch);
        for (int c = 0; c < ch; ++c) {
            const float delayed = m_delay[slot + size_t(c)];
            m_delay[slot + size_t(c)] = frame[c];
            frame[c] = float(double(delayed) * m_gain);
        }

        m_write = (m_write + 1) % m_lookahead;
    }
}
