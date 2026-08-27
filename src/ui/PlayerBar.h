#pragma once

#include <QWidget>

class FlatButton;
class SeekBar;
class QSlider;

// Barra inferior: transporte a la izquierda (bajo la columna de la caratula),
// forma de onda arriba y volumen / ecualizador / reloj abajo.
class PlayerBar : public QWidget {
    Q_OBJECT

public:
    explicit PlayerBar(QWidget* parent = nullptr);

    SeekBar* seekBar() const { return m_seek; }

    void setPlaying(bool playing);
    void setVolume(float linear);
    void setMuted(bool muted);
    void setShuffle(bool shuffle);
    void setRepeatMode(int mode);        // 0=off 1=todo 2=una
    void setEqualizerActive(bool active);
    void setEqualizerPanelOpen(bool open);
    void setAbLoopActive(bool active);

signals:
    void previousClicked();
    void stopClicked();
    void playClicked();
    void pauseClicked();
    void nextClicked();
    void shuffleToggled(bool enabled);
    void repeatCycled();
    void abLoopClicked();
    void volumeChanged(float linear);
    void muteToggled();
    void equalizerPanelToggled(bool open);
    void timeDisplayToggled();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void buildUi();
    void refreshVolumeIcon();

    // La referencia tiene CINCO botones: anterior, parar, reproducir (grande,
    // con anillo), pausa y siguiente. Reproducir y pausa son independientes.
    FlatButton* m_previous = nullptr;
    FlatButton* m_stop     = nullptr;
    FlatButton* m_play     = nullptr;
    FlatButton* m_pause    = nullptr;
    FlatButton* m_next     = nullptr;

    FlatButton* m_shuffle  = nullptr;
    FlatButton* m_abLoop   = nullptr;
    FlatButton* m_repeat   = nullptr;

    SeekBar*    m_seek     = nullptr;
    FlatButton* m_volumeIcon = nullptr;
    QSlider*    m_volume   = nullptr;
    FlatButton* m_equalizer = nullptr;
    FlatButton* m_clock    = nullptr;

    bool m_playing = false;
    bool m_muted   = false;
    int  m_repeatMode = 0;
};
