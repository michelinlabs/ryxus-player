#pragma once

#include "core/Equalizer.h"

#include <QByteArray>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

class QSettings;

// Estado persistente entre sesiones: volumen, ecualizador, listas abiertas,
// carpetas de la biblioteca y geometria de la ventana.
namespace Settings {

struct EqPreset {
    QString name;
    float   preamp = 0.0f;
    QVector<float> gains;   // kBands entradas, en dB
    // Vacios en los presets de fabrica: se usa el reparto por defecto.
    QVector<float> freqs;   // Hz por banda
    QVector<float> qs;      // factor Q por banda
};

QSettings& store();

// --- reproductor -----------------------------------------------------------
float volume();
void  setVolume(float value);
bool  muted();
void  setMuted(bool value);
bool  shuffle();
void  setShuffle(bool value);
int   repeatMode();
void  setRepeatMode(int value);

// --- ecualizador -----------------------------------------------------------
bool  eqEnabled();
void  setEqEnabled(bool value);
float eqPreamp();
void  setEqPreamp(float value);
QVector<float> eqGains();
void  setEqGains(const QVector<float>& gains);
QVector<float> eqFrequencies();
void  setEqFrequencies(const QVector<float>& freqs);
QVector<float> eqQs();
void  setEqQs(const QVector<float>& qs);
QString eqPresetName();
void    setEqPresetName(const QString& name);

// Presets de fabrica + los que guarde el usuario.
QList<EqPreset> builtinPresets();
QList<EqPreset> userPresets();
void saveUserPreset(const EqPreset& preset);
void removeUserPreset(const QString& name);

// --- rack de efectos -------------------------------------------------------
// Se guardan por identificador de dispositivo y de parametro, no por indice:
// asi anadir o reordenar efectos no invalida lo que el usuario tenia puesto.
bool  effectEnabled(const QString& deviceId, bool fallback = false);
void  setEffectEnabled(const QString& deviceId, bool value);
float effectParam(const QString& deviceId, const QString& paramId, float fallback);
void  setEffectParam(const QString& deviceId, const QString& paramId, float value);
bool  effectsRackOpen();
void  setEffectsRackOpen(bool value);

// --- audio del sistema -----------------------------------------------------
bool       systemAudioEnabled();
void       setSystemAudioEnabled(bool value);
QByteArray systemAudioSource();          // salida que se captura
void       setSystemAudioSource(const QByteArray& id);
QByteArray systemAudioOutput();          // salida por la que sale procesado
void       setSystemAudioOutput(const QByteArray& id);

// --- perfiles por interfaz -------------------------------------------------
// Cada salida por la que se ha sacado el audio procesado guarda su propio
// ecualizador y efectos: unos monitores y unos auriculares no se ecualizan
// igual. Al pasar de una a otra se guarda lo de la que se deja y se carga lo
// de la que llega.
void saveOutputProfile(const QByteArray& outputId);   // lo actual, a su perfil
bool loadOutputProfile(const QByteArray& outputId);   // false si no tiene

// Salidas que el usuario ha usado alguna vez para lo procesado, la usada mas
// recientemente primero. Son las unicas a las que se cambia solo al
// conectarlas: un monitor HDMI que se enciende no debe llevarse el sonido.
QList<QByteArray> knownOutputs();
void touchKnownOutput(const QByteArray& outputId);
void forgetOutput(const QByteArray& outputId);       // y borra su perfil

// Nombre visible de una salida conocida: hace falta para mostrarla en
// Ajustes aunque este desenchufada, cuando Windows ya no la lista.
QString outputName(const QByteArray& outputId);
void    setOutputName(const QByteArray& outputId, const QString& name);

// Pasar solo a la interfaz que se conecta, y a otra si se va la que se usa.
bool autoSwitchOutputs();
void setAutoSwitchOutputs(bool value);

// --- interfaz --------------------------------------------------------------
QByteArray windowGeometry();
void       setWindowGeometry(const QByteArray& value);

// Tamano y posicion de la ventana para una combinacion concreta de
// monitores (cuales hay, a que resolucion y donde). Asi el portatil solo, el
// portatil con el monitor externo o el cambio de resolucion recuerdan cada
// uno lo suyo. `setupKey` lo calcula MainWindow; aqui solo se guarda.
struct WindowPlacement {
    QByteArray geometry;           // saveGeometry()
    bool       maximized = false;  // maximizado a mano contra el area util
    QRect      normal;             // a donde se vuelve al desmaximizar
};
WindowPlacement windowPlacement(const QString& setupKey);
void            setWindowPlacement(const QString& setupKey, const WindowPlacement& placement);
QByteArray splitterState();
void       setSplitterState(const QByteArray& value);

// --- video -------------------------------------------------------------------
int  videoMask();                 // VideoMasks::Id
void setVideoMask(int mask);
int  videoLayout();               // MainWindow::VideoLayout
void setVideoLayout(int layout);
// La paleta de filtros sale con las disposiciones de video salvo que el
// usuario la haya cerrado: entonces no vuelve hasta que la pida.
bool videoFiltersVisible();
void setVideoFiltersVisible(bool visible);
// Arrastrar sobre el video avanza o retrocede fotogramas: cuantos por cada
// 100 pixeles recorridos con el raton.
int  videoScrubFramesPer100();
void setVideoScrubFramesPer100(int frames);

// Paso de las flechas del teclado (y de la rueda sobre el video).
int  seekStepMs();                 // avanzar / retroceder
void setSeekStepMs(int ms);
int  volumeStepPercent();          // subir / bajar volumen
void setVolumeStepPercent(int percent);

// Gestos sobre la imagen del video.
bool videoGestureSeek();           // arrastrar a los lados
void setVideoGestureSeek(bool on);
bool videoGestureVolume();         // arrastrar arriba y abajo
void setVideoGestureVolume(bool on);
bool videoGestureWheel();          // rueda del raton
void setVideoGestureWheel(bool on);
int  videoPreviewWidth();          // ancho de la previsualizacion, en px
void setVideoPreviewWidth(int px);
int    volumeBarShape();           // VideoPanel::VolumeShape
void   setVolumeBarShape(int shape);
QString volumeBarColor();          // vacio: el color de acento del tema
void   setVolumeBarColor(const QString& color);

// --- disposicion de los paneles -------------------------------------------
// Estado de QMainWindow::saveState() de los paneles acoplables: donde esta
// cada uno, su tamano, si flota y si se ve.
QByteArray dockState();
void       setDockState(const QByteArray& value);

// Disposiciones que guarda el usuario con nombre, para volver a ellas.
struct SavedLayout {
    QString    name;
    QByteArray state;
};
QList<SavedLayout> savedLayouts();
void saveLayout(const QString& name, const QByteArray& state);   // reemplaza si existe
void removeLayout(const QString& name);

// --- apariencia e idioma ---------------------------------------------------
QString themeId();
void    setThemeId(const QString& id);
QString language();          // "es" / "en"
void    setLanguage(const QString& code);

// --- imagen de fondo -------------------------------------------------------
QString backgroundImage();
void    setBackgroundImage(const QString& path);
int     backgroundTransparency();          // 0..85 (% de transparencia)
void    setBackgroundTransparency(int percent);
int     backgroundDarkening();             // 0..90 (% de velo oscuro)
void    setBackgroundDarkening(int percent);
// Capa viva: indice de Visualizations, -1 = ninguna.
int     backgroundVisualization();
void    setBackgroundVisualization(int index);
int     backgroundImageOpacity();          // 0..100, capa de imagen
void    setBackgroundImageOpacity(int percent);
int     backgroundVisualOpacity();         // 0..100, capa viva
void    setBackgroundVisualOpacity(int percent);
int     backgroundMode();                  // BackgroundHost::Mode como entero
void    setBackgroundMode(int mode);

// --- biblioteca ------------------------------------------------------------
QStringList libraryFolders();
void        setLibraryFolders(const QStringList& folders);
QString     lastBrowsedFolder();
void        setLastBrowsedFolder(const QString& folder);

// --- listas de reproduccion ------------------------------------------------
// Se guardan como "nombre" -> lista de rutas.
QList<QPair<QString, QStringList>> playlists();
void setPlaylists(const QList<QPair<QString, QStringList>>& lists);
int  activePlaylist();
void setActivePlaylist(int index);

} // namespace Settings
