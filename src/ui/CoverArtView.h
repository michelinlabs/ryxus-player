#pragma once

#include <QImage>
#include <QPixmap>
#include <QWidget>

// Caratula del panel izquierdo: cuadrada, con borde fino, y editable
// (doble clic o menu contextual para reemplazar o quitar la imagen).
class CoverArtView : public QWidget {
    Q_OBJECT

public:
    explicit CoverArtView(QWidget* parent = nullptr);

    void setCover(const QImage& image);
    QImage cover() const { return m_cover; }
    bool   hasCover() const { return !m_cover.isNull(); }

    void setEditable(bool editable);

    int heightForWidth(int width) const override { return width; }
    bool hasHeightForWidth() const override { return true; }

signals:
    void changeRequested();
    void removeRequested();
    void exportRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void rebuildScaled();

    QImage  m_cover;
    QPixmap m_scaled;
    bool    m_editable = true;
    bool    m_hovered  = false;
};
