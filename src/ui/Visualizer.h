#pragma once

#include <QVector>
#include <QWidget>

class AudioEngine;
class QTimer;

// Analizador del panel izquierdo. Reproduce el dibujo de la referencia:
// barras verticales finas espejadas respecto al centro. Un clic alterna
// entre forma de onda y espectro (FFT).
class Visualizer : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Waveform, Spectrum };

    explicit Visualizer(AudioEngine* engine, QWidget* parent = nullptr);

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void refresh();

private:
    void computeWaveform();
    void computeSpectrum();

    AudioEngine* m_engine = nullptr;
    QTimer*      m_timer  = nullptr;
    Mode         m_mode   = Mode::Waveform;
    bool         m_active = false;

    QVector<float> m_samples;   // muestras crudas del motor
    QVector<float> m_bars;      // alturas normalizadas 0..1 ya suavizadas
};
