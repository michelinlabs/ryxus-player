#include "ui/NowPlayingPanel.h"
#include "core/Lang.h"

#include "ui/CoverArtView.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/NowPlayingHeader.h"
#include "ui/Theme.h"
#include "ui/TrackDetails.h"
#include "ui/Visualizer.h"

#include <QFileInfo>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

NowPlayingPanel::NowPlayingPanel(AudioEngine* engine, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("nowPlayingPanel"));
    // Ancho orientativo, no fijo: la columna vive dentro de un divisor y el
    // usuario la estira. Se conserva un minimo para que la caratula y la ficha
    // de detalles sigan siendo legibles.
    setMinimumWidth(232);
    setMaximumWidth(560);
    resize(Theme::Metrics::LeftPanelWidth, height());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- caratula: 250x250 en (16, 37), como en la referencia --------------
    auto* coverHolder = new QWidget(this);
    auto* coverLayout = new QVBoxLayout(coverHolder);
    coverLayout->setContentsMargins(Theme::Metrics::CoverMargin, 1,
                                    Theme::Metrics::CoverMargin, 0);

    m_cover = new CoverArtView(coverHolder);
    m_cover->setFixedSize(Theme::Metrics::CoverSize, Theme::Metrics::CoverSize);
    coverLayout->addWidget(m_cover, 0, Qt::AlignHCenter);
    root->addWidget(coverHolder);

    connect(m_cover, &CoverArtView::changeRequested, this, &NowPlayingPanel::coverChangeRequested);
    connect(m_cover, &CoverArtView::removeRequested, this, &NowPlayingPanel::coverRemoveRequested);
    connect(m_cover, &CoverArtView::exportRequested, this, &NowPlayingPanel::coverExportRequested);

    // --- titulo / artista / album / formato -------------------------------
    m_header = new NowPlayingHeader(this);
    root->addWidget(m_header);
    connect(m_header, &NowPlayingHeader::ratingChanged, this, &NowPlayingPanel::ratingChanged);

    // --- analizador, inmediatamente despues, como en la referencia --------
    m_visualizer = new Visualizer(engine, this);
    m_visualizer->setFixedHeight(Theme::Metrics::VisualizerHeight);
    root->addWidget(m_visualizer);

    // --- ficha de detalles: ocupa el hueco libre del skin -----------------
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("metaScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_details = new TrackDetails(scroll);
    scroll->setWidget(m_details);
    root->addWidget(scroll, 1);

    connect(m_details, &TrackDetails::editRequested, this, &NowPlayingPanel::editTagsRequested);

    // El boton va fuera del area desplazable: con el ecualizador abierto la
    // ficha se recorta, y esta es la accion que no puede quedar escondida.
    m_editButton = new FlatButton(Lang::tr("Editar etiquetas"), this);
    m_editButton->setIconId(Icons::Tag);
    m_editButton->setGlyphSize(14);
    m_editButton->setFixedHeight(26);
    m_editButton->setColors(Theme::AccentBright, Theme::Text, Theme::Text);
    m_editButton->setToolTip(Lang::tr("Abre la pestana de etiquetas (F4)"));
    m_editButton->setEnabled(false);
    root->addWidget(m_editButton);
    connect(m_editButton, &FlatButton::clicked, this, &NowPlayingPanel::editTagsRequested);

    m_footer = new QLabel(this);
    m_footer->setObjectName(QStringLiteral("nowPlayingFooter"));
    m_footer->setFont(Theme::uiFont(8));
    m_footer->setContentsMargins(14, 2, 14, 6);
    m_footer->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_footer);
}

void NowPlayingPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_footerPath.isEmpty()) {
        m_footer->setText(m_footer->fontMetrics().elidedText(
            m_footerPath, Qt::ElideMiddle, qMax(80, m_footer->width())));
    }
}

void NowPlayingPanel::setPlayingTrack(const TrackInfo& info)
{
    m_header->setTrack(info);
    m_cover->setCover(info.cover);
    m_cover->setEditable(info.isValid());

    if (info.isValid()) {
        const QString folder = QFileInfo(info.path).absolutePath();
        m_footerPath = folder;
        // Se recorta contra el ancho real de la columna, no contra el de
        // referencia: ahora el usuario la estira con el divisor.
        m_footer->setText(m_footer->fontMetrics().elidedText(
            folder, Qt::ElideMiddle, qMax(80, m_footer->width())));
        m_footer->setToolTip(info.path);
    } else {
        m_footer->clear();
        m_footer->setToolTip(QString());
    }
}

void NowPlayingPanel::setDetailsTrack(const TrackInfo& info)
{
    m_details->setTrack(info);
    m_editButton->setEnabled(info.isValid());
}

void NowPlayingPanel::clearTrack()
{
    setPlayingTrack(TrackInfo());
    setDetailsTrack(TrackInfo());
}

void NowPlayingPanel::setPlaying(bool playing)
{
    m_visualizer->setActive(playing);
}

void NowPlayingPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // Capa unica del panel. Se pinta aqui y no por hoja de estilos porque
    // este widget sobreescribe paintEvent: la regla QSS nunca se aplicaria.
    p.fillRect(rect(), Theme::chromeBg());

    // Separador de 1 px con la columna central.
    p.setPen(QPen(Theme::Border, 1));
    p.drawLine(width() - 1, 0, width() - 1, height());
}
