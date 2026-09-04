#pragma once

#include <QIcon>
#include <QToolButton>

// 鼠标悬停时切换图标的按钮（用于关闭按钮的浅/深过渡效果）。
class HoverIconButton : public QToolButton
{
    Q_OBJECT
public:
    HoverIconButton(const QIcon &normal, const QIcon &hover, QWidget *parent = nullptr);

    // 更新两个状态图标（主题切换后重新加载）
    void setIcons(const QIcon &normal, const QIcon &hover);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QIcon m_normal;
    QIcon m_hover;
};
