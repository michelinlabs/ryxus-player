#include "ui/SettingsDialog.h"

#include "core/Lang.h"
#include "core/Settings.h"
#include "core/SystemAudioTap.h"
#include "ui/Visualizations.h"
#include "ui/BackgroundHost.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QProcess>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QSlider>
#include <QStandardPaths>
#include <QSignalBlocker>
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
    auto* root = new QVBoxLayout(this);
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

    const QList<SystemAudioTap::Device> outputs = SystemAudioTap::outputDevices();

    // El reparto correcto lo decide SystemAudioTap: capturar del cable virtual
    // y sacar por unos altavoces de verdad. Si no hay cable, devuelve vacio.
    QByteArray suggestedSource, suggestedOutput;
    SystemAudioTap::suggestRouting(&suggestedSource, &suggestedOutput);
    const bool haveCable = !suggestedSource.isEmpty();

    // Lo que ya estaba guardado manda sobre la sugerencia, salvo que no sirva.
    QByteArray sourceDefault = Settings::systemAudioSource();
    QByteArray outputDefault = Settings::systemAudioOutput();
    if (sourceDefault.isEmpty() || sourceDefault == outputDefault) {
        sourceDefault = suggestedSource;
        outputDefault = suggestedOutput;
    }

    const auto addDeviceRow = [&](const QString& label, QComboBox*& combo,
                                  const QByteArray& current) {
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
        for (const SystemAudioTap::Device& device : outputs) {
            combo->addItem(device.isDefault
                               ? Lang::tr("%1  (predeterminada)").arg(device.name)
                               : device.name,
                           device.id);
        }
        const int index = combo->findData(current);
        combo->setCurrentIndex(index >= 0 ? index : 0);
        row->addWidget(combo, 1);

        root->addLayout(row);
    };

    addDeviceRow(Lang::tr("Capturar de"), m_systemSource, sourceDefault);
    addDeviceRow(Lang::tr("Sacar procesado por"), m_systemOutput, outputDefault);

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

    if (outputs.size() < 2) {
        m_systemAudio->setEnabled(false);
        m_systemAudio->setToolTip(Lang::tr(
            "Hace falta mas de una salida de audio para poder capturar por una y "
            "reproducir por otra."));
    }

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

    // -------------------------------------------------------------- cerrar
    root->addSpacing(4);
    auto* footer = new QHBoxLayout;
    footer->addStretch(1);

    auto* closeButton = new FlatButton(Lang::tr("Cerrar"), this);
    closeButton->setFixedHeight(26);
    footer->addWidget(closeButton);
    connect(closeButton, &FlatButton::clicked, this, &QDialog::accept);

    root->addLayout(footer);

    refreshImageState();
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
