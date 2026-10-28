#include "ui/VideoMasks.h"

#include "core/Lang.h"

#include <QImage>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace VideoMasks {
namespace {

struct Grade {
    std::array<uchar, 256> r{}, g{}, b{};
    int  saturation = 256;   // en 1/256: 256 = sin cambio, 0 = gris
    int  vignette   = 0;     // oscurecimiento de los bordes, 0..255
};

uchar clampByte(double v)
{
    return uchar(std::clamp(int(std::lround(v)), 0, 255));
}

// Curva en S alrededor del gris medio: `amount` > 1 sube el contraste.
double contrast(double x, double amount)
{
    return (x - 0.5) * amount + 0.5;
}

// Rellena las tablas con f(x) en [0, 1] para cada canal.
template <typename F>
void fill(Grade& grade, F red, F green, F blue)
{
    for (int i = 0; i < 256; ++i) {
        const double x = i / 255.0;
        grade.r[i] = clampByte(red(x) * 255.0);
        grade.g[i] = clampByte(green(x) * 255.0);
        grade.b[i] = clampByte(blue(x) * 255.0);
    }
}

Grade build(int mask)
{
    Grade grade;
    using Fn = double (*)(double);
    const Fn identity = [](double x) { return x; };
    fill<Fn>(grade, identity, identity, identity);

    switch (mask) {
    case Cinema:
        // Contraste contenido, color algo apagado y el reparto clasico de cine:
        // sombras hacia el verde azulado y luces hacia el anaranjado.
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 1.12) + 0.05 * x * x; },
                 [](double x) { return contrast(x, 1.10) + 0.01; },
                 [](double x) { return contrast(x, 1.12) + 0.06 * (1.0 - x) * (1.0 - x) - 0.03 * x; });
        grade.saturation = 215;
        break;

    case Hdr:
        // Sombras levantadas (gamma) y luces contenidas, con mas color: el
        // aspecto de un HDR mapeado a pantalla normal.
        fill<Fn>(grade,
                 [](double x) { return contrast(std::pow(x, 0.80), 1.18); },
                 [](double x) { return contrast(std::pow(x, 0.80), 1.18); },
                 [](double x) { return contrast(std::pow(x, 0.80), 1.18); });
        grade.saturation = 345;
        break;

    case Grayscale:
        grade.saturation = 0;
        break;

    case Sepia:
        grade.saturation = 0;
        fill<Fn>(grade,
                 [](double x) { return x * 0.94 + 0.10; },
                 [](double x) { return x * 0.86 + 0.06; },
                 [](double x) { return x * 0.68 + 0.02; });
        break;

    case Vivid:
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 1.10); },
                 [](double x) { return contrast(x, 1.10); },
                 [](double x) { return contrast(x, 1.10); });
        grade.saturation = 400;
        break;

    case Warm:
        fill<Fn>(grade,
                 [](double x) { return x * 1.06 + 0.02; },
                 [](double x) { return x * 1.01; },
                 [](double x) { return x * 0.88; });
        break;

    case Cool:
        fill<Fn>(grade,
                 [](double x) { return x * 0.90; },
                 [](double x) { return x * 0.99 + 0.01; },
                 [](double x) { return x * 1.06 + 0.03; });
        break;

    case HighContrast:
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 1.55); },
                 [](double x) { return contrast(x, 1.55); },
                 [](double x) { return contrast(x, 1.55); });
        grade.saturation = 280;
        break;

    case Vintage:
        // Negros levantados, luces contenidas y un tono amarillento: una
        // foto de los setenta.
        fill<Fn>(grade,
                 [](double x) { return 0.10 + x * 0.84; },
                 [](double x) { return 0.08 + x * 0.78; },
                 [](double x) { return 0.06 + x * 0.62; });
        grade.saturation = 175;
        grade.vignette = 120;
        break;

    case Noir:
        grade.saturation = 0;
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 1.65); },
                 [](double x) { return contrast(x, 1.65); },
                 [](double x) { return contrast(x, 1.65); });
        grade.vignette = 150;
        break;

    case TealOrange:
        // Pieles y luces hacia el naranja, sombras hacia el verde azulado.
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 1.12) + 0.10 * x * x - 0.04 * (1.0 - x); },
                 [](double x) { return contrast(x, 1.08) + 0.02; },
                 [](double x) { return contrast(x, 1.12) + 0.14 * (1.0 - x) * (1.0 - x) - 0.10 * x * x; });
        grade.saturation = 300;
        break;

    case Pastel:
        fill<Fn>(grade,
                 [](double x) { return 0.18 + x * 0.80; },
                 [](double x) { return 0.18 + x * 0.79; },
                 [](double x) { return 0.20 + x * 0.80; });
        grade.saturation = 150;
        break;

    case Cyberpunk:
        // Rojos hacia el magenta, verdes hundidos y azules arriba.
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 1.18) + 0.08 * x; },
                 [](double x) { return contrast(x, 1.18) * 0.80; },
                 [](double x) { return contrast(x, 1.10) + 0.16; });
        grade.saturation = 330;
        grade.vignette = 70;
        break;

    case Faded:
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 0.78) + 0.03; },
                 [](double x) { return contrast(x, 0.78) + 0.02; },
                 [](double x) { return contrast(x, 0.78) + 0.04; });
        grade.saturation = 165;
        break;

    case NightVision:
        // Todo a gris y luego al verde de un intensificador de imagen.
        grade.saturation = 0;
        fill<Fn>(grade,
                 [](double x) { return contrast(x, 1.25) * 0.22; },
                 [](double x) { return contrast(x, 1.25) * 1.05 + 0.06; },
                 [](double x) { return contrast(x, 1.25) * 0.22; });
        grade.vignette = 190;
        break;

    case Negative:
        fill<Fn>(grade,
                 [](double x) { return 1.0 - x; },
                 [](double x) { return 1.0 - x; },
                 [](double x) { return 1.0 - x; });
        break;

    default:
        break;
    }
    return grade;
}

const Grade& gradeFor(int mask)
{
    static const auto kGrades = [] {
        std::array<Grade, Count> grades;
        for (int i = 0; i < Count; ++i)
            grades[size_t(i)] = build(i);
        return grades;
    }();
    return kGrades[size_t(std::clamp(mask, 0, Count - 1))];
}

} // namespace

QString name(int mask)
{
    switch (mask) {
    case None:         return Lang::tr("Ninguna");
    case Cinema:       return Lang::tr("Modo cine");
    case Hdr:          return Lang::tr("HDR");
    case Grayscale:    return Lang::tr("Escala de grises");
    case Sepia:        return Lang::tr("Sepia");
    case Vivid:        return Lang::tr("Colores vivos");
    case Warm:         return Lang::tr("Calido");
    case Cool:         return Lang::tr("Frio");
    case HighContrast: return Lang::tr("Alto contraste");
    case Vintage:      return Lang::tr("Vintage");
    case Noir:         return Lang::tr("Cine negro");
    case TealOrange:   return Lang::tr("Teal y naranja");
    case Pastel:       return Lang::tr("Pastel");
    case Cyberpunk:    return Lang::tr("Cyberpunk");
    case Faded:        return Lang::tr("Desvaido");
    case NightVision:  return Lang::tr("Vision nocturna");
    case Negative:     return Lang::tr("Negativo");
    }
    return QString();
}

bool letterboxes(int mask)
{
    return mask == Cinema;
}

void apply(QImage& image, int mask)
{
    if (mask <= None || mask >= Count || image.isNull())
        return;

    const Grade& grade = gradeFor(mask);
    const int sat = grade.saturation;
    const int width = image.width();
    const int height = image.height();

    // Vineta: la distancia al centro al cuadrado se separa en una parte por
    // fila y otra por columna, asi que por pixel solo queda una suma. 1024 =
    // nada oscurecido; en las esquinas se resta hasta `vignette`/255.
    std::vector<int> rowTerm, colTerm;
    if (grade.vignette > 0) {
        rowTerm.resize(size_t(height));
        colTerm.resize(size_t(width));
        for (int y = 0; y < height; ++y) {
            const double d = (y - height / 2.0) / (height / 2.0);
            rowTerm[size_t(y)] = int(d * d * 512.0 * grade.vignette / 255.0);
        }
        for (int x = 0; x < width; ++x) {
            const double d = (x - width / 2.0) / (width / 2.0);
            colTerm[size_t(x)] = int(d * d * 512.0 * grade.vignette / 255.0);
        }
    }

    for (int y = 0; y < height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const QRgb px = line[x];
            int r = qRed(px), g = qGreen(px), b = qBlue(px);

            if (sat != 256) {
                // Luma BT.601 en enteros, y se aleja o se acerca cada canal a
                // ella segun la saturacion.
                const int luma = (77 * r + 150 * g + 29 * b) >> 8;
                r = std::clamp(luma + (((r - luma) * sat) >> 8), 0, 255);
                g = std::clamp(luma + (((g - luma) * sat) >> 8), 0, 255);
                b = std::clamp(luma + (((b - luma) * sat) >> 8), 0, 255);
            }

            int outR = grade.r[size_t(r)], outG = grade.g[size_t(g)], outB = grade.b[size_t(b)];
            if (grade.vignette > 0) {
                const int keep = std::max(0, 1024 - rowTerm[size_t(y)] - colTerm[size_t(x)]);
                outR = (outR * keep) >> 10;
                outG = (outG * keep) >> 10;
                outB = (outB * keep) >> 10;
            }
            line[x] = qRgba(outR, outG, outB, qAlpha(px));
        }
    }
}

} // namespace VideoMasks
