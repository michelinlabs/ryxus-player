#pragma once

#include <vector>

// Limitador de picos al final de la cadena.
//
// Existe para una sola cosa: que realzar el ecualizador o encadenar efectos no
// pueda pasarse de 0 dBFS. Por debajo del umbral la ganancia es exactamente 1
// y no toca una sola muestra, asi que no es un "saturador suave" siempre
// puesto -- que es justo lo que tenia antes el ecualizador y ensuciaba
// cualquier tema bien masterizado aunque la curva estuviera plana.
//
// Lleva una ventana de anticipacion corta para poder bajar la ganancia *antes*
// de que llegue el pico, en vez de perseguirlo cuando ya ha recortado. El
// retardo que introduce es de poco mas de un milisegundo.
class Limiter {
public:
    static constexpr int kMaxChans = 8;

    void prepare(double sampleRate, int channels);
    void reset();

    // Umbral en dBFS. Por defecto -0,2 dB, que deja margen para la conversion
    // a entero de la tarjeta sin que se note.
    void setThresholdDb(double db);

    // Cuanta reduccion esta aplicando ahora mismo, en dB (>= 0). Solo para
    // mostrarla; no hace falta que sea exacta.
    double reductionDb() const;

    void process(float* interleaved, unsigned frameCount, int channels);

private:
    double m_sampleRate = 48000.0;
    int    m_channels   = 2;

    double m_threshold = 0.977;   // -0,2 dBFS en lineal
    double m_gain      = 1.0;     // ganancia aplicada, 0..1
    double m_release   = 0.0;     // persecucion al recuperar
    double m_fall      = 0.0;     // rampa de bajada en curso

    // Retardo de anticipacion: entra la senal, sale un pelin despues, ya con
    // la ganancia bajada si hacia falta.
    std::vector<float>  m_delay;    // intercalado, m_lookahead * canales
    std::vector<double> m_desired;  // ganancia pedida por cada muestra de la ventana
    int m_lookahead = 0;
    int m_write     = 0;
};
