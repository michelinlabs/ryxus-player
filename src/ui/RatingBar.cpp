#include "ui/RatingBar.h"
#include "core/Lang.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int kStarGap = 2;
}

RatingBar::RatingBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
    setToolTip(Lang::tr("Calificacion (clic en la misma estrella para quitarla)"));
}

QSize RatingBar::sizeHint() const
{
    return QSize(5 * m_starSize + 4 * kStarGap, m_starSize);
}

void RatingBar::setStarSize(int px)
{
    m_starSize = qMax(8, px);
    setFixedSize(sizeHint());
    update();
}

void RatingBar::setRating(int rating)
{
    rating = qBound(0, rating, 5);
    if (m_rating == rating)
        return;
    m_rating = rating;
    update();
}

void RatingBar::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    setCursor(readOnly ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

int RatingBar::starAt(int x) const
{
    const int index = x / (m_starSize + kStarGap);
    return (index >= 0 && index < 5) ? index + 1 : -1;
}

void RatingBar::mousePressEvent(QMouseEvent* event)
{
    if (m_readOnly || event->button() != Qt::LeftButton)
        return;

    const int star = starAt(int(event->position().x()));
    if (star < 0)
        return;

    // Volver a pulsar la estrella actual limpia la calificacion.
    const int next = (star == m_rating) ? 0 : star;
    setRating(next);
    emit ratingChanged(next);
}

void RatingBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_readOnly)
        return;
    const int hovered = starAt(int(event->position().x()));
    if (hovered != m_hovered) {
        m_hovered = hovered;
        update();
    }
}

void RatingBar::leaveEvent(QEvent* event)
{
    m_hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

void RatingBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const int shown = (m_hovered > 0 && !m_readOnly) ? m_hovered : m_rating;

    for (int i = 0; i < 5; ++i) {
        const QRectF box(i * (m_starSize + kStarGap), 0, m_starSize, m_starSize);
        const bool filled = i < shown;
        Icons::paint(p, filled ? Icons::Star : Icons::StarOutline, box,
                     filled ? (m_hovered > 0 ? Theme::AccentBright : Theme::Accent)
                            : Theme::TextFaint);
    }
}
