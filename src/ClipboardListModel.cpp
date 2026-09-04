#include "ClipboardListModel.h"

#include <QBuffer>
#include <QDateTime>
#include <QImage>
#include <QMimeData>
#include <QUrl>

#include <algorithm>

ClipboardListModel::ClipboardListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ClipboardListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_display.size();
}

QVariant ClipboardListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_display.size())
        return {};

    const ClipboardItem &item = m_items.at(m_display.at(index.row()));
    switch (role) {
    case Qt::DisplayRole:
    case PreviewRole:
        return item.preview();
    case TimeRole:
        return item.displayTime();
    case TypeRole:
        return static_cast<int>(item.type);
    case IdRole:
        return item.id;
    case ItemRole:
        return QVariant::fromValue(item);
    default:
        return {};
    }
}

Qt::ItemFlags ClipboardListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QStringList ClipboardListModel::mimeTypes() const
{
    return { QStringLiteral("text/plain"),
             QStringLiteral("text/uri-list"),
             QStringLiteral("image/png"),
             QStringLiteral("application/x-qt-image") };
}

QMimeData *ClipboardListModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.isEmpty())
        return nullptr;

    const ClipboardItem item = itemAt(indexes.first().row());
    auto *mime = new QMimeData();

    if (item.type == ClipboardItem::Image) {
        const QImage img(item.imagePath);
        if (!img.isNull()) {
            mime->setImageData(img);
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            mime->setData(QStringLiteral("image/png"), bytes);
        }
    } else if (item.type == ClipboardItem::Files) {
        QList<QUrl> urls;
        for (const QString &p : item.filePaths)
            urls << QUrl::fromLocalFile(p);
        mime->setUrls(urls);
    } else {
        mime->setText(item.text);
    }

    return mime;
}

Qt::DropActions ClipboardListModel::supportedDragActions() const
{
    // 支持剪切(移动)与复制：拖到资源管理器时可选择 剪切/复制/取消
    return Qt::MoveAction | Qt::CopyAction;
}

void ClipboardListModel::setItems(const QList<ClipboardItem> &items)
{
    beginResetModel();
    m_items = items;
    // 置顶条目始终排在最前，其余按时间倒序
    std::sort(m_items.begin(), m_items.end(), [](const ClipboardItem &a, const ClipboardItem &b) {
        if (a.isPinned != b.isPinned)
            return a.isPinned;
        return a.timestampMs > b.timestampMs;
    });
    rebuildDisplay();
    endResetModel();
}

void ClipboardListModel::prependItem(const ClipboardItem &item)
{
    // 置顶条目插到最前；非置顶条目插到置顶区之后
    int row = 0;
    if (!item.isPinned) {
        while (row < m_items.size() && m_items.at(row).isPinned)
            ++row;
    }

    const bool visible = passFilter(item);
    if (visible) {
        // 计算显示行：插入位置 row 之前通过过滤的条目数
        int dispRow = 0;
        for (int i = 0; i < row; ++i)
            if (passFilter(m_items.at(i)))
                ++dispRow;
        beginInsertRows(QModelIndex(), dispRow, dispRow);
        m_items.insert(row, item);
        for (int k = 0; k < m_display.size(); ++k)
            if (m_display.at(k) >= row)
                m_display[k] += 1;
        m_display.insert(dispRow, row);
        endInsertRows();
    } else {
        m_items.insert(row, item);
    }
}

void ClipboardListModel::removeItemById(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            int dispRow = -1;
            for (int k = 0; k < m_display.size(); ++k)
                if (m_display.at(k) == i) { dispRow = k; break; }

            if (dispRow >= 0)
                beginRemoveRows(QModelIndex(), dispRow, dispRow);
            m_items.removeAt(i);
            if (dispRow >= 0) {
                m_display.removeAt(dispRow);
                for (int k = 0; k < m_display.size(); ++k)
                    if (m_display.at(k) > i)
                        m_display[k] -= 1;
                endRemoveRows();
            }
            return;
        }
    }
}

void ClipboardListModel::refreshToTop(int row)
{
    if (row < 0 || row >= m_display.size())
        return;

    const int i = m_display.at(row);
    m_items[i].timestampMs = QDateTime::currentMSecsSinceEpoch();

    // 目标位置：所属区（置顶区/非置顶区）首部（基于显示列表）
    int target = 0;
    if (!m_items.at(i).isPinned) {
        while (target < m_display.size() && m_items.at(m_display.at(target)).isPinned)
            ++target;
    }

    if (row == target) {
        emit dataChanged(index(row), index(row), { TimeRole });
        return;
    }

    beginMoveRows(QModelIndex(), row, row, QModelIndex(), target);
    m_display.move(row, target);
    endMoveRows();
}

ClipboardItem ClipboardListModel::itemAt(int row) const
{
    if (row < 0 || row >= m_display.size())
        return {};
    return m_items.at(m_display.at(row));
}

ClipboardItem ClipboardListModel::itemById(const QString &id) const
{
    for (const ClipboardItem &item : m_items) {
        if (item.id == id)
            return item;
    }
    return {};
}

void ClipboardListModel::updateItem(const ClipboardItem &item)
{
    int i = -1;
    for (int k = 0; k < m_items.size(); ++k) {
        if (m_items.at(k).id == item.id) { i = k; break; }
    }
    if (i < 0)
        return;

    const bool wasPinned = m_items.at(i).isPinned;
    m_items[i] = item;

    int dispRow = -1;
    for (int k = 0; k < m_display.size(); ++k)
        if (m_display.at(k) == i) { dispRow = k; break; }
    const bool nowVisible = passFilter(item);

    if (dispRow >= 0 && nowVisible) {
        if (wasPinned == item.isPinned) {
            emit dataChanged(index(dispRow), index(dispRow));
            return;
        }
        // isPinned 变化：移动到目标区（基于显示列表）
        int target = 0;
        if (!item.isPinned) {
            while (target < m_display.size() && m_items.at(m_display.at(target)).isPinned)
                ++target;
            while (target < m_display.size()
                   && !m_items.at(m_display.at(target)).isPinned
                   && m_items.at(m_display.at(target)).timestampMs > item.timestampMs)
                ++target;
        }
        if (target == dispRow || target == dispRow + 1) {
            emit dataChanged(index(dispRow), index(dispRow));
            return;
        }
        if (beginMoveRows(QModelIndex(), dispRow, dispRow, QModelIndex(), target)) {
            m_display.move(dispRow, (target > dispRow) ? target - 1 : target);
            endMoveRows();
        }
    } else if (dispRow < 0 && nowVisible) {
        // 之前被过滤隐藏，现在可见：插入显示列表
        int target = 0;
        if (!item.isPinned) {
            while (target < m_display.size() && m_items.at(m_display.at(target)).isPinned)
                ++target;
            while (target < m_display.size()
                   && !m_items.at(m_display.at(target)).isPinned
                   && m_items.at(m_display.at(target)).timestampMs > item.timestampMs)
                ++target;
        }
        beginInsertRows(QModelIndex(), target, target);
        m_display.insert(target, i);
        endInsertRows();
    } else if (dispRow >= 0 && !nowVisible) {
        // 之前可见，现在被过滤隐藏：移除
        beginRemoveRows(QModelIndex(), dispRow, dispRow);
        m_display.removeAt(dispRow);
        endRemoveRows();
    }
    // 其它情况：仅更新内部数据（m_items 已更新）
}

void ClipboardListModel::setFilterType(int type)
{
    if (m_filterType == type)
        return;
    m_filterType = type;
    beginResetModel();
    rebuildDisplay();
    endResetModel();
}

int ClipboardListModel::filterType() const
{
    return m_filterType;
}

bool ClipboardListModel::passFilter(const ClipboardItem &item) const
{
    if (m_filterType < 0)
        return true;
    return static_cast<int>(item.type) == m_filterType;
}

void ClipboardListModel::rebuildDisplay()
{
    m_display.clear();
    for (int i = 0; i < m_items.size(); ++i) {
        if (passFilter(m_items.at(i)))
            m_display.append(i);
    }
}
