#include "core/Settings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace Settings {
namespace {

// Lo guardado solo se acepta si trae exactamente las bandas que tiene hoy el
// ecualizador. Cuando el numero de bandas cambia entre versiones, rellenar las
// que faltan mezcla dos repartos distintos y deja nodos duplicados encima unos
// de otros; se prefiere empezar de cero con los valores por defecto.
QVector<float> toFloats(const QVariant& value, float fallback)
{
    QVector<float> out(Equalizer::kBands, fallback);
    const QVariantList list = value.toList();
    if (list.size() != Equalizer::kBands)
        return out;
    for (int i = 0; i < Equalizer::kBands; ++i)
        out[i] = list.at(i).toFloat();
    return out;
}

QVector<float> toGains(const QVariant& value) { return toFloats(value, 0.0f); }

QVector<float> defaultFreqVector()
{
    QVector<float> out;
    for (int i = 0; i < Equalizer::kBands; ++i)
        out << Equalizer::defaultFrequencies()[i];
    return out;
}

QVector<float> toFreqs(const QVariant& value)
{
    QVector<float> out = defaultFreqVector();
    const QVariantList list = value.toList();
    if (list.size() != Equalizer::kBands)
        return out;
    for (int i = 0; i < Equalizer::kBands; ++i) {
        const float hz = list.at(i).toFloat();
        if (hz > 0.0f)
            out[i] = hz;
    }
    return out;
}

QVariantList fromGains(const QVector<float>& gains)
{
    QVariantList list;
    for (int i = 0; i < Equalizer::kBands; ++i)
        list << (i < gains.size() ? gains.at(i) : 0.0f);
    return list;
}

QVariantList fromFloats(const QVector<float>& values, float fallback)
{
    QVariantList list;
    for (int i = 0; i < Equalizer::kBands; ++i)
        list << (i < values.size() ? values.at(i) : fallback);
    return list;
}

EqPreset makePreset(const char* name, std::initializer_list<float> gains, float preamp = 0.0f)
{
    EqPreset preset;
    preset.name   = QString::fromLatin1(name);
    preset.preamp = preamp;
    preset.gains  = QVector<float>(gains);
    preset.gains.resize(Equalizer::kBands);
    return preset;
}

// El reproductor se llamo Ryxus Player, luego Roxas Player, y ahora vuelve a
// llamarse Ryxus Player. Cambiar de nombre no deberia costarle al usuario sus
// temas, listas y carpetas, asi que los ajustes guardados bajo el nombre
// anterior se traen al actual.
//
// Ojo con el detalle de haber vuelto al nombre de origen: en un equipo que
// venga de la primera epoca puede haber YA un RyxusPlayer.ini, viejo y
// abandonado, que no es el que hay que conservar. Por eso no vale la regla
// habitual de "copiar solo si no existe el destino": gana el mas reciente.
void migrateLegacySettings()
{
    const QSettings current(QSettings::IniFormat, QSettings::UserScope,
                            QStringLiteral("Ryxus"), QStringLiteral("RyxusPlayer"));
    const QSettings previous(QSettings::IniFormat, QSettings::UserScope,
                             QStringLiteral("Roxas"), QStringLiteral("RoxasPlayer"));

    const QFileInfo target(current.fileName());
    const QFileInfo legacy(previous.fileName());

    if (!legacy.exists())
        return;
    if (target.exists() && target.lastModified() >= legacy.lastModified())
        return;   // ya migrado, o lo que hay es mas nuevo

    QDir().mkpath(target.absolutePath());
    QFile::remove(target.absoluteFilePath());
    QFile::copy(legacy.absoluteFilePath(), target.absoluteFilePath());
}

// Se ejecuta una sola vez, antes de que se construya el QSettings de store().
bool ensureMigrated()
{
    static const bool done = [] {
        migrateLegacySettings();
        return true;
    }();
    return done;
}

} // namespace

QSettings& store()
{
    ensureMigrated();
    static QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                              QStringLiteral("Ryxus"), QStringLiteral("RyxusPlayer"));
    return settings;
}

// --- reproductor -----------------------------------------------------------
float volume()             { return store().value(QStringLiteral("player/volume"), 0.8f).toFloat(); }
void  setVolume(float v)   { store().setValue(QStringLiteral("player/volume"), v); }
bool  muted()              { return store().value(QStringLiteral("player/muted"), false).toBool(); }
void  setMuted(bool v)     { store().setValue(QStringLiteral("player/muted"), v); }
bool  shuffle()            { return store().value(QStringLiteral("player/shuffle"), false).toBool(); }
void  setShuffle(bool v)   { store().setValue(QStringLiteral("player/shuffle"), v); }
int   repeatMode()         { return store().value(QStringLiteral("player/repeat"), 0).toInt(); }
void  setRepeatMode(int v) { store().setValue(QStringLiteral("player/repeat"), v); }

// --- ecualizador -----------------------------------------------------------
bool  eqEnabled()            { return store().value(QStringLiteral("eq/enabled"), false).toBool(); }
void  setEqEnabled(bool v)   { store().setValue(QStringLiteral("eq/enabled"), v); }
float eqPreamp()             { return store().value(QStringLiteral("eq/preamp"), 0.0f).toFloat(); }
void  setEqPreamp(float v)   { store().setValue(QStringLiteral("eq/preamp"), v); }

QVector<float> eqGains()     { return toGains(store().value(QStringLiteral("eq/gains"))); }
void setEqGains(const QVector<float>& g) { store().setValue(QStringLiteral("eq/gains"), fromGains(g)); }

QVector<float> eqFrequencies()
{
    return toFreqs(store().value(QStringLiteral("eq/freqs")));
}
void setEqFrequencies(const QVector<float>& f)
{
    store().setValue(QStringLiteral("eq/freqs"), fromFloats(f, 1000.0f));
}

QVector<float> eqQs()
{
    return toFloats(store().value(QStringLiteral("eq/qs")), Equalizer::kDefaultQ);
}
void setEqQs(const QVector<float>& q)
{
    store().setValue(QStringLiteral("eq/qs"), fromFloats(q, Equalizer::kDefaultQ));
}

QString eqPresetName()       { return store().value(QStringLiteral("eq/preset"),
                                                    QStringLiteral("Plano")).toString(); }
void setEqPresetName(const QString& n) { store().setValue(QStringLiteral("eq/preset"), n); }

QList<EqPreset> builtinPresets()
{
    // Bandas: 32, 60, 100, 170, 310, 600, 1K, 1K8, 3K, 6K, 10K, 16K
    return {
        makePreset("Plano",       { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}),
        makePreset("Rock",        { 5,  5,  4,  3, -1, -2,  0,  1,  3,  5,  5,  4}),
        makePreset("Pop",         {-2, -1,  1,  2,  4,  4,  3,  2, -1, -2, -1,  0}),
        makePreset("Jazz",        { 4,  4,  3,  3,  1,  2,  0, -2, -1,  2,  4,  4}),
        makePreset("Clasica",     { 5,  5,  4,  4,  3,  0,  0,  0,  0,  2,  4,  4}),
        makePreset("Electronica", { 7,  6,  5,  4,  0, -1, -1,  1,  2,  3,  5,  6}),
        makePreset("Hip-Hop",     { 8,  7,  6,  4,  2,  3,  1, -1,  1,  2,  3,  3}),
        makePreset("Vocal",       {-4, -3, -2,  0,  1,  3,  5,  5,  4,  2,  0, -1}),
        makePreset("Grave +",     {10,  9,  7,  5,  3,  1,  0,  0,  0,  0,  0,  0}, -3.0f),
        makePreset("Agudo +",     { 0,  0,  0,  0,  0,  0,  1,  2,  4,  6,  8,  9}, -3.0f),
        makePreset("Loudness",    { 8,  7,  6,  3,  0, -2, -3, -2, -1,  3,  6,  7}, -2.0f),
    };
}

QList<EqPreset> userPresets()
{
    QList<EqPreset> presets;
    QSettings& s = store();
    const int count = s.beginReadArray(QStringLiteral("eq/userPresets"));
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        EqPreset preset;
        preset.name   = s.value(QStringLiteral("name")).toString();
        preset.preamp = s.value(QStringLiteral("preamp"), 0.0f).toFloat();
        preset.gains  = toGains(s.value(QStringLiteral("gains")));
        if (!preset.name.isEmpty())
            presets << preset;
    }
    s.endArray();
    return presets;
}

void saveUserPreset(const EqPreset& preset)
{
    QList<EqPreset> presets = userPresets();
    bool replaced = false;
    for (EqPreset& existing : presets) {
        if (existing.name == preset.name) {
            existing = preset;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        presets << preset;

    QSettings& s = store();
    s.beginWriteArray(QStringLiteral("eq/userPresets"), int(presets.size()));
    for (int i = 0; i < presets.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"),   presets.at(i).name);
        s.setValue(QStringLiteral("preamp"), presets.at(i).preamp);
        s.setValue(QStringLiteral("gains"),  fromGains(presets.at(i).gains));
    }
    s.endArray();
}

void removeUserPreset(const QString& name)
{
    QList<EqPreset> presets = userPresets();
    presets.erase(std::remove_if(presets.begin(), presets.end(),
                                 [&](const EqPreset& p) { return p.name == name; }),
                  presets.end());

    QSettings& s = store();
    s.remove(QStringLiteral("eq/userPresets"));
    s.beginWriteArray(QStringLiteral("eq/userPresets"), int(presets.size()));
    for (int i = 0; i < presets.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"),   presets.at(i).name);
        s.setValue(QStringLiteral("preamp"), presets.at(i).preamp);
        s.setValue(QStringLiteral("gains"),  fromGains(presets.at(i).gains));
    }
    s.endArray();
}

// --- rack de efectos -------------------------------------------------------
namespace {
QString effectKey(const QString& deviceId, const QString& leaf)
{
    return QStringLiteral("effects/") + deviceId + QLatin1Char('/') + leaf;
}
} // namespace

bool effectEnabled(const QString& deviceId, bool fallback)
{
    return store().value(effectKey(deviceId, QStringLiteral("enabled")), fallback).toBool();
}

void setEffectEnabled(const QString& deviceId, bool value)
{
    store().setValue(effectKey(deviceId, QStringLiteral("enabled")), value);
}

float effectParam(const QString& deviceId, const QString& paramId, float fallback)
{
    return store().value(effectKey(deviceId, paramId), fallback).toFloat();
}

void setEffectParam(const QString& deviceId, const QString& paramId, float value)
{
    store().setValue(effectKey(deviceId, paramId), value);
}

bool effectsRackOpen()
{
    return store().value(QStringLiteral("effects/rackOpen"), false).toBool();
}

void setEffectsRackOpen(bool value)
{
    store().setValue(QStringLiteral("effects/rackOpen"), value);
}

bool systemAudioEnabled()
{
    return store().value(QStringLiteral("systemAudio/enabled"), false).toBool();
}
void setSystemAudioEnabled(bool v)
{
    store().setValue(QStringLiteral("systemAudio/enabled"), v);
}

QByteArray systemAudioSource()
{
    return store().value(QStringLiteral("systemAudio/source")).toByteArray();
}
void setSystemAudioSource(const QByteArray& id)
{
    store().setValue(QStringLiteral("systemAudio/source"), id);
}

QByteArray systemAudioOutput()
{
    return store().value(QStringLiteral("systemAudio/output")).toByteArray();
}
void setSystemAudioOutput(const QByteArray& id)
{
    store().setValue(QStringLiteral("systemAudio/output"), id);
}

// --- interfaz --------------------------------------------------------------
QByteArray windowGeometry() { return store().value(QStringLiteral("ui/geometry")).toByteArray(); }
void setWindowGeometry(const QByteArray& v) { store().setValue(QStringLiteral("ui/geometry"), v); }

QByteArray splitterState() { return store().value(QStringLiteral("ui/splitter")).toByteArray(); }
void setSplitterState(const QByteArray& v) { store().setValue(QStringLiteral("ui/splitter"), v); }

QByteArray bodySplitterState()
{
    return store().value(QStringLiteral("ui/bodySplitter")).toByteArray();
}
void setBodySplitterState(const QByteArray& v)
{
    store().setValue(QStringLiteral("ui/bodySplitter"), v);
}

QByteArray panelsSplitterState()
{
    return store().value(QStringLiteral("ui/panelsSplitter")).toByteArray();
}
void setPanelsSplitterState(const QByteArray& v)
{
    store().setValue(QStringLiteral("ui/panelsSplitter"), v);
}

bool bodySwapped()          { return store().value(QStringLiteral("ui/bodySwapped"), false).toBool(); }
void setBodySwapped(bool v) { store().setValue(QStringLiteral("ui/bodySwapped"), v); }

bool panelsSwapped()          { return store().value(QStringLiteral("ui/panelsSwapped"), false).toBool(); }
void setPanelsSwapped(bool v) { store().setValue(QStringLiteral("ui/panelsSwapped"), v); }

bool panelVisible(const QString& id)
{
    return store().value(QStringLiteral("ui/panel/") + id, true).toBool();
}
void setPanelVisible(const QString& id, bool v)
{
    store().setValue(QStringLiteral("ui/panel/") + id, v);
}

bool eqPanelVisible() { return store().value(QStringLiteral("ui/eqPanel"), false).toBool(); }
void setEqPanelVisible(bool v) { store().setValue(QStringLiteral("ui/eqPanel"), v); }

// --- apariencia e idioma ---------------------------------------------------
QString themeId()
{
    return store().value(QStringLiteral("ui/theme"), QStringLiteral("violeta")).toString();
}
void setThemeId(const QString& id)
{
    store().setValue(QStringLiteral("ui/theme"), id);
}

QString language()
{
    return store().value(QStringLiteral("ui/language"), QStringLiteral("es")).toString();
}
void setLanguage(const QString& code)
{
    store().setValue(QStringLiteral("ui/language"), code);
}

// --- imagen de fondo -------------------------------------------------------
QString backgroundImage()
{
    return store().value(QStringLiteral("background/image")).toString();
}
void setBackgroundImage(const QString& path)
{
    store().setValue(QStringLiteral("background/image"), path);
}

int backgroundTransparency()
{
    return qBound(0, store().value(QStringLiteral("background/transparency"), 45).toInt(), 85);
}
void setBackgroundTransparency(int percent)
{
    store().setValue(QStringLiteral("background/transparency"), qBound(0, percent, 85));
}

int backgroundDarkening()
{
    return qBound(0, store().value(QStringLiteral("background/darkening"), 35).toInt(), 90);
}
void setBackgroundDarkening(int percent)
{
    store().setValue(QStringLiteral("background/darkening"), qBound(0, percent, 90));
}

int backgroundVisualization()
{
    return store().value(QStringLiteral("background/visualization"), -1).toInt();
}
void setBackgroundVisualization(int index)
{
    store().setValue(QStringLiteral("background/visualization"), index);
}

int backgroundImageOpacity()
{
    return qBound(0, store().value(QStringLiteral("background/imageOpacity"), 100).toInt(), 100);
}
void setBackgroundImageOpacity(int percent)
{
    store().setValue(QStringLiteral("background/imageOpacity"), qBound(0, percent, 100));
}

int backgroundVisualOpacity()
{
    return qBound(0, store().value(QStringLiteral("background/visualOpacity"), 70).toInt(), 100);
}
void setBackgroundVisualOpacity(int percent)
{
    store().setValue(QStringLiteral("background/visualOpacity"), qBound(0, percent, 100));
}

int backgroundMode()
{
    return store().value(QStringLiteral("background/mode"), 0).toInt();
}
void setBackgroundMode(int mode)
{
    store().setValue(QStringLiteral("background/mode"), mode);
}

// --- biblioteca ------------------------------------------------------------
QStringList libraryFolders()
{
    return store().value(QStringLiteral("library/folders")).toStringList();
}

void setLibraryFolders(const QStringList& folders)
{
    store().setValue(QStringLiteral("library/folders"), folders);
}

QString lastBrowsedFolder()
{
    const QString fallback =
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    return store().value(QStringLiteral("library/lastFolder"), fallback).toString();
}

void setLastBrowsedFolder(const QString& folder)
{
    store().setValue(QStringLiteral("library/lastFolder"), folder);
}

// --- listas de reproduccion ------------------------------------------------
QList<QPair<QString, QStringList>> playlists()
{
    QList<QPair<QString, QStringList>> result;
    QSettings& s = store();
    const int count = s.beginReadArray(QStringLiteral("playlists"));
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        const QString name = s.value(QStringLiteral("name")).toString();
        const QStringList paths = s.value(QStringLiteral("paths")).toStringList();
        if (!name.isEmpty())
            result.append({name, paths});
    }
    s.endArray();
    return result;
}

void setPlaylists(const QList<QPair<QString, QStringList>>& lists)
{
    QSettings& s = store();
    s.remove(QStringLiteral("playlists"));
    s.beginWriteArray(QStringLiteral("playlists"), int(lists.size()));
    for (int i = 0; i < lists.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"),  lists.at(i).first);
        s.setValue(QStringLiteral("paths"), lists.at(i).second);
    }
    s.endArray();
}

int  activePlaylist()        { return store().value(QStringLiteral("playlists/active"), 0).toInt(); }
void setActivePlaylist(int i) { store().setValue(QStringLiteral("playlists/active"), i); }

} // namespace Settings
