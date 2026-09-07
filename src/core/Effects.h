#pragma once

#include <array>
#include <atomic>
#include <vector>

// Cadena de efectos que va detras del ecualizador, al estilo del rack de
// dispositivos de Ableton: una fila de modulos independientes que se encienden
// por separado y se procesan en orden.
//
// Igual que en Equalizer, los parametros viajan en atomics: el hilo de UI
// escribe, el de audio lee y recalcula lo derivado solo cuando hay cambios.
// Toda la memoria se reserva en prepare(); el callback de audio no asigna.
class Effects {
public:
    enum Device {
        Compressor = 0,   // control de dinamica
        Saturator,        // saturacion suave
        Chorus,
        Delay,            // eco con realimentacion
        Reverb,
        Width,            // amplitud estereo
        DeviceCount
    };

    static constexpr int kMaxParams = 3;
    static constexpr int kMaxChans  = 2;

    struct ParamInfo {
        const char* name;
        float       minimum;
        float       maximum;
        float       defaultValue;
        const char* suffix;
        int         decimals;
    };

    struct DeviceInfo {
        const char* id;      // clave de ajustes, sin acentos ni espacios
        const char* name;    // rotulo visible (espanol, se traduce en la UI)
        const char* hint;
        int         paramCount;
        ParamInfo   params[kMaxParams];
    };

    static const DeviceInfo& info(int device);

    Effects();

    void prepare(double sampleRate);

    void  setDeviceEnabled(int device, bool on);
    bool  isDeviceEnabled(int device) const;
    bool  isAnyEnabled() const;

    void  setParam(int device, int param, float value);
    float param(int device, int param) const;

    void  resetAll();
    void  resetDevice(int device);

    // --- hilo de audio -----------------------------------------------------
    void process(float* interleaved, unsigned frameCount, int channels);

private:
    void recompute();
    void clearBuffers();

    // Lineas de retardo simples con lectura fraccionaria.
    struct DelayLine {
        std::vector<float> data;
        int write = 0;

        void  resize(int samples);
        void  clear();
        void  push(float sample);
        float readAt(float delaySamples) const;   // interpolacion lineal
        float read(int delaySamples) const;
    };

    // Reverberacion tipo Freeverb: ocho peines en paralelo y cuatro
    // pasa-todo en serie por canal.
    struct Comb {
        std::vector<float> data;
        int   index = 0;
        float store = 0.0f;
        void  resize(int n);
        void  clear();
        float process(float input, float feedback, float damp);
    };

    struct Allpass {
        std::vector<float> data;
        int   index = 0;
        void  resize(int n);
        void  clear();
        float process(float input);
    };

    double m_sampleRate = 48000.0;

    std::array<std::atomic<bool>, DeviceCount> m_enabled;
    std::array<std::array<std::atomic<float>, kMaxParams>, DeviceCount> m_params;
    std::atomic<bool> m_dirty{true};

    // --- estado del hilo de audio -----------------------------------------
    struct Cache {
        // compresor
        float threshDb = -18.0f, ratio = 3.0f, makeup = 1.0f;
        float attack = 0.0f, release = 0.0f;
        // saturador
        float drive = 1.0f, driveNorm = 1.0f, satMix = 0.0f;
        // chorus
        float chorusStep = 0.0f, chorusDepth = 0.0f, chorusMix = 0.0f, chorusBase = 0.0f;
        // eco
        float delaySamples = 0.0f, feedback = 0.0f, delayMix = 0.0f;
        // reverberacion
        float roomSize = 0.0f, damp = 0.0f, reverbMix = 0.0f;
        // estereo
        float width = 1.0f;
    } m_cache;

    float m_envelope = 0.0f;    // detector del compresor
    float m_compGain = 1.0f;
    float m_lfoPhase = 0.0f;

    std::array<DelayLine, kMaxChans> m_delay;
    std::array<DelayLine, kMaxChans> m_chorus;
    std::array<std::array<Comb, 8>, kMaxChans>    m_combs;
    std::array<std::array<Allpass, 4>, kMaxChans> m_allpass;
};
