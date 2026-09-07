#include "core/Lang.h"
#include "core/Settings.h"
#include "core/TrackInfo.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Roxas Player"));
    app.setApplicationDisplayName(QStringLiteral("Roxas Player"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("Roxas"));
    // Mismo icono que lleva embebido el ejecutable (res/roxas.rc), asi la
    // ventana, la barra de tareas y el Explorador muestran lo mismo.
    app.setWindowIcon(QIcon(QStringLiteral(":/res/roxas.ico")));

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
        QStringLiteral("Reproductor de audio con ecualizador de 12 bandas, "
                       "rack de efectos y editor de etiquetas."));
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
