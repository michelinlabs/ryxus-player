#include "ui/SettingsDialog.h"

#include "core/Lang.h"
#include "core/Settings.h"
#include "ui/BackgroundHost.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QStandardPaths>
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

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    const QVector<Theme::Palette>& list = Theme::palettes();
    for (int i = 0; i < list.size(); ++i) {
        auto* swatch = new ThemeSwatch(list.at(i), this);
        connect(swatch, &ThemeSwatch::picked, this, &SettingsDialog::onThemePicked);
        grid->addWidget(swatch, i / 4, i % 4);
        m_swatches.append(swatch);
    }
    root->addLayout(grid);
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

    auto* chooseButton = new FlatButton(Lang::tr("Elegir imagen..."), this);
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

    auto* bgNote = new QLabel(
        Lang::tr("La transparencia y el oscurecido solo actuan cuando hay una imagen de fondo."),
        this);
    bgNote->setObjectName(QStringLiteral("statusText"));
    bgNote->setFont(Theme::uiFont(8));
    bgNote->setWordWrap(true);
    root->addWidget(bgNote);

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
