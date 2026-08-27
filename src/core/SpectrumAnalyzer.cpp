#include "core/SpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float  kMinHz = 20.0f;
constexpr float  kMaxHz = 20000.0f;

// FFT iterativa radix-2 in-place sobre arrays separados de parte real e
// imaginaria. Con 2048 puntos cuesta microsegundos, asi que no compensa
// arrastrar una dependencia externa.
void fft(std::vector<float>& re, std::vector<float>& im)
{
    const size_t n = re.size();
    if (n < 2)
        return;

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * kPi / double(len);
        const float wRe = float(std::cos(angle));
        const float wIm = float(std::sin(angle));

        for (size_t i = 0; i < n; i += len) {
            float curRe = 1.0f, curIm = 0.0f;
            for (size_t k = 0; k < len / 2; ++k) {
                const size_t a = i + k;
                const size_t b = a + len / 2;

                const float tRe = re[b] * curRe - im[b] * curIm;
                const float tIm = re[b] * curIm + im[b] * curRe;

                re[b] = re[a] - tRe;
                im[b] = im[a] - tIm;
                re[a] += tRe;
                im[a] += tIm;

                const float nextRe = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = nextRe;
            }
        }
    }
}

} // namespace

SpectrumAnalyzer::SpectrumAnalyzer(int bandCount)
{
    m_window.resize(kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        m_window[i] = float(0.5 * (1.0 - std::cos(2.0 * kPi * i / (kFftSize - 1))));

    m_scratchRe.resize(kFftSize);
    m_scratchIm.resize(kFftSize);

    setBandCount(bandCount);
}

void SpectrumAnalyzer::setBandCount(int count)
{
    count = std::clamp(count, 8, 1024);
    if (int(m_levels.size()) == count)
        return;
    m_levels.assign(size_t(count), 0.0f);
    rebuildBandEdges();
}

void SpectrumAnalyzer::setDecay(float decay)
{
    m_decay = std::clamp(decay, 0.01f, 1.0f);
}

void SpectrumAnalyzer::setFloorDb(float floorDb)
{
    m_floorDb = std::min(floorDb, -10.0f);
}

void SpectrumAnalyzer::clear()
{
    std::fill(m_levels.begin(), m_levels.end(), 0.0f);
}

void SpectrumAnalyzer::rebuildBandEdges()
{
    const int bands = int(m_levels.size());
    m_binStart.assign(size_t(bands), 1);
    m_binEnd.assign(size_t(bands), 2);
    m_binCenter.assign(size_t(bands), 1.0f);

    const int usableBins = kFftSize / 2;
    const double binHz   = m_sampleRate / kFftSize;

    for (int i = 0; i < bands; ++i) {
        // Reparto logaritmico: si fuera lineal, todo el grave caeria en una
        // sola banda y los agudos ocuparian tres cuartos del ancho.
        const double t0 = double(i) / bands;
        const double t1 = double(i + 1) / bands;
        const double lowHz  = kMinHz * std::pow(double(kMaxHz) / kMinHz, t0);
        const double highHz = kMinHz * std::pow(double(kMaxHz) / kMinHz, t1);

        int lo = std::clamp(int(lowHz / binHz), 1, usableBins - 1);
        int hi = std::clamp(int(highHz / binHz) + 1, lo + 1, usableBins);

        m_binStart[size_t(i)]  = lo;
        m_binEnd[size_t(i)]    = hi;
        m_binCenter[size_t(i)] = float(std::sqrt(lowHz * highHz) / binHz);
    }
}

float SpectrumAnalyzer::bandFrequency(int index) const
{
    const int bands = int(m_levels.size());
    if (bands <= 0)
        return kMinHz;
    const double t = (double(index) + 0.5) / bands;
    return float(kMinHz * std::pow(double(kMaxHz) / kMinHz, t));
}

void SpectrumAnalyzer::update(const float* samples, int count)
{
    if (!samples || count < kFftSize || m_levels.empty())
        return;

    // Los bordes de banda dependen de la frecuencia de muestreo; si cambio,
    // se recalculan aqui (es barato y evita tener que acordarse de llamarlo).
    if (std::abs(m_edgeRate - m_sampleRate) > 0.5) {
        rebuildBandEdges();
        m_edgeRate = m_sampleRate;
    }

    for (int i = 0; i < kFftSize; ++i) {
        m_scratchRe[size_t(i)] = samples[count - kFftSize + i] * m_window[size_t(i)];
        m_scratchIm[size_t(i)] = 0.0f;
    }

    fft(m_scratchRe, m_scratchIm);

    // Normalizacion: la ventana de Hann tiene ganancia coherente 0.5, y la FFT
    // reparte la energia entre N bins.
    const float norm = 2.0f / (kFftSize * 0.5f);
    const float range = -m_floorDb;
    const int usableBins = kFftSize / 2;

    const auto binMagnitude = [this](int bin) {
        const float re = m_scratchRe[size_t(bin)];
        const float im = m_scratchIm[size_t(bin)];
        return std::sqrt(re * re + im * im);
    };

    for (size_t i = 0; i < m_levels.size(); ++i) {
        float magnitude = 0.0f;

        if (m_binEnd[i] - m_binStart[i] >= 2) {
            // Banda ancha: se queda con el pico del tramo.
            for (int bin = m_binStart[i]; bin < m_binEnd[i]; ++bin)
                magnitude = std::max(magnitude, binMagnitude(bin));
        } else {
            // En graves una banda cabe dentro de un solo bin, y repetir el
            // mismo valor en bandas contiguas dibuja escalones. Se interpola
            // entre bins vecinos usando el centro fraccionario de la banda.
            const float center = m_binCenter[i];
            const int lo = std::clamp(int(center), 1, usableBins - 2);
            const float frac = std::clamp(center - float(lo), 0.0f, 1.0f);
            magnitude = binMagnitude(lo) * (1.0f - frac)
                      + binMagnitude(lo + 1) * frac;
        }

        const float db = 20.0f * std::log10(std::max(magnitude * norm, 1e-7f));
        const float level = std::clamp((db - m_floorDb) / range, 0.0f, 1.0f);

        // Ataque instantaneo, caida suave: es lo que da la sensacion de "vivo".
        m_levels[i] = (level > m_levels[i]) ? level
                                            : m_levels[i] * (1.0f - m_decay);
    }
}

float SpectrumAnalyzer::levelAtFrequency(float hz) const
{
    const int bands = int(m_levels.size());
    if (bands <= 0)
        return 0.0f;

    hz = std::clamp(hz, kMinHz, kMaxHz);
    const double t = std::log(double(hz) / kMinHz) / std::log(double(kMaxHz) / kMinHz);
    const double pos = t * bands - 0.5;

    const int i0 = std::clamp(int(std::floor(pos)), 0, bands - 1);
    const int i1 = std::clamp(i0 + 1, 0, bands - 1);
    const float frac = float(pos - std::floor(pos));

    return m_levels[size_t(i0)] * (1.0f - frac) + m_levels[size_t(i1)] * frac;
}
