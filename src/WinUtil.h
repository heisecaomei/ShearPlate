#pragma once

#include <QColor>
#include <QtGlobal>

namespace WinUtil {

// 当前前台窗口句柄（用于上屏前还原焦点）
quintptr currentForegroundWindow();

// 是否为有效的目标窗口（排除任务栏、桌面等系统窗口）
bool isUsableWindow(quintptr hwnd);

// 给窗口开启毛玻璃（acrylic）效果，tint 为背景 tint 色
void enableAcrylic(quintptr hwnd, const QColor &tint);

// 激活指定窗口（若最小化则先还原）
void activateWindow(quintptr hwnd);

// 向当前前台窗口发送 Ctrl+V
void simulateCtrlV();

} // namespace WinUtil
