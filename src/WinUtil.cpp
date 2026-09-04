#include "WinUtil.h"

#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace WinUtil {

quintptr currentForegroundWindow()
{
#ifdef Q_OS_WIN
    return reinterpret_cast<quintptr>(GetForegroundWindow());
#else
    return 0;
#endif
}

bool isUsableWindow(quintptr hwnd)
{
#ifdef Q_OS_WIN
    if (hwnd == 0)
        return false;
    HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h))
        return false;
    wchar_t cls[256] = {0};
    GetClassNameW(h, cls, 256);
    const QString c = QString::fromWCharArray(cls);
    return !(c.contains(QStringLiteral("Shell_TrayWnd"))
             || c.contains(QStringLiteral("Progman"))
             || c.contains(QStringLiteral("WorkerW")));
#else
    Q_UNUSED(hwnd)
    return false;
#endif
}

void activateWindow(quintptr hwnd)
{
#ifdef Q_OS_WIN
    if (hwnd == 0)
        return;
    HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h))
        return;
    if (IsIconic(h))
        ShowWindow(h, SW_RESTORE);

    // 用 AttachThreadInput 提高 SetForegroundWindow 成功率
    const HWND fg = GetForegroundWindow();
    const DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
    const DWORD targetThread = GetWindowThreadProcessId(h, nullptr);
    if (fgThread != targetThread && fgThread != 0 && targetThread != 0) {
        AttachThreadInput(fgThread, targetThread, TRUE);
        BringWindowToTop(h);
        SetForegroundWindow(h);
        AttachThreadInput(fgThread, targetThread, FALSE);
    } else {
        SetForegroundWindow(h);
    }
#else
    Q_UNUSED(hwnd)
#endif
}

void simulateCtrlV()
{
#ifdef Q_OS_WIN
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
#endif
}

void enableAcrylic(quintptr hwnd, const QColor &tint)
{
    // 说明：Windows 的 SetWindowCompositionAttribute（blur/acrylic）会作用到
    // 整个窗口矩形，导致无边框透明窗口的圆角外区域出现黑色填充或 tint 覆盖圆角。
    // 为保证圆角干净、无黑色背景，此处禁用系统模糊，仅保留 paintEvent 的半透明背景。
    Q_UNUSED(hwnd)
    Q_UNUSED(tint)
}

} // namespace WinUtil
