#include "ClipboardManager.h"

#include <QBuffer>
#include <QDateTime>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QImage>
#include <QList>
#include <QMimeData>
#include <QRandomGenerator>
#include <QScreen>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "WinUtil.h"

#ifdef Q_OS_WIN
static DWORD readDwordFormat(UINT fmt);
#endif

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
{
    m_clipboard = QGuiApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardManager::onDataChanged);

#ifdef Q_OS_WIN
    // 注册系统剪贴板格式（Win10 1809+ 支持；旧系统返回普通自定义格式 ID，功能自然降级）
    m_excludeFormat = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    m_historyFormat = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    m_cloudFormat   = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
#endif
}

void ClipboardManager::setSensitiveFilter(bool enabled)
{
    m_sensitiveFilter = enabled;
}

void ClipboardManager::onDataChanged()
{
    // 本程序为“上屏/复制”而写入的内容不再记录
    if (m_selfPaste) {
        m_selfPaste = false;
        return;
    }

    // 敏感内容过滤：命中系统排除格式则忽略该次内容
    if (m_sensitiveFilter && IsSensitiveContent())
        return;

    const QMimeData *mime = m_clipboard->mimeData();
    if (!mime)
        return;

    ClipboardItem item;
    QByteArray hash;

    const QImage img = m_clipboard->image();
    if (!img.isNull()) {
        item.type = ClipboardItem::Image;
        item.image = img;
        // 判断是否为截图：宽度或高度接近屏幕分辨率（≥80%）
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            const QSize ss = screen->size();
            item.screenshot = (img.width() >= ss.width() * 0.8)
                            || (img.height() >= ss.height() * 0.8);
        }
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        hash = QCryptographicHash::hash(buf.data(), QCryptographicHash::Md5);
    } else if (!mime->urls().isEmpty() && mime->urls().first().isLocalFile()) {
        // 复制文件/文件夹：作为文件类型记录，粘贴时按文件处理
        QStringList paths;
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile())
                paths << url.toLocalFile();
        }
        if (paths.isEmpty())
            return;
        item.type = ClipboardItem::Files;
        item.filePaths = paths;
        hash = QCryptographicHash::hash(paths.join(QLatin1Char('\n')).toUtf8(), QCryptographicHash::Md5);
    } else if (!m_clipboard->text().isEmpty()) {
        item.type = ClipboardItem::Text;
        item.text = m_clipboard->text();
        hash = QCryptographicHash::hash(item.text.toUtf8(), QCryptographicHash::Md5);
    } else {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 1.5 秒内完全相同的内容视为系统重复触发，去重
    if (!hash.isEmpty() && hash == m_lastHash && (now - m_lastCaptureMs) < 1500)
        return;

    m_lastHash = hash;
    m_lastCaptureMs = now;

    item.id = QString::number(now) + QStringLiteral("_")
            + QString::number(QRandomGenerator::global()->bounded(0, 100000));
    item.timestampMs = now;

    emit clipCaptured(item);
}

bool ClipboardManager::IsSensitiveContent()
{
    if (!m_sensitiveFilter)
        return false;

#ifdef Q_OS_WIN
    // IsClipboardFormatAvailable 无需打开剪贴板，无死锁风险
    if (m_excludeFormat != 0
        && IsClipboardFormatAvailable(static_cast<UINT>(m_excludeFormat)))
        return true;

    const bool needValue =
        (m_historyFormat != 0 && IsClipboardFormatAvailable(static_cast<UINT>(m_historyFormat)))
        || (m_cloudFormat != 0 && IsClipboardFormatAvailable(static_cast<UINT>(m_cloudFormat)));
    if (!needValue)
        return false;

    // 读取 0/1 值（DWORD）才需要打开剪贴板；打开失败则保守不判定（防死锁）
    if (!OpenClipboard(nullptr))
        return false;

    bool sensitive = false;
    if (!sensitive && m_historyFormat != 0
        && IsClipboardFormatAvailable(static_cast<UINT>(m_historyFormat))) {
        sensitive = (readDwordFormat(static_cast<UINT>(m_historyFormat)) == 0);
    }
    if (!sensitive && m_cloudFormat != 0
        && IsClipboardFormatAvailable(static_cast<UINT>(m_cloudFormat))) {
        sensitive = (readDwordFormat(static_cast<UINT>(m_cloudFormat)) == 0);
    }
    CloseClipboard();
    return sensitive;
#else
    return false;
#endif
}

// 需在剪贴板已打开（OpenClipboard）时调用
#ifdef Q_OS_WIN
static DWORD readDwordFormat(UINT fmt)
{
    DWORD v = 1; // 默认允许
    HANDLE h = GetClipboardData(fmt);
    if (h) {
        const DWORD *p = static_cast<const DWORD *>(GlobalLock(h));
        if (p) {
            v = *p;
            GlobalUnlock(h);
        }
    }
    return v;
}
#endif

void ClipboardManager::writeSensitiveMarkers()
{
#ifdef Q_OS_WIN
    if (!OpenClipboard(nullptr))
        return;

    auto setDword = [](UINT fmt, DWORD val) {
        if (!fmt)
            return;
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
        if (!h)
            return;
        auto *p = static_cast<DWORD *>(GlobalLock(h));
        if (p) {
            *p = val;
            GlobalUnlock(h);
        }
        SetClipboardData(fmt, h); // 失败时由系统释放 h
    };

    // 该格式“存在即生效”
    if (m_excludeFormat != 0) {
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 1);
        if (h)
            SetClipboardData(static_cast<UINT>(m_excludeFormat), h);
    }
    setDword(static_cast<UINT>(m_historyFormat), 0); // 禁止进入系统剪贴板历史
    setDword(static_cast<UINT>(m_cloudFormat), 0);   // 禁止上传云剪贴板
    CloseClipboard();
#endif
}

void ClipboardManager::writeToClipboard(const ClipboardItem &item)
{
    m_selfPaste = true;

    auto *mime = new QMimeData();
    if (item.type == ClipboardItem::Image) {
        const QImage img(item.imagePath);
        if (img.isNull()) {
            m_selfPaste = false;
            return;
        }
        mime->setImageData(img);
    } else if (item.type == ClipboardItem::Files) {
        QList<QUrl> urls;
        for (const QString &p : item.filePaths)
            urls << QUrl::fromLocalFile(p);
        mime->setUrls(urls);
    } else {
        mime->setText(item.text);
    }
    m_clipboard->setMimeData(mime);

    // 内容被标记为敏感时，附加排除系统剪贴板历史/云剪贴板的标记
    if (m_sensitiveFilter)
        writeSensitiveMarkers();

    // 兜底：即便某些平台未同步触发 dataChanged，也确保下一次真实变化不会被误判
    QTimer::singleShot(400, this, [this]() { m_selfPaste = false; });
}

void ClipboardManager::paste(const ClipboardItem &item)
{
    writeToClipboard(item);
    WinUtil::simulateCtrlV();
}

void ClipboardManager::copyToClipboard(const ClipboardItem &item)
{
    writeToClipboard(item);
}
