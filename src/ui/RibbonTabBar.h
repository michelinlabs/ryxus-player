#pragma once

#include <QStringList>
#include <QWidget>

// Franja de pestanas del skin: texto claro y subrayado violeta en la activa,
// texto atenuado en el resto, sin marcos ni fondo por pestana.
class RibbonTabBar : public QWidget {
    Q_OBJECT

public:
    explicit RibbonTabBar(QWidget* parent = nullptr);

    void setTabs(const QStringList& labels);
    void addTab(const QString& label);
    void removeTab(int index);
    void renameTab(int index, const QString& label);

    QString tabText(int index) const;
    int     count() const { return int(m_tabs.size()); }

    int  currentIndex() const { return m_current; }
    void setCurrentIndex(int index);

    // Boton "+" al final de la franja (panel de listas de reproduccion).
    void setShowAddButton(bool show);
    void setClosableTabs(bool closable);

    QSize sizeHint() const override;

signals:
    void currentChanged(int index);
    void addRequested();
    void closeRequested(int index);
    void tabContextMenuRequested(int index, const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Tab {
        QString label;
        QRect   rect;
        QRect   closeRect;
    };

    void relayout();
    int  tabAt(const QPoint& pos) const;

    QVector<Tab> m_tabs;
    QRect m_addRect;
    int   m_current  = 0;
    int   m_hovered  = -1;
    bool  m_showAdd  = false;
    bool  m_closable = false;
    bool  m_addHovered = false;
};
