#include "core/Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace Settings {
namespace {

QVector<float> toFloats(const QVariant& value, float fallback)
{
    QVector<float> out(Equalizer::kBands, fallback);
    const QVariantList list = value.toList();
    for (int i = 0; i < Equalizer::kBands && i < list.size(); ++i)
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

// El reproductor se llamaba Ryxus Player. Si quedan ajustes de aquel nombre y
// todavia no hay archivo con el nuevo, se copian: cambiar el nombre no deberia
// costarle al usuario sus temas, listas y carpetas.
void migrateLegacySettings()
{
    const QSettings target(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("Roxas"), QStringLiteral("RoxasPlayer"));
    if (QFile::exists(target.fileName()))
        return;

    const QSettings legacy(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("Ryxus"), QStringLiteral("RyxusPlayer"));
    if (!QFile::exists(legacy.fileName()))
        return;

    QDir().mkpath(QFileInfo(target.fileName()).absolutePath());
    QFile::copy(legacy.fileName(), target.fileName());
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
                              QStringLiteral("Roxas"), QStringLiteral("RoxasPlayer"));
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
    const QVariant stored = store().value(QStringLiteral("eq/freqs"));
    if (!stored.isValid() || stored.toList().isEmpty())
        return defaultFreqVector();
    return toFloats(stored, 1000.0f);
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
    // Bandas: 60, 170, 310, 600, 1K, 3K, 6K, 12K
    return {
        makePreset("Plano",       { 0,  0,  0,  0,  0,  0,  0,  0}),
        makePreset("Rock",        { 5,  3, -1, -2,  1,  3,  5,  4}),
        makePreset("Pop",         {-1,  2,  4,  4,  2, -1, -2, -1}),
        makePreset("Jazz",        { 4,  3,  1,  2, -2, -1,  2,  4}),
        makePreset("Clasica",     { 5,  4,  3,  0,  0,  0,  3,  4}),
        makePreset("Electronica", { 6,  5,  0, -1,  2,  1,  4,  6}),
        makePreset("Hip-Hop",     { 7,  5,  2,  3, -1,  1,  2,  3}),
        makePreset("Vocal",       {-3, -2,  0,  3,  5,  4,  1, -1}),
        makePreset("Grave +",     { 9,  7,  4,  1,  0,  0,  0,  0}, -3.0f),
        makePreset("Agudo +",     { 0,  0,  0,  0,  1,  4,  7,  9}, -3.0f),
        makePreset("Loudness",    { 7,  5,  0, -2, -3, -1,  4,  7}, -2.0f),
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

// --- interfaz --------------------------------------------------------------
QByteArray windowGeometry() { return store().value(QStringLiteral("ui/geometry")).toByteArray(); }
void setWindowGeometry(const QByteArray& v) { store().setValue(QStringLiteral("ui/geometry"), v); }

QByteArray splitterState() { return store().value(QStringLiteral("ui/splitter")).toByteArray(); }
void setSplitterState(const QByteArray& v) { store().setValue(QStringLiteral("ui/splitter"), v); }

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
