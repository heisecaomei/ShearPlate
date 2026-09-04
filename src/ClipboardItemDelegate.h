#pragma once

#include <QFileIconProvider>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QStyledItemDelegate>

#include "ClipboardItem.h"

class ClipboardItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ClipboardItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void clearThumbnailCache();
    void setTextShowIcon(bool enabled);
    void reloadIcons();

    // “打开”图标（链接/文件名结尾处）点击区域，与 paint 绘制位置保持一致
    QRect openIconArea(const QRect &card, const ClipboardItem &item) const;

private:
    QPixmap thumbnail(const QString &path) const;
    QIcon fileIcon(const QString &path) const;

    mutable QHash<QString, QPixmap> m_thumbCache;
    mutable QHash<QString, QIcon> m_fileIconCache;
    mutable QFileIconProvider m_fileIconProvider;
    QIcon m_folderIcon;
    QIcon m_textIcon;
    QIcon m_linkIcon;
    QIcon m_pinDefaultIcon;
    QIcon m_pinIcon;
    bool m_textShowIcon = true;
    QFont m_font;
};
