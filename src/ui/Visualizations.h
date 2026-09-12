#pragma once

#include <QRectF>
#include <vector>

class QPainter;

// Capa de fondo viva: visualizaciones dibujadas con el audio en tiempo real,
// al estilo de las que traia el Reproductor de Windows Media.
//
// Se pinta encima de la imagen de fondo y debajo de la interfaz, y cada capa
// lleva su propia opacidad, asi que foto y visualizacion pueden convivir.
//
// Todas toman los mismos datos y respetan el mismo contrato: pintar dentro del
// area que se les da, sin tocar el estado del QPainter que reciben mas alla de
// lo que restauran ellas mismas.
namespace Visualizations {

// Lo que ve una visualizacion de cada fotograma de audio.
struct Frame {
    const std::vector<float>* spectrum = nullptr;  // 0..1 por banda, grave -> agudo
    const std::vector<float>* wave     = nullptr;  // -1..1, forma de onda
    float  bass    = 0.0f;   // energias agregadas, 0..1
    float  mid     = 0.0f;
    float  treble  = 0.0f;
    float  level   = 0.0f;   // energia global
    double seconds = 0.0;    // reloj continuo, para lo que gire o se desplace
};

struct Info {
    const char* id;     // clave de ajustes
    const char* name;   // rotulo visible (espanol; se traduce en la interfaz)
};

int         count();
const Info& info(int index);
int         indexOfId(const char* id);

// Dibuja la visualizacion `index` dentro de `area`.
void paint(QPainter& painter, const QRectF& area, int index, const Frame& frame);

} // namespace Visualizations
