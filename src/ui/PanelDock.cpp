#include "ui/PanelDock.h"

#include "core/Lang.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr int kTitleHeight = 24;
constexpr int kEdge        = 6;   // franja de agarre en los bordes al flotar
}

PanelDock::PanelDock(const QString& title, const QString& id, QWidget* content, QWidget* parent)
    : QDockWidget(title, parent)
{
    setObjectName(id);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                | QDockWidget::DockWidgetClosable);

    // El contenido va dentro de un marco propio: al flotar, el margen del
    // marco es la franja por la que se agarra para estirar, como el borde de
    // una ventana de Windows.
    m_frame = new QWidget(this);
    auto* frameLayout = new QVBoxLayout(m_frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);
    frameLayout->addWidget(content);
    m_frame->setMouseTracking(true);
    m_frame->installEventFilter(this);
    setWidget(m_frame);

    // --- barra de titulo ---------------------------------------------------
    // Los clics que no caen en los botones los atiende QDockWidget: arrastrar
    // mueve el panel y el doble clic lo saca o lo vuelve a acoplar.
    m_bar = new QWidget(this);
    QWidget* bar = m_bar;
    bar->setObjectName(QStringLiteral("panelDockTitle"));
    bar->setFixedHeight(kTitleHeight);
    bar->setCursor(Qt::SizeAllCursor);
    bar->setMouseTracking(true);
    bar->installEventFilter(this);

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 0, 2, 0);
    layout->setSpacing(0);

    m_title = new QLabel(title.toUpper(), bar);
    QFont font = Theme::uiFont(7, QFont::DemiBold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    m_title->setFont(font);
    m_title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(m_title);
    layout->addStretch(1);

    const auto makeButton = [bar](Icons::Icon icon) {
        auto* button = new FlatButton(icon, bar);
        button->setFixedSize(26, kTitleHeight);
        button->setGlyphSize(11);
        button->setStrokeWidth(1.2);
        button->setColors(Theme::TextFaint, Theme::Text, Theme::Text);
        button->setCursor(Qt::ArrowCursor);
        return button;
    };

    m_float = makeButton(Icons::WinRestore);
    layout->addWidget(m_float);
    connect(m_float, &FlatButton::clicked, this, [this]() { setFloating(!isFloating()); });

    m_hide = makeButton(Icons::WinMinimize);
    m_hide->setToolTip(Lang::tr("Minimizar el panel (se recupera desde el menu)"));
    layout->addWidget(m_hide);
    connect(m_hide, &FlatButton::clicked, this, [this]() {
        close();
        emit userClosed();
    });

    setTitleBarWidget(bar);

    connect(this, &QDockWidget::topLevelChanged, this, &PanelDock::onFloatingChanged);
    onFloatingChanged(false);
}

void PanelDock::onFloatingChanged(bool floating)
{
    m_float->setIconId(floating ? Icons::WinMaximize : Icons::WinRestore);
    m_float->setToolTip(floating ? Lang::tr("Volver a acoplar el panel")
                                 : Lang::tr("Sacar el panel a una ventana flotante"));

    // Acoplado lo estiran los separadores entre paneles; flotando, sus
    // propios bordes. El de arriba lo pone la barra de titulo.
    const int edge = floating ? kEdge : 0;
    m_frame->layout()->setContentsMargins(edge, 0, edge, edge);
    if (!floating)
        setEdgeCursor(m_frame, Qt::Edges());
    update();
}

Qt::Edges PanelDock::edgesAt(const QPoint& globalPos) const
{
    const QPoint pos = mapFromGlobal(globalPos);
    Qt::Edges edges;
    if (pos.x() < kEdge)              edges |= Qt::LeftEdge;
    if (pos.x() >= width() - kEdge)   edges |= Qt::RightEdge;
    if (pos.y() < kEdge)              edges |= Qt::TopEdge;
    if (pos.y() >= height() - kEdge)  edges |= Qt::BottomEdge;
    return edges;
}

void PanelDock::setEdgeCursor(QWidget* widget, Qt::Edges edges)
{
    if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge))
        widget->setCursor(Qt::SizeFDiagCursor);
    else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge))
        widget->setCursor(Qt::SizeBDiagCursor);
    else if (edges & (Qt::LeftEdge | Qt::RightEdge))
        widget->setCursor(Qt::SizeHorCursor);
    else if (edges & (Qt::TopEdge | Qt::BottomEdge))
        widget->setCursor(Qt::SizeVerCursor);
    else if (widget == m_bar)
        widget->setCursor(Qt::SizeAllCursor);   // la barra se arrastra entera
    else
        widget->unsetCursor();
}

bool PanelDock::eventFilter(QObject* watched, QEvent* event)
{
    // Solo flotando: acoplado, los bordes los gestiona el contenedor.
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget || !isFloating())
        return QDockWidget::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->buttons() == Qt::NoButton)
            setEdgeCursor(widget, edgesAt(mouse->globalPosition().toPoint()));
        break;
    }
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const Qt::Edges edges = edgesAt(mouse->globalPosition().toPoint());
        // El estiramiento lo hace Windows, igual que en cualquier ventana:
        // con su ajuste a bordes y sin parpadeos.
        if (mouse->button() == Qt::LeftButton && edges != Qt::Edges()) {
            if (QWindow* handle = windowHandle()) {
                handle->startSystemResize(edges);
                return true;
            }
        }
        break;
    }
    case QEvent::Leave:
        setEdgeCursor(widget, Qt::Edges());
        break;
    default:
        break;
    }
    return QDockWidget::eventFilter(watched, event);
}

void PanelDock::paintEvent(QPaintEvent* event)
{
    // Acoplado, el panel deja ver el fondo de la ventana a traves de su
    // transparencia. Flotando no hay nada detras: se pinta el color base
    // opaco para que no se vea negro, y un borde para separarlo del resto.
    if (isFloating()) {
        QPainter p(this);
        p.fillRect(rect(), Theme::Chrome);
        p.setPen(Theme::BorderLight);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        return;
    }
    QDockWidget::paintEvent(event);
}
