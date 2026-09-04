#include "ClipboardListView.h"

#include <QDrag>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QWheelEvent>

#include "ClipboardItem.h"
#include "ClipboardItemDelegate.h"
#include "ClipboardListModel.h"

ClipboardListView::ClipboardListView(QWidget *parent)
    : QListView(parent)
{
    // 列表内容与面板圆角边缘留白，避免底部 item 卡片贴边/超出面板；
    // 左侧不留 margin，与分类栏间距 = 分类栏右 padding，与分类栏左侧留白等宽
    setViewportMargins(0, 6, 8, 10);
    // 不需要滚动条，鼠标滚轮即可上下滚动
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 图片条目悬停时的大图预览窗（无边框、置顶、不抢焦点、全透明背景）
    m_previewLabel = new QLabel(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    m_previewLabel->setAttribute(Qt::WA_TranslucentBackground);
    m_previewLabel->setAttribute(Qt::WA_ShowWithoutActivating);
    m_previewLabel->setStyleSheet(QStringLiteral(
        "QLabel{background:#ffffff;border:1px solid rgba(0,0,0,70);border-radius:4px;padding:4px;}"));
    m_previewLabel->hide();
}

ClipboardListView::~ClipboardListView()
{
    delete m_previewLabel;
}

void ClipboardListView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_pressPos = event->pos();
    QListView::mousePressEvent(event);
}

void ClipboardListView::mouseMoveEvent(QMouseEvent *event)
{
    // 禁用右键拖动（右键仅用于弹出删除菜单）
    if (event->buttons() & Qt::RightButton) {
        hidePreview();
        return;
    }
    QListView::mouseMoveEvent(event);

    // 图片条目：仅鼠标位于条目左侧的图片缩略图上时，在鼠标旁显示大图预览
    const QModelIndex idx = indexAt(event->pos());
    if (idx.isValid()) {
        const ClipboardItem item = idx.data(ClipboardListModel::ItemRole).value<ClipboardItem>();
        if (item.type == ClipboardItem::Image && !item.imagePath.isEmpty()) {
            // 缩略图区域与 delegate 绘制一致：卡片内左侧 38×38
            const QRect card = visualRect(idx).adjusted(6, 5, -6, -6);
            const QRectF thumbRect(card.left() + 8,
                                   card.top() + (card.height() - 38) / 2.0,
                                   38, 38);
            if (thumbRect.contains(event->pos())) {
                if (m_previewPath != item.imagePath) {
                    m_previewPath = item.imagePath;
                    loadPreviewImage(item.imagePath);
                }
                positionPreview(event->globalPosition().toPoint());
                if (!m_previewLabel->isVisible()) {
                    m_previewLabel->show();
                    m_previewLabel->raise();
                }
                return;
            }
        }
    }
    hidePreview();
}

void ClipboardListView::mouseReleaseEvent(QMouseEvent *event)
{
    // 仅左键点击“打开”图标（紧跟链接/文件名结尾）才触发打开；右键只触发右键菜单
    if (event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            const ClipboardItem item = idx.data(ClipboardListModel::ItemRole).value<ClipboardItem>();
            const bool hasOpenIcon = (item.type == ClipboardItem::Files)
                || (item.type == ClipboardItem::Text && item.linkType() != ClipboardItem::NoLink);
            if (hasOpenIcon) {
                auto *delegate = qobject_cast<ClipboardItemDelegate *>(itemDelegate());
                if (delegate) {
                    const QRect card = visualRect(idx).adjusted(6, 5, -6, -6);
                    if (delegate->openIconArea(card, item).contains(event->pos())) {
                        emit openIconClicked(idx);
                        event->accept();
                        return;
                    }
                }
            }
        }
    }
    QListView::mouseReleaseEvent(event);
}

void ClipboardListView::wheelEvent(QWheelEvent *event)
{
    hidePreview();
    QListView::wheelEvent(event);
}

void ClipboardListView::leaveEvent(QEvent *event)
{
    hidePreview();
    QListView::leaveEvent(event);
}

void ClipboardListView::startDrag(Qt::DropActions)
{
    emit dragStarted();

    const QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty()) {
        emit dragFinished();
        return;
    }

    QMimeData *data = model()->mimeData(indexes);
    if (!data) {
        emit dragFinished();
        return;
    }

    auto *drag = new QDrag(this);
    drag->setMimeData(data);
    const QRect r = visualRect(indexes.first());
    if (!r.isEmpty()) {
        drag->setPixmap(viewport()->grab(r));
        // 以鼠标左键按下时在条目面板上的位置作为拖拽锚点
        drag->setHotSpot(m_pressPos - r.topLeft());
    }

    // 支持剪切(移动)与复制：左键拖动=复制，左键+Shift 拖动=剪切
    const Qt::DropAction result = drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::CopyAction);

    if (result == Qt::MoveAction)
        emit itemMoved(indexes.first()); // 剪切成功：删除剪贴板当前条目

    emit dragFinished();
}

void ClipboardListView::loadPreviewImage(const QString &path)
{
    QPixmap pm(path);
    if (pm.isNull()) {
        m_previewLabel->clear();
        return;
    }
    const QSize maxSize(320, 240);
    QSize s = pm.size();
    s.scale(maxSize, Qt::KeepAspectRatio);
    m_previewLabel->setPixmap(pm.scaled(s, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_previewLabel->adjustSize();
}

void ClipboardListView::positionPreview(const QPoint &globalPos)
{
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect();

    QPoint p = globalPos + QPoint(18, 14); // 鼠标右下方
    if (avail.isValid()) {
        if (p.x() + m_previewLabel->width() > avail.right())
            p.setX(globalPos.x() - m_previewLabel->width() - 18); // 超出右边界则放左侧
        if (p.y() + m_previewLabel->height() > avail.bottom())
            p.setY(globalPos.y() - m_previewLabel->height() - 14); // 超出底部则放上方
    }
    m_previewLabel->move(p);
}

void ClipboardListView::hidePreview()
{
    if (m_previewLabel && m_previewLabel->isVisible())
        m_previewLabel->hide();
    m_previewPath.clear();
}
