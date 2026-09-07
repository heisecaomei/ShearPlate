#include "ClipboardItemDelegate.h"

#include <QFileInfo>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>

#include "ClipboardItem.h"
#include "ClipboardListModel.h"
#include "Theme.h"
#include "Util.h"

ClipboardItemDelegate::ClipboardItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    m_folderIcon = loadIcon(QStringLiteral("images/filelist.png"));
    m_textIcon = loadThemedIcon(QStringLiteral("text.png"));
    m_linkIcon = loadThemedIcon(QStringLiteral("link.png"));
    m_pinDefaultIcon = loadThemedIcon(QStringLiteral("pin_default.png"));
    m_pinIcon = loadThemedIcon(QStringLiteral("pin.png"));

    // 条目文字字体：微软雅黑 9pt
    m_font = QFont(QStringLiteral("Microsoft YaHei"));
    m_font.setPointSizeF(9);
}

QPixmap ClipboardItemDelegate::thumbnail(const QString &path) const
{
    const auto it = m_thumbCache.constFind(path);
    if (it != m_thumbCache.constEnd())
        return it.value();

    QPixmap pm(path);
    if (!pm.isNull()) {
        pm = pm.scaledToHeight(52, Qt::SmoothTransformation);
        m_thumbCache.insert(path, pm);
    }
    return pm;
}

QIcon ClipboardItemDelegate::fileIcon(const QString &path) const
{
    if (path.isEmpty())
        return QIcon();

    const auto it = m_fileIconCache.constFind(path);
    if (it != m_fileIconCache.constEnd())
        return it.value();

    const QIcon icon = m_fileIconProvider.icon(QFileInfo(path));
    m_fileIconCache.insert(path, icon);
    return icon;
}

void ClipboardItemDelegate::clearThumbnailCache()
{
    m_thumbCache.clear();
    m_fileIconCache.clear();
}

void ClipboardItemDelegate::setTextShowIcon(bool enabled)
{
    m_textShowIcon = enabled;
}

void ClipboardItemDelegate::reloadIcons()
{
    m_textIcon = loadThemedIcon(QStringLiteral("text.png"));
    m_linkIcon = loadThemedIcon(QStringLiteral("link.png"));
    m_pinDefaultIcon = loadThemedIcon(QStringLiteral("pin_default.png"));
    m_pinIcon = loadThemedIcon(QStringLiteral("pin.png"));
}

QRect ClipboardItemDelegate::openIconArea(const QRect &card, const ClipboardItem &item) const
{
    const QFontMetrics fm(m_font);
    const int iconSize = fm.height();

    if (item.type == ClipboardItem::Files) {
        const qreal textLeft = card.left() + 48;
        const qreal textWidth = card.width() - 48 - 46;
        const int iconY = card.top() + (card.height() - 32) / 2;

        if (item.filePaths.size() > 1) {
            // 多文件：图标紧跟“共x个文件”标题结尾
            const qreal titleTextWidth = textWidth - iconSize - 4;
            const QString elidedTitle = fm.elidedText(item.preview(), Qt::ElideRight, int(titleTextWidth));
            const qreal titleW = fm.horizontalAdvance(elidedTitle);
            return QRect(int(textLeft + titleW + 4), iconY, iconSize, iconSize);
        }

        // 单文件：图标紧跟文件名结尾（垂直居中于文字行）
        const QRectF textRect(textLeft, card.top() + 6, textWidth, card.height() - 12);
        const qreal textW = textRect.width() - iconSize - 4;
        const QString elided = fm.elidedText(item.preview(), Qt::ElideRight, int(textW));
        const qreal tw = fm.horizontalAdvance(elided);
        return QRect(int(textRect.left() + tw + 4),
                     int(textRect.center().y() - iconSize / 2.0),
                     iconSize, iconSize);
    }

    if (item.type == ClipboardItem::Text && item.linkType() != ClipboardItem::NoLink) {
        QRectF textRect;
        if (m_textShowIcon && !m_textIcon.isNull()) {
            textRect = QRectF(card.left() + 54, card.top() + 6,
                              card.width() - 54 - 12, card.height() - 12);
        } else {
            textRect = QRectF(card.left() + 12, card.top() + 6,
                              card.width() - 24, card.height() - 12);
        }
        const qreal textWidth = textRect.width() - iconSize - 4;
        const QString elided = fm.elidedText(item.preview(), Qt::ElideRight, int(textWidth));
        const qreal tw = fm.horizontalAdvance(elided);
        return QRect(int(textRect.left() + tw + 4),
                     int(textRect.center().y() - iconSize / 2.0),
                     iconSize, iconSize);
    }

    return QRect();
}

QSize ClipboardItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    const int h = 79; // 所有类型条目高度统一
    return QSize(200, h);
}

void ClipboardItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    const ClipboardItem item = index.data(ClipboardListModel::ItemRole).value<ClipboardItem>();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF card = QRectF(option.rect).adjusted(6, 5, -6, -6);

    const bool hover = (option.state & QStyle::State_MouseOver);
    const bool selected = (option.state & QStyle::State_Selected);
    const bool missing = item.missing;

    const ThemeColors tc = Theme::current();
    QColor bg = item.isPinned ? tc.pinnedBg : tc.cardBg;
    QColor border = item.isPinned ? tc.pinnedBorder : tc.cardBorder;
    if (missing) {
        // 失效统一普通底色；不响应 hover / 选中高亮
        bg = tc.cardBg;
        border = tc.cardBorder;
    } else if (hover || selected) {
        bg = tc.cardBgHover;
        border = tc.cardBorderHover;
    }

    painter->setPen(QPen(border, 1));
    painter->setBrush(bg);
    painter->drawRoundedRect(card, 8, 8);

    // 失效：整体灰化（图标/文字降透明度），文字用次要色，不绘制图钉与打开图标
    const QColor mainText = missing ? tc.textSecondary : tc.textPrimary;
    if (missing)
        painter->setOpacity(0.6);

    // 右上角图钉（12×12，随主题与置顶状态切换）
    const QIcon &pinIcon = item.isPinned ? m_pinIcon : m_pinDefaultIcon;
    if (!missing && !pinIcon.isNull()) {
        QRect pinRect(int(card.right() - 24), int(card.top() + 6), 12, 12);
        pinIcon.paint(painter, pinRect);
    }

    const QFontMetrics fm(m_font);

    if (item.type == ClipboardItem::Image) {
        const qreal iconY = card.top() + (card.height() - 38) / 2.0;
        const QRectF iconRect(card.left() + 8, iconY, 38, 38);
        const QPixmap pm = thumbnail(item.imagePath);
        if (!pm.isNull()) {
            QSize s = pm.size();
            s.scale(iconRect.size().toSize(), Qt::KeepAspectRatio);
            QRectF dest(0, 0, s.width(), s.height());
            dest.moveCenter(iconRect.center());
            painter->drawPixmap(dest.toRect(), pm);
        }

        const QRectF textRect(card.left() + 54, card.top() + 6,
                              card.width() - 54 - 12, card.height() - 12);
        painter->setFont(m_font);
        painter->setPen(mainText);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          item.screenshot ? QStringLiteral("截图") : QStringLiteral("图片"));
    } else if (item.type == ClipboardItem::Files) {
        // 文件图标：多个/文件夹用 filelist.png，否则用系统关联图标（32px，靠左垂直居中）
        const qreal iconY = card.top() + (card.height() - 32) / 2.0;
        const QRectF iconRect(card.left() + 8, iconY, 32, 32);
        const QIcon ficon = item.useFolderIcon() ? m_folderIcon : fileIcon(item.firstFilePath());
        if (!ficon.isNull())
            ficon.paint(painter, iconRect.toRect());

        const bool multi = item.filePaths.size() > 1;
        const qreal textLeft = card.left() + 48;
        const qreal rightPad = 24; // 图钉(12)+间距(4)+padding(8)，文字延伸至卡片右边缘
        const qreal textWidth = card.width() - 48 - rightPad;
        const int lineH = QFontMetrics(m_font).height();

        if (multi) {
            // 多文件：标题“共x个文件”顶部与 logo 对齐，标题结尾处 link 图标（点击才打开文件夹）
            painter->setFont(m_font);
            painter->setPen(mainText);

            const int linkIconSize = fm.height();
            const qreal titleTextWidth = textWidth - linkIconSize - 4;
            const QString elidedTitle = fm.elidedText(item.preview(), Qt::ElideRight, int(titleTextWidth));
            painter->drawText(QRectF(textLeft, iconRect.top(), titleTextWidth, lineH),
                              Qt::AlignLeft | Qt::AlignTop, elidedTitle);

            if (!missing && !m_linkIcon.isNull()) {
                const qreal titleW = fm.horizontalAdvance(elidedTitle);
                const QRect linkRect(int(textLeft + titleW + 4),
                                     int(iconRect.top()),
                                     linkIconSize, linkIconSize);
                m_linkIcon.paint(painter, linkRect);
            }

            QStringList names;
            for (const QString &p : item.filePaths)
                names << QFileInfo(p).fileName();
            const QString list = names.join(QStringLiteral("、"));

            QFont subFont = m_font;
            subFont.setPointSizeF(subFont.pointSizeF() - 0.5);
            painter->setFont(subFont);
            painter->setPen(tc.textSecondary);
            const QRectF listRect(textLeft, iconRect.top() + lineH + 2, textWidth, lineH);
            const QString elided = QFontMetrics(subFont).elidedText(list, Qt::ElideRight, int(listRect.width()));
            painter->drawText(listRect, Qt::AlignLeft | Qt::AlignTop, elided);
        } else {
            // 单文件：文件名结尾处 link 图标（点击打开所在目录并预选）
            painter->setFont(m_font);
            painter->setPen(mainText);
            const int linkIconSize = fm.height();
            const QRectF textRect(textLeft, card.top() + 6, textWidth, card.height() - 12);
            const qreal textW = textRect.width() - linkIconSize - 4;
            const QString elided = fm.elidedText(item.preview(), Qt::ElideRight, int(textW));
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

            if (!missing && !m_linkIcon.isNull()) {
                const qreal tw = fm.horizontalAdvance(elided);
                const QRect linkRect(int(textRect.left() + tw + 4),
                                     int(textRect.center().y() - linkIconSize / 2.0),
                                     linkIconSize, linkIconSize);
                m_linkIcon.paint(painter, linkRect);
            }
        }

    } else {
        QRectF textRect;
        if (m_textShowIcon && !m_textIcon.isNull()) {
            // 文本类型图标 text.png（38px 靠左，与其它类型统一）
            const qreal iconY = card.top() + (card.height() - 38) / 2.0;
            const QRectF iconRect(card.left() + 8, iconY, 38, 38);
            m_textIcon.paint(painter, iconRect.toRect());
            textRect = QRectF(card.left() + 54, card.top() + 6,
                              card.width() - 54 - 12, card.height() - 12);
        } else {
            // 不显示图标，文本占满整行
            textRect = QRectF(card.left() + 12, card.top() + 6,
                              card.width() - 24, card.height() - 12);
        }

        const ClipboardItem::LinkType lt = item.linkType();
        const bool isLink = (lt != ClipboardItem::NoLink);

        const int linkIconSize = fm.height(); // 图标与文字等高
        const qreal textWidth = isLink ? (textRect.width() - linkIconSize - 4) : textRect.width();

        painter->setFont(m_font);
        painter->setPen(mainText);
        const QString elided = fm.elidedText(item.preview(), Qt::ElideRight, int(textWidth));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

        // 链接图标紧跟链接文字结尾，点击图标才打开链接
        if (!missing && isLink && !m_linkIcon.isNull()) {
            const qreal textW = fm.horizontalAdvance(elided);
            QRect iconRect(int(textRect.left() + textW + 4),
                           int(textRect.center().y() - linkIconSize / 2.0),
                           linkIconSize, linkIconSize);
            m_linkIcon.paint(painter, iconRect);
        }
    }

    painter->restore();
}
