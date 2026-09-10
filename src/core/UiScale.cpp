#include "core/UiScale.h"

#include <QByteArray>
#include <QtGlobal>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include <algorithm>
#include <cmath>

namespace UiScale {
namespace {

// Tamano logico para el que esta pensada la ventana. Por debajo de esto no
// caben a la vez la columna izquierda, el arbol, la lista y el panel derecho,
// ni en alto la barra de titulo, el ecualizador y la barra de reproduccion.
constexpr double kDesignWidth  = 1280.0;
constexpr double kDesignHeight = 820.0;

// Suelo: por debajo la interfaz deja de leerse, y es preferible que sobre
// ventana a que no se distinga nada.
constexpr double kMinFactor = 0.60;

} // namespace

void applyForCurrentScreen()
{
#ifdef Q_OS_WIN
    // Si el usuario ya fijo una escala a mano, manda la suya.
    if (!qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR"))
        return;

    RECT work{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0))
        return;

    double width  = double(work.right - work.left);
    double height = double(work.bottom - work.top);
    if (width <= 0.0 || height <= 0.0)
        return;

    // Windows devuelve pixeles fisicos si el proceso ya declara ser consciente
    // del DPI, y virtualizados -- es decir, ya logicos -- si no lo es. Qt razona
    // siempre en logicos, asi que en el primer caso hay que descontar la escala
    // del sistema para no comparar peras con manzanas.
    if (IsProcessDPIAware()) {
        const HDC screen = GetDC(nullptr);
        const double dpi = screen ? double(GetDeviceCaps(screen, LOGPIXELSX)) : 96.0;
        if (screen)
            ReleaseDC(nullptr, screen);
        if (dpi > 0.0) {
            width  /= dpi / 96.0;
            height /= dpi / 96.0;
        }
    }

    const double factor = std::min(width / kDesignWidth, height / kDesignHeight);
    if (factor >= 1.0)
        return;   // cabe de sobra: no se toca nada

    // A pasos de 0,05 hacia abajo: evita factores como 0,873 y garantiza que
    // el redondeo nunca deje la ventana un poco mas grande de lo que cabe.
    const double stepped = std::max(kMinFactor, std::floor(factor * 20.0) / 20.0);

    qputenv("QT_SCALE_FACTOR", QByteArray::number(stepped, 'f', 2));
#endif
}

} // namespace UiScale
