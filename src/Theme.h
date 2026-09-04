#pragma once

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

#include "AppSettings.h"

// 主题配色集合（浅色 / 深色）
struct ThemeColors
{
    QColor panelBg;
    QColor panelBorder;
    QColor cardBg;
    QColor cardBorder;
    QColor cardBgHover;
    QColor cardBorderHover;
    QColor pinnedBg;
    QColor pinnedBorder;
    QColor textPrimary;
    QColor textSecondary;
    QColor badgeBg;
    QColor badgeText;
    QColor scrollbarHandle;
    QColor scrollbarHandleHover;
};

namespace Theme {

inline ThemeColors light()
{
    ThemeColors c;
    c.panelBg = QColor(0xff, 0xff, 0xff);
    c.panelBorder = QColor(0xd8, 0xdc, 0xe2);
    c.cardBg = QColor(0xfa, 0xfa, 0xfa);
    c.cardBorder = QColor(0xf8, 0xf8, 0xf8);
    c.cardBgHover = QColor(0xe9, 0xed, 0xf3);
    c.cardBorderHover = QColor(0xd5, 0xdb, 0xe3);
    c.pinnedBg = QColor(0xf0, 0xf4, 0xf8);
    c.pinnedBorder = QColor(0xd0, 0xdd, 0xeb);
    c.textPrimary = QColor(0x2b, 0x2b, 0x2b);
    c.textSecondary = QColor(0x9a, 0xa0, 0xa6);
    c.badgeBg = QColor(0xdf, 0xe6, 0xf0);
    c.badgeText = QColor(0x5a, 0x6b, 0x7e);
    c.scrollbarHandle = QColor(0x9a, 0xa3, 0xab);
    c.scrollbarHandleHover = QColor(0x7c, 0x86, 0x8f);
    return c;
}

inline ThemeColors dark()
{
    ThemeColors c;
    c.panelBg = QColor(0x1e, 0x1f, 0x22);
    c.panelBorder = QColor(0x3a, 0x3d, 0x42);
    c.cardBg = QColor(0x2a, 0x2c, 0x30);
    c.cardBorder = QColor(0x3a, 0x3d, 0x42);
    c.cardBgHover = QColor(0x34, 0x37, 0x3c);
    c.cardBorderHover = QColor(0x4a, 0x4e, 0x54);
    c.pinnedBg = QColor(0x21, 0x2b, 0x36);
    c.pinnedBorder = QColor(0x3b, 0x4c, 0x5f);
    c.textPrimary = QColor(0xe6, 0xe8, 0xea);
    c.textSecondary = QColor(0x8a, 0x90, 0x96);
    c.badgeBg = QColor(0x3a, 0x40, 0x48);
    c.badgeText = QColor(0xa8, 0xb4, 0xc0);
    c.scrollbarHandle = QColor(0x4a, 0x4e, 0x54);
    c.scrollbarHandleHover = QColor(0x5a, 0x5f, 0x66);
    return c;
}

// 系统当前是否为深色
inline bool isSystemDark()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

// 根据设置 + 系统，返回当前生效的主题配色
inline ThemeColors current()
{
    const int mode = AppSettings::themeMode();
    if (mode == 1)
        return light();
    if (mode == 2)
        return dark();
    return isSystemDark() ? dark() : light();
}

// 当前是否深色模式
inline bool isDark()
{
    const int mode = AppSettings::themeMode();
    if (mode == 1)
        return false;
    if (mode == 2)
        return true;
    return isSystemDark();
}

// 把主题应用到整个应用（QPalette），标准控件自动跟随
inline void applyToApp()
{
    const ThemeColors c = current();

    QPalette pal;
    pal.setColor(QPalette::Window, c.panelBg);
    pal.setColor(QPalette::WindowText, c.textPrimary);
    pal.setColor(QPalette::Base, c.panelBg);
    pal.setColor(QPalette::AlternateBase, c.cardBg);
    pal.setColor(QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::Button, c.cardBg);
    pal.setColor(QPalette::ButtonText, c.textPrimary);
    pal.setColor(QPalette::ToolTipBase, c.panelBg);
    pal.setColor(QPalette::ToolTipText, c.textPrimary);
    pal.setColor(QPalette::Highlight, c.cardBorderHover);
    pal.setColor(QPalette::HighlightedText, c.textPrimary);
    pal.setColor(QPalette::PlaceholderText, c.textSecondary);
    pal.setColor(QPalette::Disabled, QPalette::Text, c.textSecondary);
    qApp->setPalette(pal);

    // 菜单跟随主题：浅色菜单+深色文字 / 深色菜单+浅色文字
    qApp->setStyleSheet(QStringLiteral(
        "QMenu{background:%1;color:%2;border:1px solid %3;padding:4px;}"
        "QMenu::item{background:transparent;color:%2;padding:6px 24px 6px 18px;border-radius:4px;}"
        "QMenu::item:selected{background:%4;color:%2;}")
        .arg(c.panelBg.name(), c.textPrimary.name(), c.cardBorder.name(), c.cardBgHover.name()));
}

} // namespace Theme
