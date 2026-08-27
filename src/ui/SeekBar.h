#pragma once

#include <QVector>
#include <QWidget>

// Barra de posicion con forma de onda: lo ya reproducido en violeta y lo que
// queda en gris claro, con los tiempos transcurrido/total en las esquinas.
class SeekBar : public QWidget {
    Q_OBJECT

public:
    explicit SeekBar(QWidget* parent = nullptr);

    void setPeaks(const QVector<float>& peaks);
    void clearPeaks();

    void setDurationMs(qint64 ms);
    void setPositionMs(qint64 ms);
    qint64 positionMs() const { return m_positionMs; }

    // Mientras el usuario arrastra, la posicion mostrada es la suya.
    bool isScrubbing() const { return m_scrubbing; }

    // El boton del reloj alterna entre transcurrido y restante (-1:23).
    void setShowRemaining(bool remaining);
    bool showsRemaining() const { return m_showRemaining; }

signals:
    void seekRequested(qint64 ms);
    void scrubbingChanged(bool scrubbing);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QRect  waveRect() const;
    double fractionAt(int x) const;
    qint64 msAt(int x) const;

    QVector<float> m_peaks;
    qint64 m_durationMs = 0;
    qint64 m_positionMs = 0;
    int    m_hoverX     = -1;
    bool   m_scrubbing  = false;
    bool   m_showRemaining = false;
};
