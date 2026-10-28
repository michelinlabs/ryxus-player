#include "ui/SettingsDialog.h"

#include "core/Lang.h"
#include "core/Settings.h"
#include "core/SystemAudioTap.h"
#include "ui/Visualizations.h"
#include "ui/BackgroundHost.h"
#include "ui/Commands.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QLinearGradient>
#include <QPainterPath>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QStandardPaths>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

// --------------------------------------------------------------- ThemeSwatch

// Tarjeta pulsable que muestra una paleta: una franja por color y el nombre
// debajo. Se ve el tema antes de aplicarlo.
class ThemeSwatch : public QWidget {
    Q_OBJECT

public:
    explicit ThemeSwatch(const Theme::Palette& palette, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_palette(palette)
    {
        setFixedSize(132, 74);
        setCursor(Qt::PointingHandCursor);
        setToolTip(palette.name);
    }

    QString paletteId() const { return m_palette.id; }

    void setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        update();
    }

signals:
    void picked(const QString& paletteId);

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        // Fondo de la tarjeta con el color de panel del propio tema.
        p.setPen(Qt::NoPen);
        p.setBrush(m_palette.chrome);
        p.drawRoundedRect(box, 4, 4);

        // Muestra: barra de acento y tres tonos de panel, como una miniatura
        // de la ventana real.
        const QRectF strip(box.left() + 7, box.top() + 7, box.width() - 14, 26);
        p.setBrush(m_palette.panel);
        p.drawRoundedRect(strip, 2, 2);

        p.setBrush(m_palette.selection);
        p.drawRoundedRect(QRectF(strip.left() + 3, strip.top() + 3,
                                 strip.width() - 6, 9), 2, 2);

        const qreal dot = 8.0;
        const QColor dots[4] = {m_palette.accentBright, m_palette.accent,
                                m_palette.wave, m_palette.textDim};
        for (int i = 0; i < 4; ++i) {
            p.setBrush(dots[i]);
            p.drawEllipse(QRectF(strip.left() + 4 + i * (dot + 4),
                                 strip.bottom() - dot - 3, dot, dot));
        }

        p.setFont(Theme::uiFont(8, m_selected ? QFont::DemiBold : QFont::Normal));
        p.setPen(m_palette.text);
        p.drawText(QRectF(box.left(), strip.bottom() + 4, box.width(), 22),
                   Qt::AlignCenter, m_palette.name);

        // Borde: acento si esta elegido, tenue al pasar por encima.
        QPen frame(m_selected ? Theme::AccentBright
                              : (m_hovered ? Theme::BorderLight : Theme::Border));
        frame.setWidthF(m_selected ? 2.0 : 1.0);
        p.setPen(frame);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, 4, 4);
    }

    void mousePressEvent(QMouseEvent*) override { emit picked(m_palette.id); }
    void enterEvent(QEnterEvent*) override { m_hovered = true;  update(); }
    void leaveEvent(QEvent*) override      { m_hovered = false; update(); }

private:
    Theme::Palette m_palette;
    bool m_selected = false;
    bool m_hovered  = false;
};

// ------------------------------------------------------------- ColorButton

// Muestra de color pulsable: al hacer clic se abre el selector de Windows/Qt.
class ColorButton : public QAbstractButton {
public:
    ColorButton(const QColor& color, QWidget* parent)
        : QAbstractButton(parent)
        , m_color(color)
    {
        setFixedSize(46, 22);
        setCursor(Qt::PointingHandCursor);
    }

    QColor color() const { return m_color; }
    void setColor(const QColor& color)
    {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(underMouse() ? Theme::AccentBright : Theme::BorderLight, 1.0));
        p.setBrush(m_color);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*) override      { update(); }

private:
    QColor m_color;
};

// --------------------------------------------------------- SubtitlePreview

// Como quedan los subtitulos con el estilo elegido, sobre una imagen con
// zonas claras y oscuras: el contraste se juzga en las dos.
class SubtitlePreview : public QWidget {
public:
    explicit SubtitlePreview(QWidget* parent)
        : QWidget(parent)
    {
        setFixedHeight(180);
    }

    void setStyle(const SubtitleStyle& style)
    {
        m_style = style;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Un "fotograma" 16:9 centrado: cielo claro arriba, suelo oscuro
        // abajo y una franja blanca que cruza justo por donde van las letras.
        const double h = height();
        const double w = std::min(double(width()), h * 16.0 / 9.0);
        const QRectF picture((width() - w) / 2.0, 0, w, h);

        QLinearGradient sky(picture.topLeft(), picture.bottomLeft());
        sky.setColorAt(0.0, QColor(0x9F, 0xC8, 0xF0));
        sky.setColorAt(0.55, QColor(0xF2, 0xD7, 0xA6));
        sky.setColorAt(0.56, QColor(0x3A, 0x34, 0x40));
        sky.setColorAt(1.0, QColor(0x12, 0x10, 0x18));
        p.fillRect(picture, sky);

        QPainterPath beam;
        beam.moveTo(picture.left() + w * 0.55, picture.bottom());
        beam.lineTo(picture.left() + w * 0.70, picture.bottom());
        beam.lineTo(picture.left() + w * 0.45, picture.top());
        beam.lineTo(picture.left() + w * 0.38, picture.top());
        p.fillPath(beam, QColor(255, 255, 255, 210));

        m_style.paint(p, picture, Lang::tr("Asi se veran los subtitulos\ncon el estilo elegido"));

        p.setPen(Theme::BorderLight);
        p.setBrush(Qt::NoBrush);
        p.drawRect(picture.adjusted(0.5, 0.5, -0.5, -0.5));
    }

private:
    SubtitleStyle m_style;
};

// ------------------------------------------------------------ SettingsDialog

namespace {

constexpr int kThemeColumns = 4;
constexpr int kSwatchHeight = 74;   // igual que el setFixedSize de ThemeSwatch

QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font = Theme::uiFont(8, QFont::DemiBold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    label->setFont(font);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(Lang::tr("Configuracion"));
    setModal(false);
    buildUi();
}

void SettingsDialog::buildUi()
{
    // Todo el contenido va en un area desplazable: con la seccion de audio
    // del sistema el dialogo ya no cabe en una pantalla normal y lo de abajo
    // quedaba cortado. El boton Cerrar queda fuera, siempre a la vista.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("settingsScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll, 1);

    auto* content = new QWidget(scroll);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // ---------------------------------------------------------- apariencia
    root->addWidget(sectionTitle(Lang::tr("APARIENCIA"), this));

    auto* themeHint = new QLabel(Lang::tr("Tema de color"), this);
    themeHint->setObjectName(QStringLiteral("detailKey"));
    themeHint->setFont(Theme::uiFont(9));
    root->addWidget(themeHint);

    // La rejilla de temas vive dentro de un area desplazable: hay bastantes
    // paletas y, sin esto, cada una que se anade estira el dialogo hasta que
    // no cabe en pantalla.
    auto* themeArea = new QScrollArea(this);
    themeArea->setObjectName(QStringLiteral("themeScroll"));
    themeArea->setFrameShape(QFrame::NoFrame);
    themeArea->setWidgetResizable(true);
    themeArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* themeGridHost = new QWidget(themeArea);
    auto* grid = new QGridLayout(themeGridHost);
    grid->setContentsMargins(0, 0, 6, 0);
    grid->setSpacing(8);

    const QVector<Theme::Palette>& list = Theme::palettes();
    for (int i = 0; i < list.size(); ++i) {
        auto* swatch = new ThemeSwatch(list.at(i), themeGridHost);
        connect(swatch, &ThemeSwatch::picked, this, &SettingsDialog::onThemePicked);
        grid->addWidget(swatch, i / kThemeColumns, i % kThemeColumns);
        m_swatches.append(swatch);
    }
    grid->setRowStretch(grid->rowCount(), 1);

    themeArea->setWidget(themeGridHost);

    // Alto para tres filas justas: se ve que hay mas y se desplaza.
    const int rows = qMin(3, (list.size() + kThemeColumns - 1) / kThemeColumns);
    themeArea->setFixedHeight(rows * kSwatchHeight + (rows - 1) * 8 + 4);
    root->addWidget(themeArea);
    refreshSwatches();

    // -------------------------------------------------------------- fondo
    root->addSpacing(6);
    root->addWidget(sectionTitle(Lang::tr("FONDO"), this));

    auto* imageRow = new QHBoxLayout;
    imageRow->setSpacing(8);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setObjectName(QStringLiteral("detailValue"));
    m_imageLabel->setFont(Theme::uiFont(9));
    imageRow->addWidget(m_imageLabel, 1);

    auto* chooseButton = new FlatButton(Lang::tr("Elegir imagen o GIF..."), this);
    chooseButton->setIconId(Icons::Image);
    chooseButton->setGlyphSize(14);
    chooseButton->setFixedHeight(26);
    chooseButton->setColors(Theme::AccentBright, Theme::Text, Theme::Text);
    imageRow->addWidget(chooseButton);
    connect(chooseButton, &FlatButton::clicked, this, &SettingsDialog::chooseImage);

    m_clearImage = new FlatButton(Lang::tr("Quitar imagen"), this);
    m_clearImage->setIconId(Icons::Trash);
    m_clearImage->setGlyphSize(14);
    m_clearImage->setFixedHeight(26);
    imageRow->addWidget(m_clearImage);
    connect(m_clearImage, &FlatButton::clicked, this, &SettingsDialog::clearImage);

    root->addLayout(imageRow);

    auto* fitRow = new QHBoxLayout;
    fitRow->setSpacing(8);
    auto* fitLabel = new QLabel(Lang::tr("Ajuste"), this);
    fitLabel->setObjectName(QStringLiteral("detailKey"));
    fitLabel->setFont(Theme::uiFont(9));
    fitLabel->setFixedWidth(170);
    fitRow->addWidget(fitLabel);

    m_fitMode = new QComboBox(this);
    m_fitMode->setFont(Theme::uiFont(9));
    m_fitMode->addItem(Lang::tr("Cubrir"));
    m_fitMode->addItem(Lang::tr("Ajustar"));
    m_fitMode->addItem(Lang::tr("Estirar"));
    m_fitMode->addItem(Lang::tr("Mosaico"));
    m_fitMode->setCurrentIndex(qBound(0, Settings::backgroundMode(), 3));
    m_fitMode->setFixedWidth(160);
    fitRow->addWidget(m_fitMode);
    fitRow->addStretch(1);
    root->addLayout(fitRow);

    connect(m_fitMode, &QComboBox::currentIndexChanged,
            this, &SettingsDialog::backgroundModeChanged);

    // Deslizadores de transparencia y oscurecido.
    const auto addSlider = [this, root](const QString& title, int value,
                                        int maximum, QSlider*& slider, QLabel*& readout) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* label = new QLabel(title, this);
        label->setObjectName(QStringLiteral("detailKey"));
        label->setFont(Theme::uiFont(9));
        label->setFixedWidth(170);
        row->addWidget(label);

        slider = new QSlider(Qt::Horizontal, this);
        slider->setObjectName(QStringLiteral("bgSlider"));
        slider->setRange(0, maximum);
        slider->setValue(value);
        slider->setFixedWidth(240);
        row->addWidget(slider);

        readout = new QLabel(QStringLiteral("%1 %").arg(value), this);
        readout->setObjectName(QStringLiteral("detailValue"));
        readout->setFont(Theme::uiFont(9));
        readout->setFixedWidth(46);
        row->addWidget(readout);

        row->addStretch(1);
        root->addLayout(row);
    };

    addSlider(Lang::tr("Transparencia de los paneles"),
              Settings::backgroundTransparency(), 85,
              m_transparency, m_transparencyValue);
    addSlider(Lang::tr("Oscurecer la imagen"),
              Settings::backgroundDarkening(), 90,
              m_darkening, m_darkeningValue);

    connect(m_transparency, &QSlider::valueChanged, this, [this](int value) {
        m_transparencyValue->setText(QStringLiteral("%1 %").arg(value));
        emit transparencyChanged(value);
    });
    connect(m_darkening, &QSlider::valueChanged, this, [this](int value) {
        m_darkeningValue->setText(QStringLiteral("%1 %").arg(value));
        emit darkeningChanged(value);
    });

    // --- capa viva ---------------------------------------------------------
    auto* visualRow = new QHBoxLayout;
    visualRow->setSpacing(8);

    auto* visualLabel = new QLabel(Lang::tr("Visualizacion"), this);
    visualLabel->setObjectName(QStringLiteral("detailKey"));
    visualLabel->setFont(Theme::uiFont(9));
    visualLabel->setFixedWidth(170);
    visualRow->addWidget(visualLabel);

    m_visualization = new QComboBox(this);
    m_visualization->setFont(Theme::uiFont(9));
    m_visualization->setFixedWidth(160);
    m_visualization->addItem(Lang::tr("Ninguna"), -1);
    for (int i = 0; i < Visualizations::count(); ++i)
        m_visualization->addItem(Lang::tr(Visualizations::info(i).name), i);
    m_visualization->setCurrentIndex(
        qMax(0, m_visualization->findData(Settings::backgroundVisualization())));
    visualRow->addWidget(m_visualization);
    visualRow->addStretch(1);
    root->addLayout(visualRow);

    connect(m_visualization, &QComboBox::currentIndexChanged, this, [this](int) {
        emit visualizationChanged(m_visualization->currentData().toInt());
    });

    // --- opacidad de cada capa, por separado -------------------------------
    addSlider(Lang::tr("Opacidad de la imagen"),
              Settings::backgroundImageOpacity(), 100,
              m_imageOpacity, m_imageOpacityValue);
    addSlider(Lang::tr("Opacidad de la visualizacion"),
              Settings::backgroundVisualOpacity(), 100,
              m_visualOpacity, m_visualOpacityValue);

    connect(m_imageOpacity, &QSlider::valueChanged, this, [this](int value) {
        m_imageOpacityValue->setText(QStringLiteral("%1 %").arg(value));
        emit imageOpacityChanged(value);
    });
    connect(m_visualOpacity, &QSlider::valueChanged, this, [this](int value) {
        m_visualOpacityValue->setText(QStringLiteral("%1 %").arg(value));
        emit visualOpacityChanged(value);
    });

    auto* bgNote = new QLabel(
        Lang::tr("El fondo tiene dos capas que conviven: la imagen (JPG, PNG, WEBP o GIF "
                 "animado) y la visualizacion, que se dibuja con el audio en vivo. Cada "
                 "una lleva su propia opacidad, y la transparencia de los paneles deja "
                 "ver las dos a traves de la interfaz."),
        this);
    bgNote->setObjectName(QStringLiteral("statusText"));
    bgNote->setFont(Theme::uiFont(8));
    bgNote->setWordWrap(true);
    root->addWidget(bgNote);

    // ------------------------------------------------- audio del sistema
    root->addSpacing(6);
    root->addWidget(sectionTitle(Lang::tr("AUDIO DEL SISTEMA"), this));

    m_systemAudio = new QCheckBox(
        Lang::tr("Aplicar el ecualizador y los efectos a todo el audio de Windows"), this);
    m_systemAudio->setFont(Theme::uiFont(9));
    m_systemAudio->setChecked(Settings::systemAudioEnabled());
    root->addWidget(m_systemAudio);

    // El reparto correcto lo decide SystemAudioTap: capturar del cable virtual
    // y sacar por unos altavoces de verdad. Si no hay cable, devuelve vacio.
    QByteArray suggestedSource;
    SystemAudioTap::suggestRouting(&suggestedSource, nullptr);
    const bool haveCable = !suggestedSource.isEmpty();

    const auto addDeviceRow = [&](const QString& label, QComboBox*& combo) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* text = new QLabel(label, this);
        text->setObjectName(QStringLiteral("detailKey"));
        text->setFont(Theme::uiFont(9));
        text->setFixedWidth(170);
        row->addWidget(text);

        combo = new QComboBox(this);
        combo->setFont(Theme::uiFont(9));
        combo->setMinimumWidth(300);
        row->addWidget(combo, 1);

        root->addLayout(row);
    };

    addDeviceRow(Lang::tr("Capturar de"), m_systemSource);
    addDeviceRow(Lang::tr("Sacar procesado por"), m_systemOutput);
    refreshSystemDevices();

    m_systemAudioStatus = new QLabel(this);
    m_systemAudioStatus->setObjectName(QStringLiteral("statusText"));
    m_systemAudioStatus->setFont(Theme::uiFont(8));
    m_systemAudioStatus->setWordWrap(true);
    m_systemAudioStatus->setText(haveCable
        ? Lang::tr(
            "Cable virtual detectado y ya elegido arriba. Falta un solo paso, y ese lo "
            "tiene que dar Windows: poner el cable como salida predeterminada.\n\n"
            "1. Pulsa el boton de abajo.\n"
            "2. En Salida, elige el cable (CABLE Input).\n"
            "3. Vuelve aqui y marca la casilla de arriba.\n\n"
            "Desde ese momento todo lo que suene en Windows entra por el cable, pasa "
            "por el ecualizador y los efectos, y sale por la salida elegida arriba.")
        : Lang::tr(
            "Windows no deja a un programa normal filtrar el sonido de los demas antes "
            "de que salga por el altavoz: eso lo hace Realtek porque instala un "
            "controlador del sistema. Lo que si se puede es capturar una salida y "
            "devolverla procesada por otra distinta.\n\n"
            "Para eso hace falta un cable de audio virtual. Instala VB-Cable (gratis) "
            "con el boton de abajo y vuelve: el resto se elige solo."));
    root->addWidget(m_systemAudioStatus);

    // El paso que no se puede dar desde aqui: se lleva al usuario al sitio.
    auto* actionRow = new QHBoxLayout;
    actionRow->setSpacing(8);
    actionRow->addStretch(1);

    auto* actionButton = new FlatButton(
        haveCable ? Lang::tr("Abrir la configuracion de sonido de Windows")
                  : Lang::tr("Descargar VB-Cable (gratis)"), this);
    actionButton->setIconId(haveCable ? Icons::Settings : Icons::ArrowRight);
    actionButton->setGlyphSize(14);
    actionButton->setFixedHeight(26);
    actionButton->setColors(Theme::AccentBright, Theme::Text, Theme::Text);
    actionRow->addWidget(actionButton);
    root->addLayout(actionRow);

    connect(actionButton, &FlatButton::clicked, this, [haveCable]() {
        if (!haveCable) {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://vb-audio.com/Cable/")));
            return;
        }
        // Pagina de sonido de Windows 10/11; si no existe, el panel clasico.
        if (!QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:sound"))))
            QProcess::startDetached(QStringLiteral("control"), {QStringLiteral("mmsys.cpl")});
    });

    const auto pushSystemAudio = [this]() {
        emit systemAudioChanged(m_systemAudio->isChecked(),
                                m_systemSource->currentData().toByteArray(),
                                m_systemOutput->currentData().toByteArray());
    };
    connect(m_systemAudio, &QCheckBox::toggled, this, pushSystemAudio);
    connect(m_systemSource, &QComboBox::currentIndexChanged, this, pushSystemAudio);
    connect(m_systemOutput, &QComboBox::currentIndexChanged, this, pushSystemAudio);

    // --------------------------------------------------------- interfaces
    // Cada salida usada para lo procesado recuerda su ecualizador y sus
    // efectos. Aqui se ven, se elige cual usar y se olvidan las que sobran.
    root->addSpacing(6);
    root->addWidget(sectionTitle(Lang::tr("INTERFACES"), this));

    m_autoSwitch = new QCheckBox(
        Lang::tr("Cambiar sola a la interfaz que se conecte, y a otra si se desconecta "
                 "la que se usa"), this);
    m_autoSwitch->setFont(Theme::uiFont(9));
    m_autoSwitch->setChecked(Settings::autoSwitchOutputs());
    root->addWidget(m_autoSwitch);
    connect(m_autoSwitch, &QCheckBox::toggled, this, [](bool on) {
        Settings::setAutoSwitchOutputs(on);
    });

    m_interfaceList = new QWidget(this);
    auto* interfaceLayout = new QVBoxLayout(m_interfaceList);
    interfaceLayout->setContentsMargins(0, 0, 0, 0);
    interfaceLayout->setSpacing(4);
    root->addWidget(m_interfaceList);

    auto* interfacesNote = new QLabel(
        Lang::tr("Cada interfaz guarda su propio ecualizador y efectos, y al pasar de una "
                 "a otra se cargan solos. Una interfaz aparece aqui la primera vez que la "
                 "eliges en \"Sacar procesado por\" con la captura encendida."),
        this);
    interfacesNote->setObjectName(QStringLiteral("statusText"));
    interfacesNote->setFont(Theme::uiFont(8));
    interfacesNote->setWordWrap(true);
    root->addWidget(interfacesNote);

    refreshInterfaces();

    // --------------------------------------------------------- subtitulos
    buildSubtitleSection(root);

    // ------------------------------------------------------------- idioma
    root->addSpacing(6);
    root->addWidget(sectionTitle(Lang::tr("IDIOMA"), this));

    auto* languageRow = new QHBoxLayout;
    languageRow->setSpacing(8);

    m_language = new QComboBox(this);
    m_language->setFont(Theme::uiFont(9));
    for (const Lang::Entry& entry : Lang::available())
        m_language->addItem(entry.nativeName, entry.code);
    m_language->setCurrentIndex(
        qMax(0, m_language->findData(Lang::code(Lang::current()))));
    m_language->setFixedWidth(160);
    languageRow->addWidget(m_language);

    m_restart = new FlatButton(Lang::tr("Reiniciar ahora"), this);
    m_restart->setIconId(Icons::Refresh);
    m_restart->setGlyphSize(14);
    m_restart->setFixedHeight(26);
    m_restart->setEnabled(false);
    languageRow->addWidget(m_restart);
    languageRow->addStretch(1);
    root->addLayout(languageRow);

    connect(m_restart, &FlatButton::clicked, this, &SettingsDialog::restartRequested);

    m_languageNote = new QLabel(
        Lang::tr("El idioma se aplica al reiniciar el reproductor."), this);
    m_languageNote->setObjectName(QStringLiteral("statusText"));
    m_languageNote->setFont(Theme::uiFont(8));
    m_languageNote->setWordWrap(true);
    root->addWidget(m_languageNote);

    connect(m_language, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString code = m_language->itemData(index).toString();
        Settings::setLanguage(code);
        // El boton solo se ofrece si de verdad hace falta reiniciar.
        m_restart->setEnabled(code != Lang::code(Lang::current()));
    });

    // ------------------------------------------------- atajos de teclado
    root->addSpacing(6);
    m_shortcutsTitle = sectionTitle(Lang::tr("ATAJOS DE TECLADO"), this);
    root->addWidget(m_shortcutsTitle);

    auto* shortcutsNote = new QLabel(
        Lang::tr("Haz clic en un atajo y pulsa la combinacion nueva; la X lo deja sin "
                 "tecla. Funcionan en cualquier ventana del programa, paneles flotantes "
                 "incluidos."), this);
    shortcutsNote->setObjectName(QStringLiteral("statusText"));
    shortcutsNote->setFont(Theme::uiFont(8));
    shortcutsNote->setWordWrap(true);
    root->addWidget(shortcutsNote);

    auto* keysGrid = new QGridLayout;
    keysGrid->setHorizontalSpacing(8);
    keysGrid->setVerticalSpacing(4);
    keysGrid->setColumnStretch(0, 1);

    QString lastGroup;
    int row = 0;
    m_keyEdits.resize(Commands::Count);
    for (int command = 0; command < Commands::Count; ++command) {
        const Commands::Info& info = Commands::info(command);

        // Un rotulo por grupo, como en el menu.
        const QString group = QString::fromLatin1(info.group);
        if (group != lastGroup) {
            lastGroup = group;
            auto* header = new QLabel(Lang::tr(info.group), this);
            header->setObjectName(QStringLiteral("detailKey"));
            header->setFont(Theme::uiFont(8, QFont::DemiBold));
            keysGrid->addWidget(header, row++, 0, 1, 3);
        }

        auto* label = new QLabel(Lang::tr(info.label), this);
        label->setFont(Theme::uiFont(9));
        keysGrid->addWidget(label, row, 0);

        auto* edit = new QKeySequenceEdit(Commands::keys(command), this);
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        edit->setFixedWidth(170);
        edit->setFont(Theme::uiFont(9));
        keysGrid->addWidget(edit, row, 1);
        m_keyEdits[command] = edit;
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this,
                [this, command](const QKeySequence& keys) { assignShortcut(command, keys); });

        auto* reset = new FlatButton(Icons::Revert, this);
        reset->setFixedDiameter(24);
        reset->setGlyphSize(13);
        reset->setToolTip(Lang::tr("Volver al atajo de fabrica"));
        keysGrid->addWidget(reset, row, 2);
        connect(reset, &FlatButton::clicked, this, [this, command]() {
            Commands::resetKeys(command);
            assignShortcut(command, Commands::keys(command));
        });

        ++row;
    }
    root->addLayout(keysGrid);

    m_shortcutsStatus = new QLabel(this);
    m_shortcutsStatus->setObjectName(QStringLiteral("statusText"));
    m_shortcutsStatus->setFont(Theme::uiFont(8));
    m_shortcutsStatus->setWordWrap(true);
    root->addWidget(m_shortcutsStatus);

    auto* resetAllRow = new QHBoxLayout;
    resetAllRow->addStretch(1);
    auto* resetAll = new FlatButton(Lang::tr("Restablecer todos los atajos"), this);
    resetAll->setIconId(Icons::Revert);
    resetAll->setGlyphSize(14);
    resetAll->setFixedHeight(26);
    resetAllRow->addWidget(resetAll);
    root->addLayout(resetAllRow);
    connect(resetAll, &FlatButton::clicked, this, [this]() {
        for (int command = 0; command < Commands::Count; ++command) {
            Commands::resetKeys(command);
            QSignalBlocker blocker(m_keyEdits[command]);
            m_keyEdits[command]->setKeySequence(Commands::keys(command));
        }
        m_shortcutsStatus->setText(Lang::tr("Atajos de fabrica restablecidos."));
        emit shortcutsChanged();
    });

    root->addStretch(1);
    scroll->setWidget(content);
    m_scroll = scroll;

    // -------------------------------------------------------------- cerrar
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(20, 8, 20, 14);
    footer->addStretch(1);

    auto* closeButton = new FlatButton(Lang::tr("Cerrar"), this);
    closeButton->setFixedHeight(26);
    footer->addWidget(closeButton);
    connect(closeButton, &FlatButton::clicked, this, &QDialog::accept);

    outer->addLayout(footer);

    refreshImageState();

    // La rueda del raton desplaza la ventana. Sin esto, al pasar por encima
    // de un desplegable o un deslizador mientras se baja, Qt le cambiaba el
    // valor sin querer (la visualizacion, el dispositivo de audio...). Solo
    // responden a la rueda despues de hacer clic en ellos.
    const auto guardWheel = [this](QWidget* widget) {
        widget->setFocusPolicy(Qt::StrongFocus);
        widget->installEventFilter(this);
    };
    for (QComboBox* combo : findChildren<QComboBox*>())
        guardWheel(combo);
    for (QSlider* slider : findChildren<QSlider*>())
        guardWheel(slider);

    // Placeholder de los atajos vacios en el idioma del programa (el de Qt
    // viene en ingles).
    for (QKeySequenceEdit* edit : std::as_const(m_keyEdits)) {
        if (auto* line = edit->findChild<QLineEdit*>())
            line->setPlaceholderText(Lang::tr("Sin atajo"));
    }

    // Tamano inicial: el que pide el contenido, sin pasar del 85 % del alto
    // disponible. Lo que no quepa se alcanza desplazando.
    const QScreen* screen = parentWidget() ? parentWidget()->screen()
                                           : QGuiApplication::primaryScreen();
    const int maxHeight = screen ? int(screen->availableGeometry().height() * 0.85) : 700;
    const QSize wanted = content->sizeHint();
    resize(wanted.width() + scroll->verticalScrollBar()->sizeHint().width() + 4,
           qMin(wanted.height() + 50, maxHeight));
}

void SettingsDialog::refreshSwatches()
{
    const QString active = Theme::currentPaletteId();
    for (ThemeSwatch* swatch : std::as_const(m_swatches))
        swatch->setSelected(swatch->paletteId() == active);
}

void SettingsDialog::onThemePicked(const QString& paletteId)
{
    emit themeChanged(paletteId);
    refreshSwatches();

    // Las tarjetas dibujan con los colores del tema recien aplicado.
    for (ThemeSwatch* swatch : std::as_const(m_swatches))
        swatch->update();
}

void SettingsDialog::refreshImageState()
{
    const QString path = Settings::backgroundImage();
    const bool has = !path.isEmpty();

    m_imageLabel->setText(has
        ? m_imageLabel->fontMetrics().elidedText(path, Qt::ElideMiddle, 380)
        : Lang::tr("Sin imagen de fondo"));
    m_imageLabel->setToolTip(has ? path : QString());

    m_clearImage->setEnabled(has);
    m_fitMode->setEnabled(has);
    m_transparency->setEnabled(has);
    m_darkening->setEnabled(has);
}

void SettingsDialog::refreshSystemDevices()
{
    const QList<SystemAudioTap::Device> outputs = SystemAudioTap::outputDevices();

    // Lo que ya estaba guardado manda sobre la sugerencia, salvo que no sirva.
    QByteArray source = Settings::systemAudioSource();
    QByteArray output = Settings::systemAudioOutput();
    if (source.isEmpty() || source == output)
        SystemAudioTap::suggestRouting(&source, &output);

    const auto fill = [&](QComboBox* combo, const QByteArray& current) {
        // Rellenar no es elegir: no se avisa a nadie.
        QSignalBlocker blocker(combo);
        combo->clear();
        for (const SystemAudioTap::Device& device : outputs) {
            combo->addItem(device.isDefault
                               ? Lang::tr("%1  (predeterminada)").arg(device.name)
                               : device.name,
                           device.id);
        }

        // Una interfaz guardada que ahora esta desenchufada se sigue
        // mostrando. Si no, el desplegable saltaria a otra salida y, al tocar
        // la casilla, se perderia la eleccion.
        int index = combo->findData(current);
        if (index < 0 && !current.isEmpty()) {
            combo->addItem(Lang::tr("Salida guardada (no conectada)"), current);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(qMax(0, index));
    };
    fill(m_systemSource, source);
    fill(m_systemOutput, output);

    const bool enough = outputs.size() >= 2;
    m_systemAudio->setEnabled(enough);
    m_systemAudio->setToolTip(enough
        ? QString()
        : Lang::tr("Hace falta mas de una salida de audio para poder capturar por una y "
                   "reproducir por otra."));

    refreshInterfaces();
}

void SettingsDialog::refreshInterfaces()
{
    if (!m_interfaceList)
        return;   // todavia construyendose

    // Con deleteLater: esto puede venir del clic de un boton de la propia
    // lista, que no se puede destruir mientras atiende su senal.
    QLayout* layout = m_interfaceList->layout();
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    const QList<SystemAudioTap::Device> outputs = SystemAudioTap::outputDevices();
    const QByteArray current = Settings::systemAudioOutput();
    const QList<QByteArray> known = Settings::knownOutputs();

    if (known.isEmpty()) {
        auto* empty = new QLabel(Lang::tr("Todavia no hay interfaces guardadas."), m_interfaceList);
        empty->setObjectName(QStringLiteral("detailKey"));
        empty->setFont(Theme::uiFont(9));
        layout->addWidget(empty);
        return;
    }

    for (const QByteArray& id : known) {
        QString name;
        bool connected = false;
        for (const SystemAudioTap::Device& device : outputs) {
            if (device.id == id) {
                name = device.name;
                connected = true;
                break;
            }
        }
        // El nombre se apunta mientras esta conectada: desenchufada, Windows
        // ya no lo da.
        if (connected)
            Settings::setOutputName(id, name);
        else
            name = Settings::outputName(id);
        if (name.isEmpty())
            name = Lang::tr("Interfaz sin nombre");

        const bool inUse = id == current;

        auto* row = new QWidget(m_interfaceList);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto* label = new QLabel(name, row);
        label->setFont(Theme::uiFont(9));
        label->setEnabled(connected);
        rowLayout->addWidget(label, 1);

        auto* state = new QLabel(inUse      ? Lang::tr("En uso")
                                 : connected ? Lang::tr("Conectada")
                                             : Lang::tr("No conectada"), row);
        state->setObjectName(QStringLiteral("detailKey"));
        state->setFont(Theme::uiFont(8, inUse ? QFont::DemiBold : QFont::Normal));
        if (inUse)
            state->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::AccentBright.name()));
        rowLayout->addWidget(state);

        auto* use = new FlatButton(Lang::tr("Usar"), row);
        use->setFixedHeight(24);
        use->setEnabled(connected && !inUse);
        rowLayout->addWidget(use);
        // Se elige en el desplegable de arriba: asi pasa por el mismo camino
        // que una eleccion a mano y MainWindow cambia el perfil.
        connect(use, &FlatButton::clicked, this, [this, id]() {
            const int index = m_systemOutput->findData(id);
            if (index >= 0)
                m_systemOutput->setCurrentIndex(index);
        });

        auto* forget = new FlatButton(Lang::tr("Olvidar"), row);
        forget->setFixedHeight(24);
        forget->setToolTip(Lang::tr("Borra su ecualizador y efectos guardados y deja de "
                                    "cambiar a ella sola."));
        rowLayout->addWidget(forget);
        connect(forget, &FlatButton::clicked, this, [this, id]() {
            Settings::forgetOutput(id);
            refreshInterfaces();
        });

        layout->addWidget(row);
    }
}

void SettingsDialog::assignShortcut(int command, const QKeySequence& keys)
{
    // Dos ordenes con la misma tecla no pueden convivir: la que la tenia se
    // queda sin ella, y se dice cual para que no desaparezca sin explicacion.
    const int other = Commands::commandUsing(keys, command);
    if (other >= 0) {
        Commands::setKeys(other, QKeySequence());
        QSignalBlocker blocker(m_keyEdits[other]);
        m_keyEdits[other]->clear();
        m_shortcutsStatus->setText(
            Lang::tr("%1 usaba %2 y se ha quedado sin atajo.")
                .arg(Lang::tr(Commands::info(other).label),
                     keys.toString(QKeySequence::NativeText)));
    } else {
        m_shortcutsStatus->clear();
    }

    // El de fabrica se guarda como "sin tocar": si cambia en otra version,
    // el usuario recibe el nuevo.
    if (keys == Commands::info(command).defaultKeys)
        Commands::resetKeys(command);
    else
        Commands::setKeys(command, keys);

    if (m_keyEdits[command]->keySequence() != keys) {
        QSignalBlocker blocker(m_keyEdits[command]);
        m_keyEdits[command]->setKeySequence(keys);
    }
    emit shortcutsChanged();
}

bool SettingsDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget && !widget->hasFocus() && m_scroll) {
            // Se le pasa al area desplazable, como si no hubiera control.
            QCoreApplication::sendEvent(m_scroll->viewport(), event);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SettingsDialog::scrollToSection(QWidget* title)
{
    if (m_scroll && title) {
        // En diferido: recien abierta, la ventana aun no ha repartido alturas.
        QTimer::singleShot(0, this, [this, title]() {
            m_scroll->verticalScrollBar()->setValue(title->y() - 8);
        });
    }
}

void SettingsDialog::showShortcuts()
{
    scrollToSection(m_shortcutsTitle);
}

void SettingsDialog::showSubtitleStyle()
{
    scrollToSection(m_subtitleTitle);
}

void SettingsDialog::buildSubtitleSection(QVBoxLayout* root)
{
    root->addSpacing(6);
    m_subtitleTitle = sectionTitle(Lang::tr("SUBTITULOS"), this);
    root->addWidget(m_subtitleTitle);

    m_subtitleStyle = SubtitleStyle::load();
    m_subtitlePreview = new SubtitlePreview(this);
    m_subtitlePreview->setStyle(m_subtitleStyle);
    root->addWidget(m_subtitlePreview);

    // Cada cambio se guarda, se ve en la vista previa y llega al video al
    // momento: no hay boton de aplicar.
    const auto apply = [this]() {
        m_subtitleStyle.save();
        m_subtitlePreview->setStyle(m_subtitleStyle);
        emit subtitleStyleChanged();
    };

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);
    int row = 0;

    const auto addRow = [&](const QString& text, QLayout* controls) {
        auto* label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("detailKey"));
        label->setFont(Theme::uiFont(9));
        label->setFixedWidth(170);
        grid->addWidget(label, row, 0);
        grid->addLayout(controls, row, 1);
        ++row;
    };
    const auto hbox = []() {
        auto* box = new QHBoxLayout;
        box->setSpacing(8);
        return box;
    };
    // Deslizador con su valor al lado; `suffix` va detras del numero.
    const auto makeSlider = [this](int minimum, int maximum, int value, const QString& suffix,
                                   QHBoxLayout* box, auto onChange) {
        auto* slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(minimum, maximum);
        slider->setValue(value);
        slider->setFixedWidth(240);
        auto* readout = new QLabel(QString::number(value) + suffix, this);
        readout->setFont(Theme::uiFont(9));
        readout->setFixedWidth(46);
        box->addWidget(slider);
        box->addWidget(readout);
        box->addStretch(1);
        connect(slider, &QSlider::valueChanged, this, [readout, suffix, onChange](int v) {
            readout->setText(QString::number(v) + suffix);
            onChange(v);
        });
        return slider;
    };
    const auto makeColor = [this](const QColor& color, const QString& title, auto onChange) {
        auto* button = new ColorButton(color, this);
        connect(button, &ColorButton::clicked, this, [this, button, title, onChange]() {
            const QColor chosen = QColorDialog::getColor(button->color(), this, title);
            if (chosen.isValid()) {
                button->setColor(chosen);
                onChange(chosen);
            }
        });
        return button;
    };

    // --- letra -----------------------------------------------------------------
    QHBoxLayout* fontRow = hbox();
    auto* family = new QFontComboBox(this);
    family->setFont(Theme::uiFont(9));
    family->setFixedWidth(240);
    family->setCurrentFont(m_subtitleStyle.family.isEmpty() ? Theme::uiFont(10)
                                                            : QFont(m_subtitleStyle.family));
    auto* bold = new QCheckBox(Lang::tr("Negrita"), this);
    bold->setFont(Theme::uiFont(9));
    bold->setChecked(m_subtitleStyle.bold);
    fontRow->addWidget(family);
    fontRow->addWidget(bold);
    fontRow->addStretch(1);
    addRow(Lang::tr("Fuente"), fontRow);
    connect(family, &QFontComboBox::currentFontChanged, this, [this, apply](const QFont& f) {
        m_subtitleStyle.family = f.family();
        apply();
    });
    connect(bold, &QCheckBox::toggled, this, [this, apply](bool on) {
        m_subtitleStyle.bold = on;
        apply();
    });

    QHBoxLayout* sizeRow = hbox();
    QSlider* size = makeSlider(2, 12, m_subtitleStyle.sizePercent, QStringLiteral(" %"), sizeRow,
                               [this, apply](int v) { m_subtitleStyle.sizePercent = v; apply(); });
    addRow(Lang::tr("Tamano (del alto de la imagen)"), sizeRow);

    QHBoxLayout* colorRow = hbox();
    ColorButton* textColor = makeColor(m_subtitleStyle.textColor, Lang::tr("Color de la letra"),
        [this, apply](const QColor& c) { m_subtitleStyle.textColor = c; apply(); });
    colorRow->addWidget(textColor);
    colorRow->addStretch(1);
    addRow(Lang::tr("Color de la letra"), colorRow);

    // --- contraste: contorno ---------------------------------------------------
    QHBoxLayout* outlineRow = hbox();
    QSlider* outline = makeSlider(0, 10, m_subtitleStyle.outline, QString(), outlineRow,
                                  [this, apply](int v) { m_subtitleStyle.outline = v; apply(); });
    ColorButton* outlineColor = makeColor(m_subtitleStyle.outlineColor, Lang::tr("Color del contorno"),
        [this, apply](const QColor& c) { m_subtitleStyle.outlineColor = c; apply(); });
    outlineRow->insertWidget(outlineRow->count() - 1, outlineColor);
    addRow(Lang::tr("Contorno (contraste)"), outlineRow);

    // --- posicion ----------------------------------------------------------------
    QHBoxLayout* positionRow = hbox();
    QSlider* position = makeSlider(0, 85, m_subtitleStyle.position, QStringLiteral(" %"), positionRow,
                                   [this, apply](int v) { m_subtitleStyle.position = v; apply(); });
    addRow(Lang::tr("Altura (desde abajo)"), positionRow);

    QHBoxLayout* alignRow = hbox();
    auto* align = new QComboBox(this);
    align->setFont(Theme::uiFont(9));
    align->setFixedWidth(160);
    align->addItem(Lang::tr("Izquierda"), int(SubtitleStyle::AlignLeft));
    align->addItem(Lang::tr("Centro"), int(SubtitleStyle::AlignCenter));
    align->addItem(Lang::tr("Derecha"), int(SubtitleStyle::AlignRight));
    align->setCurrentIndex(std::clamp(m_subtitleStyle.alignment, 0, 2));
    alignRow->addWidget(align);
    alignRow->addStretch(1);
    addRow(Lang::tr("Alineacion"), alignRow);
    connect(align, &QComboBox::currentIndexChanged, this, [this, apply](int index) {
        m_subtitleStyle.alignment = index;
        apply();
    });

    // --- fondo detras del texto ------------------------------------------------
    QHBoxLayout* bgRow = hbox();
    auto* background = new QCheckBox(Lang::tr("Poner los subtitulos sobre un fondo"), this);
    background->setFont(Theme::uiFont(9));
    background->setChecked(m_subtitleStyle.background);
    bgRow->addWidget(background);
    bgRow->addStretch(1);
    addRow(Lang::tr("Fondo"), bgRow);

    QHBoxLayout* shapeRow = hbox();
    auto* shape = new QComboBox(this);
    shape->setFont(Theme::uiFont(9));
    shape->setFixedWidth(240);
    for (int i = 0; i < SubtitleStyle::ShapeCount; ++i)
        shape->addItem(SubtitleStyle::shapeName(i), i);
    shape->setCurrentIndex(std::clamp(m_subtitleStyle.shape, 0, SubtitleStyle::ShapeCount - 1));
    ColorButton* bgColor = makeColor(m_subtitleStyle.backgroundColor, Lang::tr("Color del fondo"),
        [this, apply](const QColor& c) { m_subtitleStyle.backgroundColor = c; apply(); });
    shapeRow->addWidget(shape);
    shapeRow->addWidget(bgColor);
    shapeRow->addStretch(1);
    addRow(Lang::tr("Forma y color del fondo"), shapeRow);
    connect(shape, &QComboBox::currentIndexChanged, this, [this, apply](int index) {
        m_subtitleStyle.shape = index;
        apply();
    });

    QHBoxLayout* opacityRow = hbox();
    QSlider* opacity = makeSlider(0, 100, m_subtitleStyle.backgroundOpacity, QStringLiteral(" %"),
                                  opacityRow,
                                  [this, apply](int v) { m_subtitleStyle.backgroundOpacity = v; apply(); });
    addRow(Lang::tr("Opacidad del fondo"), opacityRow);

    // Sin fondo, sus ajustes no hacen nada: se apagan para que se note.
    const auto refreshBackground = [shape, bgColor, opacity](bool on) {
        shape->setEnabled(on);
        bgColor->setEnabled(on);
        opacity->setEnabled(on);
    };
    refreshBackground(m_subtitleStyle.background);
    connect(background, &QCheckBox::toggled, this, [this, apply, refreshBackground](bool on) {
        m_subtitleStyle.background = on;
        refreshBackground(on);
        apply();
    });

    root->addLayout(grid);

    // --- restablecer -----------------------------------------------------------
    auto* resetRow = new QHBoxLayout;
    resetRow->addStretch(1);
    auto* reset = new FlatButton(Lang::tr("Restablecer el estilo"), this);
    reset->setIconId(Icons::Revert);
    reset->setGlyphSize(14);
    reset->setFixedHeight(26);
    resetRow->addWidget(reset);
    root->addLayout(resetRow);

    connect(reset, &FlatButton::clicked, this,
            [=]() {
        const SubtitleStyle defaults;
        // Los controles se mueven sin avisar y se aplica una sola vez al final.
        {
            const QSignalBlocker b1(family), b2(bold), b3(size), b4(outline), b5(position),
                                 b6(align), b7(background), b8(shape), b9(opacity);
            family->setCurrentFont(Theme::uiFont(10));
            bold->setChecked(defaults.bold);
            size->setValue(defaults.sizePercent);
            outline->setValue(defaults.outline);
            position->setValue(defaults.position);
            align->setCurrentIndex(defaults.alignment);
            background->setChecked(defaults.background);
            shape->setCurrentIndex(defaults.shape);
            opacity->setValue(defaults.backgroundOpacity);
        }
        textColor->setColor(defaults.textColor);
        outlineColor->setColor(defaults.outlineColor);
        bgColor->setColor(defaults.backgroundColor);
        refreshBackground(defaults.background);
        m_subtitleStyle = defaults;
        apply();
    });
}

void SettingsDialog::showEvent(QShowEvent* event)
{
    // La ventana se reutiliza: lo que se conecto mientras estaba cerrada
    // tiene que aparecer al volver a abrirla.
    refreshSystemDevices();

    // Y se abre siempre desde arriba, la dejaras donde la dejaras.
    // showShortcuts() la baja despues, si es eso lo que se pidio.
    if (m_scroll)
        m_scroll->verticalScrollBar()->setValue(0);
    QDialog::showEvent(event);
}

void SettingsDialog::setSystemAudioStatus(const QString& message, bool ok)
{
    if (message.isEmpty())
        return;

    m_systemAudioStatus->setText(message);
    m_systemAudioStatus->setStyleSheet(
        ok ? QString() : QStringLiteral("color: %1;").arg(Theme::Danger.name()));

    if (!ok) {
        QSignalBlocker blocker(m_systemAudio);
        m_systemAudio->setChecked(false);
    }
}

void SettingsDialog::chooseImage()
{
    const QString start = Settings::backgroundImage().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        : QFileInfo(Settings::backgroundImage()).absolutePath();

    const QString file = QFileDialog::getOpenFileName(
        this, Lang::tr("Elegir imagen de fondo"), start,
        Lang::tr("Imagenes (*.jpg *.jpeg *.png *.bmp *.webp *.gif);;Todos los archivos (*)"));

    if (file.isEmpty())
        return;

    emit backgroundImageChanged(file);
    refreshImageState();
}

void SettingsDialog::clearImage()
{
    emit backgroundImageChanged(QString());
    refreshImageState();
}

#include "SettingsDialog.moc"
