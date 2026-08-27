#pragma once

#include <QImage>
#include <QPixmap>
#include <QString>
#include <QWidget>

// Widget central de la ventana. Pinta el color base del skin y, encima, la
// imagen de fondo elegida por el usuario. Los paneles que van sobre el se
// dibujan con alfa (ver Theme::setPanelTransparency), asi que la imagen se
// percibe a traves de toda la interfaz.
class BackgroundHost : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Cover, Fit, Stretch, Tile };
    Q_ENUM(Mode)

    explicit BackgroundHost(QWidget* parent = nullptr);

    bool    setImagePath(const QString& path);   // vacio = sin imagen
    QString imagePath() const { return m_path; }
    bool    hasImage() const  { return !m_source.isNull(); }

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // Velo oscuro sobre la imagen (0-90 %). Sin el, una foto clara deja el
    // texto de la interfaz ilegible.
    void setDarkening(int percent);
    int  darkening() const { return m_darkening; }

    static QString modeName(Mode mode);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildScaled();

    QString m_path;
    QImage  m_source;
    QPixmap m_scaled;
    Mode    m_mode      = Mode::Cover;
    int     m_darkening = 35;
};
