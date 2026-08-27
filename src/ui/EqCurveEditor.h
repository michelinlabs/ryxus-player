#pragma once

#include "core/SpectrumAnalyzer.h"

#include <QWidget>
#include <vector>

class AudioEngine;
class Equalizer;
class QTimer;

// Plot del ecualizador al estilo del EQ Eight de Ableton:
//
//   - 8 nodos arrastrables en los dos ejes (X = frecuencia, Y = ganancia)
//   - la curva resultante pasa por ellos y se rellena hasta la linea de 0 dB
//   - de fondo, el espectro del audio en tiempo real
//
// Gestos: arrastrar mueve el nodo; la rueda sobre un nodo cambia su Q;
// doble clic lo devuelve a cero; Shift restringe el arrastre a la ganancia.
class EqCurveEditor : public QWidget {
    Q_OBJECT

public:
    EqCurveEditor(Equalizer* equalizer, AudioEngine* engine, QWidget* parent = nullptr);

    void setActive(bool active);          // arranca o para el refresco
    int  selectedBand() const { return m_selected; }
    void setSelectedBand(int band);

    // Redibuja tras un cambio externo (preset, reinicio...).
    void refreshFromEngine();

    QSize sizeHint() const override;

signals:
    void bandEdited(int band);            // el usuario movio una banda
    void selectionChanged(int band);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void tick();

private:
    QRectF plotRect() const;
    double xForHz(double hz) const;
    double hzForX(double x) const;
    double yForDb(double db) const;
    double dbForY(double y) const;
    QPointF nodePos(int band) const;
    int  nodeAt(const QPointF& pos) const;

    void drawGrid(QPainter& p, const QRectF& box) const;
    void drawSpectrum(QPainter& p, const QRectF& box) const;
    void drawBandCurves(QPainter& p, const QRectF& box) const;
    void drawMainCurve(QPainter& p, const QRectF& box) const;
    void drawNodes(QPainter& p, const QRectF& box) const;
    void drawReadout(QPainter& p, const QRectF& box) const;

    Equalizer*   m_equalizer = nullptr;
    AudioEngine* m_engine    = nullptr;
    QTimer*      m_timer     = nullptr;

    SpectrumAnalyzer   m_spectrum;
    std::vector<float> m_samples;

    int  m_selected = -1;
    int  m_hovered  = -1;
    int  m_dragging = -1;
    bool m_lockFrequency = false;   // Shift: solo ganancia
    bool m_active = false;
};
