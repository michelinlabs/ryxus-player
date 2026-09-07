#include "ui/EqCurveEditor.h"

#include "core/Lang.h"

#include "core/AudioEngine.h"
#include "core/Equalizer.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMinHz  = 20.0;
constexpr double kMaxHz  = 20000.0;
constexpr double kSpanDb = 18.0;      // media escala vertical del plot

constexpr int kMarginLeft   = 36;     // etiquetas de dB
constexpr int kMarginRight  = 10;
constexpr int kMarginTop    = 10;
constexpr int kMarginBottom = 20;     // etiquetas de Hz

constexpr double kNodeRadius = 7.0;
constexpr double kGrabRadius = 13.0;

QString formatHz(double hz)
{
    if (hz >= 10000.0)
        return QStringLiteral("%1k").arg(hz / 1000.0, 0, 'f', 0);
    if (hz >= 1000.0)
        return QStringLiteral("%1k").arg(hz / 1000.0, 0, 'f', 1);
    return QStringLiteral("%1").arg(hz, 0, 'f', 0);
}

// Un tono distinto por banda, generado a partir del color de acento del tema
// activo: se barre +/-42 grados de matiz alrededor de el, del grave al agudo.
//
// Antes era una tabla fija de violetas, que era justo lo que pedia el skin
// original pero desentonaba en cuanto se elegia un tema verde o ambar. Y al
// ser tabla, tenia exactamente ocho entradas: pasar a doce bandas dejaba las
// cuatro ultimas en negro.
QColor bandColor(int band)
{
    const int last = std::max(1, Equalizer::kBands - 1);
    const double t = double(std::clamp(band, 0, last)) / last;

    const QColor base = Theme::AccentBright;

    // Un tema acromatico (Monocromo) no tiene matiz: hueF() devuelve -1. Ahi
    // las bandas se separan por brillo, que es lo coherente con el tema.
    if (base.hueF() < 0.0)
        return QColor::fromHsvF(0.0, 0.0, 0.55 + t * 0.35);

    const double hue = std::fmod(base.hueF() * 360.0 + 318.0 + t * 84.0, 360.0);

    return QColor::fromHsvF(hue / 360.0,
                            std::clamp(base.saturationF() * 1.15, 0.25, 0.85),
                            std::clamp(base.valueF() * 1.02, 0.55, 1.00));
}

} // namespace

EqCurveEditor::EqCurveEditor(Equalizer* equalizer, AudioEngine* engine, QWidget* parent)
    : QWidget(parent)
    , m_equalizer(equalizer)
    , m_engine(engine)
    // Una banda por pixel largo: con 640 el trazo ya se veia continuo en el
    // centro, pero en los graves varias bandas caian en el mismo bin de la
    // FFT. Con 1024 y la FFT mas larga, el grave deja de dibujarse a tramos.
    , m_spectrum(1024)
{
    setObjectName(QStringLiteral("eqCurveEditor"));
    setMouseTracking(true);
    setMinimumHeight(180);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(Lang::tr(
        "Arrastra los nodos: horizontal = frecuencia, vertical = ganancia.\n"
        "Rueda sobre un nodo = ancho de banda (Q). Doble clic = reiniciar.\n"
        "Manten Shift para mover solo la ganancia."));

    m_spectrum.setSampleRate(AudioEngine::kDeviceSampleRate);
    m_spectrum.setDecay(0.22f);
    m_spectrum.setFloorDb(-78.0f);
    m_samples.resize(SpectrumAnalyzer::kFftSize);

    m_timer = new QTimer(this);
    m_timer->setInterval(33);   // ~30 fps
    connect(m_timer, &QTimer::timeout, this, &EqCurveEditor::tick);
}

QSize EqCurveEditor::sizeHint() const { return QSize(640, 210); }

void EqCurveEditor::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;

    if (active) {
        m_timer->start();
    } else {
        m_timer->stop();
        m_spectrum.clear();
        update();
    }
}

void EqCurveEditor::setSelectedBand(int band)
{
    band = (band >= 0 && band < Equalizer::kBands) ? band : -1;
    if (m_selected == band)
        return;
    m_selected = band;
    update();
    emit selectionChanged(m_selected);
}

void EqCurveEditor::refreshFromEngine() { update(); }

void EqCurveEditor::tick()
{
    if (!m_engine)
        return;
    m_engine->copyVisualSamples(m_samples.data(), int(m_samples.size()));
    m_spectrum.update(m_samples.data(), int(m_samples.size()));
    update();
}

// --------------------------------------------------------------- geometria

QRectF EqCurveEditor::plotRect() const
{
    return QRectF(rect()).adjusted(kMarginLeft, kMarginTop, -kMarginRight, -kMarginBottom);
}

double EqCurveEditor::xForHz(double hz) const
{
    const QRectF box = plotRect();
    hz = std::clamp(hz, kMinHz, kMaxHz);
    const double t = std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz);
    return box.left() + t * box.width();
}

double EqCurveEditor::hzForX(double x) const
{
    const QRectF box = plotRect();
    if (box.width() <= 0.0)
        return kMinHz;
    const double t = std::clamp((x - box.left()) / box.width(), 0.0, 1.0);
    return kMinHz * std::pow(kMaxHz / kMinHz, t);
}

double EqCurveEditor::yForDb(double db) const
{
    const QRectF box = plotRect();
    return box.center().y() - (db / kSpanDb) * (box.height() / 2.0);
}

double EqCurveEditor::dbForY(double y) const
{
    const QRectF box = plotRect();
    if (box.height() <= 0.0)
        return 0.0;
    return (box.center().y() - y) / (box.height() / 2.0) * kSpanDb;
}

QPointF EqCurveEditor::nodePos(int band) const
{
    return QPointF(xForHz(m_equalizer->frequency(band)),
                   yForDb(m_equalizer->gain(band)));
}

int EqCurveEditor::nodeAt(const QPointF& pos) const
{
    int best = -1;
    double bestDistance = kGrabRadius;

    for (int b = 0; b < Equalizer::kBands; ++b) {
        const QPointF delta = nodePos(b) - pos;
        const double distance = std::hypot(delta.x(), delta.y());
        if (distance < bestDistance) {
            bestDistance = distance;
            best = b;
        }
    }
    return best;
}

// ------------------------------------------------------------------ raton

void EqCurveEditor::mousePressEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();

    if (event->button() == Qt::LeftButton) {
        const int band = nodeAt(pos);
        if (band >= 0) {
            m_dragging = band;
            m_lockFrequency = event->modifiers() & Qt::ShiftModifier;
            setSelectedBand(band);
            setCursor(Qt::ClosedHandCursor);
        } else {
            setSelectedBand(-1);
        }
        return;
    }

    if (event->button() == Qt::RightButton) {
        // Clic derecho sobre un nodo: lo devuelve a 0 dB sin moverlo de sitio.
        const int band = nodeAt(pos);
        if (band >= 0) {
            m_equalizer->setGain(band, 0.0f);
            emit bandEdited(band);
            update();
        }
    }
}

void EqCurveEditor::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();

    if (m_dragging >= 0) {
        m_equalizer->setGain(m_dragging, float(std::clamp(dbForY(pos.y()),
                                                          -double(Equalizer::kRangeDb),
                                                          double(Equalizer::kRangeDb))));
        if (!m_lockFrequency)
            m_equalizer->setFrequency(m_dragging, float(hzForX(pos.x())));

        emit bandEdited(m_dragging);
        update();
        return;
    }

    const int hovered = nodeAt(pos);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        setCursor(hovered >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        update();
    }
}

void EqCurveEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging >= 0) {
        m_dragging = -1;
        m_lockFrequency = false;
        setCursor(m_hovered >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        update();
    }
}

void EqCurveEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const int band = nodeAt(event->position());
    if (band < 0)
        return;

    m_equalizer->resetBand(band);
    emit bandEdited(band);
    update();
}

void EqCurveEditor::wheelEvent(QWheelEvent* event)
{
    const int band = (m_hovered >= 0) ? m_hovered : m_selected;
    if (band < 0) {
        event->ignore();
        return;
    }

    // La rueda abre o cierra la campana del filtro.
    const double factor = (event->angleDelta().y() > 0) ? 1.12 : 1.0 / 1.12;
    m_equalizer->setQ(band, float(m_equalizer->q(band) * factor));

    emit bandEdited(band);
    update();
    event->accept();
}

void EqCurveEditor::leaveEvent(QEvent* event)
{
    if (m_dragging < 0) {
        m_hovered = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

// ------------------------------------------------------------------ dibujo

void EqCurveEditor::drawGrid(QPainter& p, const QRectF& box) const
{
    p.setPen(QPen(Theme::Border, 1));

    static const double kTicks[] = {20, 30, 50, 80, 150, 300, 500, 1000,
                                    2000, 3000, 5000, 10000, 20000};
    p.setFont(Theme::uiFont(7));

    for (double hz : kTicks) {
        const double x = xForHz(hz);
        p.setPen(QPen(Theme::Border, 1));
        p.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));

        p.setPen(Theme::TextFaint);
        p.drawText(QRectF(x - 20, box.bottom() + 3, 40, 14),
                   Qt::AlignHCenter | Qt::AlignTop, formatHz(hz));
    }

    for (double db = -12.0; db <= 12.0; db += 6.0) {
        const double y = yForDb(db);
        p.setPen(QPen(std::abs(db) < 0.1 ? Theme::BorderLight : Theme::Border, 1));
        p.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));

        p.setPen(Theme::TextFaint);
        p.drawText(QRectF(0, y - 7, kMarginLeft - 6, 14),
                   Qt::AlignRight | Qt::AlignVCenter,
                   db > 0 ? QStringLiteral("+%1").arg(db, 0, 'f', 0)
                          : QStringLiteral("%1").arg(db, 0, 'f', 0));
    }
}

void EqCurveEditor::drawSpectrum(QPainter& p, const QRectF& box) const
{
    if (m_spectrum.levels().empty())
        return;

    // Area rellena bajo el espectro. Se dibuja tenue para que no compita con
    // la curva del ecualizador, que es lo que el usuario esta manipulando.
    //
    // Se muestrea un punto por pixel y se suaviza con una media de 5 tomas:
    // con menos resolucion el contorno sale a escalones.
    const int steps = std::max(2, int(box.width()));
    std::vector<double> levels(size_t(steps) + 1, 0.0);
    bool any = false;

    for (int i = 0; i <= steps; ++i) {
        const double x  = box.left() + box.width() * double(i) / steps;
        double sum = 0.0;
        for (int k = -2; k <= 2; ++k) {
            const double hz = hzForX(x + k * 0.5);
            sum += m_spectrum.levelAtFrequency(float(hz));
        }
        levels[size_t(i)] = sum / 5.0;
        if (levels[size_t(i)] > 0.001)
            any = true;
    }

    if (!any)
        return;

    QPainterPath area;
    area.moveTo(box.left(), box.bottom());
    for (int i = 0; i <= steps; ++i) {
        const double x = box.left() + box.width() * double(i) / steps;
        area.lineTo(x, box.bottom() - levels[size_t(i)] * box.height());
    }
    area.lineTo(box.right(), box.bottom());
    area.closeSubpath();

    QLinearGradient gradient(0, box.top(), 0, box.bottom());
    QColor top = Theme::AccentLight;
    top.setAlpha(90);
    QColor bottom = Theme::Accent;
    bottom.setAlpha(25);
    gradient.setColorAt(0.0, top);
    gradient.setColorAt(1.0, bottom);

    p.setPen(Qt::NoPen);
    p.setBrush(gradient);
    p.drawPath(area);

    QColor outline = Theme::AccentLight;
    outline.setAlpha(150);
    p.setPen(QPen(outline, 1.0));
    p.setBrush(Qt::NoBrush);

    QPainterPath contour;
    for (int i = 0; i <= steps; ++i) {
        const double x = box.left() + box.width() * double(i) / steps;
        const double y = box.bottom() - levels[size_t(i)] * box.height();
        if (i == 0) contour.moveTo(x, y);
        else        contour.lineTo(x, y);
    }
    p.drawPath(contour);
}

void EqCurveEditor::drawBandCurves(QPainter& p, const QRectF& box) const
{
    // Aportacion individual de cada banda: ayuda a entender que hace cada nodo
    // cuando varias campanas se solapan.
    p.setBrush(Qt::NoBrush);

    for (int b = 0; b < Equalizer::kBands; ++b) {
        const double gain = m_equalizer->gain(b);
        if (std::abs(gain) < 0.15)
            continue;

        const double freq = m_equalizer->frequency(b);
        const double q    = m_equalizer->q(b);

        QPainterPath path;
        const int steps = std::max(2, int(box.width() / 3));
        for (int i = 0; i <= steps; ++i) {
            const double x  = box.left() + box.width() * double(i) / steps;
            const double hz = hzForX(x);

            // Campana peaking analitica, misma forma que el biquad.
            const double ratio = hz / freq;
            const double shape = 1.0 / (1.0 + std::pow(q * (ratio - 1.0 / ratio), 2.0));
            const double y = yForDb(gain * shape);

            if (i == 0) path.moveTo(x, y);
            else        path.lineTo(x, y);
        }

        QColor color = bandColor(b);
        color.setAlpha((b == m_selected || b == m_hovered) ? 170 : 80);
        p.setPen(QPen(color, 1.2));
        p.drawPath(path);
    }
}

void EqCurveEditor::drawMainCurve(QPainter& p, const QRectF& box) const
{
    QPainterPath curve;
    const int steps = std::max(2, int(box.width()));

    // Un punto por pixel, en una sola pasada: los coeficientes de las doce
    // bandas se calculan una vez para toda la curva, no una vez por pixel.
    std::vector<float> freqs(size_t(steps) + 1);
    std::vector<float> response(size_t(steps) + 1);
    for (int i = 0; i <= steps; ++i)
        freqs[size_t(i)] = float(hzForX(box.left() + box.width() * double(i) / steps));
    m_equalizer->responseDb(freqs.data(), response.data(), steps + 1);

    for (int i = 0; i <= steps; ++i) {
        const double x = box.left() + box.width() * double(i) / steps;
        const double y = std::clamp(yForDb(response[size_t(i)]),
                                    box.top() - 40.0, box.bottom() + 40.0);
        if (i == 0) curve.moveTo(x, y);
        else        curve.lineTo(x, y);
    }

    QPainterPath filled = curve;
    filled.lineTo(box.right(), yForDb(0.0));
    filled.lineTo(box.left(), yForDb(0.0));
    filled.closeSubpath();

    QColor fill = Theme::AccentBright;
    fill.setAlpha(55);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.setClipRect(box);
    p.drawPath(filled);

    p.setPen(QPen(Theme::AccentBright, 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curve);
    p.setClipping(false);
}

void EqCurveEditor::drawNodes(QPainter& p, const QRectF& box) const
{
    p.setFont(Theme::uiFont(7, QFont::DemiBold));

    for (int b = 0; b < Equalizer::kBands; ++b) {
        const QPointF center = nodePos(b);
        if (!box.adjusted(-kNodeRadius, -kNodeRadius, kNodeRadius, kNodeRadius)
                 .contains(center))
            continue;

        const bool active = (b == m_selected || b == m_hovered || b == m_dragging);
        const double radius = active ? kNodeRadius + 1.5 : kNodeRadius;

        // Halo del nodo activo, para localizarlo sobre el espectro.
        if (active) {
            QColor halo = bandColor(b);
            halo.setAlpha(70);
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(center, radius + 5.0, radius + 5.0);
        }

        p.setPen(QPen(Theme::Chrome, 1.5));
        p.setBrush(active ? bandColor(b).lighter(125) : bandColor(b));
        p.drawEllipse(center, radius, radius);

        p.setPen(active ? Theme::Chrome : Theme::Chrome.lighter(140));
        p.drawText(QRectF(center.x() - radius, center.y() - radius,
                          radius * 2, radius * 2),
                   Qt::AlignCenter, QString::number(b + 1));
    }
}

void EqCurveEditor::drawReadout(QPainter& p, const QRectF& box) const
{
    const int band = (m_dragging >= 0) ? m_dragging
                   : (m_hovered >= 0)  ? m_hovered
                                       : m_selected;
    if (band < 0)
        return;

    const QString text = QStringLiteral("%1  ·  %2 Hz  ·  %3 dB  ·  Q %4")
        .arg(band + 1)
        .arg(formatHz(m_equalizer->frequency(band)))
        .arg(m_equalizer->gain(band) >= 0
                 ? QStringLiteral("+%1").arg(double(m_equalizer->gain(band)), 0, 'f', 1)
                 : QStringLiteral("%1").arg(double(m_equalizer->gain(band)), 0, 'f', 1))
        .arg(double(m_equalizer->q(band)), 0, 'f', 2);

    p.setFont(Theme::uiFont(8));
    const QFontMetrics fm(p.font());
    const int width = fm.horizontalAdvance(text) + 16;
    const QRectF badge(box.right() - width - 4, box.top() + 4, width, 20);

    QColor background = Theme::Chrome;
    background.setAlpha(215);
    p.setPen(QPen(bandColor(band), 1));
    p.setBrush(background);
    p.drawRoundedRect(badge, 3, 3);

    p.setPen(Theme::Text);
    p.drawText(badge, Qt::AlignCenter, text);
}

void EqCurveEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box = plotRect();
    if (box.width() <= 4 || box.height() <= 4)
        return;

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::panelDeepBg());
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);

    drawSpectrum(p, box);
    drawGrid(p, box);
    drawBandCurves(p, box);
    drawMainCurve(p, box);
    drawNodes(p, box);
    drawReadout(p, box);

    p.setPen(QPen(Theme::Border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
}
