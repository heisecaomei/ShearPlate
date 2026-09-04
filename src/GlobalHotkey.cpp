#include "GlobalHotkey.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
}

bool GlobalHotkey::registerHotkey(const QKeySequence &sequence)
{
    unregisterHotkey();

    if (sequence.isEmpty())
        return true; // 未设置热键，视为成功（禁用）

    const int key = sequence[0].key();
    const Qt::KeyboardModifiers mods = sequence[0].keyboardModifiers();

#ifdef Q_OS_WIN
    quint32 mod = MOD_NOREPEAT;
    if (mods & Qt::AltModifier) mod |= MOD_ALT;
    if (mods & Qt::ControlModifier) mod |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier) mod |= MOD_SHIFT;
    if (mods & Qt::MetaModifier) mod |= MOD_WIN;

    quint32 vk = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        vk = quint32(key);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        vk = quint32(key);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        vk = VK_F1 + quint32(key - Qt::Key_F1);
    else {
        switch (key) {
        case Qt::Key_Space: vk = VK_SPACE; break;
        case Qt::Key_Tab: vk = VK_TAB; break;
        case Qt::Key_Return:
        case Qt::Key_Enter: vk = VK_RETURN; break;
        case Qt::Key_Backspace: vk = VK_BACK; break;
        case Qt::Key_Insert: vk = VK_INSERT; break;
        case Qt::Key_Delete: vk = VK_DELETE; break;
        case Qt::Key_Home: vk = VK_HOME; break;
        case Qt::Key_End: vk = VK_END; break;
        case Qt::Key_PageUp: vk = VK_PRIOR; break;
        case Qt::Key_PageDown: vk = VK_NEXT; break;
        case Qt::Key_Up: vk = VK_UP; break;
        case Qt::Key_Down: vk = VK_DOWN; break;
        case Qt::Key_Left: vk = VK_LEFT; break;
        case Qt::Key_Right: vk = VK_RIGHT; break;
        default: vk = 0; break;
        }
    }

    if (vk == 0)
        return false;

    if (!RegisterHotKey(nullptr, m_id, mod, vk))
        return false;
#else
    Q_UNUSED(key);
    Q_UNUSED(mods);
#endif

    m_sequence = sequence;
    return true;
}

void GlobalHotkey::unregisterHotkey()
{
    if (m_sequence.isEmpty())
        return;
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, m_id);
#endif
    m_sequence = QKeySequence();
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == m_id) {
        emit activated();
        return true;
    }
#else
    Q_UNUSED(message);
#endif
    return false;
}
