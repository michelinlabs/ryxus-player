#pragma once

#include <QDialog>
#include <QVector>

class FlatButton;
class QComboBox;
class QLabel;
class QSlider;
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
    QSlider*    m_transparency = nullptr;
    QLabel*     m_transparencyValue = nullptr;
    QSlider*    m_darkening  = nullptr;
    QLabel*     m_darkeningValue = nullptr;
    QComboBox*  m_language   = nullptr;
    FlatButton* m_restart    = nullptr;
    QLabel*     m_languageNote = nullptr;
};
