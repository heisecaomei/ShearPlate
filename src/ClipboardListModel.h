#pragma once

#include <QAbstractListModel>
#include <QList>

#include "ClipboardItem.h"

class ClipboardListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        PreviewRole = Qt::UserRole + 1,
        TimeRole,
        TypeRole,
        IdRole,
        ItemRole
    };

    explicit ClipboardListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDragActions() const override;

    void setItems(const QList<ClipboardItem> &items);
    void prependItem(const ClipboardItem &item);
    void removeItemById(const QString &id);
    void refreshToTop(int row);   // 刷新该项时间戳并置顶（row 为显示行）
    void updateItem(const ClipboardItem &item);

    ClipboardItem itemAt(int row) const;
    ClipboardItem itemById(const QString &id) const;

    bool isMissingAt(int row) const; // 显示行条目当前是否被标记为失效
    void updateMissingStates();      // 定时检测磁盘存在性；失效条目灰显并沉底

    // 分类过滤：-1 全部, 0 文本, 1 图片, 2 文件
    void setFilterType(int type);
    int filterType() const;

private:
    bool passFilter(const ClipboardItem &item) const;
    void rebuildDisplay();
    void orderDisplayForMissing(); // 失效条目稳定沉底（置顶有效条目仍在最前）

    QList<ClipboardItem> m_items; // 全部条目
    QList<int> m_display;         // 显示顺序：显示行 → m_items 索引
    int m_filterType = -1;
};
