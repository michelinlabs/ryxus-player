#include "ui/FlatButton.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QPainter>

FlatButton::FlatButton(Icons::Icon icon, QWidget* parent)
    : QAbstractButton(parent)
    , m_icon(icon)
    , m_normal(Theme::TextDim)
    , m_hover(Theme::Text)
    , m_checked(Theme::AccentBright)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
}

FlatButton::FlatButton(const QString& text, QWidget* parent)
    : FlatButton(Icons::None, parent)
{
    setText(text);
}

void FlatButton::setIconId(Icons::Icon icon)
{
    if (m_icon == icon)
        return;
    m_icon = icon;
    update();
}

void FlatButton::setGlyphSize(int px)
{
    m_glyphSize = px;
    updateGeometry();
    update();
}

void FlatButton::setFixedDiameter(int px)
{
    m_diameter = px;
    setFixedSize(px, px);
}

void FlatButton::setStrokeWidth(qreal width)
{
    m_strokeWidth = width;
    update();
}

void FlatButton::setColors(const QColor& normal, const QColor& hover, const QColor& checked)
{
    m_normal  = normal;
    m_hover   = hover;
    m_checked = checked;
    update();
}

void FlatButton::setDrawsRing(bool on)
{
    m_ring = on;
    update();
}

void FlatButton::setFilledWhenChecked(bool on)
{
    m_fillChecked = on;
    update();
}

QSize FlatButton::sizeHint() const
{
    if (!text().isEmpty()) {
        const QFontMetrics fm(font());
        const int width = fm.horizontalAdvance(text()) + (m_icon != Icons::None ? m_glyphSize + 8 : 0) + 18;
        return QSize(width, m_diameter);
    }
    return QSize(m_diameter, m_diameter);
}

void FlatButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void FlatButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void FlatButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool active = isChecked();
    const bool down   = isDown();

    QColor color = m_normal;
    if (!isEnabled())
        color = Theme::TextFaint;
    else if (active)
        color = m_checked;
    else if (m_hovered || down)
        color = m_hover;

    const QRectF box = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    // Fondo: relleno violeta cuando esta activo, realce sutil al pasar encima.
    if (m_fillChecked && active) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::Selection);
        p.drawRoundedRect(box, 3, 3);
        color = Theme::Text;
    } else if (m_hovered && isEnabled()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 14));
        p.drawRoundedRect(box, 3, 3);
    }

    if (m_ring) {
        QPen ring(color);
        ring.setWidthF(1.8);
        p.setPen(ring);
        p.setBrush(Qt::NoBrush);
        const qreal radius = qMin(width(), height()) / 2.0 - 1.4;
        p.drawEllipse(QPointF(width() / 2.0, height() / 2.0), radius, radius);
    }

    if (text().isEmpty()) {
        const QRectF glyph(0, 0, m_glyphSize, m_glyphSize);
        Icons::paint(p, m_icon, glyph.translated(rect().center() - glyph.center().toPoint()),
                     color, m_strokeWidth);
        return;
    }

    // Variante con texto (por ejemplo "A-B" en la barra inferior).
    QRectF textRect = rect();
    if (m_icon != Icons::None) {
        const QRectF glyph(8, (height() - m_glyphSize) / 2.0, m_glyphSize, m_glyphSize);
        Icons::paint(p, m_icon, glyph, color, m_strokeWidth);
        textRect.setLeft(glyph.right() + 6);
    }

    p.setPen(color);
    p.drawText(textRect, Qt::AlignCenter, text());
}
