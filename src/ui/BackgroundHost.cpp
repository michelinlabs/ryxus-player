#include "ui/BackgroundHost.h"
#include "core/Lang.h"
#include "ui/Theme.h"

#include <QImageReader>
#include <QPainter>
#include <QResizeEvent>

BackgroundHost::BackgroundHost(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("centralRoot"));
    // El widget pinta su fondo por completo: Qt puede saltarse el borrado.
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

QString BackgroundHost::modeName(Mode mode)
{
    switch (mode) {
    case Mode::Cover:   return Lang::tr("Cubrir");
    case Mode::Fit:     return Lang::tr("Ajustar");
    case Mode::Stretch: return Lang::tr("Estirar");
    case Mode::Tile:    return Lang::tr("Mosaico");
    }
    return QString();
}

bool BackgroundHost::setImagePath(const QString& path)
{
    if (path.isEmpty()) {
        m_path.clear();
        m_source = QImage();
        m_scaled = QPixmap();
        update();
        return true;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);   // respeta la orientacion EXIF
    const QImage image = reader.read();
    if (image.isNull())
        return false;

    m_path   = path;
    m_source = image;
    rebuildScaled();
    update();
    return true;
}

void BackgroundHost::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    rebuildScaled();
    update();
}

void BackgroundHost::setDarkening(int percent)
{
    percent = qBound(0, percent, 90);
    if (m_darkening == percent)
        return;
    m_darkening = percent;
    update();
}

void BackgroundHost::resizeEvent(QResizeEvent* event)
{
    rebuildScaled();
    QWidget::resizeEvent(event);
}

void BackgroundHost::rebuildScaled()
{
    if (m_source.isNull() || width() <= 0 || height() <= 0) {
        m_scaled = QPixmap();
        return;
    }

    // El escalado se hace una vez por cambio de tamano, no en cada repintado:
    // con una foto grande y 30 fps de visualizador seria carisimo.
    const qreal dpr = devicePixelRatioF();
    const QSize target = (size() * dpr);

    QImage rendered;
    switch (m_mode) {
    case Mode::Cover:
        rendered = m_source.scaled(target, Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);
        break;
    case Mode::Fit:
        rendered = m_source.scaled(target, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        break;
    case Mode::Stretch:
        rendered = m_source.scaled(target, Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);
        break;
    case Mode::Tile:
        rendered = m_source;   // se repite en el paintEvent
        break;
    }

    m_scaled = QPixmap::fromImage(rendered);
    m_scaled.setDevicePixelRatio(dpr);
}

void BackgroundHost::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // 1) color base del skin: es lo que se ve cuando no hay imagen, y lo que
    //    rellena los bordes en el modo "Ajustar".
    p.fillRect(rect(), Theme::Chrome);

    if (m_scaled.isNull()) {
        return;
    }

    // 2) la imagen.
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_mode == Mode::Tile) {
        p.drawTiledPixmap(rect(), m_scaled);
    } else {
        const QSize logical = m_scaled.size() / m_scaled.devicePixelRatio();
        const QPoint origin(rect().center().x() - logical.width() / 2,
                            rect().center().y() - logical.height() / 2);
        p.drawPixmap(QRect(origin, logical), m_scaled);
    }

    // 3) velo oscuro: mantiene el contraste del texto sobre fotos claras.
    if (m_darkening > 0)
        p.fillRect(rect(), QColor(0, 0, 0, m_darkening * 255 / 100));
}
