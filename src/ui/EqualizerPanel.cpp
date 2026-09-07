#include "ui/EqualizerPanel.h"
#include "core/Lang.h"

#include "core/AudioEngine.h"
#include "core/Settings.h"
#include "ui/EffectsRack.h"
#include "ui/EqCurveEditor.h"
#include "ui/EqSlider.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
// Alto del panel sin el rack de efectos: cabecera + preamplificador + plot.
constexpr int kBaseHeight = 220;
}

EqualizerPanel::EqualizerPanel(Equalizer* equalizer, AudioEngine* engine, QWidget* parent)
    : QWidget(parent)
    , m_equalizer(equalizer)
    , m_engine(engine)
{
    setObjectName(QStringLiteral("equalizerPanel"));
    setFixedHeight(kBaseHeight);
    buildUi();
    loadFromSettings();

    setRackOpen(Settings::effectsRackOpen());
}

void EqualizerPanel::buildUi()
{
    // El ecualizador no ocupa todo el ancho, pero se pega al borde izquierdo
    // de su columna: asi arranca justo donde termina el panel izquierdo, sin
    // un hueco muerto delante.
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* inner = new QWidget(this);
    inner->setObjectName(QStringLiteral("eqInner"));
    inner->setMaximumWidth(940);
    inner->setMinimumWidth(560);
    outer->addWidget(inner, 0);

    // El rack de efectos ocupa la franja libre a la derecha del ecualizador.
    // Estando al lado y no debajo, abrirlo no le come alto al plot.
    m_rack = new EffectsRack(&m_engine->effects(), this);
    m_rack->hide();
    outer->addWidget(m_rack, 1);
    outer->addStretch(0);

    auto* root = new QVBoxLayout(inner);
    root->setContentsMargins(14, 7, 14, 9);
    root->setSpacing(6);

    // --- cabecera ----------------------------------------------------------
    auto* header = new QHBoxLayout;
    header->setSpacing(10);

    auto* title = new QLabel(Lang::tr("ECUALIZADOR"), inner);
    QFont titleFont = Theme::uiFont(8, QFont::DemiBold);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    title->setFont(titleFont);
    title->setObjectName(QStringLiteral("sectionTitle"));
    header->addWidget(title);

    m_enable = new QCheckBox(Lang::tr("Activado"), inner);
    m_enable->setFont(Theme::uiFont(9));
    header->addWidget(m_enable);
    connect(m_enable, &QCheckBox::toggled, this, [this](bool on) {
        m_equalizer->setEnabled(on);
        Settings::setEqEnabled(on);
        m_curve->refreshFromEngine();
        emit enabledChanged(on || m_engine->effects().isAnyEnabled());
    });

    header->addSpacing(8);

    auto* presetLabel = new QLabel(Lang::tr("Preset"), inner);
    presetLabel->setFont(Theme::uiFont(9));
    presetLabel->setObjectName(QStringLiteral("metaFormLabel"));
    header->addWidget(presetLabel);

    m_presets = new QComboBox(inner);
    m_presets->setFont(Theme::uiFont(9));
    m_presets->setFixedWidth(130);
    header->addWidget(m_presets);
    connect(m_presets, &QComboBox::currentIndexChanged, this, &EqualizerPanel::applyPreset);

    m_store = new FlatButton(Lang::tr("Guardar"), inner);
    m_store->setIconId(Icons::Save);
    m_store->setGlyphSize(14);
    m_store->setFixedHeight(24);
    header->addWidget(m_store);
    connect(m_store, &FlatButton::clicked, this, &EqualizerPanel::storeUserPreset);

    m_effectsButton = new FlatButton(Lang::tr("Efectos"), inner);
    m_effectsButton->setIconId(Icons::Equalizer);
    m_effectsButton->setGlyphSize(14);
    m_effectsButton->setFixedHeight(24);
    m_effectsButton->setCheckable(true);
    m_effectsButton->setFilledWhenChecked(true);
    m_effectsButton->setToolTip(Lang::tr("Mostrar u ocultar el rack de efectos"));
    header->addWidget(m_effectsButton);
    connect(m_effectsButton, &FlatButton::toggled, this, &EqualizerPanel::setRackOpen);

    m_reset = new FlatButton(Lang::tr("Reiniciar"), inner);
    m_reset->setIconId(Icons::Revert);
    m_reset->setGlyphSize(14);
    m_reset->setFixedHeight(24);
    header->addWidget(m_reset);
    connect(m_reset, &FlatButton::clicked, this, &EqualizerPanel::resetBands);

    header->addStretch(1);

    m_hint = new QLabel(inner);
    m_hint->setObjectName(QStringLiteral("statusText"));
    m_hint->setFont(Theme::uiFont(8));
    m_hint->setText(Lang::tr("arrastra los nodos  ·  rueda = Q"));
    header->addWidget(m_hint);

    header->addSpacing(4);

    m_close = new FlatButton(Icons::Close, inner);
    m_close->setFixedDiameter(24);
    m_close->setGlyphSize(14);
    m_close->setStrokeWidth(1.3);
    header->addWidget(m_close);
    connect(m_close, &FlatButton::clicked, this, &EqualizerPanel::closeRequested);

    root->addLayout(header);

    // --- cuerpo: preamplificador + plot ------------------------------------
    auto* body = new QHBoxLayout;
    body->setSpacing(10);

    m_preamp = new EqSlider(QStringLiteral("Pre"), inner);
    m_preamp->setRange(-Equalizer::kPreampRangeDb, Equalizer::kPreampRangeDb);
    m_preamp->setFixedWidth(40);
    body->addWidget(m_preamp);
    connect(m_preamp, &EqSlider::valueChanged, this, [this](float db) {
        m_equalizer->setPreamp(db);
        Settings::setEqPreamp(db);
        m_curve->refreshFromEngine();
        markPresetAsCustom();
    });

    m_curve = new EqCurveEditor(m_equalizer, m_engine, inner);
    body->addWidget(m_curve, 1);
    connect(m_curve, &EqCurveEditor::bandEdited, this, &EqualizerPanel::onBandEdited);

    root->addLayout(body, 1);

    // El indicador del ecualizador en la barra inferior tambien se enciende
    // con los efectos: el sonido deja de salir plano igual.
    connect(m_rack, &EffectsRack::activityChanged, this, [this](bool anyEffect) {
        emit enabledChanged(m_enable->isChecked() || anyEffect);
    });

    refreshPresetList();
}

void EqualizerPanel::setRackOpen(bool open)
{
    if (!m_rack)
        return;

    m_rack->setVisible(open);
    Settings::setEffectsRackOpen(open);

    if (m_effectsButton->isChecked() != open) {
        QSignalBlocker blocker(m_effectsButton);
        m_effectsButton->setChecked(open);
    }
}

void EqualizerPanel::setAnalyzerActive(bool active)
{
    m_curve->setActive(active && isVisible());
}

void EqualizerPanel::onBandEdited(int)
{
    persistBands();
    markPresetAsCustom();
}

void EqualizerPanel::persistBands() const
{
    QVector<float> gains, freqs, qs;
    gains.reserve(Equalizer::kBands);
    freqs.reserve(Equalizer::kBands);
    qs.reserve(Equalizer::kBands);

    for (int b = 0; b < Equalizer::kBands; ++b) {
        gains << m_equalizer->gain(b);
        freqs << m_equalizer->frequency(b);
        qs    << m_equalizer->q(b);
    }

    Settings::setEqGains(gains);
    Settings::setEqFrequencies(freqs);
    Settings::setEqQs(qs);
}

void EqualizerPanel::refreshPresetList()
{
    const QString previous = m_presets->currentText();

    m_loading = true;
    m_presets->clear();
    for (const Settings::EqPreset& preset : Settings::builtinPresets())
        m_presets->addItem(preset.name);

    const QList<Settings::EqPreset> user = Settings::userPresets();
    if (!user.isEmpty()) {
        m_presets->insertSeparator(m_presets->count());
        for (const Settings::EqPreset& preset : user)
            m_presets->addItem(preset.name);
    }
    m_loading = false;

    const int index = m_presets->findText(previous);
    if (index >= 0)
        m_presets->setCurrentIndex(index);
}

void EqualizerPanel::applyPreset(int comboIndex)
{
    if (m_loading || comboIndex < 0)
        return;

    const QString name = m_presets->itemText(comboIndex);
    if (name.isEmpty() || name == QStringLiteral("Personalizado"))
        return;

    QList<Settings::EqPreset> all = Settings::builtinPresets();
    all += Settings::userPresets();

    for (const Settings::EqPreset& preset : all) {
        if (preset.name != name)
            continue;

        m_loading = true;
        m_preamp->setValue(preset.preamp, false);
        m_equalizer->setPreamp(preset.preamp);

        for (int band = 0; band < Equalizer::kBands; ++band) {
            m_equalizer->setGain(band, band < preset.gains.size() ? preset.gains.at(band) : 0.0f);

            // Un preset de fabrica no lleva frecuencias ni Q: se vuelve al
            // reparto por defecto para que la curva sea la esperada.
            m_equalizer->setFrequency(band, band < preset.freqs.size()
                                                ? preset.freqs.at(band)
                                                : Equalizer::defaultFrequencies()[band]);
            m_equalizer->setQ(band, band < preset.qs.size()
                                        ? preset.qs.at(band)
                                        : Equalizer::kDefaultQ);
        }
        m_loading = false;

        Settings::setEqPresetName(name);
        Settings::setEqPreamp(preset.preamp);
        persistBands();
        m_curve->refreshFromEngine();
        return;
    }
}

void EqualizerPanel::markPresetAsCustom()
{
    if (m_loading)
        return;

    // Al mover un nodo, el preset deja de describir el estado real.
    const int index = m_presets->findText(QStringLiteral("Personalizado"));
    m_loading = true;
    if (index < 0) {
        m_presets->addItem(QStringLiteral("Personalizado"));
        m_presets->setCurrentIndex(m_presets->count() - 1);
    } else {
        m_presets->setCurrentIndex(index);
    }
    m_loading = false;
    Settings::setEqPresetName(QStringLiteral("Personalizado"));
}

void EqualizerPanel::storeUserPreset()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, Lang::tr("Guardar preset"),
        Lang::tr("Nombre del preset:"), QLineEdit::Normal,
        Lang::tr("Mi preset"), &accepted);

    if (!accepted || name.trimmed().isEmpty())
        return;

    Settings::EqPreset preset;
    preset.name   = name.trimmed();
    preset.preamp = m_preamp->value();
    for (int b = 0; b < Equalizer::kBands; ++b) {
        preset.gains << m_equalizer->gain(b);
        preset.freqs << m_equalizer->frequency(b);
        preset.qs    << m_equalizer->q(b);
    }

    Settings::saveUserPreset(preset);
    refreshPresetList();

    const int index = m_presets->findText(preset.name);
    if (index >= 0)
        m_presets->setCurrentIndex(index);
}

void EqualizerPanel::resetBands()
{
    m_loading = true;
    m_preamp->setValue(0.0f, false);
    m_equalizer->resetBands();
    m_loading = false;

    if (m_rack)
        m_rack->resetAll();

    persistBands();
    Settings::setEqPreamp(0.0f);

    const int flat = m_presets->findText(QStringLiteral("Plano"));
    if (flat >= 0) {
        m_loading = true;
        m_presets->setCurrentIndex(flat);
        m_loading = false;
        Settings::setEqPresetName(QStringLiteral("Plano"));
    }
    m_curve->refreshFromEngine();
}

void EqualizerPanel::loadFromSettings()
{
    m_loading = true;

    const bool enabled = Settings::eqEnabled();
    m_enable->setChecked(enabled);
    m_equalizer->setEnabled(enabled);

    const float preamp = Settings::eqPreamp();
    m_preamp->setValue(preamp, false);
    m_equalizer->setPreamp(preamp);

    const QVector<float> gains = Settings::eqGains();
    const QVector<float> freqs = Settings::eqFrequencies();
    const QVector<float> qs    = Settings::eqQs();

    for (int band = 0; band < Equalizer::kBands; ++band) {
        m_equalizer->setGain(band, band < gains.size() ? gains.at(band) : 0.0f);
        m_equalizer->setFrequency(band, band < freqs.size()
                                            ? freqs.at(band)
                                            : Equalizer::defaultFrequencies()[band]);
        m_equalizer->setQ(band, band < qs.size() ? qs.at(band) : Equalizer::kDefaultQ);
    }

    const int index = m_presets->findText(Settings::eqPresetName());
    if (index >= 0)
        m_presets->setCurrentIndex(index);

    m_loading = false;
    m_curve->refreshFromEngine();
}

void EqualizerPanel::saveToSettings() const
{
    Settings::setEqEnabled(m_enable->isChecked());
    Settings::setEqPreamp(m_preamp->value());
    persistBands();
}
