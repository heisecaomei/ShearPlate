#include "HoverIconButton.h"

HoverIconButton::HoverIconButton(const QIcon &normal, const QIcon &hover, QWidget *parent)
    : QToolButton(parent)
    , m_normal(normal)
    , m_hover(hover)
{
    setIcon(m_normal);
    setIconSize(QSize(16, 16));
    setAutoRaise(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    // 悬停/按下只切换图标，不改变背景
    setStyleSheet(QStringLiteral(
        "QToolButton{background:transparent;border:none;}"
        "QToolButton:hover{background:transparent;border:none;}"
        "QToolButton:pressed{background:transparent;border:none;}"));
}

void HoverIconButton::setIcons(const QIcon &normal, const QIcon &hover)
{
    m_normal = normal;
    m_hover = hover;
    setIcon(m_normal);
}

void HoverIconButton::enterEvent(QEnterEvent *event)
{
    setIcon(m_hover);
    QToolButton::enterEvent(event);
}

void HoverIconButton::leaveEvent(QEvent *event)
{
    setIcon(m_normal);
    QToolButton::leaveEvent(event);
}
