#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "ClipboardItem.h"

// 以磁盘文件形式保存剪贴板记录：
//   文本  ->  <dataDir>/<id>.json
//   图片  ->  <dataDir>/<id>.json + <dataDir>/<id>.png
// 并提供按保留时长（默认 12 小时）的过期清理。
class ClipboardStore : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardStore(QObject *parent = nullptr);

    void setRetentionHours(int hours);
    int retentionHours() const;

    QString dataDir() const;

    // 加载全部记录（按时间倒序）
    QList<ClipboardItem> loadAll() const;

    // 保存一条记录；成功返回 true，并把 item.imagePath 修正为落盘路径
    bool save(ClipboardItem &item);

    // 删除一条记录（同时删除可能的图片文件）
    bool remove(const QString &id);

    // 刷新一条记录的时间戳（重新计时保留时长）
    bool refreshTimestamp(ClipboardItem &item);

    // 清理超过保留时长的记录，返回被删除记录的 id 列表
    QStringList cleanupExpired();

private:
    QString jsonPath(const QString &id) const;
    QString imagePath(const QString &id) const;

    int m_retentionHours = 12;
};
