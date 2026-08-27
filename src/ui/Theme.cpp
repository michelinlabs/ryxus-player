#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPalette>

namespace Theme {

// Valores iniciales: el skin de referencia (violeta nocturno).
QColor Chrome        (0x19, 0x19, 0x19);
QColor ChromeLight   (0x1F, 0x1F, 0x1F);
QColor Panel         (0x28, 0x28, 0x28);
QColor PanelAlt      (0x25, 0x25, 0x25);
QColor PanelDeep     (0x21, 0x21, 0x21);
QColor Selection     (0x40, 0x3C, 0x57);
QColor SelectionSoft (0x33, 0x30, 0x46);
QColor Accent        (0x63, 0x5D, 0x8B);
QColor AccentLight   (0x76, 0x70, 0xA0);
QColor AccentBright  (0x9A, 0x92, 0xC8);
QColor Text          (0xEF, 0xEF, 0xEF);
QColor TextDim       (0xA0, 0xA0, 0xA0);
QColor TextFaint     (0x6E, 0x6E, 0x6E);
QColor Border        (0x30, 0x30, 0x30);
QColor BorderLight   (0x3A, 0x3A, 0x3A);
QColor Danger        (0xC0, 0x5A, 0x5A);
QColor Wave          (0xD8, 0xD8, 0xD8);

namespace {

float   g_transparency = 0.0f;
QString g_paletteId    = QStringLiteral("violeta");

QColor applyAlpha(const QColor& base)
{
    QColor c = base;
    c.setAlpha(panelAlpha());
    return c;
}

Palette make(const char* id, const char* name, bool light,
             const char* chrome, const char* chromeLight, const char* panel,
             const char* panelAlt, const char* panelDeep,
             const char* selection, const char* selectionSoft,
             const char* accent, const char* accentLight, const char* accentBright,
             const char* text, const char* textDim, const char* textFaint,
             const char* border, const char* borderLight,
             const char* danger, const char* wave)
{
    Palette p;
    p.id            = QString::fromLatin1(id);
    p.name          = QString::fromUtf8(name);
    p.light         = light;
    p.chrome        = QColor(QLatin1String(chrome));
    p.chromeLight   = QColor(QLatin1String(chromeLight));
    p.panel         = QColor(QLatin1String(panel));
    p.panelAlt      = QColor(QLatin1String(panelAlt));
    p.panelDeep     = QColor(QLatin1String(panelDeep));
    p.selection     = QColor(QLatin1String(selection));
    p.selectionSoft = QColor(QLatin1String(selectionSoft));
    p.accent        = QColor(QLatin1String(accent));
    p.accentLight   = QColor(QLatin1String(accentLight));
    p.accentBright  = QColor(QLatin1String(accentBright));
    p.text          = QColor(QLatin1String(text));
    p.textDim       = QColor(QLatin1String(textDim));
    p.textFaint     = QColor(QLatin1String(textFaint));
    p.border        = QColor(QLatin1String(border));
    p.borderLight   = QColor(QLatin1String(borderLight));
    p.danger        = QColor(QLatin1String(danger));
    p.wave          = QColor(QLatin1String(wave));
    return p;
}

} // namespace

const QVector<Palette>& palettes()
{
    static const QVector<Palette> list = {
        //   id            nombre              claro  chrome     chromeL    panel      panelAlt   panelDeep  selection  selSoft    accent     accentL    accentB    text       textDim    textFaint  border     borderL    danger     wave
        make("violeta",    "Violeta nocturno", false, "#191919", "#1F1F1F", "#282828", "#252525", "#212121", "#403C57", "#333046", "#635D8B", "#7670A0", "#9A92C8", "#EFEFEF", "#A0A0A0", "#6E6E6E", "#303030", "#3A3A3A", "#C05A5A", "#D8D8D8"),
        make("ambar",      "Ámbar",            false, "#17150F", "#1E1B13", "#26221A", "#221E17", "#1B1812", "#4A3A1C", "#372C17", "#8A6A2A", "#B08A38", "#E0A94A", "#F2EDE2", "#A79C87", "#6F6759", "#332D22", "#3E3729", "#C0603A", "#E6DCC6"),
        make("bosque",     "Bosque",           false, "#131813", "#191F19", "#202722", "#1C231E", "#161C17", "#2C4433", "#223328", "#3F7551", "#56966A", "#74C089", "#E9F1E9", "#97A79A", "#667069", "#283027", "#313B31", "#B85B4E", "#D2E2D2"),
        make("oceano",     "Océano",           false, "#101619", "#151D21", "#1B252A", "#182126", "#131A1E", "#23414F", "#1B3240", "#2E6E86", "#3E8FA9", "#58B6D0", "#E6F1F5", "#93A7B0", "#62727A", "#243036", "#2D3B42", "#C0605A", "#CFE3EA"),
        make("carmesi",    "Carmesí",          false, "#180F11", "#1F1417", "#271A1D", "#221619", "#1B1113", "#4C2530", "#371B24", "#8A3345", "#AE4459", "#D75E76", "#F4E9EC", "#AC9296", "#736063", "#332126", "#3E282E", "#D0604A", "#E8D4D9"),
        make("nord",       "Nord",             false, "#2E3440", "#333B49", "#3B4252", "#363D4C", "#2B303B", "#4C566A", "#434C5E", "#5E81AC", "#81A1C1", "#88C0D0", "#ECEFF4", "#AEB6C4", "#7B8494", "#434C5E", "#4C566A", "#BF616A", "#D8DEE9"),
        make("monocromo",  "Monocromo",        false, "#141414", "#1A1A1A", "#242424", "#202020", "#1A1A1A", "#3A3A3A", "#2E2E2E", "#5E5E5E", "#7C7C7C", "#ABABAB", "#F0F0F0", "#A0A0A0", "#6C6C6C", "#2E2E2E", "#383838", "#B45A5A", "#DADADA"),
        make("papel",      "Papel (claro)",    true,  "#E8E6E1", "#EFEDE8", "#F7F5F1", "#F1EFEA", "#E4E1DA", "#C9C2DC", "#DCD7E9", "#6E5F9E", "#8878BC", "#5B4C8C", "#26232B", "#5E5966", "#8E8896", "#D6D2CA", "#C7C2B9", "#B04A3A", "#9A93A8"),
    };
    return list;
}

const Palette& paletteById(const QString& id)
{
    const QVector<Palette>& list = palettes();
    for (const Palette& p : list)
        if (p.id == id)
            return p;
    return list.first();
}

QString currentPaletteId() { return g_paletteId; }

void setPalette(const QString& id)
{
    const Palette& p = paletteById(id);
    g_paletteId = p.id;

    Chrome        = p.chrome;
    ChromeLight   = p.chromeLight;
    Panel         = p.panel;
    PanelAlt      = p.panelAlt;
    PanelDeep     = p.panelDeep;
    Selection     = p.selection;
    SelectionSoft = p.selectionSoft;
    Accent        = p.accent;
    AccentLight   = p.accentLight;
    AccentBright  = p.accentBright;
    Text          = p.text;
    TextDim       = p.textDim;
    TextFaint     = p.textFaint;
    Border        = p.border;
    BorderLight   = p.borderLight;
    Danger        = p.danger;
    Wave          = p.wave;
}

void setPanelTransparency(float t)
{
    g_transparency = qBound(0.0f, t, 0.85f);
}

float panelTransparency() { return g_transparency; }

int panelAlpha()
{
    return qRound(255.0f * (1.0f - g_transparency));
}

QColor chromeBg()        { return applyAlpha(Chrome); }
QColor chromeLightBg()   { return applyAlpha(ChromeLight); }
QColor panelBg()         { return applyAlpha(Panel); }
QColor panelAltBg()      { return applyAlpha(PanelAlt); }
QColor panelDeepBg()     { return applyAlpha(PanelDeep); }
QColor selectionBg()     { return applyAlpha(Selection); }
QColor selectionSoftBg() { return applyAlpha(SelectionSoft); }

QFont uiFont(int pointSize, QFont::Weight weight)
{
    QFont f(QStringLiteral("Segoe UI"));
    if (!QFontDatabase::families().contains(QStringLiteral("Segoe UI")))
        f = QFont(QStringLiteral("Noto Sans"));
    f.setPointSize(pointSize);
    f.setWeight(weight);
    f.setHintingPreference(QFont::PreferFullHinting);
    return f;
}

QString styleSheet()
{
    QFile f(QStringLiteral(":/res/roxas.qss"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QString qss = QString::fromUtf8(f.readAll());

    // Un unico origen de verdad para los colores.
    // El orden importa: "@chrome" es prefijo de "@chromeLight", asi que los
    // tokens largos van primero para no truncar a los que los contienen.
    //
    // Los tokens de FONDO salen como rgba() con el alfa global, para que la
    // imagen de fondo se vea a traves de los paneles. Los tokens marcados
    // como opacos (menus, dialogos, texto y bordes) mantienen su hex: son
    // ventanas propias o trazos que deben leerse siempre.
    const int alpha = panelAlpha();

    const auto rgba = [alpha](const QColor& c) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
    };

    struct Tok { const char* key; const QColor* color; bool translucent; };
    const Tok tokens[] = {
        // opacos explicitos (menus, dialogos, ventana raiz)
        {"@opaqueChromeLight", &ChromeLight,   false},
        {"@opaquePanelAlt",    &PanelAlt,      false},
        {"@opaquePanelDeep",   &PanelDeep,     false},
        {"@opaqueChrome",      &Chrome,        false},
        {"@opaquePanel",       &Panel,         false},

        // fondos translucidos
        {"@selectionSoft",     &SelectionSoft, true},
        {"@chromeLight",       &ChromeLight,   true},
        {"@panelDeep",         &PanelDeep,     true},
        {"@selection",         &Selection,     true},
        {"@panelAlt",          &PanelAlt,      true},
        {"@chrome",            &Chrome,        true},
        {"@panel",             &Panel,         true},

        // texto y bordes: siempre opacos
        {"@accentBright",      &AccentBright,  false},
        {"@accentLight",       &AccentLight,   false},
        {"@borderLight",       &BorderLight,   false},
        {"@textFaint",         &TextFaint,     false},
        {"@textDim",           &TextDim,       false},
        {"@danger",            &Danger,        false},
        {"@border",            &Border,        false},
        {"@accent",            &Accent,        false},
        {"@wave",              &Wave,          false},
        {"@text",              &Text,          false},
    };

    for (const Tok& t : tokens) {
        qss.replace(QLatin1String(t.key),
                    t.translucent ? rgba(*t.color) : t.color->name(QColor::HexRgb));
    }

    return qss;
}

void applyPalette(QApplication* app)
{
    QPalette pal;
    pal.setColor(QPalette::Window,          Panel);
    pal.setColor(QPalette::WindowText,      Text);
    pal.setColor(QPalette::Base,            Panel);
    pal.setColor(QPalette::AlternateBase,   PanelAlt);
    pal.setColor(QPalette::Text,            Text);
    pal.setColor(QPalette::Button,          Panel);
    pal.setColor(QPalette::ButtonText,      Text);
    pal.setColor(QPalette::Highlight,       Selection);
    pal.setColor(QPalette::HighlightedText, Text);
    pal.setColor(QPalette::ToolTipBase,     Panel);
    pal.setColor(QPalette::ToolTipText,     Text);
    pal.setColor(QPalette::PlaceholderText, TextFaint);
    pal.setColor(QPalette::Disabled, QPalette::Text,       TextFaint);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, TextFaint);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, TextFaint);
    app->setPalette(pal);
}

void apply(QApplication* app)
{
    app->setStyle(QStringLiteral("Fusion"));
    app->setFont(uiFont());
    applyPalette(app);
    app->setStyleSheet(styleSheet());
}

void refreshStyleSheet(QApplication* app)
{
    // Se vuelve a generar entera: los tokens de fondo dependen del alfa y de
    // la paleta activa.
    applyPalette(app);
    app->setStyleSheet(styleSheet());
}

} // namespace Theme
