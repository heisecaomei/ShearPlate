#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

// 应用设置：保留时长存于 QSettings；开机自启动写入 HKCU Run 键。
namespace AppSettings {

inline int retentionHours()
{
    QSettings s;
    return s.value(QStringLiteral("retentionHours"), 12).toInt();
}

inline void setRetentionHours(int hours)
{
    QSettings s;
    s.setValue(QStringLiteral("retentionHours"), hours);
}

inline QString hotkey()
{
    QSettings s;
    return s.value(QStringLiteral("hotkey"), QStringLiteral("Ctrl+Shift+V")).toString();
}

inline void setHotkey(const QString &sequence)
{
    QSettings s;
    s.setValue(QStringLiteral("hotkey"), sequence);
}

inline bool clickToTop()
{
    QSettings s;
    return s.value(QStringLiteral("clickToTop"), true).toBool();
}

inline void setClickToTop(bool enabled)
{
    QSettings s;
    s.setValue(QStringLiteral("clickToTop"), enabled);
}

inline bool textShowIcon()
{
    QSettings s;
    return s.value(QStringLiteral("textShowIcon"), true).toBool();
}

inline void setTextShowIcon(bool enabled)
{
    QSettings s;
    s.setValue(QStringLiteral("textShowIcon"), enabled);
}

// 主题模式：0=跟随系统 1=浅色 2=深色
inline int themeMode()
{
    QSettings s;
    return s.value(QStringLiteral("themeMode"), 0).toInt();
}

inline void setThemeMode(int mode)
{
    QSettings s;
    s.setValue(QStringLiteral("themeMode"), mode);
}

// 毛玻璃（半透明）效果开关
inline bool glassEffect()
{
    QSettings s;
    return s.value(QStringLiteral("glassEffect"), true).toBool();
}

inline void setGlassEffect(bool enabled)
{
    QSettings s;
    s.setValue(QStringLiteral("glassEffect"), enabled);
}

inline bool autoStart()
{
    QSettings s(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                QSettings::NativeFormat);
    return s.contains(QStringLiteral("ShearPlate"));
}

// 敏感内容过滤：启用后本程序记录/写入时附带排除系统剪贴板历史/云剪贴板的标记
inline bool sensitiveFilter()
{
    QSettings s;
    return s.value(QStringLiteral("sensitiveFilter"), true).toBool();
}

inline void setSensitiveFilter(bool enabled)
{
    QSettings s;
    s.setValue(QStringLiteral("sensitiveFilter"), enabled);
}

inline void setAutoStart(bool enabled)
{
    QSettings s(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                QSettings::NativeFormat);
    if (enabled) {
        s.setValue(QStringLiteral("ShearPlate"),
                   QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        s.remove(QStringLiteral("ShearPlate"));
    }
}

} // namespace AppSettings
