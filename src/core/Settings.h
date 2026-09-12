#pragma once

#include "core/Equalizer.h"

#include <QByteArray>
#include <QMap>
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

// --- interfaz --------------------------------------------------------------
QByteArray windowGeometry();
void       setWindowGeometry(const QByteArray& value);
QByteArray splitterState();
void       setSplitterState(const QByteArray& value);

// --- disposicion de las columnas -------------------------------------------
// Tamanos de los dos divisores, si estan intercambiadas y cuales se ven.
QByteArray bodySplitterState();
void       setBodySplitterState(const QByteArray& value);
QByteArray panelsSplitterState();
void       setPanelsSplitterState(const QByteArray& value);
bool       bodySwapped();
void       setBodySwapped(bool value);
bool       panelsSwapped();
void       setPanelsSwapped(bool value);
bool       panelVisible(const QString& id);
void       setPanelVisible(const QString& id, bool value);
bool       eqPanelVisible();
void       setEqPanelVisible(bool value);

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
