#pragma once

#include <array>
#include <atomic>

// Ecualizador parametrico de 8 bandas.
//
// Cada banda es un filtro peaking-EQ biquad (formulas RBJ Audio-EQ-Cookbook)
// en cascada, precedido de un preamplificador. A diferencia de un ecualizador
// grafico clasico, aqui la frecuencia y el Q de cada banda tambien son
// ajustables: es lo que permite arrastrar los nodos en los dos ejes sobre la
// curva, al estilo del EQ Eight de Ableton.
//
// Las tres magnitudes se publican con atomics: el hilo de UI escribe, el hilo
// de audio recalcula los coeficientes solo cuando hay cambios, sin bloqueos ni
// asignaciones de memoria.
class Equalizer {
public:
    static constexpr int kBands    = 8;
    static constexpr int kMaxChans = 8;

    static constexpr float kRangeDb       = 15.0f;   // +/- por banda
    static constexpr float kPreampRangeDb = 12.0f;
    static constexpr float kMinHz         = 30.0f;
    static constexpr float kMaxHz         = 18000.0f;
    static constexpr float kMinQ          = 0.30f;
    static constexpr float kMaxQ          = 8.00f;
    static constexpr float kDefaultQ      = 1.20f;

    // Reparto inicial de las 8 bandas, de grave a agudo.
    static const std::array<float, kBands>& defaultFrequencies();

    Equalizer();

    void prepare(double sampleRate);
    double sampleRate() const { return m_sampleRate; }

    void  setEnabled(bool on)  { m_enabled.store(on, std::memory_order_relaxed); }
    bool  isEnabled() const    { return m_enabled.load(std::memory_order_relaxed); }

    void  setGain(int band, float dB);
    float gain(int band) const;

    void  setFrequency(int band, float hz);
    float frequency(int band) const;

    void  setQ(int band, float q);
    float q(int band) const;

    void  setPreamp(float dB);
    float preamp() const;

    // Devuelve todas las bandas a su reparto y ganancia inicial.
    void  resetBands();
    void  resetBand(int band);

    // --- hilo de audio -----------------------------------------------------
    void process(float* interleaved, unsigned frameCount, int channels);

    // --- hilo de UI: respuesta en magnitud (dB) para dibujar la curva ------
    float responseDb(float freqHz) const;

private:
    struct Coeffs { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State  { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

    void recompute();
    static Coeffs peaking(double freq, double sampleRate, double q, double gainDb);

    double m_sampleRate = 48000.0;

    std::array<std::atomic<float>, kBands> m_gains;
    std::array<std::atomic<float>, kBands> m_freqs;
    std::array<std::atomic<float>, kBands> m_qs;
    std::atomic<float> m_preamp{0.0f};
    std::atomic<bool>  m_enabled{false};
    std::atomic<bool>  m_dirty{true};

    // Solo los toca el hilo de audio.
    std::array<Coeffs, kBands> m_coeffs;
    std::array<std::array<State, kBands>, kMaxChans> m_state{};
    float m_preampLinear = 1.0f;
};
