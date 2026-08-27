#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QVector>

class QApplication;

namespace Theme {

// Colores activos. No son constantes: cambiarlos es lo que cambia el tema, y
// asi todo el codigo que ya los usa (Theme::Panel, Theme::Accent...) sigue
// funcionando sin tocar una linea.
extern QColor Chrome;        // barra de titulo, columna izquierda, barra inferior
extern QColor ChromeLight;   // franja de pestanas y de busqueda
extern QColor Panel;         // fondo de listas y menus
extern QColor PanelAlt;      // fila alterna
extern QColor PanelDeep;     // hundidos (campos, rail del deslizador)

extern QColor Selection;     // fila seleccionada
extern QColor SelectionSoft; // hover sobre fila
extern QColor Accent;
extern QColor AccentLight;
extern QColor AccentBright;

extern QColor Text;
extern QColor TextDim;
extern QColor TextFaint;

extern QColor Border;
extern QColor BorderLight;
extern QColor Danger;
extern QColor Wave;          // parte no reproducida de la onda

// --- temas -----------------------------------------------------------------

struct Palette {
    QString id;
    QString name;
    QColor chrome, chromeLight, panel, panelAlt, panelDeep;
    QColor selection, selectionSoft, accent, accentLight, accentBright;
    QColor text, textDim, textFaint;
    QColor border, borderLight, danger, wave;
    bool   light = false;    // solo informativo, para el previsualizador
};

const QVector<Palette>& palettes();
const Palette&          paletteById(const QString& id);
QString                 currentPaletteId();
void                    setPalette(const QString& id);

// --- transparencia global de los paneles -----------------------------------
//
// Cuando hay una imagen de fondo, los paneles se vuelven translucidos para
// dejarla ver. `t` va de 0 (opaco, sin imagen visible) a 0.85 (muy
// transparente). Los colores *Bg() de abajo ya aplican ese alfa, y son los
// que deben usar los widgets que se pintan a mano.
void  setPanelTransparency(float t);
float panelTransparency();
int   panelAlpha();

QColor chromeBg();
QColor chromeLightBg();
QColor panelBg();
QColor panelAltBg();
QColor panelDeepBg();
QColor selectionBg();
QColor selectionSoftBg();

// Metricas del layout, medidas pixel a pixel sobre objetive.jpg (1600x856).
namespace Metrics {
constexpr int TitleBarHeight    = 36;   // 0..36
constexpr int TabStripHeight    = 35;   // 36..71
constexpr int LeftPanelWidth    = 285;  // 0..285
constexpr int TreePanelWidth    = 208;  // 285..493
constexpr int RightPanelWidth   = 375;  // 1225..1600
constexpr int PlayerBarHeight   = 85;   // 771..856
constexpr int RowHeight         = 33;   // paso de fila del arbol y la lista
constexpr int PlaylistRowHeight = 48;   // paso de fila del panel derecho
constexpr int ToolRowHeight     = 36;   // franjas de busqueda rapida
constexpr int HeaderHeight      = 39;   // cabecera de columnas / barra de agrupacion
constexpr int FilterRowHeight   = 37;   // fila de letras de unidad
constexpr int TotalsHeight      = 24;   // linea de totales del panel derecho
constexpr int CoverSize         = 250;  // caratula: 250x250 en (16,37)
constexpr int CoverMargin       = 16;
constexpr int VisualizerHeight  = 63;   // onda del panel izquierdo
}

QFont uiFont(int pointSize = 9, QFont::Weight weight = QFont::Normal);
QString styleSheet();
void apply(QApplication* app);
void applyPalette(QApplication* app);
void refreshStyleSheet(QApplication* app);

} // namespace Theme
