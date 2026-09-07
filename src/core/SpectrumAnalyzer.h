#pragma once

#include <vector>

// Analizador de espectro por FFT, compartido por el visualizador del panel
// izquierdo y el fondo del plot del ecualizador.
//
// No es un QObject a proposito: se le entregan muestras y devuelve niveles,
// sin dependencias de Qt ni de hilos.
class SpectrumAnalyzer {
public:
    // 16384 puntos = 2,9 Hz por bin a 48 kHz, el doble de resolucion que los
    // 8192 de antes. Es lo que se nota en los graves: por debajo de 100 Hz una
    // banda del reparto logaritmico cabia dentro de un solo bin y la linea
    // salia a tramos rectos.
    static constexpr int kFftSize = 16384;

    explicit SpectrumAnalyzer(int bandCount = 512);

    void setBandCount(int count);
    int  bandCount() const { return int(m_levels.size()); }

    void setSampleRate(double rate) { m_sampleRate = rate; }
    void setDecay(float decay);          // 0..1, caida por actualizacion
    void setFloorDb(float floorDb);      // suelo de la escala, p. ej. -75 dB

    // `samples` debe traer al menos kFftSize muestras mono.
    void update(const float* samples, int count);
    void clear();

    // Nivel normalizado 0..1 de cada banda, ya suavizado.
    const std::vector<float>& levels() const { return m_levels; }

    // Frecuencia central (Hz) de la banda `index`, en reparto logaritmico.
    float bandFrequency(int index) const;

    // Nivel 0..1 interpolado en una frecuencia arbitraria; lo usa el plot del
    // ecualizador, cuyo eje X no coincide con el reparto de bandas.
    float levelAtFrequency(float hz) const;

private:
    void rebuildBandEdges();

    double m_sampleRate = 48000.0;
    double m_edgeRate   = 0.0;   // frecuencia con la que se calcularon los bordes
    float  m_decay      = 0.30f;
    float  m_floorDb    = -75.0f;

    std::vector<float> m_levels;
    std::vector<int>   m_binStart;   // primer bin de FFT de cada banda
    std::vector<int>   m_binEnd;
    std::vector<float> m_binCenter;  // bin fraccionario del centro de banda
    std::vector<float> m_window;     // ventana de Hann precalculada
    std::vector<float> m_scratchRe;
    std::vector<float> m_scratchIm;
};
