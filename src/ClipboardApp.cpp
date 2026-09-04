#include "ClipboardApp.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QKeySequence>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMimeData>
#include <QStyleHints>
#include <QTimer>
#include <QUrl>

#include "AppSettings.h"
#include "ClipboardListModel.h"
#include "ClipboardManager.h"
#include "ClipboardPanel.h"
#include "ClipboardStore.h"
#include "GlobalHotkey.h"
#include "SettingsDialog.h"
#include "Theme.h"
#include "TrayController.h"
#include "WinUtil.h"

ClipboardApp *ClipboardApp::s_self = nullptr;

ClipboardApp::ClipboardApp(QObject *parent)
    : QObject(parent)
{
    s_self = this;
    qRegisterMetaType<ClipboardItem>("ClipboardItem");
    qApp->installNativeEventFilter(this);

    m_store = new ClipboardStore(this);
    m_store->setRetentionHours(AppSettings::retentionHours());

    m_model = new ClipboardListModel(this);
    m_model->setItems(m_store->loadAll());

    m_panel = new ClipboardPanel(); // 顶级窗口，无父对象
    m_panel->setModel(m_model);
    m_panel->setTextShowIcon(AppSettings::textShowIcon());

    m_manager = new ClipboardManager(this);
    m_manager->setSensitiveFilter(AppSettings::sensitiveFilter());

    m_tray = new TrayController(m_panel, this);

    connect(m_manager, &ClipboardManager::clipCaptured, this, &ClipboardApp::onClipCaptured);
    connect(m_panel, &ClipboardPanel::pasteRequested, this, &ClipboardApp::onPasteRequested);
    connect(m_panel, &ClipboardPanel::deleteRequested, this, &ClipboardApp::onDeleteRequested);
    connect(m_panel, &ClipboardPanel::togglePinRequested, this, &ClipboardApp::onTogglePinRequested);
    connect(m_panel, &ClipboardPanel::panelHidden, this, [this]() { ClearPendingPaste(); });
    connect(m_panel, &ClipboardPanel::settingsRequested, this, &ClipboardApp::onOpenSettings);
    connect(m_tray, &TrayController::openSettingsRequested, this, &ClipboardApp::onOpenSettings);
    connect(m_tray, &TrayController::quitRequested, this, &ClipboardApp::onQuit);

    // 全局快捷键呼出/隐藏面板（默认 Ctrl+Shift+V，可在设置中修改）
    m_hotkey = new GlobalHotkey(this);
    qApp->installNativeEventFilter(m_hotkey);
    m_hotkey->registerHotkey(QKeySequence(AppSettings::hotkey()));
    connect(m_hotkey, &GlobalHotkey::activated, this, &ClipboardApp::onHotkeyActivated);

    // 启动时立即清理一次过期记录
    m_store->cleanupExpired();

    // 每分钟清理一次超过保留时长的记录
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ClipboardApp::onCleanupTimer);
    m_cleanupTimer->start(60 * 1000);

    // 低频缓存最近有效前台窗口（作为“等待焦点”粘贴的目标窗口依据）
    m_foregroundTimer = new QTimer(this);
    connect(m_foregroundTimer, &QTimer::timeout, this, [this]() {
        const quintptr hwnd = WinUtil::currentForegroundWindow();
        if (hwnd != 0 && hwnd != m_panel->winId() && WinUtil::isUsableWindow(hwnd))
            m_lastUsableForeground = hwnd;
    });
    m_foregroundTimer->start(500);

    // 面板隐藏时定期检查文件条目是否仍存在，不存在则删除（低频轮询，资源占用小）
    m_fileCheckTimer = new QTimer(this);
    connect(m_fileCheckTimer, &QTimer::timeout, this, &ClipboardApp::onFileCheckTimer);
    m_fileCheckTimer->start(10 * 1000);

#ifdef Q_OS_WIN
    // 剪贴板覆盖监听（WM_CLIPBOARDUPDATE）
    AddClipboardFormatListener(reinterpret_cast<HWND>(m_panel->winId()));
#endif

    // 应用主题，并跟随系统深浅色变化
    Theme::applyToApp();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) {
        Theme::applyToApp();
        m_panel->refreshTheme();
        m_tray->reloadIcon();
    });

    m_tray->show();
}

ClipboardApp::~ClipboardApp()
{
    ClearPendingPaste();
    if (m_settingsDlg) {
        delete m_settingsDlg;
        m_settingsDlg = nullptr;
    }
#ifdef Q_OS_WIN
    RemoveClipboardFormatListener(reinterpret_cast<HWND>(m_panel->winId()));
#endif
    delete m_panel;
    s_self = nullptr;
}

bool ClipboardApp::acquireSingleInstance()
{
    const QString name = QStringLiteral("ShearPlate-SingleInstance");

    // 先尝试连接已有实例
    QLocalSocket probe;
    probe.connectToServer(name);
    if (probe.waitForConnected(300)) {
        probe.write("activate");
        probe.flush();
        probe.waitForBytesWritten(300);
        return false; // 已有实例在运行，已通知其激活
    }

    // 作为第一个实例，创建本地服务
    m_server = new QLocalServer(this);
    if (!m_server->listen(name)) {
        QLocalServer::removeServer(name);
        m_server->listen(name);
    }
    connect(m_server, &QLocalServer::newConnection, this, &ClipboardApp::onNewInstanceConnection);
    return true;
}

void ClipboardApp::onNewInstanceConnection()
{
    QLocalSocket *client = m_server->nextPendingConnection();
    if (client) {
        client->disconnectFromServer();
        client->deleteLater();
    }
    m_tray->showPanel();
}

void ClipboardApp::onHotkeyActivated()
{
    m_tray->togglePanel();
}

void ClipboardApp::onClipCaptured(const ClipboardItem &item)
{
    // 永久去重：与现有条目内容一致时，刷新其失效时间并置顶，不新增
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (m_model->itemAt(i).sameAs(item)) {
            ClipboardItem refreshed = m_model->itemAt(i);
            m_store->refreshTimestamp(refreshed);
            m_model->refreshToTop(i);
            return;
        }
    }

    ClipboardItem toSave = item;
    if (!m_store->save(toSave))
        return;
    toSave.image = QImage(); // 释放内存中的大图
    m_model->prependItem(toSave);
}

void ClipboardApp::onPasteRequested(const ClipboardItem &item, bool cutAfterPaste)
{
    // 取消之前的悬置（用户点击另一个条目）
    ClearPendingPaste();

    // 写入剪贴板（带自身标志与敏感标记；文本 CF_UNICODETEXT / 图片 CF_DIB / 文件 CF_HDROP 由 Qt 自动映射）
    m_manager->writeToClipboard(item);
    m_isSelfAction = true;
    QTimer::singleShot(400, this, [this]() { m_isSelfAction = false; });

    // 焦点窗口可直接粘贴：立即模拟 Ctrl+V，流程结束
    if (CanPasteNow()) {
        WinUtil::simulateCtrlV();
        if (cutAfterPaste)
            onDeleteRequested(item.id); // 仅删除历史条目，不删源文件
        return;
    }

    // 无有效焦点（或焦点在本程序面板）：进入等待焦点状态
    // 记录目标窗口：优先当前前台窗口；若为面板/无效，用最近缓存的有效窗口。
    // 只有该目标窗口重新获得焦点才自动粘贴，避免任意窗口抢焦点触发误粘贴。
    quintptr target = WinUtil::currentForegroundWindow();
    if (target == 0 || target == m_panel->winId() || !WinUtil::isUsableWindow(target))
        target = m_lastUsableForeground;

    BeginPendingPaste(item, cutAfterPaste, target);
}

void ClipboardApp::onDeleteRequested(const QString &id)
{
    m_store->remove(id);
    m_model->removeItemById(id);
}

void ClipboardApp::onTogglePinRequested(const QString &id)
{
    ClipboardItem item = m_model->itemById(id);
    if (item.id.isEmpty())
        return;
    item.isPinned = !item.isPinned;
    m_store->save(item);
    m_model->updateItem(item);
}

void ClipboardApp::onOpenSettings()
{
    // 单例：已打开则前置聚焦，避免多个设置窗口
    if (m_settingsDlg) {
        m_settingsDlg->raise();
        m_settingsDlg->activateWindow();
        return;
    }

    m_settingsDlg = new SettingsDialog();
    // 设置项已自动保存；任一变化即时刷新应用
    connect(m_settingsDlg, &SettingsDialog::settingsChanged, this, [this]() {
        m_store->setRetentionHours(AppSettings::retentionHours());
        m_store->cleanupExpired();
        m_hotkey->registerHotkey(QKeySequence(AppSettings::hotkey()));
        m_panel->setTextShowIcon(AppSettings::textShowIcon());
        m_manager->setSensitiveFilter(AppSettings::sensitiveFilter());
        Theme::applyToApp();
        m_panel->refreshTheme();
        m_tray->reloadIcon();
    });
    connect(m_settingsDlg, &QDialog::finished, this, [this]() {
        m_settingsDlg->deleteLater();
        m_settingsDlg = nullptr;
    });
    m_settingsDlg->show();
}

void ClipboardApp::onQuit()
{
    qApp->quit();
}

void ClipboardApp::onCleanupTimer()
{
    const QStringList removed = m_store->cleanupExpired();
    for (const QString &id : removed)
        m_model->removeItemById(id);
}

void ClipboardApp::onFileCheckTimer()
{
    // 仅面板隐藏时检查，避免干扰正在查看/操作的用户
    if (m_panel && m_panel->isVisible())
        return;

    QStringList toRemove;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const ClipboardItem item = m_model->itemAt(i);
        if (item.type != ClipboardItem::Files || item.filePaths.isEmpty())
            continue;

        bool missing = false;
        for (const QString &p : item.filePaths) {
            if (!QFileInfo::exists(p)) {
                missing = true;
                break;
            }
        }
        if (missing)
            toRemove << item.id;
    }

    for (const QString &id : toRemove)
        onDeleteRequested(id);
}

// ---------------- 等待焦点粘贴状态机 ----------------

bool ClipboardApp::CanPasteNow() const
{
#ifdef Q_OS_WIN
    const HWND fg = GetForegroundWindow();
    if (!fg || !IsWindow(fg))
        return false;
    if (fg == reinterpret_cast<HWND>(m_panel->winId()))
        return false; // 焦点在本程序面板
    if (!WinUtil::isUsableWindow(reinterpret_cast<quintptr>(fg)))
        return false; // 桌面/任务栏等

    GUITHREADINFO gti{ sizeof(GUITHREADINFO) };
    return GetGUIThreadInfo(GetWindowThreadProcessId(fg, nullptr), &gti)
        && gti.hwndFocus != nullptr;
#else
    return false;
#endif
}

void ClipboardApp::BeginPendingPaste(const ClipboardItem &item, bool cutAfterPaste, quintptr targetWindow)
{
    m_pendingState = PendingPasteState::WaitingForFocus;
    m_pendingItem = item;
    m_pendingDeleteAfterPaste = cutAfterPaste;
    m_pendingTargetWindow = targetWindow;

#ifdef Q_OS_WIN
    m_focusHook = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, nullptr,
                                  &ClipboardApp::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
#endif

    m_pendingTimer = new QTimer(this);
    m_pendingTimer->setSingleShot(true);
    connect(m_pendingTimer, &QTimer::timeout, this, [this]() { OnPendingTimeout(); });
    m_pendingTimer->start(30 * 1000);
}

void ClipboardApp::ClearPendingPaste()
{
    if (m_pendingState == PendingPasteState::Idle)
        return;

#ifdef Q_OS_WIN
    if (m_focusHook) {
        UnhookWinEvent(m_focusHook);
        m_focusHook = nullptr;
    }
#endif
    if (m_pendingTimer) {
        m_pendingTimer->stop();
        m_pendingTimer->deleteLater();
        m_pendingTimer = nullptr;
    }
    m_pendingState = PendingPasteState::Idle;
}

void ClipboardApp::OnPendingTimeout()
{
    ClearPendingPaste(); // 超时：取消悬置，不执行任何操作
}

void CALLBACK ClipboardApp::WinEventProc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG, LONG, DWORD, DWORD)
{
    if (!s_self)
        return;
    const quintptr h = reinterpret_cast<quintptr>(hwnd);
    QMetaObject::invokeMethod(s_self, [h]() {
        if (s_self)
            s_self->OnPendingFocusChanged(h);
    }, Qt::QueuedConnection);
}

void ClipboardApp::OnPendingFocusChanged(quintptr hwnd)
{
    if (m_pendingState != PendingPasteState::WaitingForFocus)
        return;

    const HWND h = reinterpret_cast<HWND>(hwnd);
    if (!h || !IsWindow(h))
        return;
    if (h == reinterpret_cast<HWND>(m_panel->winId()))
        return; // 本程序面板
    if (!WinUtil::isUsableWindow(hwnd))
        return; // 桌面/任务栏等非目标窗口

    // 只有目标窗口（或其子控件）重新获得焦点才自动粘贴，避免任意窗口抢焦点触发误粘贴
    const HWND focusTop = GetAncestor(h, GA_ROOT);
    if (focusTop != reinterpret_cast<HWND>(m_pendingTargetWindow))
        return;

    const ClipboardItem doneItem = m_pendingItem;
    const bool deleteAfter = m_pendingDeleteAfterPaste;
    ClearPendingPaste();                // 1. 清除定时器与钩子
    WinUtil::simulateCtrlV();           // 2. 模拟粘贴
    if (deleteAfter)
        onDeleteRequested(doneItem.id); // 3. 剪切语义：删除历史条目（不删源文件）
}

bool ClipboardApp::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_CLIPBOARDUPDATE
        && m_pendingState == PendingPasteState::WaitingForFocus) {
        OnClipboardUpdated();
    }
#endif
    return false;
}

void ClipboardApp::OnClipboardUpdated()
{
    if (m_isSelfAction)
        return; // 自身写入，忽略
    if (!ClipboardMatchesPending())
        ClearPendingPaste(); // 内容被覆盖：取消悬置，不粘贴不删除
}

bool ClipboardApp::ClipboardMatchesPending() const
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime)
        return false;

    if (m_pendingItem.type == ClipboardItem::Text)
        return mime->hasText() && mime->text() == m_pendingItem.text;

    if (m_pendingItem.type == ClipboardItem::Files) {
        // 内容比对基于文件路径列表排序后的字符串
        QStringList a = m_pendingItem.filePaths;
        QStringList b;
        for (const QUrl &u : mime->urls())
            if (u.isLocalFile())
                b << u.toLocalFile();
        a.sort();
        b.sort();
        return a == b;
    }

    if (m_pendingItem.type == ClipboardItem::Image)
        return mime->hasImage(); // 仅按格式存在判断，节省资源

    return false;
}
