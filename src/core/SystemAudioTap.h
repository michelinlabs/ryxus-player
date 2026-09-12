#pragma once

#include "core/Effects.h"
#include "core/Equalizer.h"
#include "core/Limiter.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

class AudioEngine;
class QTimer;

// Aplica el ecualizador y los efectos a TODO lo que suena en Windows, no solo
// a lo que reproduce el programa: YouTube, un juego, otro reproductor.
//
// Funciona capturando la mezcla de una salida (WASAPI loopback), procesandola y
// sacandola por OTRA salida distinta. Lo de "otra" no es un capricho: si se
// devolviera a la misma, lo que sale volveria a entrar y se realimentaria hasta
// reventar. Windows no deja a un programa normal meterse en medio de la mezcla
// del sistema, asi que esta es la unica via sin instalar un controlador.
//
// En la practica esto quiere decir que hace falta una salida que no se escuche
// -- un cable de audio virtual del tipo VB-Cable -- puesta como predeterminada
// de Windows, y escuchar por la de verdad. Sin eso, lo unico que se consigue es
// oir el sonido dos veces.
class SystemAudioTap : public QObject {
    Q_OBJECT

public:
    struct Device {
        QByteArray id;
        QString    name;
        bool       isDefault = false;
        bool       isVirtual = false;   // cable virtual, mirroring, etc.
        bool       isCable   = false;   // VB-Cable / Voicemeeter concretamente
    };

    explicit SystemAudioTap(QObject* parent = nullptr);
    ~SystemAudioTap() override;

    // Motor al que se le pasan las muestras ya procesadas para que el espectro
    // del ecualizador, la onda del panel izquierdo y el fondo reaccionen
    // tambien a lo que entra por el cable.
    void setVisualSink(AudioEngine* engine) { m_visualSink = engine; }

    // Salidas del sistema. La primera lista sirve para elegir que se captura;
    // la misma sirve para elegir por donde sale lo procesado.
    static QList<Device> outputDevices();

    // Elige el reparto que hace que los efectos se apliquen a todo Windows:
    // capturar del cable virtual y sacar por unos altavoces de verdad. Deja
    // ambos vacios si no hay cable instalado.
    static void suggestRouting(QByteArray* source, QByteArray* output);

    // `sourceId` vacio = la salida predeterminada de Windows.
    bool start(const QByteArray& sourceId, const QByteArray& outputId, QString* error);
    void stop();
    bool isRunning() const { return m_running; }

    QString lastError() const { return m_error; }

signals:
    void stoppedUnexpectedly(const QString& reason);

private:
    void applySettings();      // trae ecualizador y efectos desde los ajustes

    // Llamados desde los hilos de audio de miniaudio.
    void onCaptured(const void* input, unsigned frameCount);
    void onPlayback(void* output, unsigned frameCount);

    friend void systemTapCaptureCallback(void*, void*, const void*, unsigned);
    friend void systemTapPlaybackCallback(void*, void*, const void*, unsigned);

    void*  m_capture  = nullptr;   // ma_device*
    void*  m_playback = nullptr;   // ma_device*
    void*  m_ring     = nullptr;   // ma_pcm_rb*
    bool   m_running  = false;
    QString m_error;

    // Cadena propia: compartir la del reproductor haria que dos hilos de audio
    // tocaran el estado de los mismos filtros a la vez.
    Equalizer m_equalizer;
    Effects   m_effects;
    Limiter   m_limiter;

    AudioEngine* m_visualSink = nullptr;

    // Los mandos se leen de los ajustes cada poco, que es donde el panel del
    // ecualizador y el rack de efectos los dejan escritos.
    QTimer* m_sync = nullptr;
};
