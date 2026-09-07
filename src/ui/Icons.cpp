#include "ui/Icons.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include <cmath>

namespace Icons {
namespace {

constexpr qreal kPi = 3.14159265358979323846;

void triangleRight(QPainter& p, qreal cx, qreal cy, qreal w, qreal h, const QColor& c)
{
    QPolygonF tri;
    tri << QPointF(cx - w / 2, cy - h / 2)
        << QPointF(cx + w / 2, cy)
        << QPointF(cx - w / 2, cy + h / 2);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPolygon(tri);
}

void triangleLeft(QPainter& p, qreal cx, qreal cy, qreal w, qreal h, const QColor& c)
{
    QPolygonF tri;
    tri << QPointF(cx + w / 2, cy - h / 2)
        << QPointF(cx - w / 2, cy)
        << QPointF(cx + w / 2, cy + h / 2);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPolygon(tri);
}

// dir: 0=derecha 1=abajo 2=izquierda 3=arriba
void chevron(QPainter& p, qreal cx, qreal cy, qreal size, int dir)
{
    QPainterPath path;
    const qreal h = size / 2;
    switch (dir) {
    case 0:
        path.moveTo(cx - h * 0.6, cy - h);
        path.lineTo(cx + h * 0.6, cy);
        path.lineTo(cx - h * 0.6, cy + h);
        break;
    case 1:
        path.moveTo(cx - h, cy - h * 0.6);
        path.lineTo(cx, cy + h * 0.6);
        path.lineTo(cx + h, cy - h * 0.6);
        break;
    case 2:
        path.moveTo(cx + h * 0.6, cy - h);
        path.lineTo(cx - h * 0.6, cy);
        path.lineTo(cx + h * 0.6, cy + h);
        break;
    default:
        path.moveTo(cx - h, cy + h * 0.6);
        path.lineTo(cx, cy - h * 0.6);
        path.lineTo(cx + h, cy + h * 0.6);
        break;
    }
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

void starPath(QPainterPath& path, qreal cx, qreal cy, qreal rOuter, qreal rInner)
{
    for (int i = 0; i < 10; ++i) {
        const qreal r = (i % 2 == 0) ? rOuter : rInner;
        const qreal a = -kPi / 2 + i * kPi / 5;
        const QPointF pt(cx + r * std::cos(a), cy + r * std::sin(a));
        if (i == 0)
            path.moveTo(pt);
        else
            path.lineTo(pt);
    }
    path.closeSubpath();
}

void speakerBody(QPainter& p, const QColor& c)
{
    QPainterPath body;
    body.moveTo(4, 9.5);
    body.lineTo(7.5, 9.5);
    body.lineTo(11.5, 5.5);
    body.lineTo(11.5, 18.5);
    body.lineTo(7.5, 14.5);
    body.lineTo(4, 14.5);
    body.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPath(body);
}

} // namespace

void paint(QPainter& p, Icon id, const QRectF& rect, const QColor& color, qreal strokeWidth)
{
    if (id == None)
        return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = qMin(rect.width(), rect.height()) / 24.0;
    p.translate(rect.center());
    p.scale(s, s);
    p.translate(-12.0, -12.0);

    QPen pen(color);
    pen.setWidthF(strokeWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (id) {
    case Play:
        // El centro de masa de un triangulo esta a un tercio de la base, no en
        // el centro de su caja: centrando la caja el glifo se ve corrido hacia
        // la izquierda. Se desplaza w/6 a la derecha para que el centroide
        // caiga justo en el centro del anillo.
        triangleRight(p, 12.0 + 11.0 / 6.0, 12, 11, 13, color);
        break;

    case Pause:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(7.5, 5.5, 3.2, 13), 1.0, 1.0);
        p.drawRoundedRect(QRectF(13.3, 5.5, 3.2, 13), 1.0, 1.0);
        break;

    case Stop:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(6.5, 6.5, 11, 11), 1.4, 1.4);
        break;

    case Prev:
        triangleLeft(p, 13.5, 12, 10, 12, color);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(5.8, 6, 2.4, 12), 1.0, 1.0);
        break;

    case Next:
        triangleRight(p, 10.5, 12, 10, 12, color);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(15.8, 6, 2.4, 12), 1.0, 1.0);
        break;

    case Shuffle: {
        QPainterPath a;
        a.moveTo(3, 7);
        a.lineTo(7, 7);
        a.cubicTo(12, 7, 12, 17, 17, 17);
        QPainterPath b;
        b.moveTo(3, 17);
        b.lineTo(7, 17);
        b.cubicTo(12, 17, 12, 7, 17, 7);
        p.drawPath(a);
        p.drawPath(b);
        triangleRight(p, 19.4, 7, 4.2, 5.6, color);
        triangleRight(p, 19.4, 17, 4.2, 5.6, color);
        break;
    }

    case Repeat:
    case RepeatOne: {
        QPainterPath r;
        r.arcMoveTo(QRectF(4.5, 5.5, 15, 13), 130);
        r.arcTo(QRectF(4.5, 5.5, 15, 13), 130, -285);
        p.drawPath(r);
        triangleRight(p, 8.4, 6.6, 4.2, 5.0, color);
        if (id == RepeatOne) {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(QRectF(11.3, 9.4, 1.8, 6.4), 0.8, 0.8);
            p.drawRoundedRect(QRectF(9.7, 9.4, 3.4, 1.7), 0.8, 0.8);
        }
        break;
    }

    case ABLoop: {
        QFont f = p.font();
        f.setPointSizeF(8.5);
        f.setBold(true);
        p.setFont(f);
        p.setPen(color);
        p.drawText(QRectF(0, 0, 24, 24), Qt::AlignCenter, QStringLiteral("A-B"));
        break;
    }

    case Volume:
        speakerBody(p, color);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(9.5, 7.0, 8.0, 10.0), -60 * 16, 120 * 16);
        p.drawArc(QRectF(11.0, 4.5, 11.0, 15.0), -60 * 16, 120 * 16);
        break;

    case VolumeLow:
        speakerBody(p, color);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(9.5, 7.0, 8.0, 10.0), -60 * 16, 120 * 16);
        break;

    case VolumeMute:
        speakerBody(p, color);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(14.5, 9.0), QPointF(20.0, 15.0));
        p.drawLine(QPointF(20.0, 9.0), QPointF(14.5, 15.0));
        break;

    case Equalizer: {
        const qreal xs[3] = {7.0, 12.0, 17.0};
        const qreal ks[3] = {8.5, 14.5, 11.0};
        for (int i = 0; i < 3; ++i) {
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(xs[i], 4.0), QPointF(xs[i], 20.0));
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(QRectF(xs[i] - 2.6, ks[i] - 1.4, 5.2, 2.8), 1.2, 1.2);
        }
        break;
    }

    case Clock:
        p.drawEllipse(QPointF(12, 12), 8.2, 8.2);
        p.drawLine(QPointF(12, 12), QPointF(12, 7.2));
        p.drawLine(QPointF(12, 12), QPointF(15.6, 13.6));
        break;

    case Star: {
        QPainterPath sp;
        starPath(sp, 12, 12.4, 8.4, 3.7);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(sp);
        break;
    }

    case StarOutline: {
        QPainterPath sp;
        starPath(sp, 12, 12.4, 8.4, 3.7);
        pen.setWidthF(1.2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(sp);
        break;
    }

    case Plus:
        p.drawLine(QPointF(12, 6), QPointF(12, 18));
        p.drawLine(QPointF(6, 12), QPointF(18, 12));
        break;

    case Minus:
        p.drawLine(QPointF(6, 12), QPointF(18, 12));
        break;

    case More:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        for (int i = 0; i < 3; ++i)
            p.drawEllipse(QPointF(6.0 + i * 6.0, 12.0), 1.35, 1.35);
        break;

    case SortUpDown:
        p.drawLine(QPointF(8.5, 5.5), QPointF(8.5, 18.5));
        p.drawLine(QPointF(15.5, 5.5), QPointF(15.5, 18.5));
        chevron(p, 8.5, 7.2, 5.0, 3);
        chevron(p, 15.5, 16.8, 5.0, 1);
        break;

    case Search:
        p.drawEllipse(QPointF(10.6, 10.6), 5.6, 5.6);
        p.drawLine(QPointF(14.8, 14.8), QPointF(19.4, 19.4));
        break;

    case Menu:
        for (int i = 0; i < 3; ++i)
            p.drawLine(QPointF(5, 7.0 + i * 5.0), QPointF(19, 7.0 + i * 5.0));
        break;

    case Close:
    case WinClose:
        p.drawLine(QPointF(6.5, 6.5), QPointF(17.5, 17.5));
        p.drawLine(QPointF(17.5, 6.5), QPointF(6.5, 17.5));
        break;

    case WinMinimize:
        p.drawLine(QPointF(6.0, 12.0), QPointF(18.0, 12.0));
        break;

    case WinMaximize:
        p.drawRect(QRectF(6.5, 6.5, 11, 11));
        break;

    case WinRestore:
        p.drawRect(QRectF(5.5, 8.5, 9.5, 9.5));
        p.drawPolyline(QPolygonF() << QPointF(8.5, 8.5) << QPointF(8.5, 5.5)
                                   << QPointF(18.5, 5.5) << QPointF(18.5, 15.5)
                                   << QPointF(15.5, 15.5));
        break;

    case Folder: {
        QPainterPath fp;
        fp.moveTo(4, 8.0);
        fp.lineTo(9.5, 8.0);
        fp.lineTo(11.2, 10.0);
        fp.lineTo(20, 10.0);
        fp.lineTo(20, 18.5);
        fp.lineTo(4, 18.5);
        fp.closeSubpath();
        p.drawPath(fp);
        break;
    }

    case FolderOpen: {
        QPainterPath fp;
        fp.moveTo(4, 18.5);
        fp.lineTo(4, 7.5);
        fp.lineTo(9.5, 7.5);
        fp.lineTo(11.2, 9.6);
        fp.lineTo(17.5, 9.6);
        fp.lineTo(17.5, 12.0);
        p.drawPath(fp);
        QPainterPath lid;
        lid.moveTo(4, 18.5);
        lid.lineTo(7.2, 12.0);
        lid.lineTo(21.0, 12.0);
        lid.lineTo(17.8, 18.5);
        lid.closeSubpath();
        p.drawPath(lid);
        break;
    }

    case ChevronRight:
        chevron(p, 12, 12, 8, 0);
        break;
    case ChevronDown:
        chevron(p, 12, 12, 8, 1);
        break;
    case ChevronUp:
        chevron(p, 12, 12, 8, 3);
        break;

    case ArrowRight:
        p.drawLine(QPointF(5, 12), QPointF(17, 12));
        chevron(p, 15.5, 12, 8, 0);
        break;

    case Check:
        p.drawPolyline(QPolygonF() << QPointF(5.5, 12.5) << QPointF(10.0, 17.0)
                                   << QPointF(18.5, 7.5));
        break;

    case Grip:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 2; ++c)
                p.drawEllipse(QPointF(9.5 + c * 5.0, 7.0 + r * 5.0), 1.15, 1.15);
        break;

    case Tag: {
        QPainterPath tp;
        tp.moveTo(11.5, 4.5);
        tp.lineTo(19.5, 4.5);
        tp.lineTo(19.5, 12.5);
        tp.lineTo(11.0, 21.0);
        tp.lineTo(3.0, 13.0);
        tp.closeSubpath();
        p.drawPath(tp);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(15.9, 8.1), 1.5, 1.5);
        break;
    }

    case Save: {
        QPainterPath sp;
        sp.moveTo(4.5, 4.5);
        sp.lineTo(16.5, 4.5);
        sp.lineTo(19.5, 7.5);
        sp.lineTo(19.5, 19.5);
        sp.lineTo(4.5, 19.5);
        sp.closeSubpath();
        p.drawPath(sp);
        p.drawRect(QRectF(8.0, 4.5, 8.0, 5.0));
        p.drawRect(QRectF(7.5, 13.0, 9.0, 6.5));
        break;
    }

    case Revert: {
        QPainterPath rp;
        rp.arcMoveTo(QRectF(4.5, 5.0, 15, 14), 160);
        rp.arcTo(QRectF(4.5, 5.0, 15, 14), 160, 260);
        p.drawPath(rp);
        triangleLeft(p, 6.6, 8.6, 4.4, 5.2, color);
        break;
    }

    case Image:
        p.drawRoundedRect(QRectF(4.0, 6.0, 16.0, 12.5), 1.6, 1.6);
        p.drawPolyline(QPolygonF() << QPointF(4.8, 16.2) << QPointF(9.5, 11.2)
                                   << QPointF(13.0, 14.6) << QPointF(16.0, 11.8)
                                   << QPointF(19.2, 15.2));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(15.4, 9.6), 1.4, 1.4);
        break;

    case Info:
        p.drawEllipse(QPointF(12, 12), 8.2, 8.2);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(12, 7.9), 1.05, 1.05);
        p.drawRoundedRect(QRectF(11.1, 10.4, 1.9, 6.2), 0.9, 0.9);
        break;

    case Trash:
        p.drawLine(QPointF(4.5, 7.0), QPointF(19.5, 7.0));
        p.drawPolyline(QPolygonF() << QPointF(9.5, 7.0) << QPointF(9.5, 4.8)
                                   << QPointF(14.5, 4.8) << QPointF(14.5, 7.0));
        p.drawPolyline(QPolygonF() << QPointF(6.5, 7.0) << QPointF(7.6, 19.6)
                                   << QPointF(16.4, 19.6) << QPointF(17.5, 7.0));
        break;

    case Settings: {
        // Corona dentada + eje: ocho dientes trapezoidales alrededor del centro.
        QPainterPath gear;
        constexpr int kTeeth = 8;
        const qreal outer = 10.2, inner = 7.6;
        for (int i = 0; i < kTeeth; ++i) {
            const qreal a0 = i * 2.0 * kPi / kTeeth;
            const qreal wTooth = kPi / kTeeth * 0.52;
            const qreal wGap   = kPi / kTeeth * 0.48;

            const QPointF p0(12 + inner * std::cos(a0 - wGap),
                             12 + inner * std::sin(a0 - wGap));
            const QPointF p1(12 + outer * std::cos(a0 - wTooth),
                             12 + outer * std::sin(a0 - wTooth));
            const QPointF p2(12 + outer * std::cos(a0 + wTooth),
                             12 + outer * std::sin(a0 + wTooth));
            const QPointF p3(12 + inner * std::cos(a0 + wGap),
                             12 + inner * std::sin(a0 + wGap));

            if (i == 0) gear.moveTo(p0); else gear.lineTo(p0);
            gear.lineTo(p1);
            gear.lineTo(p2);
            gear.lineTo(p3);
        }
        gear.closeSubpath();

        QPainterPath hub;
        hub.addEllipse(QPointF(12, 12), 3.6, 3.6);

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(gear.subtracted(hub));
        break;
    }

    case Refresh: {
        QPainterPath rp;
        rp.arcMoveTo(QRectF(4.5, 4.5, 15, 15), 60);
        rp.arcTo(QRectF(4.5, 4.5, 15, 15), 60, 280);
        p.drawPath(rp);
        triangleRight(p, 17.4, 8.4, 4.4, 5.2, color);
        break;
    }

    default:
        break;
    }

    p.restore();
}

QIcon icon(Icon id, const QColor& color, int px)
{
    static QHash<QString, QIcon> cache;
    const QString key = QStringLiteral("%1|%2|%3").arg(int(id)).arg(color.name()).arg(px);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    paint(p, id, QRectF(0, 0, px, px), color);
    p.end();

    const QIcon ic(pm);
    cache.insert(key, ic);
    return ic;
}

} // namespace Icons
