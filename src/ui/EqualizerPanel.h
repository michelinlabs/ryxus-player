#pragma once

#include "core/Equalizer.h"

#include <QWidget>

class AudioEngine;
class EffectsRack;
class EqCurveEditor;
class EqSlider;
class FlatButton;
class QCheckBox;
class QComboBox;
class QLabel;

// Seccion de ecualizador de 12 bandas. El grueso del panel es el plot con los
// nodos arrastrables (EqCurveEditor); alrededor quedan el preamplificador,
// los presets y el interruptor general.
//
// A la derecha, en la franja que quedaba vacia, va el rack de efectos: se
// muestra con el boton "Efectos" y no le quita alto al plot.
class EqualizerPanel : public QWidget {
    Q_OBJECT

public:
    EqualizerPanel(Equalizer* equalizer, AudioEngine* engine, QWidget* parent = nullptr);

    void loadFromSettings();
    void saveToSettings() const;

    // El plot solo se refresca cuando el panel esta visible y hay audio.
    void setAnalyzerActive(bool active);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void closeRequested();
    void enabledChanged(bool enabled);

private slots:
    void setRackOpen(bool open);
    void applyPreset(int comboIndex);
    void storeUserPreset();
    void resetBands();
    void onBandEdited(int band);

private:
    void buildUi();
    void refreshPresetList();
    void persistBands() const;
    void markPresetAsCustom();

    Equalizer*   m_equalizer = nullptr;
    AudioEngine* m_engine    = nullptr;

    QCheckBox*     m_enable  = nullptr;
    QComboBox*     m_presets = nullptr;
    EqSlider*      m_preamp  = nullptr;
    EqCurveEditor* m_curve   = nullptr;
    EffectsRack*   m_rack    = nullptr;
    QWidget*       m_inner   = nullptr;
    QLabel*        m_hint    = nullptr;
    FlatButton*    m_effectsButton = nullptr;
    FlatButton*    m_reset   = nullptr;
    FlatButton*    m_store   = nullptr;
    FlatButton*    m_close   = nullptr;

    bool m_loading = false;
};
