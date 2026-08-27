#include "ui/EqSlider.h"
#include "core/Lang.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kLabelHeight  = 15;   // etiqueta de frecuencia, abajo
constexpr int kValueHeight  = 14;   // lectura en dB, arriba
constexpr int kRailWidth    = 4;
constexpr int kHandleWidth  = 20;
constexpr int kHandleHeight = 9;
constexpr int kColumnWidth  = 38;
}

EqSlider::EqSlider(const QString& label, QWidget* parent)
    : QWidget(parent)
    , m_label(label)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(Lang::tr("Arrastra para ajustar - doble clic para 0 dB"));
}

QSize EqSlider::sizeHint() const       { return QSize(kColumnWidth, 150); }
QSize EqSlider::minimumSizeHint() const { return QSize(kColumnWidth - 8, 110); }

void EqSlider::setLabel(const QString& label)
{
    m_label = label;
    update();
}

void EqSlider::setRange(float minDb, float maxDb)
{
    m_minDb = minDb;
    m_maxDb = maxDb;
    setValue(m_value, false);
}

void EqSlider::setValue(float db, bool notify)
{
    const float clamped = std::clamp(db, m_minDb, m_maxDb);
    if (std::fabs(clamped - m_value) < 0.001f)
        return;

    m_value = clamped;
    update();
    if (notify)
        emit valueChanged(m_value);
}

QRect EqSlider::railRect() const
{
    const int top    = kValueHeight + kHandleHeight / 2 + 2;
    const int bottom = height() - kLabelHeight - kHandleHeight / 2 - 2;
    return QRect((width() - kRailWidth) / 2, top, kRailWidth, std::max(1, bottom - top));
}

float EqSlider::valueAt(int y) const
{
    const QRect rail = railRect();
    const double t = std::clamp(double(y - rail.top()) / double(rail.height()), 0.0, 1.0);
    return float(m_maxDb - t * (m_maxDb - m_minDb));
}

int EqSlider::yForValue(float db) const
{
    const QRect rail = railRect();
    const double t = double(m_maxDb - db) / double(m_maxDb - m_minDb);
    return rail.top() + int(t * rail.height());
}

void EqSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    setValue(valueAt(int(event->position().y())));
}

void EqSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging)
        setValue(valueAt(int(event->position().y())));
}

void EqSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
}

void EqSlider::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        setValue(0.0f);
}

void EqSlider::wheelEvent(QWheelEvent* event)
{
    const float step = (event->modifiers() & Qt::ControlModifier) ? 0.5f : 1.0f;
    setValue(m_value + (event->angleDelta().y() > 0 ? step : -step));
    event->accept();
}

void EqSlider::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void EqSlider::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void EqSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect rail = railRect();
    const int centerY = yForValue(0.0f);
    const int handleY = yForValue(m_value);

    // --- lectura en dB -----------------------------------------------------
    p.setFont(Theme::uiFont(8));
    p.setPen(std::fabs(m_value) < 0.05f ? Theme::TextFaint
                                        : (m_hovered ? Theme::Text : Theme::TextDim));
    const QString reading = (m_value > 0.05f)
        ? QStringLiteral("+%1").arg(double(m_value), 0, 'f', 1)
        : QStringLiteral("%1").arg(double(m_value), 0, 'f', 1);
    p.drawText(QRect(0, 0, width(), kValueHeight), Qt::AlignCenter, reading);

    // --- rail --------------------------------------------------------------
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::PanelDeep);
    p.drawRoundedRect(rail, 2, 2);

    // Marcas cada 6 dB, para leer la posicion de un vistazo.
    p.setPen(QPen(Theme::Border, 1));
    for (float db = m_minDb; db <= m_maxDb + 0.01f; db += 6.0f) {
        const int y = yForValue(db);
        const int len = (std::fabs(db) < 0.05f) ? 7 : 4;
        p.drawLine(rail.center().x() - len, y, rail.center().x() - kRailWidth / 2 - 1, y);
        p.drawLine(rail.center().x() + kRailWidth / 2 + 1, y, rail.center().x() + len, y);
    }

    // --- relleno desde 0 dB hasta el valor actual --------------------------
    if (std::fabs(m_value) > 0.05f) {
        const QRect fill(rail.left(), std::min(centerY, handleY),
                         rail.width(), std::abs(centerY - handleY));
        p.setPen(Qt::NoPen);
        p.setBrush(m_hovered ? Theme::AccentBright : Theme::Accent);
        p.drawRoundedRect(fill, 2, 2);
    }

    // --- tirador -----------------------------------------------------------
    const QRect handle((width() - kHandleWidth) / 2, handleY - kHandleHeight / 2,
                       kHandleWidth, kHandleHeight);
    p.setPen(QPen(Theme::Chrome, 1));
    p.setBrush(m_hovered || m_dragging ? Theme::Text : Theme::Wave);
    p.drawRoundedRect(handle, 2.5, 2.5);

    // Ranura central del tirador, detalle del skin de referencia.
    p.setPen(QPen(Theme::Chrome, 1));
    p.drawLine(handle.left() + 5, handle.center().y(),
               handle.right() - 5, handle.center().y());

    // --- etiqueta de frecuencia -------------------------------------------
    p.setFont(Theme::uiFont(8));
    p.setPen(m_hovered ? Theme::Text : Theme::TextDim);
    p.drawText(QRect(0, height() - kLabelHeight, width(), kLabelHeight),
               Qt::AlignCenter, m_label);
}
