#include "ui/TitleBar.h"
#include "core/Lang.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::Metrics::TitleBarHeight);
    setAttribute(Qt::WA_StyledBackground, false);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch(1);

    const auto makeButton = [this](Icons::Icon icon, const QColor& hover) {
        auto* button = new FlatButton(icon, this);
        button->setFixedSize(46, Theme::Metrics::TitleBarHeight);
        button->setGlyphSize(16);
        button->setStrokeWidth(1.2);
        button->setColors(Theme::TextDim, hover, hover);
        return button;
    };

    m_settings = makeButton(Icons::Settings, Theme::Text);
    m_settings->setToolTip(Lang::tr("Configuracion"));
    m_settings->setGlyphSize(17);
    layout->addWidget(m_settings);
    connect(m_settings, &FlatButton::clicked, this, &TitleBar::settingsRequested);

    m_menu = makeButton(Icons::Menu, Theme::Text);
    m_menu->setToolTip(Lang::tr("Menu de la aplicacion"));
    m_menu->setGlyphSize(15);
    layout->addWidget(m_menu);
    connect(m_menu, &FlatButton::clicked, this, [this]() {
        emit menuRequested(m_menu->mapToGlobal(QPoint(0, m_menu->height())));
    });

    m_minimize = makeButton(Icons::WinMinimize, Theme::Text);
    m_maximize = makeButton(Icons::WinMaximize, Theme::Text);
    m_close    = makeButton(Icons::WinClose,    QColor(0xFF, 0xFF, 0xFF));

    layout->addWidget(m_minimize);
    layout->addWidget(m_maximize);
    layout->addWidget(m_close);

    connect(m_minimize, &FlatButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximize, &FlatButton::clicked, this, &TitleBar::maximizeRequested);
    connect(m_close,    &FlatButton::clicked, this, &TitleBar::closeRequested);
}

void TitleBar::setMaximized(bool maximized)
{
    m_isMaximized = maximized;
    m_maximize->setIconId(maximized ? Icons::WinRestore : Icons::WinMaximize);
}

void TitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        emit menuRequested(event->globalPosition().toPoint());
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // startSystemMove delega el arrastre al gestor de ventanas: mantiene
        // el ajuste a bordes (Aero Snap) que se perderia moviendo a mano.
        if (QWindow* handle = window()->windowHandle()) {
            handle->startSystemMove();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit maximizeRequested();
}

void TitleBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // Medido en la referencia: la barra de titulo es #1F1F1F, un punto mas
    // clara que la columna izquierda.
    p.fillRect(rect(), Theme::chromeLightBg());

    // Logotipo: triangulo de reproduccion recortado, en el violeta de acento.
    const qreal cy = height() / 2.0;
    QPainterPath mark;
    mark.moveTo(11.0, cy - 7.0);
    mark.lineTo(23.0, cy);
    mark.lineTo(11.0, cy + 7.0);
    mark.closeSubpath();

    QPainterPath notch;
    notch.moveTo(11.0, cy - 0.5);
    notch.lineTo(18.5, cy - 0.5);
    notch.lineTo(18.5, cy + 3.5);
    notch.lineTo(11.0, cy + 3.5);
    notch.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::AccentBright);
    p.drawPath(mark.subtracted(notch));

    QFont logoFont = Theme::uiFont(9, QFont::DemiBold);
    logoFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    p.setFont(logoFont);
    p.setPen(Theme::Text);
    p.drawText(QRect(30, 0, 200, height()), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("ROXAS PLAYER"));
}
