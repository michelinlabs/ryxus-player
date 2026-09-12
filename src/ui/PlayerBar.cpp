#include "ui/PlayerBar.h"
#include "core/Lang.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/SeekBar.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>

PlayerBar::PlayerBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("playerBar"));
    setFixedHeight(Theme::Metrics::PlayerBarHeight);
    buildUi();
}

void PlayerBar::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ------------------------------------------------ transporte (izquierda)
    auto* transportBlock = new QWidget(this);
    transportBlock->setFixedWidth(Theme::Metrics::LeftPanelWidth);
    auto* transportLayout = new QVBoxLayout(transportBlock);
    transportLayout->setContentsMargins(0, 4, 0, 2);
    transportLayout->setSpacing(0);

    auto* mainRow = new QHBoxLayout;
    mainRow->setSpacing(5);
    mainRow->addStretch(1);

    const auto makeTransport = [transportBlock](Icons::Icon icon, int diameter, int glyph) {
        auto* button = new FlatButton(icon, transportBlock);
        button->setFixedDiameter(diameter);
        button->setGlyphSize(glyph);
        button->setColors(Theme::Wave, Theme::Text, Theme::AccentBright);
        return button;
    };

    m_previous = makeTransport(Icons::Prev,  32, 19);
    m_stop     = makeTransport(Icons::Stop,  32, 15);
    m_play     = makeTransport(Icons::Play,  42, 21);
    m_pause    = makeTransport(Icons::Pause, 32, 17);
    m_next     = makeTransport(Icons::Next,  32, 19);

    // Solo el de reproducir lleva anillo, como en la referencia.
    m_play->setDrawsRing(true);
    m_play->setColors(Theme::Text, Theme::AccentBright, Theme::AccentBright);
    m_play->setToolTip(Lang::tr("Reproducir"));
    m_pause->setToolTip(Lang::tr("Pausa"));
    m_stop->setToolTip(Lang::tr("Parar"));
    m_previous->setToolTip(Lang::tr("Anterior"));
    m_next->setToolTip(Lang::tr("Siguiente"));

    mainRow->addWidget(m_previous);
    mainRow->addWidget(m_stop);
    mainRow->addWidget(m_play);
    mainRow->addWidget(m_pause);
    mainRow->addWidget(m_next);
    mainRow->addStretch(1);
    transportLayout->addLayout(mainRow);

    auto* toggleRow = new QHBoxLayout;
    toggleRow->setSpacing(14);
    toggleRow->addStretch(1);

    const auto makeToggle = [transportBlock](Icons::Icon icon) {
        auto* button = new FlatButton(icon, transportBlock);
        button->setFixedDiameter(26);
        button->setGlyphSize(17);
        button->setStrokeWidth(1.4);
        button->setColors(Theme::TextFaint, Theme::Text, Theme::AccentBright);
        return button;
    };

    m_shuffle = makeToggle(Icons::Shuffle);
    m_shuffle->setCheckable(true);
    m_shuffle->setToolTip(Lang::tr("Orden aleatorio"));

    m_abLoop = makeToggle(Icons::ABLoop);
    m_abLoop->setCheckable(true);
    m_abLoop->setToolTip(Lang::tr("Repetir fragmento A-B"));

    m_repeat = makeToggle(Icons::Repeat);
    m_repeat->setToolTip(Lang::tr("Repeticion: desactivada / lista / pista"));

    toggleRow->addWidget(m_shuffle);
    toggleRow->addWidget(m_abLoop);
    toggleRow->addWidget(m_repeat);
    toggleRow->addStretch(1);
    transportLayout->addLayout(toggleRow);

    root->addWidget(transportBlock);

    // ------------------------------------------- onda + volumen (derecha)
    auto* rightBlock = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightBlock);
    rightLayout->setContentsMargins(0, 2, 8, 2);
    rightLayout->setSpacing(0);

    m_seek = new SeekBar(rightBlock);
    rightLayout->addWidget(m_seek, 1);

    auto* toolRow = new QHBoxLayout;
    toolRow->setContentsMargins(6, 0, 0, 0);
    toolRow->setSpacing(8);

    m_volumeIcon = new FlatButton(Icons::Volume, rightBlock);
    m_volumeIcon->setFixedDiameter(26);
    m_volumeIcon->setGlyphSize(18);
    m_volumeIcon->setStrokeWidth(1.4);
    m_volumeIcon->setToolTip(Lang::tr("Silenciar"));
    toolRow->addWidget(m_volumeIcon);

    m_volume = new QSlider(Qt::Horizontal, rightBlock);
    m_volume->setObjectName(QStringLiteral("volumeSlider"));
    m_volume->setRange(0, 100);
    m_volume->setFixedWidth(120);
    m_volume->setToolTip(Lang::tr("Volumen"));
    toolRow->addWidget(m_volume);

    toolRow->addSpacing(10);

    m_equalizer = new FlatButton(Icons::Equalizer, rightBlock);
    m_equalizer->setFixedDiameter(26);
    m_equalizer->setGlyphSize(18);
    m_equalizer->setStrokeWidth(1.4);
    m_equalizer->setCheckable(true);
    m_equalizer->setColors(Theme::TextFaint, Theme::Text, Theme::AccentBright);
    m_equalizer->setToolTip(Lang::tr("Ecualizador de 12 bandas y efectos"));
    toolRow->addWidget(m_equalizer);

    m_clock = new FlatButton(Icons::Clock, rightBlock);
    m_clock->setFixedDiameter(26);
    m_clock->setGlyphSize(18);
    m_clock->setStrokeWidth(1.4);
    m_clock->setColors(Theme::TextFaint, Theme::Text, Theme::AccentBright);
    m_clock->setToolTip(Lang::tr("Alternar tiempo transcurrido / restante"));
    toolRow->addWidget(m_clock);

    toolRow->addStretch(1);
    rightLayout->addLayout(toolRow);

    root->addWidget(rightBlock, 1);

    // --- conexiones --------------------------------------------------------
    connect(m_previous,  &FlatButton::clicked, this, &PlayerBar::previousClicked);
    connect(m_stop,      &FlatButton::clicked, this, &PlayerBar::stopClicked);
    connect(m_play,      &FlatButton::clicked, this, &PlayerBar::playClicked);
    connect(m_pause,     &FlatButton::clicked, this, &PlayerBar::pauseClicked);
    connect(m_next,      &FlatButton::clicked, this, &PlayerBar::nextClicked);
    connect(m_shuffle,   &FlatButton::toggled, this, &PlayerBar::shuffleToggled);
    connect(m_abLoop,    &FlatButton::clicked, this, &PlayerBar::abLoopClicked);
    connect(m_repeat,    &FlatButton::clicked, this, &PlayerBar::repeatCycled);
    connect(m_volumeIcon,&FlatButton::clicked, this, &PlayerBar::muteToggled);
    connect(m_equalizer, &FlatButton::toggled, this, &PlayerBar::equalizerPanelToggled);
    connect(m_clock,     &FlatButton::clicked, this, &PlayerBar::timeDisplayToggled);

    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        emit volumeChanged(value / 100.0f);
    });
}

void PlayerBar::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;

    // Se resalta el boton que refleja el estado actual.
    m_play->setColors(playing ? Theme::AccentBright : Theme::Text,
                      Theme::AccentBright, Theme::AccentBright);
    m_play->setToolTip(playing ? Lang::tr("Volver al principio de la pista")
                               : Lang::tr("Reproducir"));
    m_pause->setColors(playing ? Theme::Wave : Theme::TextFaint,
                       Theme::Text, Theme::AccentBright);
}

void PlayerBar::setVolume(float linear)
{
    const int value = qBound(0, int(linear * 100.0f + 0.5f), 100);
    if (m_volume->value() != value) {
        QSignalBlocker blocker(m_volume);
        m_volume->setValue(value);
    }
    refreshVolumeIcon();
}

void PlayerBar::setMuted(bool muted)
{
    m_muted = muted;
    refreshVolumeIcon();
}

void PlayerBar::refreshVolumeIcon()
{
    if (m_muted || m_volume->value() == 0)
        m_volumeIcon->setIconId(Icons::VolumeMute);
    else if (m_volume->value() < 45)
        m_volumeIcon->setIconId(Icons::VolumeLow);
    else
        m_volumeIcon->setIconId(Icons::Volume);

    m_volumeIcon->setColors(m_muted ? Theme::Danger : Theme::TextDim,
                            Theme::Text, Theme::AccentBright);
}

void PlayerBar::setShuffle(bool shuffle)
{
    QSignalBlocker blocker(m_shuffle);
    m_shuffle->setChecked(shuffle);
}

void PlayerBar::setRepeatMode(int mode)
{
    m_repeatMode = mode;
    m_repeat->setIconId(mode == 2 ? Icons::RepeatOne : Icons::Repeat);
    m_repeat->setChecked(mode != 0);

    static const char* const kNames[] = {"desactivada", "toda la lista", "pista actual"};
    m_repeat->setToolTip(Lang::tr("Repeticion: %1")
                             .arg(QString::fromLatin1(kNames[qBound(0, mode, 2)])));
}

void PlayerBar::setEqualizerActive(bool active)
{
    // Cuando el EQ esta procesando, el icono queda en violeta aunque el panel
    // este cerrado: es la senal de que el sonido no sale plano.
    m_equalizer->setColors(active ? Theme::AccentLight : Theme::TextFaint,
                           Theme::Text, Theme::AccentBright);
}

void PlayerBar::setEqualizerPanelOpen(bool open)
{
    QSignalBlocker blocker(m_equalizer);
    m_equalizer->setChecked(open);
}

void PlayerBar::setAbLoopActive(bool active)
{
    QSignalBlocker blocker(m_abLoop);
    m_abLoop->setChecked(active);
}

void PlayerBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::chromeBg());
    p.setPen(QPen(Theme::Border, 1));
    p.drawLine(0, 0, width(), 0);
}
