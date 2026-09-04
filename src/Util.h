#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>

#include "Theme.h"

// 加载图标：优先读取 exe 同目录下的相对路径（便于用户不重新编译即可替换图片），
// 找不到时回退到编译进 qrc 的资源（":/images/..."）。
inline QString resolveImagePath(const QString &relative)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    candidates << appDir + QStringLiteral("/") + relative;
    candidates << relative; // 相对于当前工作目录

    for (const QString &p : candidates) {
        if (QFileInfo::exists(p))
            return QDir::cleanPath(p);
    }
    // 回退到内嵌资源
    return QStringLiteral(":/") + relative;
}

inline QIcon loadIcon(const QString &relative)
{
    return QIcon(resolveImagePath(relative));
}

inline QPixmap loadPixmap(const QString &relative, int w, int h)
{
    return loadIcon(relative).pixmap(QSize(w, h));
}

// 加载主题图标：根据当前浅色/深色模式，自动选择 light/ 或 dark/ 子目录
inline QIcon loadThemedIcon(const QString &name)
{
    const QString dir = Theme::isDark() ? QStringLiteral("dark") : QStringLiteral("light");
    return loadIcon(QStringLiteral("images/") + dir + QStringLiteral("/") + name);
}

inline QPixmap loadThemedPixmap(const QString &name, int w, int h)
{
    return loadThemedIcon(name).pixmap(QSize(w, h));
}
