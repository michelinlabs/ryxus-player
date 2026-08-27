#include "core/Lang.h"
#include "core/Settings.h"
#include "core/TrackInfo.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

namespace {

// Icono de la aplicacion, generado con el mismo trazo que el logotipo de la
// barra de titulo para no arrastrar un .ico aparte.
QIcon buildAppIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::Chrome);
        p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.18, size * 0.18);
        Icons::paint(p, Icons::Play,
                     QRectF(size * 0.18, size * 0.18, size * 0.64, size * 0.64),
                     Theme::AccentBright);
        p.end();

        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Roxas Player"));
    app.setApplicationDisplayName(QStringLiteral("Roxas Player"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("Roxas"));
    app.setWindowIcon(buildAppIcon());

    // TrackInfo viaja entre el hilo de escaneo y el de interfaz.
    qRegisterMetaType<TrackInfo>("TrackInfo");
    qRegisterMetaType<QVector<TrackInfo>>("QVector<TrackInfo>");

    // Idioma y tema se resuelven antes de construir nada: la interfaz se
    // arma una sola vez con las cadenas y los colores definitivos.
    Lang::setCurrent(Lang::fromCode(Settings::language()));
    Theme::setPalette(Settings::themeId());
    Theme::apply(&app);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Reproductor de audio con ecualizador de 8 bandas y editor de etiquetas."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("archivos"),
                                 QStringLiteral("Archivos de audio a reproducir."),
                                 QStringLiteral("[archivos...]"));
    parser.process(app);

    MainWindow window;
    window.show();

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        window.openPaths(positional);

    return app.exec();
}
