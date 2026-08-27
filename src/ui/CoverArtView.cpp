#include "ui/CoverArtView.h"
#include "core/Lang.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

CoverArtView::CoverArtView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(120, 120);
    setAttribute(Qt::WA_Hover, true);
    setToolTip(Lang::tr("Doble clic para cambiar la caratula"));
}

void CoverArtView::setCover(const QImage& image)
{
    m_cover = image;
    rebuildScaled();
    update();
}

void CoverArtView::setEditable(bool editable)
{
    m_editable = editable;
    setCursor(editable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void CoverArtView::rebuildScaled()
{
    if (m_cover.isNull() || width() <= 2) {
        m_scaled = QPixmap();
        return;
    }
    const int side = qMin(width(), height()) - 2;
    m_scaled = QPixmap::fromImage(
        m_cover.scaled(side * devicePixelRatioF(), side * devicePixelRatioF(),
                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_scaled.setDevicePixelRatio(devicePixelRatioF());
}

void CoverArtView::resizeEvent(QResizeEvent* event)
{
    rebuildScaled();
    QWidget::resizeEvent(event);
}

void CoverArtView::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void CoverArtView::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void CoverArtView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_editable && event->button() == Qt::LeftButton)
        emit changeRequested();
}

void CoverArtView::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_editable)
        return;

    QMenu menu(this);
    menu.addAction(Icons::icon(Icons::Image, Theme::TextDim),
                   Lang::tr("Cambiar caratula..."),
                   this, &CoverArtView::changeRequested);
    QAction* exportAction = menu.addAction(Icons::icon(Icons::Save, Theme::TextDim),
                                           Lang::tr("Exportar caratula..."),
                                           this, &CoverArtView::exportRequested);
    menu.addSeparator();
    QAction* removeAction = menu.addAction(Icons::icon(Icons::Trash, Theme::TextDim),
                                           Lang::tr("Quitar caratula"),
                                           this, &CoverArtView::removeRequested);

    exportAction->setEnabled(hasCover());
    removeAction->setEnabled(hasCover());

    menu.exec(event->globalPos());
}

void CoverArtView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect box = rect();

    if (!m_scaled.isNull()) {
        const QSize logical = m_scaled.size() / m_scaled.devicePixelRatio();
        const QRect target(box.center().x() - logical.width() / 2,
                           box.center().y() - logical.height() / 2,
                           logical.width(), logical.height());
        p.drawPixmap(target, m_scaled);

        p.setPen(QPen(QColor(0, 0, 0, 120), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(target.adjusted(0, 0, -1, -1));
    } else {
        // Marcador de posicion cuando la pista no trae caratula incrustada.
        p.setPen(QPen(Theme::Border, 1));
        p.setBrush(Theme::panelDeepBg());
        p.drawRect(box.adjusted(0, 0, -1, -1));

        const int side = qMin(box.width(), box.height()) / 4;
        Icons::paint(p, Icons::Image,
                     QRectF(box.center().x() - side / 2.0, box.center().y() - side / 2.0,
                            side, side),
                     Theme::TextFaint, 1.4);
    }

    // Al pasar el raton se insinua que la imagen es editable.
    if (m_hovered && m_editable) {
        p.setPen(QPen(Theme::AccentBright, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box.adjusted(0, 0, -1, -1));
    }
}
