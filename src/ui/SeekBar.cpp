#include "ui/SeekBar.h"
#include "core/TrackInfo.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>

namespace {
constexpr int kTimeStripHeight = 16;   // franja superior con los tiempos
constexpr int kSideMargin      = 8;
}

SeekBar::SeekBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(46);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFont(Theme::uiFont(8));
}

void SeekBar::setPeaks(const QVector<float>& peaks)
{
    m_peaks = peaks;
    update();
}

void SeekBar::clearPeaks()
{
    m_peaks.clear();
    update();
}

void SeekBar::setDurationMs(qint64 ms)
{
    if (m_durationMs == ms)
        return;
    m_durationMs = std::max<qint64>(0, ms);
    update();
}

void SeekBar::setPositionMs(qint64 ms)
{
    if (m_scrubbing)
        return;   // no pisar la posicion que el usuario esta arrastrando
    ms = std::clamp<qint64>(ms, 0, m_durationMs);
    if (m_positionMs == ms)
        return;
    m_positionMs = ms;
    update();
}

void SeekBar::setShowRemaining(bool remaining)
{
    if (m_showRemaining == remaining)
        return;
    m_showRemaining = remaining;
    update();
}

QRect SeekBar::waveRect() const
{
    return rect().adjusted(kSideMargin, kTimeStripHeight, -kSideMargin, -4);
}

double SeekBar::fractionAt(int x) const
{
    const QRect wave = waveRect();
    if (wave.width() <= 0)
        return 0.0;
    return std::clamp(double(x - wave.left()) / double(wave.width()), 0.0, 1.0);
}

qint64 SeekBar::msAt(int x) const
{
    return qint64(fractionAt(x) * double(m_durationMs));
}

void SeekBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_durationMs <= 0)
        return;

    m_scrubbing = true;
    m_positionMs = msAt(int(event->position().x()));
    emit scrubbingChanged(true);
    update();
}

void SeekBar::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverX = int(event->position().x());

    if (m_scrubbing && m_durationMs > 0) {
        m_positionMs = msAt(m_hoverX);
    } else if (m_durationMs > 0 && waveRect().adjusted(-kSideMargin, 0, kSideMargin, 0)
                                       .contains(event->position().toPoint())) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           TrackInfo::formatDuration(int(msAt(m_hoverX))), this);
    }
    update();
}

void SeekBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_scrubbing)
        return;

    m_scrubbing = false;
    const qint64 target = msAt(int(event->position().x()));
    m_positionMs = target;
    emit scrubbingChanged(false);
    emit seekRequested(target);
    update();
}

void SeekBar::leaveEvent(QEvent* event)
{
    m_hoverX = -1;
    update();
    QWidget::leaveEvent(event);
}

void SeekBar::wheelEvent(QWheelEvent* event)
{
    if (m_durationMs <= 0)
        return;

    // Rueda = salto de 5 s, como en la referencia.
    const qint64 delta = (event->angleDelta().y() > 0) ? 5000 : -5000;
    const qint64 target = std::clamp<qint64>(m_positionMs + delta, 0, m_durationMs);
    m_positionMs = target;
    emit seekRequested(target);
    update();
}

void SeekBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    // Sin relleno: la barra inferior ya pinto su capa translucida.

    const QRect wave = waveRect();
    const double fraction = (m_durationMs > 0)
                          ? double(m_positionMs) / double(m_durationMs)
                          : 0.0;
    const int playedWidth = int(wave.width() * fraction);

    // --- tiempos -----------------------------------------------------------
    p.setFont(font());
    p.setPen(Theme::Text);
    const QString elapsed = m_showRemaining
        ? QStringLiteral("-") + TrackInfo::formatDuration(int(m_durationMs - m_positionMs))
        : TrackInfo::formatDuration(int(m_positionMs));
    p.drawText(QRect(kSideMargin, 0, 120, kTimeStripHeight),
               Qt::AlignVCenter | Qt::AlignLeft, elapsed);

    p.setPen(Theme::TextDim);
    p.drawText(QRect(width() - 120 - kSideMargin, 0, 120, kTimeStripHeight),
               Qt::AlignVCenter | Qt::AlignRight,
               TrackInfo::formatDuration(int(m_durationMs)));

    if (wave.width() <= 0 || wave.height() <= 0)
        return;

    const double centerY = wave.center().y() + 0.5;
    const double maxHalf = wave.height() / 2.0 - 1.0;

    p.setPen(Qt::NoPen);

    if (m_peaks.isEmpty()) {
        // Sin analisis todavia: barra plana, misma particion de colores.
        const QRect played(wave.left(), int(centerY - 3), playedWidth, 6);
        const QRect rest(wave.left() + playedWidth, int(centerY - 3),
                         wave.width() - playedWidth, 6);
        p.setBrush(Theme::AccentLight);
        p.drawRect(played);
        p.setBrush(m_durationMs > 0 ? Theme::Wave : Theme::Border);
        p.drawRect(rest);
    } else {
        // Una columna de 1 px por pixel de ancho, tomando el pico maximo del
        // tramo que le corresponde: asi la onda no cambia al redimensionar.
        const double peaksPerPixel = double(m_peaks.size()) / double(wave.width());

        for (int x = 0; x < wave.width(); ++x) {
            const int from = int(x * peaksPerPixel);
            const int to   = std::min<int>(int((x + 1) * peaksPerPixel) + 1, m_peaks.size());

            float peak = 0.0f;
            for (int i = from; i < to; ++i)
                peak = std::max(peak, m_peaks.at(i));

            const double half = std::max(1.0, peak * maxHalf);
            p.setBrush(x < playedWidth ? Theme::AccentLight : Theme::Wave);
            p.drawRect(QRectF(wave.left() + x, centerY - half, 1.0, half * 2.0));
        }
    }

    // --- cursor de posicion y previsualizacion al pasar el raton ----------
    if (m_durationMs > 0) {
        p.setPen(QPen(Theme::Text, 1));
        const int cursorX = wave.left() + playedWidth;
        p.drawLine(cursorX, wave.top() - 2, cursorX, wave.bottom() + 2);
    }

    if (m_hoverX >= 0 && !m_scrubbing && m_durationMs > 0) {
        p.setPen(QPen(QColor(255, 255, 255, 70), 1));
        p.drawLine(m_hoverX, wave.top(), m_hoverX, wave.bottom());
    }
}
