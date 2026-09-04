#pragma once

#include <QByteArray>
#include <QClipboard>
#include <QObject>

#include "ClipboardItem.h"

// 实时监听系统剪贴板变化，捕获文本 / 图片 / 文件路径；
// 支持敏感内容过滤（排除系统剪贴板历史 / 云剪贴板），并提供“上屏”能力。
class ClipboardManager : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardManager(QObject *parent = nullptr);

    // 启用/关闭敏感内容过滤（默认开启）
    void setSensitiveFilter(bool enabled);

    // 把条目内容写入系统剪贴板（带自身标志与敏感标记），不发送 Ctrl+V
    void writeToClipboard(const ClipboardItem &item);

public slots:
    // 由调用方先还原前台窗口焦点，再调用本函数完成粘贴
    void paste(const ClipboardItem &item);
    // 仅把内容写入系统剪贴板（不发送 Ctrl+V），供用户自行粘贴
    void copyToClipboard(const ClipboardItem &item);

signals:
    void clipCaptured(const ClipboardItem &item);

private slots:
    void onDataChanged();

private:
    // 检测当前剪贴板是否被标记为敏感内容（防死锁）
    bool IsSensitiveContent();
    // 本程序写入内容后，附加排除系统剪贴板历史/云剪贴板的标记
    void writeSensitiveMarkers();

    QClipboard *m_clipboard = nullptr;
    bool m_selfPaste = false;      // 本次写剪贴板是否由本程序发起（避免重复记录）
    QByteArray m_lastHash;         // 去重
    qint64 m_lastCaptureMs = 0;
    quintptr m_excludeFormat = 0;  // ExcludeClipboardContentFromMonitorProcessing
    quintptr m_historyFormat = 0;  // CanIncludeInClipboardHistory
    quintptr m_cloudFormat = 0;    // CanUploadToCloudClipboard
    bool m_sensitiveFilter = true;
};
