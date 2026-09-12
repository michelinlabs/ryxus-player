#include "ui/Visualizations.h"

#include "ui/Theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Visualizations {
namespace {

const Info kVisualizations[] = {
    {"bars",    "Barras"},
    {"mirror",  "Espejo"},
    {"wave",    "Onda"},
    {"mandala", "Mandala"},
    {"rings",   "Anillos"},
    {"nebula",  "Nebulosa"},
};

// Color del tema desplazado en matiz. Las visualizaciones se construyen con la
// paleta activa en vez de con colores fijos: asi un tema verde no acaba con un
// mandala violeta encima.
QColor accent(double shiftDegrees, int alpha, double valueScale = 1.0)
{
    const QColor base = Theme::AccentBright;

    // Un tema acromatico (Monocromo) no tiene matiz que desplazar.
    if (base.hueF() < 0.0) {
        QColor grey = QColor::fromHsvF(0.0, 0.0,
                                       std::clamp(base.valueF() * valueScale, 0.0, 1.0));
        grey.setAlpha(alpha);
        return grey;
    }

    const double hue = std::fmod(base.hueF() * 360.0 + shiftDegrees + 360.0, 360.0);
    QColor out = QColor::fromHsvF(hue / 360.0,
                                  std::clamp(base.saturationF() * 1.1, 0.2, 0.95),
                                  std::clamp(base.valueF() * valueScale, 0.15, 1.0));
    out.setAlpha(alpha);
    return out;
}

float bandAt(const std::vector<float>& spectrum, double position)
{
    if (spectrum.empty())
        return 0.0f;
    const double scaled = std::clamp(position, 0.0, 1.0) * double(spectrum.size() - 1);
    const size_t low = size_t(scaled);
    const size_t high = std::min(low + 1, spectrum.size() - 1);
    const double frac = scaled - double(low);
    return float(spectrum[low] * (1.0 - frac) + spectrum[high] * frac);
}

// Media de un entorno de bandas. El espectro crudo es irregular de banda a
// banda, y dibujarlo tal cual deja contornos con dientes de sierra; para las
// formas cerradas -- mandala, anillos -- interesa la envolvente, no el detalle.
float bandSmooth(const std::vector<float>& spectrum, double position, int radius = 3)
{
    if (spectrum.empty())
        return 0.0f;

    const double step = 1.0 / double(spectrum.size());
    float sum = 0.0f;
    int taken = 0;
    for (int offset = -radius; offset <= radius; ++offset) {
        sum += bandAt(spectrum, position + offset * step);
        ++taken;
    }
    return sum / float(taken);
}

// --------------------------------------------------------------------- barras

void paintBars(QPainter& p, const QRectF& area, const Frame& frame)
{
    const std::vector<float>& spectrum = *frame.spectrum;
    if (spectrum.empty())
        return;

    const int count = int(spectrum.size());
    const qreal span  = area.width() / count;
    const qreal width = std::max<qreal>(1.0, span * 0.66);

    QLinearGradient gradient(0, area.top(), 0, area.bottom());
    gradient.setColorAt(0.0, accent(0, 40, 1.05));
    gradient.setColorAt(0.55, accent(-18, 120));
    gradient.setColorAt(1.0, accent(-35, 210, 0.85));

    p.setPen(Qt::NoPen);
    p.setBrush(gradient);

    for (int i = 0; i < count; ++i) {
        const qreal h = spectrum[size_t(i)] * area.height();
        if (h < 1.0)
            continue;
        const qreal x = area.left() + i * span + (span - width) / 2.0;
        p.drawRoundedRect(QRectF(x, area.bottom() - h, width, h), 2, 2);
    }
}

// --------------------------------------------------------------------- espejo

void paintMirror(QPainter& p, const QRectF& area, const Frame& frame)
{
    const std::vector<float>& spectrum = *frame.spectrum;
    if (spectrum.empty())
        return;

    const int count = int(spectrum.size());
    const qreal span  = area.width() / count;
    const qreal width = std::max<qreal>(1.0, span * 0.7);
    const qreal mid   = area.center().y();
    const qreal reach = area.height() / 2.0;

    QLinearGradient gradient(0, area.top(), 0, area.bottom());
    gradient.setColorAt(0.0,  accent(25, 30));
    gradient.setColorAt(0.5,  accent(0, 200, 1.05));
    gradient.setColorAt(1.0,  accent(-25, 30));

    p.setPen(Qt::NoPen);
    p.setBrush(gradient);

    for (int i = 0; i < count; ++i) {
        const qreal h = spectrum[size_t(i)] * reach;
        if (h < 1.0)
            continue;
        const qreal x = area.left() + i * span + (span - width) / 2.0;
        p.drawRoundedRect(QRectF(x, mid - h, width, h * 2.0), 2, 2);
    }
}

// ----------------------------------------------------------------------- onda

void paintWave(QPainter& p, const QRectF& area, const Frame& frame)
{
    const std::vector<float>& wave = *frame.wave;
    if (wave.size() < 4)
        return;

    const qreal mid   = area.center().y();
    const qreal reach = area.height() * 0.47;
    const int   steps = std::min<int>(int(area.width()), int(wave.size()));

    // Tres trazos del mismo recorrido, cada vez mas anchos y transparentes:
    // sale un halo sin necesidad de desenfocar nada.
    const struct { qreal width; int alpha; double shift; } kPasses[] = {
        {9.0, 26, 20}, {4.0, 70, 8}, {1.6, 220, 0},
    };

    QPainterPath path;
    for (int i = 0; i < steps; ++i) {
        const size_t index = size_t(double(i) / steps * (wave.size() - 1));
        const qreal x = area.left() + area.width() * double(i) / (steps - 1);
        const qreal y = mid - wave[index] * reach;
        if (i == 0) path.moveTo(x, y);
        else        path.lineTo(x, y);
    }

    p.setBrush(Qt::NoBrush);
    for (const auto& pass : kPasses) {
        QPen pen(accent(pass.shift, pass.alpha));
        pen.setWidthF(pass.width);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.drawPath(path);
    }
}

// -------------------------------------------------------------------- mandala

void paintMandala(QPainter& p, const QRectF& area, const Frame& frame)
{
    const std::vector<float>& spectrum = *frame.spectrum;
    if (spectrum.empty())
        return;

    const QPointF centre = area.center();

    // Radios distintos por eje: el mandala se calcula circular, de radio 0..1,
    // y se estira a lo ancho y a lo alto al colocar cada punto. Antes usaba el
    // lado corto de la ventana, asi que en una pantalla apaisada dejaba casi
    // media anchura vacia a cada costado.
    const qreal rx = area.width()  * 0.49;
    const qreal ry = area.height() * 0.49;

    p.save();
    p.translate(centre);

    // Tres coronas superpuestas girando a distinta velocidad y en sentidos
    // opuestos: es lo que da la sensacion de caleidoscopio en movimiento.
    const struct { int petals; double speed; double scale; int alpha; double shift; } kLayers[] = {
        {12, 0.08, 1.00, 150,   0},
        { 8, -0.13, 0.74, 110,  28},
        {16, 0.19, 0.50,  80, -26},
    };

    for (const auto& layer : kLayers) {
        p.save();
        p.rotate(frame.seconds * layer.speed * 360.0);

        // Se recorre media hoja y se refleja: la simetria sale exacta y cuesta
        // la mitad de cuentas.
        constexpr int kStepsPerPetal = 26;
        QPainterPath path;

        const double base = layer.scale * 0.34;   // radio normalizado, 0..1
        const double amp  = layer.scale * 0.66;

        for (int petal = 0; petal < layer.petals; ++petal) {
            for (int step = 0; step <= kStepsPerPetal; ++step) {
                const double within = double(step) / kStepsPerPetal;

                // La banda que alimenta el radio va del grave al agudo segun se
                // avanza dentro del petalo, y vuelve: por eso el reflejo.
                const double fold = within <= 0.5 ? within * 2.0 : (1.0 - within) * 2.0;
                const double radius = base + amp * bandSmooth(spectrum, fold, 4);

                const double angle = (double(petal) + within) / layer.petals * 2.0 * M_PI;
                const QPointF point(radius * std::cos(angle) * rx,
                                    radius * std::sin(angle) * ry);

                if (petal == 0 && step == 0) path.moveTo(point);
                else                          path.lineTo(point);
            }
        }
        path.closeSubpath();

        QPen pen(accent(layer.shift, layer.alpha, 1.05));
        pen.setWidthF(1.6);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(accent(layer.shift, layer.alpha / 5));
        p.drawPath(path);

        p.restore();
    }

    // Nucleo que late con el grave.
    const qreal coreX = rx * (0.06 + frame.bass * 0.16) * 2.4;
    const qreal coreY = ry * (0.06 + frame.bass * 0.16) * 2.4;
    QRadialGradient glow(QPointF(0, 0), std::max(coreX, coreY));
    glow.setColorAt(0.0, accent(0, 170, 1.1));
    glow.setColorAt(1.0, accent(0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(QPointF(0, 0), coreX, coreY);

    p.restore();
}

// -------------------------------------------------------------------- anillos

void paintRings(QPainter& p, const QRectF& area, const Frame& frame)
{
    const QPointF centre = area.center();

    // Elipses, no circunferencias: los anillos se estiran con la ventana en vez
    // de quedarse en el circulo que cabe en el lado corto.
    const qreal rx = area.width()  * 0.5;
    const qreal ry = area.height() * 0.5;

    p.setBrush(Qt::NoBrush);

    constexpr int kRings = 11;
    for (int i = 0; i < kRings; ++i) {
        const double t = double(i) / (kRings - 1);

        // Los de dentro responden al grave y los de fuera al agudo, que es como
        // se lee un espectro sin necesidad de etiquetas.
        const float energy = bandSmooth(*frame.spectrum, t, 5);
        const double factor = (0.10 + t * 0.90) * (1.0 + energy * 0.20);

        const int alpha = int(30 + energy * 190);
        QPen pen(accent(t * 60.0 - 30.0, alpha, 1.0));
        pen.setWidthF(1.2 + energy * 6.0);
        p.setPen(pen);
        p.drawEllipse(centre, rx * factor, ry * factor);
    }

    // Disco central con el nivel general.
    QRadialGradient glow(centre, std::max(rx, ry) * 0.32);
    glow.setColorAt(0.0, accent(0, int(40 + frame.level * 150), 1.1));
    glow.setColorAt(1.0, accent(0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(centre, rx * 0.32, ry * 0.32);
}

// ------------------------------------------------------------------ nebulosa

void paintNebula(QPainter& p, const QRectF& area, const Frame& frame)
{
    // Manchas grandes que se desplazan despacio y respiran con cada franja del
    // espectro. Es la unica que no dibuja lineas: hace de ambiente.
    const struct { double x, y, speed, shift; float Frame::*energy; } kBlobs[] = {
        {0.25, 0.35, 0.031,  -30, &Frame::bass},
        {0.72, 0.28, 0.023,   18, &Frame::mid},
        {0.55, 0.70, 0.037,   48, &Frame::treble},
        {0.15, 0.78, 0.017,    4, &Frame::mid},
        {0.85, 0.62, 0.029,  -14, &Frame::bass},
    };

    // El lado largo: las manchas tienen que cubrir la ventana entera, no solo
    // la banda central que cabe en el lado corto.
    const qreal reach = std::max(area.width(), area.height());

    p.setPen(Qt::NoPen);
    for (const auto& blob : kBlobs) {
        const double drift = frame.seconds * blob.speed;
        const qreal cx = area.left() + area.width()  * (blob.x + 0.06 * std::sin(drift * 2.0));
        const qreal cy = area.top()  + area.height() * (blob.y + 0.05 * std::cos(drift * 1.6));

        const float energy = frame.*(blob.energy);
        const qreal radius = reach * (0.22 + energy * 0.30);

        // Alfas generosos: al ser manchas difuminadas y superpuestas, con poca
        // opacidad no se distinguian del fondo liso.
        QRadialGradient gradient(QPointF(cx, cy), radius);
        gradient.setColorAt(0.0, accent(blob.shift, int(60 + energy * 170), 1.05));
        gradient.setColorAt(0.6, accent(blob.shift, int(22 + energy * 70)));
        gradient.setColorAt(1.0, accent(blob.shift, 0));

        p.setBrush(gradient);
        p.drawEllipse(QPointF(cx, cy), radius, radius);
    }
}

} // namespace

int count() { return int(std::size(kVisualizations)); }

const Info& info(int index)
{
    return kVisualizations[std::clamp(index, 0, count() - 1)];
}

int indexOfId(const char* id)
{
    for (int i = 0; i < count(); ++i) {
        if (std::strcmp(kVisualizations[i].id, id) == 0)
            return i;
    }
    return 0;
}

void paint(QPainter& painter, const QRectF& area, int index, const Frame& frame)
{
    if (!frame.spectrum || !frame.wave || area.isEmpty())
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(area);

    switch (std::clamp(index, 0, count() - 1)) {
    case 0: paintBars(painter, area, frame);    break;
    case 1: paintMirror(painter, area, frame);  break;
    case 2: paintWave(painter, area, frame);    break;
    case 3: paintMandala(painter, area, frame); break;
    case 4: paintRings(painter, area, frame);   break;
    case 5: paintNebula(painter, area, frame);  break;
    }

    painter.restore();
}

} // namespace Visualizations
