#include "ui/SubtitleStyle.h"

#include "core/Lang.h"
#include "core/Settings.h"
#include "ui/Theme.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>

#include <algorithm>

namespace {
const QString kGroup = QStringLiteral("subtitles/");
}

QString SubtitleStyle::shapeName(int shape)
{
    switch (shape) {
    case ShapeLineBox:     return Lang::tr("Caja por linea");
    case ShapeRoundedLine: return Lang::tr("Caja redondeada por linea");
    case ShapeBlock:       return Lang::tr("Un bloque para todo el texto");
    case ShapeBand:        return Lang::tr("Banda de lado a lado");
    }
    return QString();
}

SubtitleStyle SubtitleStyle::load()
{
    const QSettings& s = Settings::store();
    SubtitleStyle style;
    style.family      = s.value(kGroup + QStringLiteral("family"), style.family).toString();
    style.bold        = s.value(kGroup + QStringLiteral("bold"), style.bold).toBool();
    style.sizePercent = s.value(kGroup + QStringLiteral("size"), style.sizePercent).toInt();
    style.textColor   = QColor(s.value(kGroup + QStringLiteral("color"),
                                       style.textColor.name()).toString());
    style.outline     = s.value(kGroup + QStringLiteral("outline"), style.outline).toInt();
    style.outlineColor = QColor(s.value(kGroup + QStringLiteral("outlineColor"),
                                        style.outlineColor.name()).toString());
    style.position    = s.value(kGroup + QStringLiteral("position"), style.position).toInt();
    style.alignment   = s.value(kGroup + QStringLiteral("align"), style.alignment).toInt();
    style.background  = s.value(kGroup + QStringLiteral("background"), style.background).toBool();
    style.shape       = s.value(kGroup + QStringLiteral("shape"), style.shape).toInt();
    style.backgroundColor = QColor(s.value(kGroup + QStringLiteral("backgroundColor"),
                                           style.backgroundColor.name()).toString());
    style.backgroundOpacity = s.value(kGroup + QStringLiteral("backgroundOpacity"),
                                      style.backgroundOpacity).toInt();
    return style;
}

void SubtitleStyle::save() const
{
    QSettings& s = Settings::store();
    s.setValue(kGroup + QStringLiteral("family"), family);
    s.setValue(kGroup + QStringLiteral("bold"), bold);
    s.setValue(kGroup + QStringLiteral("size"), sizePercent);
    s.setValue(kGroup + QStringLiteral("color"), textColor.name());
    s.setValue(kGroup + QStringLiteral("outline"), outline);
    s.setValue(kGroup + QStringLiteral("outlineColor"), outlineColor.name());
    s.setValue(kGroup + QStringLiteral("position"), position);
    s.setValue(kGroup + QStringLiteral("align"), alignment);
    s.setValue(kGroup + QStringLiteral("background"), background);
    s.setValue(kGroup + QStringLiteral("shape"), shape);
    s.setValue(kGroup + QStringLiteral("backgroundColor"), backgroundColor.name());
    s.setValue(kGroup + QStringLiteral("backgroundOpacity"), backgroundOpacity);
}

void SubtitleStyle::paint(QPainter& p, const QRectF& picture, const QString& text) const
{
    if (text.isEmpty() || picture.isEmpty())
        return;

    // --- letra ---------------------------------------------------------------
    // El tamano va con el alto de la imagen, no con el del panel: igual de
    // grande en una ventana pequena que a pantalla completa.
    QFont font = family.isEmpty() ? Theme::uiFont(10) : QFont(family);
    font.setBold(bold);
    font.setPixelSize(std::max(10, int(picture.height() * std::clamp(sizePercent, 1, 20) / 100.0)));
    const QFontMetricsF metrics(font);

    const QStringList lines = text.split(QLatin1Char('\n'));
    const double lineHeight = metrics.lineSpacing();
    const double padX = metrics.height() * 0.35;
    const double padY = metrics.height() * 0.08;
    const double margin = picture.width() * 0.04;

    // El bloque se apoya por abajo a `position` % del alto de la imagen, sin
    // salirse por arriba.
    const double blockHeight = lineHeight * lines.size();
    const double bottom = picture.bottom() - picture.height() * std::clamp(position, 0, 95) / 100.0;
    const double top = std::max(picture.top() + padY, bottom - blockHeight);

    QVector<QRectF> lineRects;
    lineRects.reserve(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        const double width = metrics.horizontalAdvance(lines.at(i));
        double x = picture.center().x() - width / 2.0;
        if (alignment == AlignLeft)
            x = picture.left() + margin;
        else if (alignment == AlignRight)
            x = picture.right() - margin - width;
        lineRects << QRectF(x, top + i * lineHeight, width, lineHeight);
    }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // --- fondo ---------------------------------------------------------------
    // Todas las piezas van en un unico trazado que se rellena una vez: donde
    // dos cajas se tocan no se acumula opacidad.
    if (background && backgroundOpacity > 0) {
        QPainterPath box;
        const double radius = lineHeight * 0.28;
        switch (shape) {
        case ShapeLineBox:
        case ShapeRoundedLine:
            for (const QRectF& line : std::as_const(lineRects)) {
                const QRectF r = line.adjusted(-padX, -padY, padX, padY);
                if (shape == ShapeRoundedLine)
                    box.addRoundedRect(r, radius, radius);
                else
                    box.addRect(r);
            }
            break;
        case ShapeBlock: {
            QRectF all;
            for (const QRectF& line : std::as_const(lineRects))
                all = all.isNull() ? line : all.united(line);
            const QRectF r = all.adjusted(-padX * 1.4, -padY * 3, padX * 1.4, padY * 3);
            box.addRoundedRect(r, radius * 0.6, radius * 0.6);
            break;
        }
        case ShapeBand:
        default:
            box.addRect(QRectF(picture.left(), top - padY * 3, picture.width(),
                               blockHeight + padY * 6));
            break;
        }

        QColor fill = backgroundColor;
        fill.setAlphaF(std::clamp(backgroundOpacity, 0, 100) / 100.0);
        p.fillPath(box.simplified(), fill);
    }

    // --- texto con contorno ------------------------------------------------
    QPainterPath glyphs;
    for (int i = 0; i < lines.size(); ++i)
        glyphs.addText(QPointF(lineRects.at(i).left(), lineRects.at(i).top() + metrics.ascent()),
                       font, lines.at(i));

    if (outline > 0) {
        const double width = font.pixelSize() * std::clamp(outline, 0, 10) / 30.0;
        p.strokePath(glyphs, QPen(outlineColor, std::max(1.0, width),
                                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }
    p.fillPath(glyphs, textColor);
    p.restore();
}
