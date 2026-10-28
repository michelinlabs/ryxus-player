#include "ui/VideoFilterPanel.h"

#include "core/Lang.h"
#include "ui/Theme.h"
#include "ui/VideoMasks.h"

#include <QAbstractButton>
#include <QEnterEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int kTileWidth   = 100;
constexpr int kThumbHeight = 56;      // 16:9 dentro del ancho de la tarjeta
constexpr int kTileHeight  = kThumbHeight + 24;
constexpr int kGap         = 8;
constexpr int kPreviewWidth = 360;

// Sin video abierto las miniaturas salen de una imagen de muestra con cielo,
// piel, verde y sombras: se nota bien lo que hace cada filtro.
QImage sampleImage()
{
    QImage image(320, 180, QImage::Format_RGB32);
    QPainter p(&image);
    QLinearGradient sky(0, 0, 0, 110);
    sky.setColorAt(0.0, QColor(0x6F, 0xA8, 0xDC));
    sky.setColorAt(1.0, QColor(0xF4, 0xC2, 0x8B));
    p.fillRect(QRect(0, 0, 320, 110), sky);
    p.fillRect(QRect(0, 110, 320, 70), QColor(0x2F, 0x5D, 0x3A));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(0xFF, 0xE0, 0x7A));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(250, 60), 24, 24);
    p.setBrush(QColor(0xE8, 0xB4, 0x96));
    p.drawEllipse(QPointF(95, 100), 26, 30);
    p.setBrush(QColor(0x22, 0x22, 0x2A));
    p.drawRect(QRectF(70, 128, 50, 52));
    p.setBrush(QColor(0xC0, 0x39, 0x2B));
    p.drawRect(QRectF(170, 120, 40, 60));
    return image;
}

QImage masked(const QImage& source, int mask, const QSize& size)
{
    QImage image = source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                         .convertToFormat(QImage::Format_RGB32);
    VideoMasks::apply(image, mask);
    return image;
}

} // namespace

// ---------------------------------------------------------------- FilterTile

// Una tarjeta de la paleta: miniatura con la mascara aplicada y su nombre.
class FilterTile : public QAbstractButton {
public:
    FilterTile(int mask, QWidget* parent)
        : QAbstractButton(parent)
        , m_mask(mask)
    {
        setFixedSize(kTileWidth, kTileHeight);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QString());
    }

    int mask() const { return m_mask; }

    void setThumbnail(const QImage& image)
    {
        m_thumb = image;
        update();
    }

    void setSelected(bool selected)
    {
        m_selected = selected;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF thumb(1, 1, width() - 2, kThumbHeight);
        QPainterPath clip;
        clip.addRoundedRect(thumb, 4, 4);
        p.save();
        p.setClipPath(clip);
        if (!m_thumb.isNull()) {
            // Recorte centrado: la miniatura llena la tarjeta sin deformarse.
            const QRectF src((m_thumb.width() - thumb.width()) / 2.0,
                             (m_thumb.height() - thumb.height()) / 2.0,
                             thumb.width(), thumb.height());
            p.drawImage(thumb, m_thumb, src);
        }
        // Modo cine: sus franjas tambien en la miniatura.
        if (VideoMasks::letterboxes(m_mask)) {
            const double band = thumb.width() / VideoMasks::kCinemaAspect;
            const double bar = (thumb.height() - band) / 2.0;
            p.fillRect(QRectF(thumb.left(), thumb.top(), thumb.width(), bar), Qt::black);
            p.fillRect(QRectF(thumb.left(), thumb.bottom() - bar, thumb.width(), bar), Qt::black);
        }
        p.restore();

        QPen frame(m_selected ? Theme::AccentBright
                              : (underMouse() ? Theme::BorderLight : Theme::Border));
        frame.setWidthF(m_selected ? 2.0 : 1.0);
        p.setPen(frame);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(thumb, 4, 4);

        p.setPen(m_selected ? Theme::Text : Theme::TextDim);
        p.setFont(Theme::uiFont(8, m_selected ? QFont::DemiBold : QFont::Normal));
        p.drawText(QRectF(0, kThumbHeight + 3, width(), 20), Qt::AlignCenter,
                   VideoMasks::name(m_mask));
    }

    void enterEvent(QEnterEvent* event) override
    {
        QAbstractButton::enterEvent(event);
        update();
    }
    void leaveEvent(QEvent* event) override
    {
        QAbstractButton::leaveEvent(event);
        update();
    }

private:
    int    m_mask = 0;
    QImage m_thumb;
    bool   m_selected = false;
};

// ----------------------------------------------------------- VideoFilterPanel

VideoFilterPanel::VideoFilterPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("videoFilterPanel"));
    setMinimumSize(kTileWidth * 2 + kGap * 3, kTileHeight + kGap * 2);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Si no caben todas, se desplaza: el panel puede quedar estrecho.
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_grid = new QWidget(scroll);
    m_grid->setObjectName(QStringLiteral("videoFilterGrid"));
    scroll->setWidget(m_grid);
    root->addWidget(scroll);

    auto* grid = new QGridLayout(m_grid);
    grid->setContentsMargins(kGap, kGap, kGap, kGap);
    grid->setSpacing(kGap);

    for (int mask = 0; mask < VideoMasks::Count; ++mask) {
        auto* tile = new FilterTile(mask, m_grid);
        tile->installEventFilter(this);
        connect(tile, &FilterTile::clicked, this, [this, mask]() {
            setCurrentMask(mask);
            emit maskSelected(mask);
        });
        m_tiles << tile;
    }

    // Vista grande al pasar el raton: ventana de tipo tooltip, que no roba
    // el foco ni aparece en la barra de tareas.
    m_preview = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
    m_preview->setAttribute(Qt::WA_ShowWithoutActivating);
    m_preview->setStyleSheet(QStringLiteral("QLabel { background: %1; border: 1px solid %2; }")
                                 .arg(Theme::Chrome.name(), Theme::BorderLight.name()));

    m_source = sampleImage();
    relayoutTiles();
    rebuildThumbnails();
    setCurrentMask(0);
}

VideoFilterPanel::~VideoFilterPanel()
{
    delete m_preview;   // ventana suelta, sin padre
}

void VideoFilterPanel::setSource(const QImage& frame)
{
    m_source = frame.isNull() ? sampleImage() : frame;

    // Todas a la vez distraen: tras las primeras, solo se mueve la miniatura
    // (y la vista grande) que tiene el raton encima.
    if (m_fullRefreshes > 0) {
        --m_fullRefreshes;
        rebuildThumbnails();
    } else if (m_previewMask >= 0) {
        refreshTile(m_previewMask);
    }
    if (m_previewMask >= 0 && m_preview->isVisible())
        showPreview(m_previewMask, m_previewPos);
}

void VideoFilterPanel::resetSource()
{
    m_fullRefreshes = 3;
    m_source = sampleImage();
    rebuildThumbnails();
}

void VideoFilterPanel::refreshTile(int mask)
{
    for (FilterTile* tile : std::as_const(m_tiles)) {
        if (tile->mask() != mask)
            continue;
        tile->setThumbnail(masked(m_source, mask, QSize(kTileWidth, kThumbHeight)));
        return;
    }
}

void VideoFilterPanel::setCurrentMask(int mask)
{
    m_current = mask;
    for (FilterTile* tile : std::as_const(m_tiles))
        tile->setSelected(tile->mask() == mask);
}

void VideoFilterPanel::rebuildThumbnails()
{
    // Una sola reduccion para todas y luego cada mascara sobre esa copia:
    // nueve miniaturas cuestan menos que un fotograma grande.
    const QSize size(kTileWidth, kThumbHeight);
    const QImage base = m_source.scaled(size, Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_RGB32);
    for (FilterTile* tile : std::as_const(m_tiles)) {
        QImage thumb = base.copy();
        VideoMasks::apply(thumb, tile->mask());
        tile->setThumbnail(thumb);
    }
}

void VideoFilterPanel::relayoutTiles()
{
    auto* grid = static_cast<QGridLayout*>(m_grid->layout());
    const int available = std::max(1, width() - kGap * 2 - 14);   // 14: barra de desplazamiento
    const int columns = std::max(1, (available + kGap) / (kTileWidth + kGap));

    for (FilterTile* tile : std::as_const(m_tiles))
        grid->removeWidget(tile);
    // Lo que se estiro en un reparto anterior (con otro numero de columnas)
    // dejaria un hueco en medio de las miniaturas.
    for (int c = 0; c < grid->columnCount(); ++c)
        grid->setColumnStretch(c, 0);
    for (int r = 0; r < grid->rowCount(); ++r)
        grid->setRowStretch(r, 0);
    for (int i = 0; i < m_tiles.size(); ++i)
        grid->addWidget(m_tiles.at(i), i / columns, i % columns, Qt::AlignLeft | Qt::AlignTop);
    grid->setColumnStretch(columns, 1);
    grid->setRowStretch(int((m_tiles.size() + columns - 1) / columns), 1);
}

void VideoFilterPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayoutTiles();
}

void VideoFilterPanel::hideEvent(QHideEvent* event)
{
    hidePreview();
    QWidget::hideEvent(event);
}

bool VideoFilterPanel::eventFilter(QObject* watched, QEvent* event)
{
    auto* tile = dynamic_cast<FilterTile*>(watched);
    if (tile) {
        if (event->type() == QEvent::Enter)
            showPreview(tile->mask(), tile->mapToGlobal(QPoint(0, 0)));
        else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress)
            hidePreview();
    }
    return QWidget::eventFilter(watched, event);
}

void VideoFilterPanel::showPreview(int mask, const QPoint& tileTopLeft)
{
    m_previewMask = mask;
    m_previewPos = tileTopLeft;

    const QSize size(kPreviewWidth, kPreviewWidth * 9 / 16);
    QImage image = masked(m_source, mask, size);

    // El nombre del filtro sobre la propia vista, abajo.
    {
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing, true);
        if (VideoMasks::letterboxes(mask)) {
            const double band = image.width() / VideoMasks::kCinemaAspect;
            const int bar = int((image.height() - band) / 2.0);
            p.fillRect(QRect(0, 0, image.width(), bar), Qt::black);
            p.fillRect(QRect(0, image.height() - bar, image.width(), bar), Qt::black);
        }
        const QRect label(0, image.height() - 28, image.width(), 28);
        p.fillRect(label, QColor(0, 0, 0, 150));
        p.setPen(Qt::white);
        p.setFont(Theme::uiFont(9, QFont::DemiBold));
        p.drawText(label, Qt::AlignCenter, VideoMasks::name(mask));
    }
    m_preview->setPixmap(QPixmap::fromImage(image));
    m_preview->adjustSize();

    // Encima de la tarjeta (la paleta suele ir abajo), sin salirse de la
    // pantalla; si arriba no cabe, debajo.
    QPoint pos(tileTopLeft.x() + kTileWidth / 2 - m_preview->width() / 2,
               tileTopLeft.y() - m_preview->height() - 8);
    if (const QScreen* screen = QGuiApplication::screenAt(tileTopLeft)) {
        const QRect area = screen->availableGeometry();
        pos.setX(std::clamp(pos.x(), area.left(), area.right() - m_preview->width()));
        if (pos.y() < area.top())
            pos.setY(tileTopLeft.y() + kTileHeight + 8);
    }
    m_preview->move(pos);
    m_preview->show();
}

void VideoFilterPanel::hidePreview()
{
    m_previewMask = -1;
    if (m_preview)
        m_preview->hide();
}
