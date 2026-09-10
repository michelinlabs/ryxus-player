#include "ui/CoverCropDialog.h"

#include "core/Lang.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

constexpr int kViewSide   = 420;   // lado del area de previsualizacion
constexpr int kHandleSide = 12;    // tirador de la esquina inferior derecha
constexpr int kMinCrop    = 32;    // lado minimo del recorte, en pixeles reales

const int kResolutions[] = {300, 500, 600, 800, 1000, 1200};

} // namespace

// ------------------------------------------------------------------ CropView

// Muestra la imagen encajada en el area y encima un cuadrado de seleccion que
// se arrastra para moverlo y se estira por la esquina inferior derecha. El
// cuadrado se guarda en coordenadas de la imagen original, no de la pantalla:
// asi el recorte no pierde precision al reescalar la vista.
class CoverCropDialog::CropView : public QWidget {
public:
    explicit CropView(const QImage& source, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_source(source)
    {
        setFixedSize(kViewSide, kViewSide);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
        selectAll();
    }

    QRect cropRect() const { return m_crop; }
    const QImage& source() const { return m_source; }

    // Cuadrado mayor posible, centrado.
    void selectAll()
    {
        const int side = std::min(m_source.width(), m_source.height());
        m_crop = QRect((m_source.width()  - side) / 2,
                       (m_source.height() - side) / 2,
                       side, side);
        update();
        if (m_onChange)
            m_onChange();
    }

    void setChangeHandler(std::function<void()> handler) { m_onChange = std::move(handler); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        p.fillRect(rect(), Theme::panelDeepBg());

        const QRect target = imageRect();
        p.drawImage(target, m_source);

        // Todo lo que queda fuera del recorte se oscurece.
        const QRect crop = toView(m_crop);
        QRegion outside(target);
        outside -= QRegion(crop);
        p.setClipRegion(outside);
        p.fillRect(target, QColor(0, 0, 0, 140));
        p.setClipping(false);

        p.setPen(QPen(Theme::AccentBright, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(crop.adjusted(0, 0, -1, -1));

        // Guias de tercios: ayudan a encuadrar.
        QPen guide(QColor(255, 255, 255, 60));
        guide.setWidth(1);
        p.setPen(guide);
        for (int i = 1; i < 3; ++i) {
            const int x = crop.left() + crop.width()  * i / 3;
            const int y = crop.top()  + crop.height() * i / 3;
            p.drawLine(x, crop.top(), x, crop.bottom());
            p.drawLine(crop.left(), y, crop.right(), y);
        }

        p.setPen(Qt::NoPen);
        p.setBrush(Theme::AccentBright);
        p.drawRect(handleRect(crop));
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton)
            return;

        const QRect crop = toView(m_crop);
        if (handleRect(crop).contains(event->pos())) {
            m_resizing = true;
        } else if (crop.contains(event->pos())) {
            m_dragging = true;
            setCursor(Qt::ClosedHandCursor);
        }
        m_grabPoint = event->pos();
        m_grabCrop  = m_crop;
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const QRect crop = toView(m_crop);

        if (!m_dragging && !m_resizing) {
            if (handleRect(crop).contains(event->pos()))
                setCursor(Qt::SizeFDiagCursor);
            else
                setCursor(crop.contains(event->pos()) ? Qt::OpenHandCursor : Qt::ArrowCursor);
            return;
        }

        // El desplazamiento se convierte a pixeles de la imagen original.
        const double factor = 1.0 / std::max(scale(), 1e-6);
        const int dx = int((event->pos().x() - m_grabPoint.x()) * factor);
        const int dy = int((event->pos().y() - m_grabPoint.y()) * factor);

        if (m_resizing) {
            const int maxSide = std::min(m_source.width()  - m_grabCrop.left(),
                                         m_source.height() - m_grabCrop.top());
            const int side = std::clamp(m_grabCrop.width() + std::max(dx, dy),
                                        kMinCrop, maxSide);
            m_crop = QRect(m_grabCrop.topLeft(), QSize(side, side));
        } else {
            m_crop = QRect(QPoint(std::clamp(m_grabCrop.left() + dx,
                                             0, m_source.width()  - m_grabCrop.width()),
                                  std::clamp(m_grabCrop.top() + dy,
                                             0, m_source.height() - m_grabCrop.height())),
                           m_grabCrop.size());
        }

        update();
        if (m_onChange)
            m_onChange();
    }

    void mouseReleaseEvent(QMouseEvent*) override
    {
        m_dragging = false;
        m_resizing = false;
        setCursor(Qt::OpenHandCursor);
    }

private:
    // Factor de la imagen original a la vista.
    double scale() const
    {
        if (m_source.isNull())
            return 1.0;
        return std::min(double(kViewSide) / m_source.width(),
                        double(kViewSide) / m_source.height());
    }

    QRect imageRect() const
    {
        const double s = scale();
        const int w = int(m_source.width()  * s);
        const int h = int(m_source.height() * s);
        return QRect((kViewSide - w) / 2, (kViewSide - h) / 2, w, h);
    }

    QRect toView(const QRect& imageSpace) const
    {
        const double s = scale();
        const QRect base = imageRect();
        return QRect(base.left() + int(imageSpace.left() * s),
                     base.top()  + int(imageSpace.top()  * s),
                     std::max(1, int(imageSpace.width()  * s)),
                     std::max(1, int(imageSpace.height() * s)));
    }

    static QRect handleRect(const QRect& crop)
    {
        return QRect(crop.right() - kHandleSide + 1, crop.bottom() - kHandleSide + 1,
                     kHandleSide, kHandleSide);
    }

    QImage m_source;
    QRect  m_crop;
    QPoint m_grabPoint;
    QRect  m_grabCrop;
    bool   m_dragging = false;
    bool   m_resizing = false;
    std::function<void()> m_onChange;
};

// ------------------------------------------------------------ CoverCropDialog

CoverCropDialog::CoverCropDialog(const QImage& source, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("settingsDialog"));   // reutiliza el estilo
    setWindowTitle(Lang::tr("Ajustar la caratula"));
    setModal(true);
    buildUi(source);
}

void CoverCropDialog::buildUi(const QImage& source)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(10);

    auto* hint = new QLabel(
        Lang::tr("Arrastra el cuadro para elegir el encuadre y tira de la esquina\n"
                 "para cambiar su tamano. La caratula se guarda cuadrada."), this);
    hint->setObjectName(QStringLiteral("detailKey"));
    hint->setFont(Theme::uiFont(9));
    root->addWidget(hint);

    m_view = new CropView(source, this);
    root->addWidget(m_view, 0, Qt::AlignHCenter);

    // --- resolucion de salida ---------------------------------------------
    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    auto* label = new QLabel(Lang::tr("Resolucion"), this);
    label->setObjectName(QStringLiteral("detailKey"));
    label->setFont(Theme::uiFont(9));
    row->addWidget(label);

    m_resolution = new QComboBox(this);
    m_resolution->setFont(Theme::uiFont(9));
    m_resolution->setFixedWidth(120);
    for (int side : kResolutions)
        m_resolution->addItem(QStringLiteral("%1 x %1").arg(side), side);
    m_resolution->setCurrentIndex(2);   // 600 px: suficiente y no infla el archivo
    row->addWidget(m_resolution);

    auto* selectAll = new FlatButton(Lang::tr("Toda la imagen"), this);
    selectAll->setIconId(Icons::Image);
    selectAll->setGlyphSize(14);
    selectAll->setFixedHeight(24);
    row->addWidget(selectAll);
    connect(selectAll, &FlatButton::clicked, this, [this]() { m_view->selectAll(); });

    row->addStretch(1);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("statusText"));
    m_summary->setFont(Theme::uiFont(8));
    row->addWidget(m_summary);

    root->addLayout(row);

    // --- aceptar / cancelar -----------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(Lang::tr("Usar esta caratula"));
    buttons->button(QDialogButtonBox::Cancel)->setText(Lang::tr("Cancelar"));
    buttons->setFont(Theme::uiFont(9));
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    const auto refreshSummary = [this] {
        const QRect crop = m_view->cropRect();
        m_summary->setText(Lang::tr("Recorte: %1 x %2 px")
                               .arg(crop.width())
                               .arg(crop.height()));
    };
    m_view->setChangeHandler(refreshSummary);
    refreshSummary();
}

QImage CoverCropDialog::result() const
{
    const QImage& source = m_view->source();
    const QRect   crop   = m_view->cropRect().intersected(source.rect());
    if (source.isNull() || crop.isEmpty())
        return QImage();

    const int side = m_resolution->currentData().toInt();

    // Se recorta primero y se reescala despues, para no perder detalle
    // reescalando la imagen entera. Si el recorte es mas pequeno que la
    // resolucion pedida no se amplia: ampliar solo anade peso, no nitidez.
    const QImage cropped = source.copy(crop);
    if (cropped.width() <= side)
        return cropped;

    return cropped.scaled(side, side, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
