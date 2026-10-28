#pragma once

#include <QColor>
#include <QString>

class QPainter;
class QRectF;

// Como se pintan los subtitulos sobre el video: letra, contorno, posicion y
// un fondo opcional detras del texto para ganar contraste.
//
// Lo usan el panel de video y la vista previa de Configuracion, asi que lo
// que se ve al ajustarlo es exactamente lo que saldra sobre la imagen.
struct SubtitleStyle {
    enum Align { AlignLeft = 0, AlignCenter, AlignRight };

    // Forma del fondo detras del texto.
    enum Shape {
        ShapeLineBox = 0,     // una caja por linea, ajustada al texto
        ShapeRoundedLine,     // igual, con las esquinas redondeadas
        ShapeBlock,           // un solo bloque para todas las lineas
        ShapeBand,            // una franja de lado a lado de la imagen
        ShapeCount
    };

    QString family;             // vacio: la tipografia de la interfaz
    bool    bold        = true;
    int     sizePercent = 5;    // alto de letra, en % del alto de la imagen
    QColor  textColor   = Qt::white;

    // "Contraste": el contorno que separa la letra de la imagen.
    int     outline      = 4;   // 0 = sin contorno, 10 = muy grueso
    QColor  outlineColor = Qt::black;

    int     position  = 6;      // % del alto de la imagen desde abajo
    int     alignment = AlignCenter;

    bool    background = false;
    int     shape      = ShapeRoundedLine;
    QColor  backgroundColor = Qt::black;
    int     backgroundOpacity = 60;   // 0..100

    static QString shapeName(int shape);   // ya traducido

    // Leen y guardan en los ajustes del programa.
    static SubtitleStyle load();
    void save() const;

    // Pinta `text` (varias lineas con '\n') dentro de `picture`, el
    // rectangulo de la imagen visible.
    void paint(QPainter& p, const QRectF& picture, const QString& text) const;
};
