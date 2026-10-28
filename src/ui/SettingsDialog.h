#pragma once

#include "ui/SubtitleStyle.h"

#include <QDialog>
#include <QVector>

class FlatButton;
class QCheckBox;
class QComboBox;
class QKeySequence;
class QKeySequenceEdit;
class QLabel;
class QScrollArea;
class QSlider;
class SubtitlePreview;
class ThemeSwatch;

// Ventana de configuracion: tema de color, imagen de fondo e idioma.
// Los cambios de tema y de fondo se aplican en vivo; el idioma necesita
// reiniciar y el propio dialogo ofrece hacerlo.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void themeChanged(const QString& paletteId);
    void backgroundImageChanged(const QString& path);
    void backgroundModeChanged(int mode);
    void transparencyChanged(int percent);
    void darkeningChanged(int percent);
    void restartRequested();
    void visualizationChanged(int index);
    void imageOpacityChanged(int percent);
    void visualOpacityChanged(int percent);

    // Algun atajo de teclado cambio: MainWindow los vuelve a leer.
    void shortcutsChanged();

    // Cambio el estilo de los subtitulos (ya guardado en los ajustes).
    void subtitleStyleChanged();

    // Ecualizador y efectos sobre todo el audio de Windows.
    void systemAudioChanged(bool enabled, const QByteArray& source,
                            const QByteArray& output);

private slots:
    void chooseImage();
    void clearImage();
    void onThemePicked(const QString& paletteId);

private:
    void buildUi();
    void refreshImageState();
    void refreshSwatches();

    QVector<ThemeSwatch*> m_swatches;
    QLabel*     m_imageLabel = nullptr;
    FlatButton* m_clearImage = nullptr;
    QComboBox*  m_fitMode    = nullptr;
    QComboBox*  m_visualization   = nullptr;
    QSlider*    m_imageOpacity    = nullptr;
    QLabel*     m_imageOpacityValue = nullptr;
    QSlider*    m_visualOpacity   = nullptr;
    QLabel*     m_visualOpacityValue = nullptr;
    QSlider*    m_transparency = nullptr;
    QLabel*     m_transparencyValue = nullptr;
    QSlider*    m_darkening  = nullptr;
    QLabel*     m_darkeningValue = nullptr;
    QComboBox*  m_language   = nullptr;
    FlatButton* m_restart    = nullptr;
    QLabel*     m_languageNote = nullptr;

    QCheckBox*  m_systemAudio       = nullptr;
    QComboBox*  m_systemSource      = nullptr;
    QComboBox*  m_systemOutput      = nullptr;
    QLabel*     m_systemAudioStatus = nullptr;
    QCheckBox*  m_autoSwitch        = nullptr;

    QScrollArea* m_scroll           = nullptr;
    QLabel*      m_shortcutsTitle   = nullptr;
    QLabel*      m_shortcutsStatus  = nullptr;
    QVector<QKeySequenceEdit*> m_keyEdits;   // uno por orden de Commands

    QLabel*          m_subtitleTitle   = nullptr;
    SubtitlePreview* m_subtitlePreview = nullptr;
    SubtitleStyle    m_subtitleStyle;
    void buildSubtitleSection(class QVBoxLayout* root);
    void scrollToSection(QWidget* title);
    void assignShortcut(int command, const QKeySequence& keys);
    QWidget*    m_interfaceList     = nullptr;

public:
    // El resultado real de encender la captura lo sabe MainWindow.
    void setSystemAudioStatus(const QString& message, bool ok);

    // Vuelve a leer las salidas de Windows: las interfaces USB pueden
    // conectarse o desconectarse con el programa abierto.
    void refreshSystemDevices();

    // Rehace la lista de interfaces guardadas (cual esta conectada, cual en
    // uso). MainWindow la llama tras cada cambio de salida.
    void refreshInterfaces();

    // Desplaza la ventana hasta la seccion de atajos de teclado.
    void showShortcuts();
    // Y hasta la del estilo de los subtitulos.
    void showSubtitleStyle();

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
};
