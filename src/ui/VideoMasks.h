#pragma once

#include <QString>

class QImage;

// Mascaras de imagen para el video: un retoque de color que se aplica a cada
// fotograma antes de dibujarlo.
//
// Cada mascara se reduce a tres tablas de 256 entradas (una por canal) mas un
// factor de saturacion, calculadas una sola vez. Asi aplicarla a un fotograma
// Full HD es un recorrido lineal sin coma flotante, que cabe de sobra entre
// fotograma y fotograma.
namespace VideoMasks {

enum Id {
    None = 0,
    Cinema,        // contraste suave, sombras frias y luces calidas; franjas 2,39:1
    Hdr,           // sombras levantadas, mas contraste local y color
    Grayscale,
    Sepia,
    Vivid,
    Warm,
    Cool,
    HighContrast,
    Vintage,       // negros levantados, tono calido apagado y vineta
    Noir,          // blanco y negro duro, con vineta
    TealOrange,    // el contraste de color de las peliculas de accion
    Pastel,        // claro y suave, poco color
    Cyberpunk,     // magentas y cianes de neon
    Faded,         // pelicula desvaida, sin negros ni blancos puros
    NightVision,   // verde monocromo con vineta, como unas gafas nocturnas
    Negative,      // colores invertidos
    Count
};

QString name(int mask);            // ya traducido

// Recorta a la proporcion de cine (2,39:1) con franjas negras al dibujar.
bool letterboxes(int mask);
constexpr double kCinemaAspect = 2.39;

// Aplica la mascara sobre `image`, que debe venir en Format_RGB32 o
// Format_ARGB32(_Premultiplied). None no toca nada.
void apply(QImage& image, int mask);

} // namespace VideoMasks
