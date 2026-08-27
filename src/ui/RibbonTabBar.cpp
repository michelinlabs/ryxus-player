#include "ui/RibbonTabBar.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int kPaddingX   = 14;   // aire a cada lado del texto
constexpr int kFirstInset = 10;   // margen izquierdo de la franja
constexpr int kAddWidth   = 30;
constexpr int kCloseWidth = 16;
}

RibbonTabBar::RibbonTabBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::Metrics::TabStripHeight);
    setMouseTracking(true);
    setFont(Theme::uiFont(9));
}

void RibbonTabBar::setTabs(const QStringList& labels)
{
    m_tabs.clear();
    for (const QString& label : labels)
        m_tabs.append(Tab{label, QRect(), QRect()});
    m_current = m_tabs.isEmpty() ? -1 : qBound(0, m_current, int(m_tabs.size()) - 1);
    relayout();
    update();
}

void RibbonTabBar::addTab(const QString& label)
{
    m_tabs.append(Tab{label, QRect(), QRect()});
    relayout();
    update();
}

void RibbonTabBar::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    m_tabs.remove(index);
    if (m_current >= m_tabs.size())
        m_current = int(m_tabs.size()) - 1;
    relayout();
    update();
    emit currentChanged(m_current);
}

void RibbonTabBar::renameTab(int index, const QString& label)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    m_tabs[index].label = label;
    relayout();
    update();
}

QString RibbonTabBar::tabText(int index) const
{
    if (index < 0 || index >= m_tabs.size())
        return QString();
    return m_tabs.at(index).label;
}

void RibbonTabBar::setCurrentIndex(int index)
{
    if (index == m_current || index < 0 || index >= m_tabs.size())
        return;
    m_current = index;
    update();
    emit currentChanged(m_current);
}

void RibbonTabBar::setShowAddButton(bool show)
{
    m_showAdd = show;
    relayout();
    update();
}

void RibbonTabBar::setClosableTabs(bool closable)
{
    m_closable = closable;
    relayout();
    update();
}

QSize RibbonTabBar::sizeHint() const
{
    const QFontMetrics fm(font());
    int width = kFirstInset;
    for (const Tab& tab : m_tabs)
        width += fm.horizontalAdvance(tab.label) + kPaddingX * 2
               + (m_closable ? kCloseWidth : 0);
    if (m_showAdd)
        width += kAddWidth;
    return QSize(width, Theme::Metrics::TabStripHeight);
}

void RibbonTabBar::relayout()
{
    const QFontMetrics fm(font());
    int x = kFirstInset;

    for (Tab& tab : m_tabs) {
        const int extra = m_closable ? kCloseWidth : 0;
        const int width = fm.horizontalAdvance(tab.label) + kPaddingX * 2 + extra;
        tab.rect = QRect(x, 0, width, height());
        tab.closeRect = m_closable
            ? QRect(tab.rect.right() - kCloseWidth - 4, (height() - kCloseWidth) / 2,
                    kCloseWidth, kCloseWidth)
            : QRect();
        x += width;
    }

    m_addRect = m_showAdd ? QRect(x, 0, kAddWidth, height()) : QRect();
}

int RibbonTabBar::tabAt(const QPoint& pos) const
{
    for (int i = 0; i < m_tabs.size(); ++i)
        if (m_tabs.at(i).rect.contains(pos))
            return i;
    return -1;
}

void RibbonTabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint pos = event->pos();

    if (m_showAdd && m_addRect.contains(pos)) {
        emit addRequested();
        return;
    }

    const int index = tabAt(pos);
    if (index < 0)
        return;

    if (m_closable && m_tabs.at(index).closeRect.contains(pos)) {
        emit closeRequested(index);
        return;
    }

    setCurrentIndex(index);
}

void RibbonTabBar::mouseMoveEvent(QMouseEvent* event)
{
    const int hovered = tabAt(event->pos());
    const bool addHovered = m_showAdd && m_addRect.contains(event->pos());

    if (hovered != m_hovered || addHovered != m_addHovered) {
        m_hovered = hovered;
        m_addHovered = addHovered;
        setCursor((hovered >= 0 || addHovered) ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void RibbonTabBar::leaveEvent(QEvent* event)
{
    m_hovered = -1;
    m_addHovered = false;
    update();
    QWidget::leaveEvent(event);
}

void RibbonTabBar::contextMenuEvent(QContextMenuEvent* event)
{
    const int index = tabAt(event->pos());
    if (index >= 0)
        emit tabContextMenuRequested(index, event->globalPos());
}

void RibbonTabBar::paintEvent(QPaintEvent*)
{
    relayout();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Theme::chromeLightBg());

    for (int i = 0; i < m_tabs.size(); ++i) {
        const Tab& tab = m_tabs.at(i);
        const bool active = (i == m_current);

        QColor color = Theme::TextDim;
        if (active)
            color = Theme::Text;
        else if (i == m_hovered)
            color = Theme::Text.darker(115);

        QFont f = font();
        f.setWeight(active ? QFont::DemiBold : QFont::Normal);
        p.setFont(f);
        p.setPen(color);

        QRect textRect = tab.rect;
        if (m_closable)
            textRect.setRight(tab.closeRect.left() - 2);
        p.drawText(textRect, Qt::AlignCenter, tab.label);

        // Subrayado violeta de 2 px bajo la pestana activa.
        if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(Theme::AccentBright);
            p.drawRect(QRect(tab.rect.left() + 6, height() - 2,
                             tab.rect.width() - 12, 2));
        }

        if (m_closable && (i == m_hovered || active))
            Icons::paint(p, Icons::Close, tab.closeRect,
                         i == m_hovered ? Theme::Text : Theme::TextFaint, 1.3);
    }

    if (m_showAdd)
        Icons::paint(p, Icons::Plus,
                     QRectF(m_addRect).adjusted(7, 7, -7, -7),
                     m_addHovered ? Theme::Text : Theme::TextDim, 1.5);
}
