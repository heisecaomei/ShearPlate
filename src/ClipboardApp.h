#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

#include "ClipboardItem.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class ClipboardStore;
class ClipboardManager;
class ClipboardListModel;
class ClipboardPanel;
class TrayController;
class GlobalHotkey;
class QTimer;
class QLocalServer;

// 悬置粘贴状态
enum class PendingPasteState { Idle, WaitingForFocus };

// 应用总控：串联存储、监控、面板、托盘、单实例、全局热键与“等待焦点”粘贴状态机。
class ClipboardApp : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit ClipboardApp(QObject *parent = nullptr);
    ~ClipboardApp() override;

    // 返回 true 表示本进程是唯一实例；false 表示已有实例在运行（已通知其激活）
    bool acquireSingleInstance();

    // 处理 WM_CLIPBOARDUPDATE（剪贴板覆盖监听）
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void onClipCaptured(const ClipboardItem &item);
    void onPasteRequested(const ClipboardItem &item, bool cutAfterPaste);
    void onDeleteRequested(const QString &id);
    void onTogglePinRequested(const QString &id);
    void onOpenSettings();
    void onQuit();
    void onCleanupTimer();
    void onNewInstanceConnection();
    void onHotkeyActivated();
    void onFileCheckTimer();

private:
    // 当前焦点窗口是否可直接粘贴（存在、非本程序、非桌面/任务栏、有焦点控件）
    bool CanPasteNow() const;
    void BeginPendingPaste(const ClipboardItem &item, bool cutAfterPaste, quintptr targetWindow);
    void ClearPendingPaste();
    void OnPendingFocusChanged(quintptr hwnd);
    void OnPendingTimeout();
    void OnClipboardUpdated();
    bool ClipboardMatchesPending() const;

#ifdef Q_OS_WIN
    static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
#endif

    ClipboardStore *m_store = nullptr;
    ClipboardManager *m_manager = nullptr;
    ClipboardListModel *m_model = nullptr;
    ClipboardPanel *m_panel = nullptr;
    TrayController *m_tray = nullptr;
    GlobalHotkey *m_hotkey = nullptr;
    QTimer *m_cleanupTimer = nullptr;
    QTimer *m_foregroundTimer = nullptr; // 低频缓存最近有效前台窗口
    QTimer *m_fileCheckTimer = nullptr;
    QLocalServer *m_server = nullptr;
    class SettingsDialog *m_settingsDlg = nullptr; // 设置对话框单例
    quintptr m_lastUsableForeground = 0; // 最近一次有效的前台窗口（非面板、非系统窗口）

    // —— 悬置粘贴状态 ——
    PendingPasteState m_pendingState = PendingPasteState::Idle;
    ClipboardItem m_pendingItem;
    bool m_pendingDeleteAfterPaste = false;
    quintptr m_pendingTargetWindow = 0; // 等待焦点的目标窗口（只有它重新获得焦点才粘贴）
    QTimer *m_pendingTimer = nullptr;   // 30 秒超时
    HWINEVENTHOOK m_focusHook = nullptr;
    bool m_isSelfAction = false;        // 自身写剪贴板标志（抑制 WM_CLIPBOARDUPDATE）

    static ClipboardApp *s_self;
};
