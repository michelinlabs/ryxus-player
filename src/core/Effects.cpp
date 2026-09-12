#include "core/Effects.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Tamanos clasicos de Freeverb, medidos a 44100 Hz. Se reescalan a la
// frecuencia real en prepare().
constexpr int kCombTuning[8]    = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
constexpr int kAllpassTuning[4] = {556, 441, 341, 225};
constexpr int kStereoSpread     = 23;

inline float dbToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }

// Evita denormales, que en el hilo de audio cuestan mas que el propio filtro.
inline float undenormal(float v) { return std::abs(v) < 1e-20f ? 0.0f : v; }

const Effects::DeviceInfo kDevices[Effects::DeviceCount] = {
    {"compressor", "Compresor", "Iguala la dinamica y levanta lo que queda bajo", 3, {
        {"Umbral",   -48.0f,   0.0f, -18.0f, " dB", 0},
        {"Ratio",      1.0f,  12.0f,   3.0f, ":1",  1},
        {"Ganancia",   0.0f,  18.0f,   3.0f, " dB", 1},
    }},
    {"saturator", "Saturacion", "Calienta la senal con distorsion suave", 2, {
        {"Drive",      0.0f,  24.0f,   8.0f, " dB", 1},
        {"Mezcla",     0.0f, 100.0f,  35.0f, " %",  0},
        {nullptr, 0.0f, 0.0f, 0.0f, nullptr, 0},
    }},
    {"chorus", "Chorus", "Duplica la senal y la desafina un poco", 3, {
        {"Velocidad",  0.05f,  6.0f,   0.6f, " Hz", 2},
        {"Profundo",   0.0f, 100.0f,  45.0f, " %",  0},
        {"Mezcla",     0.0f, 100.0f,  30.0f, " %",  0},
    }},
    {"delay", "Eco", "Repeticiones con realimentacion", 3, {
        {"Tiempo",    20.0f,1000.0f, 330.0f, " ms", 0},
        {"Feedback",   0.0f,  90.0f,  35.0f, " %",  0},
        {"Mezcla",     0.0f, 100.0f,  25.0f, " %",  0},
    }},
    {"reverb", "Reverberacion", "Cola de sala, de cabina a nave", 3, {
        {"Tamano",     0.0f, 100.0f,  55.0f, " %",  0},
        {"Amortigua",  0.0f, 100.0f,  45.0f, " %",  0},
        {"Mezcla",     0.0f, 100.0f,  25.0f, " %",  0},
    }},
    {"width", "Estereo", "Abre o cierra la imagen estereo", 1, {
        {"Amplitud",   0.0f, 200.0f, 130.0f, " %",  0},
        {nullptr, 0.0f, 0.0f, 0.0f, nullptr, 0},
        {nullptr, 0.0f, 0.0f, 0.0f, nullptr, 0},
    }},
};

} // namespace

const Effects::DeviceInfo& Effects::info(int device)
{
    return kDevices[std::clamp(device, 0, int(DeviceCount) - 1)];
}

// ------------------------------------------------------------- lineas base

void Effects::DelayLine::resize(int samples)
{
    data.assign(size_t(std::max(samples, 4)), 0.0f);
    write = 0;
}

void Effects::DelayLine::clear()
{
    std::fill(data.begin(), data.end(), 0.0f);
    write = 0;
}

void Effects::DelayLine::push(float sample)
{
    if (data.empty())
        return;
    data[size_t(write)] = sample;
    write = (write + 1) % int(data.size());
}

float Effects::DelayLine::read(int delaySamples) const
{
    if (data.empty())
        return 0.0f;
    const int n = int(data.size());
    const int index = ((write - std::clamp(delaySamples, 1, n - 1)) % n + n) % n;
    return data[size_t(index)];
}

float Effects::DelayLine::readAt(float delaySamples) const
{
    if (data.size() < 4)
        return 0.0f;
    const int   n = int(data.size());
    const float d = std::clamp(delaySamples, 1.0f, float(n - 2));
    const int   whole = int(d);
    const float frac  = d - float(whole);
    const float a = read(whole);
    const float b = read(whole + 1);
    return a + (b - a) * frac;
}

void Effects::Comb::resize(int n)
{
    data.assign(size_t(std::max(n, 4)), 0.0f);
    index = 0;
    store = 0.0f;
}

void Effects::Comb::clear()
{
    std::fill(data.begin(), data.end(), 0.0f);
    index = 0;
    store = 0.0f;
}

float Effects::Comb::process(float input, float feedback, float damp)
{
    const float output = data[size_t(index)];
    store = undenormal(output * (1.0f - damp) + store * damp);
    data[size_t(index)] = undenormal(input + store * feedback);
    index = (index + 1) % int(data.size());
    return output;
}

void Effects::Allpass::resize(int n)
{
    data.assign(size_t(std::max(n, 4)), 0.0f);
    index = 0;
}

void Effects::Allpass::clear()
{
    std::fill(data.begin(), data.end(), 0.0f);
    index = 0;
}

float Effects::Allpass::process(float input)
{
    const float buffered = data[size_t(index)];
    const float output   = buffered - input;
    data[size_t(index)]  = undenormal(input + buffered * 0.5f);
    index = (index + 1) % int(data.size());
    return output;
}

// ------------------------------------------------------------------ estado

Effects::Effects()
{
    for (int d = 0; d < DeviceCount; ++d) {
        m_enabled[size_t(d)].store(false, std::memory_order_relaxed);
        const DeviceInfo& di = info(d);
        for (int p = 0; p < kMaxParams; ++p) {
            m_params[size_t(d)][size_t(p)].store(
                p < di.paramCount ? di.params[p].defaultValue : 0.0f,
                std::memory_order_relaxed);
        }
    }
    prepare(48000.0);
}

void Effects::prepare(double sampleRate)
{
    if (sampleRate > 0.0)
        m_sampleRate = sampleRate;

    const double scale = m_sampleRate / 44100.0;

    for (int c = 0; c < kMaxChans; ++c) {
        m_delay[size_t(c)].resize(int(m_sampleRate * 1.2) + 4);    // hasta 1,2 s
        m_chorus[size_t(c)].resize(int(m_sampleRate * 0.06) + 4);  // hasta 60 ms

        const int spread = (c == 0) ? 0 : kStereoSpread;
        for (int i = 0; i < 8; ++i)
            m_combs[size_t(c)][size_t(i)].resize(int(kCombTuning[i] * scale) + spread);
        for (int i = 0; i < 4; ++i)
            m_allpass[size_t(c)][size_t(i)].resize(int(kAllpassTuning[i] * scale) + spread);
    }

    clearBuffers();
    m_envelope = 0.0f;
    m_compGain = 1.0f;
    m_lfoPhase = 0.0f;
    m_dirty.store(true, std::memory_order_release);
}

void Effects::clearBuffers()
{
    for (int c = 0; c < kMaxChans; ++c) {
        m_delay[size_t(c)].clear();
        m_chorus[size_t(c)].clear();
        for (Comb& comb : m_combs[size_t(c)])
            comb.clear();
        for (Allpass& ap : m_allpass[size_t(c)])
            ap.clear();
    }
}

void Effects::setDeviceEnabled(int device, bool on)
{
    if (device < 0 || device >= DeviceCount)
        return;
    m_enabled[size_t(device)].store(on, std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

bool Effects::isDeviceEnabled(int device) const
{
    if (device < 0 || device >= DeviceCount)
        return false;
    return m_enabled[size_t(device)].load(std::memory_order_relaxed);
}

bool Effects::isAnyEnabled() const
{
    for (int d = 0; d < DeviceCount; ++d)
        if (m_enabled[size_t(d)].load(std::memory_order_relaxed))
            return true;
    return false;
}

void Effects::setParam(int device, int paramIndex, float value)
{
    if (device < 0 || device >= DeviceCount)
        return;
    const DeviceInfo& di = info(device);
    if (paramIndex < 0 || paramIndex >= di.paramCount)
        return;

    const ParamInfo& pi = di.params[paramIndex];
    m_params[size_t(device)][size_t(paramIndex)].store(
        std::clamp(value, pi.minimum, pi.maximum), std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

float Effects::param(int device, int paramIndex) const
{
    if (device < 0 || device >= DeviceCount)
        return 0.0f;
    if (paramIndex < 0 || paramIndex >= kMaxParams)
        return 0.0f;
    return m_params[size_t(device)][size_t(paramIndex)].load(std::memory_order_relaxed);
}

void Effects::resetDevice(int device)
{
    if (device < 0 || device >= DeviceCount)
        return;
    const DeviceInfo& di = info(device);
    for (int p = 0; p < di.paramCount; ++p)
        m_params[size_t(device)][size_t(p)].store(di.params[p].defaultValue,
                                                  std::memory_order_relaxed);
    m_dirty.store(true, std::memory_order_release);
}

void Effects::resetAll()
{
    for (int d = 0; d < DeviceCount; ++d) {
        m_enabled[size_t(d)].store(false, std::memory_order_relaxed);
        resetDevice(d);
    }
    m_dirty.store(true, std::memory_order_release);
}

void Effects::recompute()
{
    const auto value = [this](int d, int p) {
        return m_params[size_t(d)][size_t(p)].load(std::memory_order_relaxed);
    };

    m_cache.threshDb = value(Compressor, 0);
    m_cache.ratio    = std::max(1.0f, value(Compressor, 1));
    m_cache.makeup   = dbToLinear(value(Compressor, 2));
    m_cache.attack   = float(std::exp(-1.0 / (0.010 * m_sampleRate)));   // 10 ms
    m_cache.release  = float(std::exp(-1.0 / (0.150 * m_sampleRate)));   // 150 ms

    m_cache.drive     = dbToLinear(value(Saturator, 0));
    m_cache.driveNorm = 1.0f / std::tanh(std::max(m_cache.drive, 1.0f));
    m_cache.satMix    = value(Saturator, 1) / 100.0f;

    m_cache.chorusStep  = float(2.0 * kPi * double(value(Chorus, 0)) / m_sampleRate);
    m_cache.chorusDepth = float(double(value(Chorus, 1)) / 100.0 * 0.006 * m_sampleRate);
    m_cache.chorusMix   = value(Chorus, 2) / 100.0f;
    m_cache.chorusBase  = float(0.014 * m_sampleRate);

    m_cache.delaySamples = float(double(value(Delay, 0)) / 1000.0 * m_sampleRate);
    m_cache.feedback     = value(Delay, 1) / 100.0f;
    m_cache.delayMix     = value(Delay, 2) / 100.0f;

    m_cache.roomSize  = 0.70f + value(Reverb, 0) / 100.0f * 0.283f;
    m_cache.damp      = value(Reverb, 1) / 100.0f * 0.4f;
    m_cache.reverbMix = value(Reverb, 2) / 100.0f;

    m_cache.width = value(Width, 0) / 100.0f;
}

// ------------------------------------------------------------ hilo de audio

void Effects::process(float* interleaved, unsigned frameCount, int channels)
{
    if (!interleaved || channels <= 0 || frameCount == 0)
        return;

    if (m_dirty.exchange(false, std::memory_order_acquire))
        recompute();

    const bool comp   = isDeviceEnabled(Compressor);
    const bool sat    = isDeviceEnabled(Saturator);
    const bool chorus = isDeviceEnabled(Chorus);
    const bool delay  = isDeviceEnabled(Delay);
    const bool reverb = isDeviceEnabled(Reverb);
    const bool width  = isDeviceEnabled(Width);

    if (!(comp || sat || chorus || delay || reverb || width))
        return;

    const int ch = std::min(channels, kMaxChans);

    for (unsigned f = 0; f < frameCount; ++f) {
        float* frame = interleaved + size_t(f) * size_t(channels);

        // --- compresor: deteccion enlazada entre canales -------------------
        if (comp) {
            float peak = 0.0f;
            for (int c = 0; c < ch; ++c)
                peak = std::max(peak, std::abs(frame[c]));

            const float coeff = (peak > m_envelope) ? m_cache.attack : m_cache.release;
            m_envelope = undenormal(peak + coeff * (m_envelope - peak));

            const float levelDb = 20.0f * std::log10(std::max(m_envelope, 1e-6f));
            const float overDb  = levelDb - m_cache.threshDb;
            const float slope   = 1.0f - 1.0f / m_cache.ratio;

            // Rodilla suave de 6 dB: sin ella la compresion entra de golpe al
            // cruzar el umbral y se oye el salto en cada transitorio.
            constexpr float kKnee = 6.0f;
            float cutDb;
            if (overDb <= -kKnee * 0.5f) {
                cutDb = 0.0f;
            } else if (overDb >= kKnee * 0.5f) {
                cutDb = overDb * slope;
            } else {
                const float t = overDb + kKnee * 0.5f;
                cutDb = slope * t * t / (2.0f * kKnee);
            }

            // La ganancia tambien se persigue, no se salta: el detector ya va
            // suavizado, pero la curva de la rodilla puede moverse rapido.
            const float targetGain = dbToLinear(-cutDb) * m_cache.makeup;
            m_compGain += (targetGain - m_compGain) * 0.25f;

            for (int c = 0; c < ch; ++c)
                frame[c] *= m_compGain;
        }

        // --- saturacion ----------------------------------------------------
        if (sat) {
            for (int c = 0; c < ch; ++c) {
                const float wet = std::tanh(frame[c] * m_cache.drive) * m_cache.driveNorm;
                frame[c] += (wet - frame[c]) * m_cache.satMix;
            }
        }

        // --- chorus: retardo modulado, LFO en cuadratura entre canales -----
        if (chorus) {
            m_lfoPhase += m_cache.chorusStep;
            if (m_lfoPhase > float(2.0 * kPi))
                m_lfoPhase -= float(2.0 * kPi);

            for (int c = 0; c < ch; ++c) {
                const float phase = m_lfoPhase + (c == 1 ? float(kPi / 2.0) : 0.0f);
                const float taps  = m_cache.chorusBase
                                  + m_cache.chorusDepth * (0.5f + 0.5f * std::sin(phase));
                DelayLine& line = m_chorus[size_t(c)];
                const float wet = line.readAt(taps);
                line.push(frame[c]);
                frame[c] += (wet - frame[c]) * m_cache.chorusMix;
            }
        }

        // --- eco -----------------------------------------------------------
        if (delay) {
            for (int c = 0; c < ch; ++c) {
                DelayLine& line = m_delay[size_t(c)];
                const float echo = line.readAt(m_cache.delaySamples);
                line.push(undenormal(frame[c] + echo * m_cache.feedback));
                frame[c] += echo * m_cache.delayMix;
            }
        }

        // --- reverberacion -------------------------------------------------
        if (reverb) {
            for (int c = 0; c < ch; ++c) {
                const float input = frame[c] * 0.015f;
                float wet = 0.0f;
                for (Comb& comb : m_combs[size_t(c)])
                    wet += comb.process(input, m_cache.roomSize, m_cache.damp);
                for (Allpass& ap : m_allpass[size_t(c)])
                    wet = ap.process(wet);
                frame[c] += (wet - frame[c]) * m_cache.reverbMix;
            }
        }

        // --- amplitud estereo (medio / lado) -------------------------------
        if (width && ch >= 2) {
            const float mid  = (frame[0] + frame[1]) * 0.5f;
            const float side = (frame[0] - frame[1]) * 0.5f * m_cache.width;
            frame[0] = mid + side;
            frame[1] = mid - side;
        }

        // Sin recortador propio: encadenar realce, eco y reverberacion puede
        // pasarse de 0 dBFS, pero de eso se ocupa el limitador del final de la
        // cadena, que solo actua cuando de verdad hace falta.
    }
}
