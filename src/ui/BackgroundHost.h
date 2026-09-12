#pragma once

#include "core/SpectrumAnalyzer.h"
#include "ui/Visualizations.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QWidget>

#include <vector>

class AudioEngine;
class QMovie;
class QTimer;

// Widget central de la ventana. Pinta el color base del skin y, encima, el
// fondo elegido por el usuario. Los paneles que van sobre el se dibujan con
// alfa (ver Theme::setPanelTransparency), asi que el fondo se percibe a traves
// de toda la interfaz.
//
// El fondo puede ser una imagen fija, una animada (GIF o WEBP), o nada. Y por
// encima de cualquiera de las tres se puede superponer un vumetro que responde
// a lo que esta sonando.
class BackgroundHost : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Cover, Fit, Stretch, Tile };
    Q_ENUM(Mode)

    explicit BackgroundHost(QWidget* parent = nullptr);
    ~BackgroundHost() override;

    bool    setImagePath(const QString& path);   // vacio = sin imagen
    QString imagePath() const { return m_path; }
    bool    hasImage() const  { return !m_source.isNull() || m_movie != nullptr; }
    bool    isAnimated() const { return m_movie != nullptr; }

    // Hay algo detras de los paneles que merezca verse a traves de ellos.
    bool    hasBackdrop() const { return hasImage() || visualizerEnabled(); }

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // Velo oscuro sobre la imagen (0-90 %). Sin el, una foto clara deja el
    // texto de la interfaz ilegible.
    void setDarkening(int percent);
    int  darkening() const { return m_darkening; }

    // Opacidad de la capa de imagen (0-100 %). Independiente de la de la capa
    // viva: las dos se superponen y cada una se gradua por separado.
    void setImageOpacity(int percent);
    int  imageOpacity() const { return m_imageOpacity; }

    // --- capa viva ---------------------------------------------------------
    //
    // Visualizacion dibujada con el audio en tiempo real, encima de la imagen y
    // debajo de la interfaz. Necesita el motor para leer las muestras.
    void setEngine(AudioEngine* engine);

    // -1 = ninguna. Ver Visualizations::info().
    void setVisualization(int index);
    int  visualization() const { return m_visualization; }

    void setVisualOpacity(int percent);
    int  visualOpacity() const { return m_visualOpacity; }

    bool visualizerEnabled() const { return m_visualization >= 0; }

    // Formatos de imagen que admite el fondo, para el dialogo de archivos.
    static QString imageFilter();
    static QString modeName(Mode mode);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void clearSource();
    void rebuildScaled();
    void rescaleMovie();
    void refreshVisualizerTimer();
    void drawLiveLayer(QPainter& p);

    QString m_path;
    QImage  m_source;
    QPixmap m_scaled;
    QMovie* m_movie     = nullptr;
    Mode    m_mode      = Mode::Cover;
    int     m_darkening = 35;

    int m_imageOpacity  = 100;
    int m_visualOpacity = 70;

    AudioEngine* m_engine        = nullptr;
    QTimer*      m_visualTimer   = nullptr;
    int          m_visualization = -1;

    SpectrumAnalyzer   m_spectrum;
    std::vector<float> m_samples;
    std::vector<float> m_bars;     // espectro suavizado, 0..1
    std::vector<float> m_wave;     // forma de onda recortada para el osciloscopio
    Visualizations::Frame m_frame;
    QElapsedTimer m_clock;
};
