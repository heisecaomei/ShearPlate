# ShearPlate 代码修订方案

> 依据需求：敏感内容过滤 / 置顶外观 / 多文件布局 / 点击交互重构（PendingPaste 状态机）。
> 文档给出：涉及文件、关键代码片段、逻辑说明、设计建议。

---

## 一、现有代码结构分析

| 文件 | 职责 | 本次修改 |
|---|---|---|
| `src/main.cpp` | 入口 | 无 |
| `src/ClipboardApp.{h,cpp}` | 总控：上屏 `onPasteRequested`、去重、文件失效检查、Toast | **大改**：PendingPaste 状态机、移除 Toast、焦点判断、剪贴板格式监听 |
| `src/ClipboardManager.{h,cpp}` | 剪贴板监听/捕获/写入/粘贴 | **改**：注册系统格式、`IsSensitiveContent()`、写入敏感标记 |
| `src/ClipboardItem.h` | 条目模型（type/text/imagePath/filePaths/screenshot/sameAs） | **改**：新增 `isPinned` |
| `src/ClipboardStore.{h,cpp}` | JSON 持久化 | **改**：`isPinned` 读写 |
| `src/ClipboardListModel.{h,cpp}` | 列表模型 | 基本不变 |
| `src/ClipboardItemDelegate.{h,cpp}` | 条目绘制（paint/openIconArea） | **改**：置顶背景+图钉、多文件布局 |
| `src/ClipboardListView.{h,cpp}` | 列表视图（拖拽/打开图标/禁用右键拖动） | 不变 |
| `src/ClipboardPanel.{h,cpp}` | 面板（clicked→pasteRequested、右键菜单） | **改**：clicked 携带 Shift、隐藏时取消悬置、nativeEvent 转发 WM_CLIPBOARDUPDATE |
| `src/SettingsDialog.{h,cpp}` | 设置对话框 | **改**：新增"启用敏感内容过滤"复选框 |
| `src/AppSettings.h` | 设置存取（QSettings） | **改**：新增 `sensitiveFilter` |
| `src/Theme.h` | 主题配色 | **改**：新增 `pinnedBg/pinnedBorder`，普通条目浅色改 #FAFAFA/#F8F8F8 |
| `src/WinUtil.{h,cpp}` | Win32 封装（activateWindow/simulateCtrlV/isUsableWindow） | **改**：新增焦点判断辅助 |
| `src/GlobalHotkey.{h,cpp}` | 全局快捷键 | 不变 |
| `src/ToastWidget.{h,cpp}` | 全透明提示 | **删除**（需求 4.3 要求移除所有用户可见提示） |
| `resources.qrc` | 资源 | **改**：新增 `pin_default.png`、`pin.png`（light/dark） |

---

## 二、修改点 1：敏感内容过滤

### 2.1 涉及文件
`ClipboardManager.{h,cpp}`、`AppSettings.h`、`SettingsDialog.{h,cpp}`、`ClipboardApp.cpp`（传设置给 Manager）。

### 2.2 注册系统剪贴板格式（构造时一次）

```cpp
// ClipboardManager.h
#include <windows.h>  // 或放入 .cpp 的 #ifdef Q_OS_WIN
UINT m_excludeFormat = 0;  // ExcludeClipboardContentFromMonitorProcessing
UINT m_historyFormat = 0;  // CanIncludeInClipboardHistory
UINT m_cloudFormat   = 0;  // CanUploadToCloudClipboard
bool m_sensitiveFilter = true;  // 由设置注入
bool IsSensitiveContent();      // 检测，防死锁
void writeSensitiveMarkers();   // 本程序写入时附加标记

// 构造：
m_excludeFormat = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
m_historyFormat = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
m_cloudFormat   = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
```

### 2.3 IsSensitiveContent()（防死锁）

```cpp
bool ClipboardManager::IsSensitiveContent()
{
    if (!m_sensitiveFilter)
        return false;

    // IsClipboardFormatAvailable 不需要 OpenClipboard，无死锁风险
    if (m_excludeFormat != 0 && IsClipboardFormatAvailable(m_excludeFormat))
        return true;

    // 需要读取 0/1 值（DWORD）时才打开剪贴板；打开失败则保守返回 false
    if ((m_historyFormat != 0 && IsClipboardFormatAvailable(m_historyFormat))
        || (m_cloudFormat != 0 && IsClipboardFormatAvailable(m_cloudFormat))) {
        if (!OpenClipboard(nullptr))
            return false; // 被占用：不判定，防死锁
        bool sensitive = false;
        if (!sensitive && m_historyFormat != 0 && IsClipboardFormatAvailable(m_historyFormat))
            sensitive = readDwordFormat(m_historyFormat) == 0;
        if (!sensitive && m_cloudFormat != 0 && IsClipboardFormatAvailable(m_cloudFormat))
            sensitive = readDwordFormat(m_cloudFormat) == 0;
        CloseClipboard();
        return sensitive;
    }
    return false;
}

static DWORD readDwordFormat(UINT fmt)
{
    DWORD v = 1;
    HANDLE h = GetClipboardData(fmt);
    if (h) {
        const DWORD *p = static_cast<const DWORD *>(GlobalLock(h));
        if (p) { v = *p; GlobalUnlock(h); }
    }
    return v;
}
```

> 防死锁说明：`IsClipboardFormatAvailable` 无需开锁；`OpenClipboard` 只在 Qt 的 `dataChanged` 信号发出后（此时 Qt 已关闭其内部锁）调用一次，失败即放弃判断，绝不重试/等待。

### 2.4 onDataChanged 入口过滤

```cpp
void ClipboardManager::onDataChanged()
{
    if (m_selfPaste) { m_selfPaste = false; return; }
    if (m_sensitiveFilter && IsSensitiveContent())
        return; // 忽略敏感内容，不记录
    // ... 原有捕获逻辑
}
```

### 2.5 本程序写入时附加敏感标记（不污染系统剪贴板历史/云剪贴板）

统一改写为 `writeToClipboard(item)`，先经 Qt 写入内容，再用 WinAPI 附加三个格式（独立开锁会话，避免与 Qt 锁冲突）：

```cpp
void ClipboardManager::writeToClipboard(const ClipboardItem &item)
{
    m_selfPaste = true;
    auto *mime = new QMimeData();
    if (item.type == Image)      { QImage img(item.imagePath); if (!img.isNull()) mime->setImageData(img); }
    else if (item.type == Files) { QList<QUrl> urls; for (auto &p : item.filePaths) urls << QUrl::fromLocalFile(p); mime->setUrls(urls); } // CF_HDROP
    else                         { mime->setText(item.text); }  // CF_UNICODETEXT
    m_clipboard->setMimeData(mime);

    if (m_sensitiveFilter)
        writeSensitiveMarkers();

    QTimer::singleShot(400, this, [this]{ m_selfPaste = false; });
}

void ClipboardManager::writeSensitiveMarkers()
{
    if (!OpenClipboard(nullptr)) return;
    auto setDword = [](UINT fmt, DWORD v){
        if (!fmt) return;
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
        if (h) { auto *p = static_cast<DWORD*>(GlobalLock(h)); *p = v; GlobalUnlock(h); SetClipboardData(fmt, h); }
    };
    if (m_excludeFormat) { HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 1); if (h) SetClipboardData(m_excludeFormat, h); } // 存在即生效
    setDword(m_historyFormat, 0);
    setDword(m_cloudFormat,   0);
    CloseClipboard();
}
```

> `paste()` 保留：写入 + `simulateCtrlV`。`copyToClipboard` 可重定向到 `writeToClipboard`。

### 2.6 设置项

- `AppSettings.h`：`sensitiveFilter()` 默认 true / `setSensitiveFilter()`。
- `SettingsDialog`：常规组加 `m_sensitiveCheck`（"启用敏感内容过滤"），`accept()` 写回。
- `ClipboardApp::onOpenSettings`：把新值传给 `m_manager->setSensitiveFilter(...)`。

---

## 三、修改点 2：置顶条目外观

### 3.1 模型与持久化
- `ClipboardItem.h`：加 `bool isPinned = false;`。
- `ClipboardStore`：JSON 加 `"pinned"` 字段读写。

### 3.2 主题
`Theme.h` 增加 `pinnedBg / pinnedBorder`：
```cpp
// light：普通卡片改为 #FAFAFA / #F8F8F8；置顶 #F0F4F8 / #D0DDEB
c.cardBg    = QColor(0xfa,0xfa,0xfa);
c.cardBorder= QColor(0xf8,0xf8,0xf8);
c.pinnedBg    = QColor(0xf0,0xf4,0xf8);
c.pinnedBorder= QColor(0xd0,0xdd,0xeb);
// dark：置顶深色变体
c.pinnedBg    = QColor(0x21,0x2b,0x36);
c.pinnedBorder= QColor(0x3b,0x4c,0x5f);
```

### 3.3 图标资源
- 新增 `images/light/pin_default.png`、`images/light/pin.png`、`images/dark/pin_default.png`、`images/dark/pin.png`（12×12）。
- `resources.qrc` 注册 4 个文件。
- Delegate 加载：`m_pinDefaultIcon = loadThemedIcon("pin_default.png")`、`m_pinIcon = loadThemedIcon("pin.png")`（构造 + `reloadIcons`）。

### 3.4 paint()
```cpp
QColor bg = item.isPinned ? tc.pinnedBg : tc.cardBg;
QColor border = item.isPinned ? tc.pinnedBorder : tc.cardBorder;
if (hover || selected) { /* 保持 hover 优先或叠加 */ }
painter->drawRoundedRect(card, 8, 8);

// 右上角图钉（12×12）
const QIcon &pin = item.isPinned ? m_pinIcon : m_pinDefaultIcon;
if (!pin.isNull()) {
    QRect pinRect(int(card.right() - 24), int(card.top() + 6), 12, 12);
    pin.paint(painter, pinRect);
}
```

> 说明：需求只要求"置顶外观"。`isPinned` 目前无置顶切换入口，建议后续在 `ClipboardListView` 中拦截图钉区域点击发出 `togglePinRequested`（不在本次范围）。

---

## 四、修改点 3：多文件文字布局

目标：文件列表（副行）文字矩形延伸到卡片右边缘（固定右 padding），若右侧有图标则扣除。

统一右侧预留：
```cpp
const qreal rightPad = 24;  // 图钉(12)+间距(4)+padding(8)
const qreal textWidth = card.width() - 48 - rightPad;   // 文字可用宽度
```

- **多文件标题行**：`titleTextWidth = textWidth - linkIconSize - 4`（预留 link 图标），link 图标紧跟标题结尾。
- **多文件副行（文件名列表）**：`listRect = (textLeft, ..., textWidth, lineH)`，文字占满到 `card.right()-rightPad`。
- **单文件/文本链接**：`elided` 宽度 = `textWidth - linkIconSize - 4`，link 图标紧跟文件名/链接文字结尾。
- **图片**：文字短，不受影响。

> 原 `card.width()-48-46` 预留 46px 过宽，且副行右侧实际无图标；改为统一 24px 预留可显著加宽文字可用区。

---

## 五、修改点 4：点击条目交互逻辑重构（核心）

### 5.1 状态定义（ClipboardApp）

```cpp
enum class PendingPasteState { Idle, WaitingForFocus };

struct PendingPaste {
    ClipboardItem item;
    bool deleteAfterPaste = false;
    QTimer *timeoutTimer = nullptr;   // 30s
    HWINEVENTHOOK focusHook = nullptr;
};

// ClipboardApp : public QObject, public QAbstractNativeEventFilter
PendingPasteState m_pendingState = PendingPasteState::Idle;
PendingPaste m_pending;
bool m_isSelfAction = false;         // 自身写剪贴板标志
static ClipboardApp *s_self;         // SetWinEventHook 静态回调访问实例
```

### 5.2 信号携带 Shift（剪切语义）

- `ClipboardPanel`：`pasteRequested(const ClipboardItem &, bool cutAfterPaste)`。
- clicked 处理：
```cpp
connect(m_list, &QListView::clicked, this, [this](const QModelIndex &index){
    if (!index.isValid() || !m_model) return;
    const bool shift = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
    emit pasteRequested(m_model->itemAt(index.row()), shift);
});
```
- 面板隐藏时：`connect(this, &ClipboardPanel::slideOut.../hide 事件)` → `ClearPendingPaste()`。

### 5.3 统一入口 onPasteRequested

```cpp
void ClipboardApp::onPasteRequested(const ClipboardItem &item, bool cutAfterPaste)
{
    ClearPendingPaste();                    // 先取消旧悬置（用户点击另一条目）
    m_manager->writeToClipboard(item);      // 写剪贴板（带 m_selfPaste + 敏感标记）

    if (CanPasteNow()) {                    // 焦点有效且非面板
        WinUtil::simulateCtrlV();
        if (cutAfterPaste) onDeleteRequested(item.id);  // 只删历史，不删源文件
        return;
    }
    BeginPendingPaste(item, cutAfterPaste); // 进入 WaitingForFocus
}
```

### 5.4 CanPasteNow()（GetForegroundWindow + GetGUIThreadInfo）

```cpp
bool ClipboardApp::CanPasteNow() const
{
#ifdef Q_OS_WIN
    HWND fg = GetForegroundWindow();
    if (!fg || !IsWindow(fg)) return false;
    if (fg == reinterpret_cast<HWND>(m_panel->winId())) return false;   // 面板
    if (!WinUtil::isUsableWindow(reinterpret_cast<quintptr>(fg))) return false; // 桌面/任务栏
    GUITHREADINFO gti{ sizeof(GUITHREADINFO) };
    return GetGUIThreadInfo(GetWindowThreadProcessId(fg, nullptr), &gti) && gti.hwndFocus != nullptr;
#else
    return false;
#endif
}
```

### 5.5 BeginPendingPaste / ClearPendingPaste

```cpp
void ClipboardApp::BeginPendingPaste(const ClipboardItem &item, bool cut)
{
    m_pendingState = PendingPasteState::WaitingForFocus;
    m_pending.item = item;
    m_pending.deleteAfterPaste = cut;

    // 焦点监听（WINEVENT_OUTOFCONTEXT + 回调投递主线程）
    m_pending.focusHook = SetWinEventHook(
        EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, nullptr,
        &ClipboardApp::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    m_pending.timeoutTimer = new QTimer(this);
    m_pending.timeoutTimer->setSingleShot(true);
    connect(m_pending.timeoutTimer, &QTimer::timeout, this, &ClipboardApp::OnPendingTimeout);
    m_pending.timeoutTimer->start(30 * 1000);
}

void ClipboardApp::ClearPendingPaste()
{
    if (m_pendingState == PendingPasteState::Idle) return;
    if (m_pending.focusHook)    { UnhookWinEvent(m_pending.focusHook);    m_pending.focusHook = nullptr; }
    if (m_pending.timeoutTimer) { m_pending.timeoutTimer->stop(); m_pending.timeoutTimer->deleteLater(); m_pending.timeoutTimer = nullptr; }
    m_pendingState = PendingPasteState::Idle;
}
```

### 5.6 焦点回调（静态 → 主线程）

```cpp
void CALLBACK ClipboardApp::WinEventProc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG, LONG, DWORD, DWORD)
{
    if (!s_self) return;
    QMetaObject::invokeMethod(s_self, [s_self, hwnd]{ s_self->OnPendingFocusChanged(hwnd); },
                              Qt::QueuedConnection);
}

void ClipboardApp::OnPendingFocusChanged(HWND hwnd)
{
    if (m_pendingState != PendingPasteState::WaitingForFocus) return;
    if (!hwnd || !IsWindow(hwnd)) return;
    if (hwnd == reinterpret_cast<HWND>(m_panel->winId())) return;          // 本程序
    if (!WinUtil::isUsableWindow(reinterpret_cast<quintptr>(hwnd))) return; // 桌面/任务栏

    PendingPaste done = m_pending;
    ClearPendingPaste();          // 1. 清定时器与钩子
    WinUtil::simulateCtrlV();     // 2. 模拟粘贴
    if (done.deleteAfterPaste)
        onDeleteRequested(done.item.id);  // 3. 删除历史条目（不删源文件）
}
```

### 5.7 剪贴板覆盖监听（AddClipboardFormatListener + WM_CLIPBOARDUPDATE）

- 构造：`AddClipboardFormatListener(reinterpret_cast<HWND>(m_panel->winId()));`
- `ClipboardApp` 实现 `QAbstractNativeEventFilter::nativeEventFilter`，处理 `WM_CLIPBOARDUPDATE`：

```cpp
bool ClipboardApp::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG*>(message);
    if (msg->message == WM_CLIPBOARDUPDATE && m_pendingState == PendingPasteState::WaitingForFocus)
        OnClipboardUpdated();
#endif
    return false;
}

void ClipboardApp::OnClipboardUpdated()
{
    if (m_isSelfAction) return;          // 自身写入（写剪贴板时置位，400ms 后复位）
    if (!ClipboardMatchesPending())
        ClearPendingPaste();             // 内容被覆盖 → 取消悬置，不粘贴不删除
}
```

`ClipboardMatchesPending()` 比对（防死锁：仅在 Qt 数据已可取时调用 `QGuiApplication::clipboard()->mimeData()`）：
- 文本：`mime->text() == m_pending.item.text`。
- 文件：`mime->urls()` 本地路径排序后与 `m_pending.item.filePaths` 排序后相等。
- 图片：比较 `image/png` 字节哈希（仅 `deleteAfterPaste` 时严格比对，其余按格式存在判断）。

### 5.8 超时 / 取消 / 退出

- `OnPendingTimeout()` → `ClearPendingPaste()`。
- 用户点击另一条目 → `onPasteRequested` 开头 `ClearPendingPaste()`。
- 面板隐藏 → 面板 hide 信号 → `ClearPendingPaste()`。
- 程序退出 → 析构 `ClearPendingPaste()`；`RemoveClipboardFormatListener`。

### 5.9 移除用户可见提示

- 删除 `ToastWidget.{h,cpp}`、CMake 条目、`ClipboardApp` 的 `m_toast` 及 `onPasteRequested` 中的 toast 调用。
- 失败/等待不再有任何弹窗或托盘提示，仅通过条目增删与置顶图标反馈。

---

## 六、技术实现要点汇总

1. `SetWinEventHook`：`WINEVENT_OUTOFCONTEXT` + 静态回调 + `QMetaObject::invokeMethod(..., QueuedConnection)` 投递主线程，避免跨线程操作 Qt 对象。
2. `AddClipboardFormatListener`：注册到面板 HWND，`nativeEventFilter` 拦截 `WM_CLIPBOARDUPDATE`；自身操作靠 `m_isSelfAction`（写入时置位，400ms 复位，与现有 `m_selfPaste` 语义一致）。
3. `SendInput` 粘贴复用 `WinUtil::simulateCtrlV`；写剪贴板统一走 `writeToClipboard`（带敏感标记 + self 标志）。
4. 全部清理集中在 `ClearPendingPaste()`（钩子 + 定时器），杜绝泄漏。
5. 焦点监听排除桌面（Progman/WorkerW）、任务栏（Shell_TrayWnd）、本程序面板。
6. 文件条目剪切仅删历史记录（`onDeleteRequested` 只删 store/model），不触碰源文件。
7. 图片比对：常规等待仅按格式判断，`deleteAfterPaste` 时才计算哈希。

---

## 七、设计建议与风险

1. **置顶交互缺失**：本次只实现 `isPinned` 外观；建议下一步补"点击图钉切换置顶 + 置顶条目排序靠前"，否则置顶配色/图标无实际触发入口。
2. **剪贴板死锁**：`IsSensitiveContent`/`ClipboardMatchesPending` 均在 Qt `dataChanged`/`WM_CLIPBOARDUPDATE` 之后读取，Qt 内部锁已释放；`OpenClipboard` 失败一律放弃判断，绝不重试。
3. **等待焦点与现有"缓存 m_lastForeground + activateWindow"冲突**：新逻辑不再需要 `m_lastForeground` 缓存与 `onForegroundTimer`、`beforePanelShow` 捕获；可删除相关代码，简化 `ClipboardApp`。
4. **`WM_CLIPBOARDUPDATE` 与 Qt `dataChanged` 双通道**：自身写入会被两者都触发，统一由 `m_isSelfAction`/`m_selfPaste` 抑制，避免重复记录与误判。
5. **`SetWinEventHook` 性能**：`EVENT_OBJECT_FOCUS` 事件频率中等，仅 `WaitingForFocus` 期间挂载、粘贴后立即卸载，常驻开销为零。
6. **旧版兼容**：`ExcludeClipboardContentFromMonitorProcessing` 等格式在 Win10 1809 以下不存在，`RegisterClipboardFormatW` 仍返回有效 ID（作为普通自定义格式），`IsClipboardFormatAvailable` 恒 false，功能自然降级。
7. **实施顺序建议**：① 敏感过滤（独立、低风险）→ ② 置顶外观 → ③ 多文件布局 → ④ 点击交互重构（最大，最后做并整体回归）。
