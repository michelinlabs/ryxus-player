#include "ui/EffectsRack.h"

#include "core/Lang.h"
#include "core/Settings.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>
#include <utility>

namespace {

constexpr int kTicks      = 1000;   // resolucion interna de cada deslizador
constexpr int kColumns    = 3;      // 3 x 2 tarjetas
constexpr int kCardWidth  = 170;
constexpr int kCardHeight = 95;
constexpr int kLabelWidth = 52;
constexpr int kValueWidth = 40;
constexpr int kGap        = 6;

} // namespace

EffectsRack::EffectsRack(Effects* effects, QWidget* parent)
    : QWidget(parent)
    , m_effects(effects)
{
    setObjectName(QStringLiteral("effectsRack"));
    buildUi();
    loadFromSettings();
}

int EffectsRack::toSlider(const Effects::ParamInfo& info, float value)
{
    const float span = info.maximum - info.minimum;
    if (span <= 0.0f)
        return 0;
    return qBound(0, int(std::lround((value - info.minimum) / span * kTicks)), kTicks);
}

float EffectsRack::fromSlider(const Effects::ParamInfo& info, int ticks)
{
    const float span = info.maximum - info.minimum;
    return info.minimum + span * float(qBound(0, ticks, kTicks)) / float(kTicks);
}

QString EffectsRack::formatValue(const Effects::ParamInfo& info, float value)
{
    return QStringLiteral("%1%2")
        .arg(double(value), 0, 'f', info.decimals)
        .arg(QString::fromLatin1(info.suffix));
}

void EffectsRack::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 7, 14, 9);   // alineado con la columna del EQ
    root->setSpacing(0);

    // En ventanas estrechas la rejilla no cabe entera al lado del plot, asi
    // que se desplaza en horizontal en vez de encoger las tarjetas hasta que
    // los deslizadores dejan de poder tocarse.
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("effectsScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(scroll);

    auto* host = new QWidget(scroll);
    auto* grid = new QGridLayout(host);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(kGap);
    grid->setVerticalSpacing(kGap);

    for (int device = 0; device < Effects::DeviceCount; ++device)
        grid->addWidget(buildCard(device), device / kColumns, device % kColumns);

    grid->setColumnStretch(kColumns, 1);
    scroll->setWidget(host);

    // Ancho fijo: las tarjetas no se estiran, asi que darle mas espacio solo
    // dejaria hueco vacio, y darle menos obligaria a desplazar la rejilla. Con
    // esto los seis dispositivos se ven siempre enteros y quien cede ancho es
    // el ecualizador, que si sabe aprovechar el que le toque.
    setFixedWidth(sizeHint().width());
    setMaximumHeight(sizeHint().height());
}

QSize EffectsRack::sizeHint() const
{
    const int rows = (Effects::DeviceCount + kColumns - 1) / kColumns;
    return QSize(kColumns * kCardWidth + (kColumns - 1) * kGap + 14,
                 rows * kCardHeight + (rows - 1) * kGap + 12);
}

QWidget* EffectsRack::buildCard(int device)
{
    const Effects::DeviceInfo& info = Effects::info(device);

    auto* frame = new QWidget(this);
    frame->setObjectName(QStringLiteral("effectCard"));
    frame->setFixedSize(kCardWidth, kCardHeight);
    frame->setToolTip(Lang::tr(info.hint));

    auto* column = new QVBoxLayout(frame);
    column->setContentsMargins(8, 4, 8, 5);
    column->setSpacing(4);

    // --- cabecera: la casilla lleva el nombre del efecto -------------------
    //
    // Es un QCheckBox de verdad, no un icono conmutable: apagado se ve la
    // casilla vacia, que es como se lee de un vistazo que el efecto no esta
    // haciendo nada.
    auto* power = new QCheckBox(Lang::tr(info.name), frame);
    power->setObjectName(QStringLiteral("effectPower"));
    power->setFont(Theme::uiFont(8, QFont::DemiBold));
    power->setToolTip(Lang::tr("Activar o desactivar este efecto"));
    column->addWidget(power);

    // --- mandos ------------------------------------------------------------
    for (int p = 0; p < info.paramCount; ++p) {
        const Effects::ParamInfo& pi = info.params[p];

        auto* line = new QHBoxLayout;
        line->setSpacing(5);

        auto* name = new QLabel(Lang::tr(pi.name), frame);
        name->setObjectName(QStringLiteral("effectParam"));
        name->setFont(Theme::uiFont(8));
        name->setFixedWidth(kLabelWidth);
        line->addWidget(name);

        auto* slider = new QSlider(Qt::Horizontal, frame);
        slider->setObjectName(QStringLiteral("effectSlider"));
        slider->setRange(0, kTicks);
        slider->setSingleStep(kTicks / 100);
        slider->setPageStep(kTicks / 20);
        slider->setFixedHeight(14);
        // Sin foco de teclado: si no, al abrir el rack el area desplazable
        // salta para "revelar" el primer deslizador y corta las tarjetas.
        slider->setFocusPolicy(Qt::NoFocus);
        line->addWidget(slider, 1);

        auto* value = new QLabel(frame);
        value->setObjectName(QStringLiteral("effectValue"));
        value->setFont(Theme::uiFont(8));
        value->setFixedWidth(kValueWidth);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(value);

        column->addLayout(line);

        const ParamRow rowInfo{device, p, slider, value};
        m_rows.append(rowInfo);

        connect(slider, &QSlider::valueChanged, this, [this, rowInfo](int ticks) {
            applyParam(rowInfo, ticks, true);
        });
    }

    column->addStretch(1);

    m_cards.append(Card{device, frame, power});

    connect(power, &QCheckBox::toggled, this, [this, device](bool on) {
        setDeviceEnabled(device, on, true);
    });

    return frame;
}

void EffectsRack::applyParam(const ParamRow& row, int ticks, bool persist)
{
    const Effects::DeviceInfo& info = Effects::info(row.device);
    const Effects::ParamInfo&  pi   = info.params[row.index];

    const float value = fromSlider(pi, ticks);
    m_effects->setParam(row.device, row.index, value);
    row.value->setText(formatValue(pi, value));

    if (persist && !m_loading) {
        Settings::setEffectParam(QString::fromLatin1(info.id),
                                 QString::fromLatin1(pi.name), value);
    }
}

void EffectsRack::setDeviceEnabled(int device, bool on, bool persist)
{
    m_effects->setDeviceEnabled(device, on);

    for (const Card& card : std::as_const(m_cards)) {
        if (card.device == device)
            refreshCardLook(card);
    }

    if (persist && !m_loading)
        Settings::setEffectEnabled(QString::fromLatin1(Effects::info(device).id), on);

    emit activityChanged(m_effects->isAnyEnabled());
}

void EffectsRack::refreshCardLook(const Card& card)
{
    const bool on = m_effects->isDeviceEnabled(card.device);

    // El estado encendido se marca con una propiedad dinamica y lo pinta la
    // hoja de estilos. Asi el color sigue saliendo de la paleta activa, cosa
    // que no pasaria con un setStyleSheet() de colores fijos por widget.
    for (QWidget* widget : {card.frame, static_cast<QWidget*>(card.power)}) {
        widget->setProperty("on", on);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }

    // Los mandos de un efecto apagado se ven, pero atenuados: dejan claro que
    // no estan haciendo nada sin esconder como quedo configurado.
    for (const ParamRow& row : std::as_const(m_rows)) {
        if (row.device != card.device)
            continue;
        row.slider->setEnabled(on);
        row.value->setEnabled(on);
    }
}

void EffectsRack::loadFromSettings()
{
    m_loading = true;

    for (const ParamRow& row : std::as_const(m_rows)) {
        const Effects::DeviceInfo& info = Effects::info(row.device);
        const Effects::ParamInfo&  pi   = info.params[row.index];

        const float value = Settings::effectParam(QString::fromLatin1(info.id),
                                                  QString::fromLatin1(pi.name),
                                                  pi.defaultValue);
        QSignalBlocker blocker(row.slider);
        row.slider->setValue(toSlider(pi, value));
        applyParam(row, row.slider->value(), false);
    }

    for (const Card& card : std::as_const(m_cards)) {
        const bool on = Settings::effectEnabled(
            QString::fromLatin1(Effects::info(card.device).id));
        QSignalBlocker blocker(card.power);
        card.power->setChecked(on);
        setDeviceEnabled(card.device, on, false);
    }

    m_loading = false;
    emit activityChanged(m_effects->isAnyEnabled());
}

void EffectsRack::resetAll()
{
    m_effects->resetAll();

    // Sin m_loading a proposito: aqui si hay que guardar el estado nuevo. Los
    // controles se mueven con las senales bloqueadas y se aplica a mano.
    for (const ParamRow& row : std::as_const(m_rows)) {
        const Effects::ParamInfo& pi = Effects::info(row.device).params[row.index];
        QSignalBlocker blocker(row.slider);
        row.slider->setValue(toSlider(pi, pi.defaultValue));
        applyParam(row, row.slider->value(), true);
    }
    for (const Card& card : std::as_const(m_cards)) {
        QSignalBlocker blocker(card.power);
        card.power->setChecked(false);
        setDeviceEnabled(card.device, false, true);
    }

    emit activityChanged(false);
}
